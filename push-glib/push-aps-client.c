/* push-aps-client.c
 *
 * Copyright (C) 2012 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "push-aps-client.h"
#include "push-debug.h"

/**
 * SECTION:push-aps-client
 * @title: PushApsClient
 * @short_description: Client to send notifications to APS (iOS) devices.
 *
 * #PushApsClient provides a convenient way to send notifications to Apple
 * iOS device. Simply create an instance of the client with your TLS
 * credentials registered with the App Store, connect, and then use
 * push_aps_client_deliver_async() to deliver notifications.
 *
 * It is important that consumers connect to the "identity-removed" signal
 * so that they may remove devices that are no longer valid. Failure to do
 * so may cause Apple to revoke your access to APS for a period of time.
 */

#define PUSH_APS_CLIENT_TIMEOUT_SECONDS 2

G_DEFINE_TYPE(PushApsClient, push_aps_client, G_TYPE_OBJECT)

#pragma pack(1)
typedef struct
{
   guint32 time;
   guint16 token_length;
   guint8 token[32];
} FeedbackMessage;
#pragma pack()

struct _PushApsClientPrivate
{
   PushApsClientMode mode;
   GTlsCertificate *tls_certificate;
   GError *tls_error;
   gchar *ssl_cert_file;
   gchar *ssl_key_file;
   GIOStream *gateway_stream;
   GHashTable *results;
   guint last_id;
   FeedbackMessage fb_msg;
   guint8 gw_read_buf[6];
   guint feedback_interval;
   guint feedback_handler;

   GCancellable *dispose_cancellable;

   guint state;
   GQueue *queue;
};

enum
{
   PROP_0,
   PROP_MODE,
   PROP_SSL_CERT_FILE,
   PROP_SSL_KEY_FILE,
   PROP_TLS_CERTIFICATE,
   PROP_FEEDBACK_INTERVAL,
   LAST_PROP
};

enum
{
   STATE_0,
   STATE_CONNECTING,
   STATE_CONNECTED,
   STATE_DISPOSED,
};

enum
{
   IDENTITY_REMOVED,
   LAST_SIGNAL
};

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];

static void push_aps_client_try_load_tls (PushApsClient *client);

static gchar *
_hex_encode (const guint8 *buffer,
             gsize         len)
{
   GString *str;
   guint i;

   g_assert(buffer);
   g_assert(len);

   str = g_string_new(NULL);
   for (i = 0; i < len; i++) {
      g_string_append_printf(str, "%02x", buffer[i]);
   }
   return g_string_free(str, FALSE);
}

static const gchar *
get_error_message (PushApsClientError error)
{
   switch ((guint)error) {
   case PUSH_APS_CLIENT_ERROR_PROCESSING_ERROR:
      return _("Processing Error");
   case PUSH_APS_CLIENT_ERROR_MISSING_DEVICE_TOKEN:
      return _("Missing Device Token");
   case PUSH_APS_CLIENT_ERROR_MISSING_TOPIC:
      return _("Missing Topic");
   case PUSH_APS_CLIENT_ERROR_MISSING_PAYLOAD:
      return _("Missing Payload");
   case PUSH_APS_CLIENT_ERROR_INVALID_TOKEN_SIZE:
      return _("Invalid Token Size");
   case PUSH_APS_CLIENT_ERROR_INVALID_TOPIC_SIZE:
      return _("Invalid Topic Size");
   case PUSH_APS_CLIENT_ERROR_INVALID_PAYLOAD_SIZE:
      return _("Invalid Payload Size");
   case PUSH_APS_CLIENT_ERROR_INVALID_TOKEN:
      return _("Invalid Token");
   default:
      return _("An unknown error ocurred during delivery.");
   }
   g_assert_not_reached();
}

static void
push_aps_client_dispatch_error (PushApsClient      *client,
                                guint32             result_id,
                                PushApsClientError  code)
{
   PushApsClientPrivate *priv;
   GSimpleAsyncResult *simple;
   PushApsIdentity *identity;
   const gchar *device_token;

   ENTRY;

   g_assert(PUSH_IS_APS_CLIENT(client));

   priv = client->priv;

   if ((simple = g_hash_table_lookup(priv->results, &result_id))) {
      if (code == PUSH_APS_CLIENT_ERROR_INVALID_TOKEN) {
         device_token = g_object_get_data(G_OBJECT(simple), "device-token");
         if (device_token) {
            identity = g_object_new(PUSH_TYPE_APS_IDENTITY,
                                    "device-token", device_token,
                                    NULL);
            g_signal_emit(client, gSignals[IDENTITY_REMOVED], 0, identity);
            g_object_unref(identity);
         }
      }

      g_simple_async_result_set_error(simple,
                                      PUSH_APS_CLIENT_ERROR,
                                      code,
                                      "%s",
                                      get_error_message(code));
      g_simple_async_result_complete_in_idle(simple);
      g_hash_table_remove(priv->results, &result_id);
   }

   EXIT;
}

static void
push_aps_client_dispatch (PushApsClient *client)
{
   PushApsClientPrivate *priv;
   GSimpleAsyncResult *simple;
   GHashTableIter iter;

   ENTRY;

   g_assert(PUSH_IS_APS_CLIENT(client));

   priv = client->priv;

   g_hash_table_iter_init(&iter, priv->results);
   while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&simple)) {
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
      g_simple_async_result_complete_in_idle(simple);
   }

   EXIT;
}

static void
push_aps_client_read_gateway_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
   PushApsClient *client = user_data;
   const guint8 *buffer;
   GInputStream *input = (GInputStream *)object;
   guint32 result_id;
   GError *error = NULL;
   guint8 command;
   guint8 status;
   gssize ret;

   ENTRY;

   g_assert(PUSH_IS_APS_CLIENT(client));
   g_assert(G_IS_INPUT_STREAM(input));

   buffer = client->priv->gw_read_buf;
   ret = g_input_stream_read_finish(input, result, &error);

   switch (ret) {
   case -1:
      /* TODO: push_aps_client_fail(client); */
      EXIT;
   case 0:
      /* EOF */
      push_aps_client_dispatch(client);
      EXIT;
   default:
      DUMP_BYTES(gateway, ((guint8 *)&client->priv->gw_read_buf), ret);
      g_assert_cmpint(ret, ==, 6);
      command = buffer[0];
      g_assert_cmpint(command, ==, 8);
      status = buffer[1];
      g_assert(status <= 8 || status == 255);
      result_id = GUINT32_FROM_BE(*(guint32 *)&buffer[2]);
      push_aps_client_dispatch_error(client, result_id, status);
      g_input_stream_read_async(input,
                                client->priv->gw_read_buf,
                                sizeof client->priv->gw_read_buf,
                                G_PRIORITY_DEFAULT,
                                NULL, /* priv->shutdown, */
                                push_aps_client_read_gateway_cb,
                                client);
      EXIT;
   }

   g_assert_not_reached();
}

static void
push_aps_client_connect_event_cb (GSocketClient      *socket_client,
                                  GSocketClientEvent  event,
                                  GSocketConnectable *connectable,
                                  GIOStream          *connection,
                                  PushApsClient      *client)
{
   ENTRY;

   g_assert(G_IS_SOCKET_CLIENT(socket_client));
   g_assert(PUSH_IS_APS_CLIENT(client));

   if (event == G_SOCKET_CLIENT_TLS_HANDSHAKING) {
      g_tls_connection_set_certificate(G_TLS_CONNECTION(connection),
                                       client->priv->tls_certificate);
   }

   EXIT;
}

static void
push_aps_client_read_feedback_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
   PushApsClientPrivate *priv;
   PushApsIdentity *identity;
   PushApsClient *client = user_data;
   GInputStream *stream = (GInputStream *)object;
   GError *error = NULL;
   gssize ret;
   gchar *device_token;

   ENTRY;

   g_assert(G_IS_INPUT_STREAM(stream));

   priv = client->priv;

   ret = g_input_stream_read_finish(stream, result, &error);

   switch (ret) {
   case -1:
   case 0:
      EXIT;
   default:
      DUMP_BYTES(feedback, ((guint8 *)&priv->fb_msg), ret);
      if (ret != 38) {
         /* TODO: push_aps_client_fail(client); */
         EXIT;
      }
      priv->fb_msg.time = GUINT32_FROM_BE(priv->fb_msg.time);
      priv->fb_msg.token_length = GUINT16_FROM_BE(priv->fb_msg.token_length);
      if (priv->fb_msg.token_length != 32) {
         /* TODO: push_aps_client_fail(client); */
         EXIT;
      }
      device_token = _hex_encode(priv->fb_msg.token, 32);
      if (!device_token) {
         /* TODO: push_aps_client_fail(client); */
         EXIT;
      }
      identity = g_object_new(PUSH_TYPE_APS_IDENTITY,
                              "device-token", device_token,
                              NULL);
      g_signal_emit(client, gSignals[IDENTITY_REMOVED], 0, identity);
      g_input_stream_read_async(stream,
                                (guint8 *)&client->priv->fb_msg,
                                sizeof client->priv->fb_msg,
                                G_PRIORITY_DEFAULT,
                                NULL, /* priv->shutdown */
                                push_aps_client_read_feedback_cb,
                                client);
      g_object_unref(identity);
      g_free(device_token);
      EXIT;
   }

   g_assert_not_reached();
}

static void
push_aps_client_connect_feedback_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
   GSocketConnection *conn;
   PushApsClient *client = user_data;
   GSocketClient *socket_client = (GSocketClient *)object;
   GInputStream *stream;
   GError *error = NULL;

   ENTRY;

   g_assert(G_IS_SOCKET_CLIENT(socket_client));
   g_assert(PUSH_IS_APS_CLIENT(client));

   if (!(conn = g_socket_client_connect_to_host_finish(socket_client,
                                                       result,
                                                       &error))) {
      g_warning("Failed to connect to APS feedback: %s", error->message);
      g_error_free(error);
      EXIT;
   } else {
      stream = g_io_stream_get_input_stream(G_IO_STREAM(conn));
      g_input_stream_read_async(stream,
                                (guint *)&client->priv->fb_msg,
                                sizeof client->priv->fb_msg,
                                G_PRIORITY_DEFAULT,
                                NULL, /* priv->shutdown */
                                push_aps_client_read_feedback_cb,
                                client);
      g_object_unref(conn);
      EXIT;
   }

   g_assert_not_reached();
}

static gboolean
push_aps_client_feedback_cb (gpointer data)
{
   PushApsClientPrivate *priv;
   PushApsClient *client = data;
   GSocketClient *socket_client;
   const gchar *host;
   gboolean ret = TRUE;

   ENTRY;

   g_assert(PUSH_IS_APS_CLIENT(client));

   priv = client->priv;

   socket_client = g_object_new(G_TYPE_SOCKET_CLIENT,
                                "family", G_SOCKET_FAMILY_IPV4,
                                "protocol", G_SOCKET_PROTOCOL_TCP,
                                "timeout", 60,
                                "tls", TRUE,
                                "type", G_SOCKET_TYPE_STREAM,
                                NULL);
   g_signal_connect(socket_client,
                    "event",
                    G_CALLBACK(push_aps_client_connect_event_cb),
                    client);
   host = (priv->mode == PUSH_APS_CLIENT_PRODUCTION) ?
          "feedback.push.apple.com" :
          "feedback.sandbox.push.apple.com";
   g_socket_client_connect_to_host_async(socket_client,
                                         host,
                                         2196,
                                         NULL, /* TODO: priv->shutdown */
                                         push_aps_client_connect_feedback_cb,
                                         client);
   g_object_unref(socket_client);

   RETURN(ret);
}

static void
push_aps_client_write (PushApsClient *client,
                       GByteArray    *buffer)
{
   PushApsClientPrivate *priv;
   GOutputStream *stream;
   GError *error = NULL;
   gsize bytes_written;

   ENTRY;

   g_assert(PUSH_IS_APS_CLIENT(client));
   g_assert(client->priv->gateway_stream);
   g_assert(buffer);

   priv = client->priv;

   stream = g_io_stream_get_output_stream(priv->gateway_stream);
   g_assert(stream);

   /*
    * TODO: Probably should do this asynchronously.
    */

   DUMP_BYTES(buffer, buffer->data, buffer->len);

   if (!g_output_stream_write_all(stream,
                                  (gconstpointer)buffer->data,
                                  buffer->len,
                                  &bytes_written,
                                  NULL,
                                  &error)) {
      g_warning("Failed to write to APS stream: %s", error->message);
      g_error_free(error);
      EXIT;
   }

   g_assert_cmpint(bytes_written, ==, buffer->len);

   EXIT;
}

static void
push_aps_client_queue (PushApsClient *client,
                       GByteArray    *buffer)
{
   PushApsClientPrivate *priv;

   g_assert(PUSH_IS_APS_CLIENT(client));
   g_assert(buffer);

   ENTRY;

   priv = client->priv;

   if (priv->state == STATE_CONNECTED) {
      push_aps_client_write(client, buffer);
   } else {
      g_queue_push_tail(priv->queue, g_byte_array_ref(buffer));
   }

   EXIT;
}

static void
push_aps_client_connect_gateway_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GSocketConnection *conn;
   GSocketClient *socket_client = (GSocketClient *)object;
   PushApsClient *client;
   GInputStream *input;
   GByteArray *buffer;
   GError *error = NULL;

   ENTRY;

   g_assert(G_IS_SOCKET_CLIENT(socket_client));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   client = PUSH_APS_CLIENT(g_async_result_get_source_object(user_data));

   if (!(conn = g_socket_client_connect_to_host_finish(socket_client,
                                                       result,
                                                       &error))) {
      if (client->priv->state != STATE_DISPOSED) {
         client->priv->state = STATE_0;
      }
      g_simple_async_result_take_error(simple, error);
   } else {
      client->priv->state = STATE_CONNECTED;
      g_clear_object(&client->priv->gateway_stream);
      client->priv->gateway_stream = G_IO_STREAM(conn);

      /*
       * Start reading responses from the TLS stream.
       */
      input = g_io_stream_get_input_stream(G_IO_STREAM(conn));
      g_input_stream_read_async(input,
                                client->priv->gw_read_buf,
                                sizeof client->priv->gw_read_buf,
                                G_PRIORITY_DEFAULT,
                                NULL, /* priv->shutdown, */
                                push_aps_client_read_gateway_cb,
                                client);

      /*
       * Setup our timeout to connect to feedback on interval.
       */
      if (!client->priv->feedback_handler) {
         client->priv->feedback_handler =
            g_timeout_add_seconds(60 * client->priv->feedback_interval,
                                  (GSourceFunc)push_aps_client_feedback_cb,
                                  client);
      }

      /*
       * Flush any pending requests.
       */
      while ((buffer = g_queue_pop_head(client->priv->queue))) {
         push_aps_client_write(client, buffer);
         g_byte_array_unref(buffer);
      }
   }

   g_simple_async_result_set_op_res_gboolean(simple, !!conn);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
   g_object_unref(client);

   EXIT;
}

/**
 * push_aps_client_connect_async:
 * @client: (in): A #PushApsClient.
 * @cancellable: (allow-none): A #GCancellable, or %NULL.
 * @callback: A callback to execute.
 * @user_data: User data for @callback.
 *
 * Asynchronously connects to the Apple Push Notification gateway. A TLS
 * connection is used using the TLS certificate provided. This will have
 * been loaded using the "ssl-cert-file" and "ssl-key-file" properties
 * or the "tls-certificate" property.
 *
 * Upon success or error, @callback will be executed and should call
 * push_aps_client_connect_finish() to comlete the request.
 *
 * If "mode" is set to PUSH_APS_CLIENT_SANDBOX, then the client will connect
 * to "gateway.sandbox.push.apple.com" instead of "gateway.push.apple.com".
 */
static void
push_aps_client_connect_async (PushApsClient       *client,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
   PushApsClientPrivate *priv;
   GSimpleAsyncResult *simple;
   GSocketClient *socket_client;
   const gchar *host;

   ENTRY;

   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(client->priv->state == STATE_0);

   priv = client->priv;

   priv->state = STATE_CONNECTING;

   if (priv->tls_error) {
      g_simple_async_report_gerror_in_idle(G_OBJECT(client),
                                           callback,
                                           user_data,
                                           priv->tls_error);
      EXIT;
   }

   if (!priv->tls_certificate) {
      g_simple_async_report_error_in_idle(G_OBJECT(client),
                                          callback,
                                          user_data,
                                          PUSH_APS_CLIENT_ERROR,
                                          PUSH_APS_CLIENT_ERROR_TLS_NOT_AVAILABLE,
                                          _("TLS has not yet been configured."));
      EXIT;
   }

   if (priv->gateway_stream) {
      g_simple_async_report_error_in_idle(G_OBJECT(client),
                                          callback,
                                          user_data,
                                          PUSH_APS_CLIENT_ERROR,
                                          PUSH_APS_CLIENT_ERROR_ALREADY_CONNECTED,
                                          _("The client is already connected."));
      EXIT;
   }

   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      push_aps_client_connect_async);

   socket_client = g_object_new(G_TYPE_SOCKET_CLIENT,
                                "family", G_SOCKET_FAMILY_IPV4,
                                "protocol", G_SOCKET_PROTOCOL_TCP,
                                "timeout", 60,
                                "tls", TRUE,
                                "type", G_SOCKET_TYPE_STREAM,
                                NULL);
   g_signal_connect(socket_client,
                    "event",
                    G_CALLBACK(push_aps_client_connect_event_cb),
                    client);
   host = (priv->mode == PUSH_APS_CLIENT_PRODUCTION) ?
          "gateway.push.apple.com" :
          "gateway.sandbox.push.apple.com";
   g_socket_client_connect_to_host_async(socket_client,
                                         host,
                                         2195,
                                         cancellable,
                                         push_aps_client_connect_gateway_cb,
                                         simple);
   g_object_unref(socket_client);

   EXIT;
}

/**
 * push_aps_client_finish:
 * @client: (in): A #PushApsClient.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to connect to the APS gateway.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
static gboolean
push_aps_client_connect_finish (PushApsClient  *client,
                                GAsyncResult   *result,
                                GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

static inline guint8
from_hex (guint8 c)
{
   switch (c) {
   case '0': return 0;
   case '1': return 1;
   case '2': return 2;
   case '3': return 3;
   case '4': return 4;
   case '5': return 5;
   case '6': return 6;
   case '7': return 7;
   case '8': return 8;
   case '9': return 9;
   case 'a': return 0xa;
   case 'b': return 0xb;
   case 'c': return 0xc;
   case 'd': return 0xd;
   case 'e': return 0xe;
   case 'f': return 0xf;
   default: return 0;
   }
}

static GByteArray *
push_aps_client_encode (PushApsClient *client,
                        const gchar   *device_token,
                        GDateTime     *expires_at,
                        const gchar   *message,
                        guint32        request_id)
{
   GByteArray *ret = NULL;
   guint32 b32;
   guint16 b16;
   guchar *data;
   guint8 b8;
   guint i;
   gsize len;

   ENTRY;

   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), NULL);
   g_return_val_if_fail(device_token, NULL);
   g_return_val_if_fail(message, NULL);

   ret = g_byte_array_sized_new(64);

   /*
    * Command (we talk enhanced notification format).
    */
   b8 = 1;
   g_byte_array_append(ret, &b8, 1);

   /*
    * Client side generated identifier for error-response packet.
    */
   b32 = GUINT32_TO_BE(request_id);
   g_byte_array_append(ret, (guint8 *)&b32, 4);

   /*
    * Expiry field (A fixed UNIX epoch date in UTC seconds).
    * The expiry value should be in network order. If value is positive,
    * APNs tries to deliver the notification at least once.
    */
   b32 = 0;
   if (expires_at) {
      b32 = (guint32)g_date_time_to_unix(expires_at);
      b32 = GUINT32_TO_BE(b32);
   }
   g_byte_array_append(ret, (guint8 *)&b32, 4);

   /*
    * Get token length.
    */
   len = strlen(device_token) / 2;
   b16 = GUINT16_TO_BE(len);
   g_assert_cmpint(len % 2, ==, 0);

   /*
    * Encode the token from hex.
    */
   data = g_malloc0(len);
   for (i = 0; i < len; i++) {
      guint8 v = (from_hex(device_token[i*2]) << 4)
               | (from_hex(device_token[i*2+1]));
      data[i] = v;
   }

   /*
    * Add token length and token to buffer.
    */
   g_byte_array_append(ret, (guint8 *)&b16, 2);
   g_byte_array_append(ret, (guint8 *)data, len);
   g_free(data);

   /*
    * Payload length and payload.
    */
   len = strlen(message);
   b16 = GUINT16_TO_BE(len);
   g_byte_array_append(ret, (guint8 *)&b16, 2);
   g_byte_array_append(ret, (guint8 *)message, len);

   RETURN(ret);
}

static gboolean
push_aps_client_complete_result (GSimpleAsyncResult *simple)
{
   PushApsClient *client;
   guint32 request_id;

   ENTRY;

   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));
   client = PUSH_APS_CLIENT(
         g_async_result_get_source_object(
               G_ASYNC_RESULT(simple)));
   g_assert(PUSH_IS_APS_CLIENT(client));
   request_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple),
                                                  "request-id"));

   if (client->priv->results) {
      if (g_hash_table_lookup(client->priv->results, &request_id)) {
         g_simple_async_result_set_op_res_gboolean(simple, TRUE);
         g_simple_async_result_complete_in_idle(simple);
         g_hash_table_remove(client->priv->results, &request_id);
      }
   }

   g_object_unref(simple);
   g_object_unref(client);

   RETURN(FALSE);
}

static void
push_aps_client_connect_gateway_cb2 (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
   PushApsClientPrivate *priv;
   PushApsClient *client = (PushApsClient *)object;
   GError *error = NULL;

   g_assert(PUSH_IS_APS_CLIENT(client));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(result));

   priv = client->priv;

   if (!push_aps_client_connect_finish(client, result, &error)) {
      if (priv->state == STATE_0) {
         /*
          * TODO: Add exponential backoff.
          */
         push_aps_client_connect_async(client,
                                       priv->dispose_cancellable,
                                       push_aps_client_connect_gateway_cb2,
                                       NULL);
         EXIT;
      }
   }

   EXIT;
}

/**
 * push_aps_client_deliver_async:
 * @client: A #PushApsClient.
 * @identity: A #PushApsIdentity.
 * @message: A #PushApsMessage.
 * @cancellable: (allow-none: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to execute upon completion.
 * @user_data: User data for @callback.
 *
 * Asynchronously requests that @message be delivered to @identity.
 * The message is serialized and sent via the Apple push notification
 * gateway who performs the actual delivery to the identified device.
 *
 * @callback MUST call push_aps_client_deliver_finish() with the
 * provided #GAsyncResult.
 */
void
push_aps_client_deliver_async (PushApsClient       *client,
                               PushApsIdentity     *identity,
                               PushApsMessage      *message,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
   PushApsClientPrivate *priv;
   GSimpleAsyncResult *simple;
   const gchar *device_token;
   GByteArray *buffer;
   guint32 *request_id;

   ENTRY;

   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   g_return_if_fail(PUSH_IS_APS_IDENTITY(identity));
   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = client->priv;

   if (priv->tls_error) {
      g_simple_async_report_gerror_in_idle(G_OBJECT(client),
                                           callback,
                                           user_data,
                                           priv->tls_error);
      EXIT;
   }

   /*
    * Start connecting if needed.
    */
   if (priv->state == STATE_0) {
      push_aps_client_connect_async(client,
                                    priv->dispose_cancellable,
                                    push_aps_client_connect_gateway_cb2,
                                    NULL);
   }

   /*
    * Build buffer to deliver to gateway and async result for
    * completion of asynchronous request.
    */
   device_token = push_aps_identity_get_device_token(identity);
   request_id = g_new0(guint32, 1);
   *request_id = ++priv->last_id;
   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      push_aps_client_deliver_async);
   g_simple_async_result_set_check_cancellable(simple, cancellable);
   g_object_set_data_full(G_OBJECT(simple), "device-token",
                          g_strdup(device_token), g_free);
   g_object_set_data(G_OBJECT(simple), "request-id",
                     GINT_TO_POINTER(*request_id));
   buffer = push_aps_client_encode(client,
                                   device_token,
                                   push_aps_message_get_expires_at(message),
                                   push_aps_message_get_json(message),
                                   *request_id);
   g_hash_table_insert(priv->results, request_id, simple);

   /*
    * Add a timeout to complete the request if we don't get notified of
    * success (which is what usually happens).
    */
   g_timeout_add_seconds(1,
                         (GSourceFunc)push_aps_client_complete_result,
                         g_object_ref(simple));

   /*
    * Write bytes to gateway immediately if we can, otherwise queue them.
    */
   push_aps_client_queue(client, buffer);

   g_byte_array_unref(buffer);

   EXIT;
}

/**
 * push_aps_client_deliver_finish:
 * @client: A #PushApsClient.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to push_aps_client_deliver_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
push_aps_client_deliver_finish (PushApsClient  *client,
                                GAsyncResult   *result,
                                GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

PushApsClientMode
push_aps_client_get_mode (PushApsClient *client)
{
   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), 0);
   return client->priv->mode;
}

static void
push_aps_client_set_mode (PushApsClient     *client,
                          PushApsClientMode  mode)
{
   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   g_return_if_fail((mode == PUSH_APS_CLIENT_PRODUCTION) ||
                    (mode == PUSH_APS_CLIENT_SANDBOX));
   client->priv->mode = mode;
}

/**
 * push_aps_client_get_ssl_cert_file:
 * @client: (in): A #PushApsClient.
 *
 * Fetches the "ssl-cert-file" property. This is a path to the file
 * containing the TLS certificate.
 *
 * Returns: A string which should not be modified or freed.
 */
const gchar *
push_aps_client_get_ssl_cert_file (PushApsClient *client)
{
   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), NULL);
   return client->priv->ssl_cert_file;
}

/**
 * push_aps_client_get_ssl_key_file:
 * @client: (in): A #PushApsClient.
 *
 * Fetches the "ssl-key-file" property. This is a path to the file
 * containing the TLS private key.
 *
 * Returns: A string which should not be modified or freed.
 */
const gchar *
push_aps_client_get_ssl_key_file (PushApsClient *client)
{
   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), NULL);
   return client->priv->ssl_key_file;
}

/**
 * push_aps_client_get_tls_certificate:
 * @client: (in): A #PushApsClient.
 *
 * Fetches the #GTlsCertificate used to communicate with the
 * Apple Push Notification gateway.
 *
 * Returns: (transfer none): A #GTlsCertificate.
 */
GTlsCertificate *
push_aps_client_get_tls_certificate (PushApsClient *client)
{
   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), NULL);
   return client->priv->tls_certificate;
}

static void
push_aps_client_set_ssl_cert_file (PushApsClient *client,
                                   const gchar   *ssl_cert_file)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   client->priv->ssl_cert_file = g_strdup(ssl_cert_file);
   push_aps_client_try_load_tls(client);
   EXIT;
}

static void
push_aps_client_set_ssl_key_file (PushApsClient *client,
                                  const gchar   *ssl_key_file)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   client->priv->ssl_key_file = g_strdup(ssl_key_file);
   push_aps_client_try_load_tls(client);
   EXIT;
}

static void
push_aps_client_set_tls_certificate (PushApsClient   *client,
                                     GTlsCertificate *tls_certificate)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   if (tls_certificate) {
      client->priv->tls_certificate = g_object_ref(tls_certificate);
   }
   EXIT;
}

static void
push_aps_client_try_load_tls (PushApsClient *client)
{
   PushApsClientPrivate *priv;
   GTlsCertificate *tls;
   GError *error = NULL;

   g_return_if_fail(PUSH_IS_APS_CLIENT(client));

   priv = client->priv;

   if (priv->ssl_cert_file && priv->ssl_key_file) {
      if (!(tls = g_tls_certificate_new_from_files(priv->ssl_cert_file,
                                                   priv->ssl_key_file,
                                                   &error))) {
         g_clear_error(&priv->tls_error);
         g_warning("TLS Certificate Error: %s", error->message);
         priv->tls_error = error;
      } else {
         push_aps_client_set_tls_certificate(client, tls);
         g_object_unref(tls);
      }
   }
}

/**
 * push_aps_client_get_feedback_interval:
 * @client: (in): A #PushApsClient.
 *
 * Fetches the "feedback-interval" property, the number of minutes between
 * connecting to the feedback service for status information.
 *
 * Returns: A #guint containing the feedback interval in minutes.
 */
guint
push_aps_client_get_feedback_interval (PushApsClient *client)
{
   g_return_val_if_fail(PUSH_IS_APS_CLIENT(client), 0);
   return client->priv->feedback_interval;
}

static void
push_aps_client_set_feedback_interval (PushApsClient *client,
                                       guint          feedback_interval)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_APS_CLIENT(client));
   g_return_if_fail(feedback_interval > 0);
   client->priv->feedback_interval = feedback_interval;
   EXIT;
}

static void
push_aps_client_dispose (GObject *object)
{
   PushApsClientPrivate *priv;
   GHashTableIter iter;
   GHashTable *hash;
   GByteArray *array;
   gpointer key;
   gpointer value;

   ENTRY;

   priv = PUSH_APS_CLIENT(object)->priv;

   priv->state = STATE_DISPOSED;

   if (priv->dispose_cancellable) {
      g_cancellable_cancel(priv->dispose_cancellable);
      g_clear_object(&priv->dispose_cancellable);
   }

   if (priv->gateway_stream) {
      g_io_stream_close(priv->gateway_stream, NULL, NULL);
      g_clear_object(&priv->gateway_stream);
   }

   while ((array = g_queue_pop_head(priv->queue))) {
      g_byte_array_free(array, TRUE);
   }

   g_queue_free(priv->queue);
   priv->queue = NULL;

   if ((hash = priv->results)) {
      priv->results = NULL;
      g_hash_table_iter_init(&iter, hash);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         g_simple_async_result_set_error(value,
                                         PUSH_APS_CLIENT_ERROR,
                                         PUSH_APS_CLIENT_ERROR_CANCELLED,
                                         _("Request was cancelled due to "
                                           "shutting down."));
         g_simple_async_result_complete_in_idle(value);
      }
      g_hash_table_unref(hash);
   }

   if (priv->feedback_handler) {
      g_source_remove(priv->feedback_handler);
      priv->feedback_handler = 0;
   }

   g_clear_object(&priv->tls_certificate);

   EXIT;
}

static void
push_aps_client_finalize (GObject *object)
{
   PushApsClientPrivate *priv;

   ENTRY;

   priv = PUSH_APS_CLIENT(object)->priv;

   g_free(priv->ssl_cert_file);
   priv->ssl_cert_file = NULL;

   g_free(priv->ssl_key_file);
   priv->ssl_key_file = NULL;

   g_clear_error(&priv->tls_error);

   G_OBJECT_CLASS(push_aps_client_parent_class)->finalize(object);

   EXIT;
}

static void
push_aps_client_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
   PushApsClient *client = PUSH_APS_CLIENT(object);

   switch (prop_id) {
   case PROP_FEEDBACK_INTERVAL:
      g_value_set_uint(value, push_aps_client_get_feedback_interval(client));
      break;
   case PROP_MODE:
      g_value_set_enum(value, push_aps_client_get_mode(client));
      break;
   case PROP_SSL_CERT_FILE:
      g_value_set_string(value, push_aps_client_get_ssl_cert_file(client));
      break;
   case PROP_SSL_KEY_FILE:
      g_value_set_string(value, push_aps_client_get_ssl_key_file(client));
      break;
   case PROP_TLS_CERTIFICATE:
      g_value_set_object(value, push_aps_client_get_tls_certificate(client));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_aps_client_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
   PushApsClient *client = PUSH_APS_CLIENT(object);

   switch (prop_id) {
   case PROP_FEEDBACK_INTERVAL:
      push_aps_client_set_feedback_interval(client, g_value_get_uint(value));
      break;
   case PROP_MODE:
      push_aps_client_set_mode(client, g_value_get_enum(value));
      break;
   case PROP_SSL_CERT_FILE:
      push_aps_client_set_ssl_cert_file(client, g_value_get_string(value));
      break;
   case PROP_SSL_KEY_FILE:
      push_aps_client_set_ssl_key_file(client, g_value_get_string(value));
      break;
   case PROP_TLS_CERTIFICATE:
      push_aps_client_set_tls_certificate(client, g_value_get_object(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_aps_client_class_init (PushApsClientClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->dispose = push_aps_client_dispose;
   object_class->finalize = push_aps_client_finalize;
   object_class->get_property = push_aps_client_get_property;
   object_class->set_property = push_aps_client_set_property;
   g_type_class_add_private(object_class, sizeof(PushApsClientPrivate));

   gParamSpecs[PROP_FEEDBACK_INTERVAL] =
      g_param_spec_uint("feedback-interval",
                        _("Feedback Interval"),
                        _("The interval minutes between feedback connections."),
                        1,
                        G_MAXUINT,
                        10,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_FEEDBACK_INTERVAL,
                                   gParamSpecs[PROP_FEEDBACK_INTERVAL]);

   gParamSpecs[PROP_MODE] =
      g_param_spec_enum("mode",
                        _("Mode"),
                        _("The mode of the client."),
                        PUSH_TYPE_APS_CLIENT_MODE,
                        PUSH_APS_CLIENT_PRODUCTION,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_MODE,
                                   gParamSpecs[PROP_MODE]);

   gParamSpecs[PROP_SSL_CERT_FILE] =
      g_param_spec_string("ssl-cert-file",
                          _("SSL Certificate File"),
                          _("Path to SSL certificate file."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_SSL_CERT_FILE,
                                   gParamSpecs[PROP_SSL_CERT_FILE]);

   gParamSpecs[PROP_SSL_KEY_FILE] =
      g_param_spec_string("ssl-key-file",
                          _("SSL Key File"),
                          _("Path to SSL key file."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_SSL_KEY_FILE,
                                   gParamSpecs[PROP_SSL_KEY_FILE]);

   gParamSpecs[PROP_TLS_CERTIFICATE] =
      g_param_spec_object("tls-certificate",
                          _("TLS Certificate"),
                          _("A TLS certificate to communicate with."),
                          G_TYPE_TLS_CERTIFICATE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_TLS_CERTIFICATE,
                                   gParamSpecs[PROP_TLS_CERTIFICATE]);

   gSignals[IDENTITY_REMOVED] =
      g_signal_new("identity-removed",
                   PUSH_TYPE_APS_CLIENT,
                   G_SIGNAL_RUN_FIRST,
                   0,
                   NULL,
                   NULL,
                   g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE,
                   1,
                   PUSH_TYPE_APS_IDENTITY);

   EXIT;
}

static void
push_aps_client_init (PushApsClient *client)
{
   ENTRY;
   client->priv = G_TYPE_INSTANCE_GET_PRIVATE(client,
                                              PUSH_TYPE_APS_CLIENT,
                                              PushApsClientPrivate);
   client->priv->mode = PUSH_APS_CLIENT_PRODUCTION;
   client->priv->results =
      g_hash_table_new_full(g_int_hash, g_int_equal,
                            g_free, g_object_unref);
   client->priv->last_id = g_random_int();
   client->priv->feedback_interval = 10;
   client->priv->queue = g_queue_new();
   EXIT;
}

GType
push_aps_client_mode_get_type (void)
{
   static GType type_id;
   static gsize initialized = FALSE;
   static GEnumValue values[] = {
      { PUSH_APS_CLIENT_PRODUCTION, "PUSH_APS_CLIENT_PRODUCTION", "PRODUCTION" },
      { PUSH_APS_CLIENT_SANDBOX, "PUSH_APS_CLIENT_SANDBOX", "SANDBOX" },
      { 0 }
   };

   if (g_once_init_enter(&initialized)) {
      type_id = g_enum_register_static("PushApsClientMode", values);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}

GQuark
push_aps_client_error_quark (void)
{
   return g_quark_from_static_string("push-aps-client-error-quark");
}
