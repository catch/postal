/* mongo-server.c
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
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

#include "mongo-bson.h"
#include "mongo-debug.h"
#include "mongo-message.h"
#include "mongo-message-delete.h"
#include "mongo-message-insert.h"
#include "mongo-message-getmore.h"
#include "mongo-message-msg.h"
#include "mongo-message-kill-cursors.h"
#include "mongo-message-query.h"
#include "mongo-message-reply.h"
#include "mongo-message-update.h"
#include "mongo-operation.h"
#include "mongo-server.h"

G_DEFINE_TYPE(MongoServer, mongo_server, G_TYPE_SOCKET_SERVICE)

struct _MongoServerPrivate
{
   GHashTable *client_contexts;
};

struct _MongoClientContext
{
   volatile gint      ref_count;
   GCancellable      *cancellable;
   GSocketConnection *connection;
   GOutputStream     *output;
   MongoServer       *server;
   guint8            *incoming;
   gboolean           failed;

#pragma pack(push, 4)
   struct {
      guint32 msg_len;
      guint32 request_id;
      guint32 response_to;
      guint32 op_code;
   } header;
#pragma pack(pop)
};

enum
{
   REQUEST_STARTED,
   REQUEST_FINISHED,
   REQUEST_READ,
   REQUEST_REPLY,
   REQUEST_MSG,
   REQUEST_UPDATE,
   REQUEST_INSERT,
   REQUEST_QUERY,
   REQUEST_GETMORE,
   REQUEST_DELETE,
   REQUEST_KILL_CURSORS,
   LAST_SIGNAL
};

static guint gSignals[LAST_SIGNAL];

extern gboolean _mongo_message_get_paused (MongoMessage *message);
extern gboolean _mongo_message_set_paused (MongoMessage *message,
                                           gboolean      _paused);

static MongoClientContext *
mongo_client_context_new (MongoServer       *server,
                          GSocketConnection *connection);
static void
mongo_client_context_dispatch (MongoClientContext *client);

static void
mongo_client_context_fail (MongoClientContext *client);

static void
mongo_server_read_header_cb (GInputStream *stream,
                             GAsyncResult *result,
                             gpointer      user_data);

MongoServer *
mongo_server_new (void)
{
   return g_object_new(MONGO_TYPE_SERVER, NULL);
}

static void
mongo_server_read_msg_cb (GInputStream *stream,
                          GAsyncResult *result,
                          gpointer      user_data)
{
   MongoClientContext *client = user_data;
   GError *error = NULL;
   gssize ret;

   ENTRY;

   g_assert(G_INPUT_STREAM(stream));
   g_assert(G_IS_ASYNC_RESULT(result));

   ret = g_input_stream_read_finish(stream, result, &error);
   switch (ret) {
   case -1:
   case 0:
      mongo_client_context_fail(client);
      GOTO(cleanup);
      break;
   default:
      mongo_client_context_dispatch(client);
      break;
   }

   /*
    * Start reading next message header.
    */
   memset((guint8 *)&client->header, 0, sizeof client->header);
   g_input_stream_read_async(stream,
                             (guint8 *)&client->header,
                             sizeof client->header,
                             G_PRIORITY_DEFAULT,
                             client->cancellable,
                             (GAsyncReadyCallback)mongo_server_read_header_cb,
                             mongo_client_context_ref(client));

cleanup:
   mongo_client_context_unref(client);

   EXIT;
}

static void
mongo_server_read_header_cb (GInputStream *stream,
                             GAsyncResult *result,
                             gpointer      user_data)
{
   MongoClientContext *client = user_data;
   GError *error = NULL;
   gssize ret;

   ENTRY;

   g_assert(G_INPUT_STREAM(stream));
   g_assert(G_IS_ASYNC_RESULT(result));

   ret = g_input_stream_read_finish(stream, result, &error);
   switch (ret) {
   case -1:
   case 0:
   default:
      mongo_client_context_fail(client);
      GOTO(cleanup);
      break;
   case 16:
      client->header.msg_len = GUINT32_FROM_LE(client->header.msg_len);
      client->header.request_id = GUINT32_FROM_LE(client->header.request_id);
      client->header.response_to = GUINT32_FROM_LE(client->header.response_to);
      client->header.op_code = GUINT32_FROM_LE(client->header.op_code);
      if (client->header.msg_len <= 16) {
         mongo_client_context_fail(client);
         GOTO(cleanup);
      }

      g_assert(!client->incoming);
      client->incoming = g_malloc(client->header.msg_len);
      g_input_stream_read_async(stream,
                                client->incoming,
                                client->header.msg_len - sizeof client->header,
                                G_PRIORITY_DEFAULT,
                                client->cancellable,
                                (GAsyncReadyCallback)mongo_server_read_msg_cb,
                                mongo_client_context_ref(client));
      break;
   }

cleanup:
   mongo_client_context_unref(client);

   EXIT;
}

static gboolean
mongo_server_incoming (GSocketService    *service,
                       GSocketConnection *connection,
                       GObject           *source_object)
{
   MongoServerPrivate *priv;
   MongoClientContext *client;
   GInputStream *stream;
   MongoServer *server = (MongoServer *)service;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_SERVER(server), FALSE);
   g_return_val_if_fail(G_IS_SOCKET_CONNECTION(connection), FALSE);

   priv = server->priv;

   /*
    * Store the client context for tracking things like cursors, last
    * operation id, and error code, for each client. By keeping this
    * information in a per-client structure, we can avoid likelihood of
    * leaking information from one client to another (such as cursor
    * ids of the other clients).
    */
   client = mongo_client_context_new(server, connection);
   g_hash_table_insert(priv->client_contexts, connection, client);

   /*
    * Start listening for the first 16 bytes containing the message
    * header. Once we have this data, we can start processing an incoming
    * message.
    */
   stream = g_io_stream_get_input_stream(G_IO_STREAM(connection));
   g_input_stream_read_async(stream,
                             (guint8 *)&client->header,
                             sizeof client->header,
                             G_PRIORITY_DEFAULT,
                             client->cancellable,
                             (GAsyncReadyCallback)mongo_server_read_header_cb,
                             mongo_client_context_ref(client));

   RETURN(TRUE);
}

static gboolean
mongo_server_request_read (MongoServer        *server,
                           MongoClientContext *client,
                           MongoMessage       *message)
{
   MongoMessageClass *klass;
   gboolean handled = FALSE;
   guint _signal;

   g_assert(MONGO_IS_SERVER(server));
   g_assert(client);
   g_assert(MONGO_IS_MESSAGE(message));

   klass = MONGO_MESSAGE_GET_CLASS(message);

   switch (klass->operation) {
   case MONGO_OPERATION_REPLY:
      _signal = REQUEST_REPLY;
      break;
   case MONGO_OPERATION_UPDATE:
      _signal = REQUEST_UPDATE;
      break;
   case MONGO_OPERATION_INSERT:
      _signal = REQUEST_INSERT;
      break;
   case MONGO_OPERATION_QUERY:
      _signal = REQUEST_QUERY;
      break;
   case MONGO_OPERATION_GETMORE:
      _signal = REQUEST_GETMORE;
      break;
   case MONGO_OPERATION_DELETE:
      _signal = REQUEST_DELETE;
      break;
   case MONGO_OPERATION_KILL_CURSORS:
      _signal = REQUEST_KILL_CURSORS;
      break;
   case MONGO_OPERATION_MSG:
      _signal = REQUEST_MSG;
      break;
   default:
      g_assert_not_reached();
   }

   g_signal_emit(server, gSignals[_signal], 0, client, message, &handled);

   return handled;
}

static void
mongo_server_finalize (GObject *object)
{
   MongoServerPrivate *priv;

   ENTRY;

   priv = MONGO_SERVER(object)->priv;

   g_hash_table_unref(priv->client_contexts);

   G_OBJECT_CLASS(mongo_server_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_server_class_init (MongoServerClass *klass)
{
   GObjectClass *object_class;
   GSocketServiceClass *service_class;

   klass->request_read = mongo_server_request_read;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_server_finalize;
   g_type_class_add_private(object_class, sizeof(MongoServerPrivate));

   service_class = G_SOCKET_SERVICE_CLASS(klass);
   service_class->incoming = mongo_server_incoming;

   /**
    * MongoServer:request-started:
    *
    * The "request-started" signal is emitted when a new request has
    * been parsed from the client. Only the header fields of the message
    * have been filled in such as "request-id" and "response-to".
    */
   gSignals[REQUEST_STARTED] =
      g_signal_new("request-started",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_started),
                   NULL,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_NONE,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_FINISHED] =
      g_signal_new("request-finished",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_finished),
                   NULL,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_NONE,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   /**
    * MongoServer:request-read:
    *
    * The "request-read" signal is emitted when a request has been fully
    * read from the client and all fields filled in. This is a good place
    * to handle all incoming requests. If you handle this function and
    * return %TRUE to suppress further events from firing, the signals for
    * individual request types will not fire.
    *
    * Returning %TRUE supresses further events.
    */
   gSignals[REQUEST_READ] =
      g_signal_new("request-read",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_read),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_MSG] =
      g_signal_new("request-reply",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_reply),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_MSG] =
      g_signal_new("request-msg",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_msg),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_UPDATE] =
      g_signal_new("request-update",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_update),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_INSERT] =
      g_signal_new("request-insert",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_insert),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_QUERY] =
      g_signal_new("request-query",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_query),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_GETMORE] =
      g_signal_new("request-getmore",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_getmore),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_DELETE] =
      g_signal_new("request-delete",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_delete),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);

   gSignals[REQUEST_KILL_CURSORS] =
      g_signal_new("request-kill_cursors",
                   MONGO_TYPE_SERVER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MongoServerClass, request_kill_cursors),
                   g_signal_accumulator_true_handled,
                   NULL,
                   g_cclosure_marshal_generic,
                   G_TYPE_BOOLEAN,
                   2,
                   MONGO_TYPE_CLIENT_CONTEXT,
                   MONGO_TYPE_MESSAGE);
}

static void
mongo_server_init (MongoServer *server)
{
   server->priv = G_TYPE_INSTANCE_GET_PRIVATE(server,
                                              MONGO_TYPE_SERVER,
                                              MongoServerPrivate);
   server->priv->client_contexts =
      g_hash_table_new_full(g_direct_hash,
                            g_direct_equal,
                            NULL,
                            (GDestroyNotify)mongo_client_context_unref);
}

static void
mongo_client_context_dispose (MongoClientContext *context)
{
   ENTRY;

   g_clear_object(&context->cancellable);
   g_clear_object(&context->connection);
   g_clear_object(&context->output);

   if (context->server) {
      g_object_remove_weak_pointer(G_OBJECT(context->server),
                                   (gpointer *)&context->server);
   }

   EXIT;
}

static MongoClientContext *
mongo_client_context_new (MongoServer       *server,
                          GSocketConnection *connection)
{
   MongoClientContext *context;
   GOutputStream *output_stream;

   ENTRY;
   context = g_slice_new0(MongoClientContext);
   context->ref_count = 1;
   context->cancellable = g_cancellable_new();
   context->server = server;
   context->connection = g_object_ref(connection);
   context->failed = FALSE;
   output_stream = g_io_stream_get_output_stream(G_IO_STREAM(connection));
   context->output = g_buffered_output_stream_new(output_stream);
   g_buffered_output_stream_set_auto_grow(
         G_BUFFERED_OUTPUT_STREAM(context->output),
         TRUE);
   g_object_add_weak_pointer(G_OBJECT(context->server),
                             (gpointer *)&context->server);
   RETURN(context);
}

void
mongo_client_context_write (MongoClientContext  *client,
                            MongoMessage        *message,
                            MongoMessage        *reply)
{
   gboolean handled;
   guint8 *buf;
   gsize buflen;
   gsize written;

   ENTRY;

   g_assert(client);
   g_assert(message);
   g_assert(reply);

   if (!client->failed) {
      if ((buf = mongo_message_save_to_data(reply, &buflen))) {
         if (g_output_stream_write_all(client->output,
                                       buf,
                                       buflen,
                                       &written,
                                       client->cancellable,
                                       NULL)) {
            g_output_stream_flush(client->output, client->cancellable, NULL);
            g_signal_emit(client->server,
                          gSignals[REQUEST_FINISHED],
                          0,
                          client,
                          message,
                          &handled);
            g_free(buf);
            EXIT;
         }
      }

      mongo_client_context_fail(client);
      g_free(buf);
   }

   EXIT;
}

static void
mongo_client_context_dispatch (MongoClientContext *client)
{
   MongoMessageReply *reply;
   MongoMessage *message = NULL;
   MongoBson *bson;
   gboolean handled;
   gboolean wants_reply = FALSE;
   guint8 *data = NULL;
   gsize data_len;
   GType type_id = G_TYPE_NONE;
   GList list = { 0 };

   ENTRY;

   g_assert(client);
   g_assert(client->incoming);

   data = client->incoming;
   data_len = client->header.msg_len - sizeof client->header;
   client->incoming = NULL;

   switch (client->header.op_code) {
   case MONGO_OPERATION_REPLY:
      type_id = MONGO_TYPE_MESSAGE_REPLY;
      break;
   case MONGO_OPERATION_MSG:
      type_id = MONGO_TYPE_MESSAGE_MSG;
      break;
   case MONGO_OPERATION_UPDATE:
      type_id = MONGO_TYPE_MESSAGE_UPDATE;
      break;
   case MONGO_OPERATION_INSERT:
      type_id = MONGO_TYPE_MESSAGE_INSERT;
      break;
   case MONGO_OPERATION_QUERY:
      type_id = MONGO_TYPE_MESSAGE_QUERY;
      wants_reply = TRUE;
      break;
   case MONGO_OPERATION_GETMORE:
      type_id = MONGO_TYPE_MESSAGE_GETMORE;
      wants_reply = TRUE;
      break;
   case MONGO_OPERATION_DELETE:
      type_id = MONGO_TYPE_MESSAGE_DELETE;
      break;
   case MONGO_OPERATION_KILL_CURSORS:
      type_id = MONGO_TYPE_MESSAGE_KILL_CURSORS;
      break;
   default:
      mongo_client_context_fail(client);
      GOTO(cleanup);
      break;
   }

   /*
    * Create a MongoMessage for the incoming request.
    */
   message = g_object_new(type_id,
                          "request-id", client->header.request_id,
                          "response-to", client->header.response_to,
                          NULL);

   DUMP_BYTES(buf, data, data_len);

   /*
    * Load the message buffer into the MongoMessage instance.
    */
   if (!mongo_message_load_from_data(message, data, data_len)) {
      mongo_client_context_fail(client);
      GOTO(cleanup);
   }

   /*
    * Emit request signals for this request. The default "request-read"
    * handler will emit the proper signal for the request.
    */
   g_signal_emit(client->server, gSignals[REQUEST_READ], 0,
                 client, message, &handled);

   /*
    * TODO: Technically we need to hold a lock on the message before checking
    *       to see if it is paused since it is possible for it to be handled
    *       by a thread and then moved to complete state before we readback.
    *       If that thread then unpaused it and we already wrote the response,
    *       we could send a response twice.
    */

   if (wants_reply) {
      if (!_mongo_message_get_paused(message)) {
         if ((reply = MONGO_MESSAGE_REPLY(mongo_message_get_reply(message)))) {
            mongo_client_context_write(client, message, MONGO_MESSAGE(reply));
         } else {
            reply = g_object_new(MONGO_TYPE_MESSAGE_REPLY,
                                 "cursor-id", G_GUINT64_CONSTANT(0),
                                 "flags", MONGO_REPLY_QUERY_FAILURE,
                                 "request-id", -1,
                                 "response-to", client->header.request_id,
                                 NULL);
            bson = mongo_bson_new_empty();
            mongo_bson_append_string(bson, "$err", "Your request is denied.");
            mongo_bson_append_int(bson, "code", 0);
            list.data = bson;
            mongo_message_reply_set_documents(reply, &list);
            mongo_client_context_write(client, message, MONGO_MESSAGE(reply));
            mongo_bson_unref(bson);
            g_object_unref(reply);
         }
      }
   }

cleanup:
   g_clear_object(&message);
   g_free(data);

   EXIT;
}

static void
mongo_client_context_fail (MongoClientContext *client)
{
   g_assert(client);

   client->failed = TRUE;
   g_io_stream_close(G_IO_STREAM(client->connection),
                     client->cancellable,
                     NULL);
}

gchar *
mongo_client_context_get_uri (MongoClientContext *client)
{
   GInetSocketAddress *saddr;
   GSocketAddress *addr;
   GInetAddress *iaddr;
   gchar *str;
   gchar *str2;
   guint port;

   g_assert(client);

   if (!(addr = g_socket_connection_get_remote_address(client->connection,
                                                       NULL))) {
      return NULL;
   }

   if ((saddr = G_INET_SOCKET_ADDRESS(addr))) {
      iaddr = g_inet_socket_address_get_address(saddr);
      str = g_inet_address_to_string(iaddr);
      port = g_inet_socket_address_get_port(saddr);
      str2 = g_strdup_printf("%s:%u", str, port);
      g_free(str);
      return str2;
   }

   /*
    * TODO: Support socket addresses.
    */

   return NULL;
}

/**
 * MongoClientContext_ref:
 * @context: A #MongoClientContext.
 *
 * Atomically increments the reference count of @context by one.
 *
 * Returns: (transfer full): A reference to @context.
 */
MongoClientContext *
mongo_client_context_ref (MongoClientContext *context)
{
   g_return_val_if_fail(context != NULL, NULL);
   g_return_val_if_fail(context->ref_count > 0, NULL);

   ENTRY;
   g_atomic_int_inc(&context->ref_count);
   RETURN(context);
}

/**
 * mongo_client_context_unref:
 * @context: A MongoClientContext.
 *
 * Atomically decrements the reference count of @context by one.  When the
 * reference count reaches zero, the structure will be destroyed and
 * freed.
 */
void
mongo_client_context_unref (MongoClientContext *context)
{
   g_return_if_fail(context != NULL);
   g_return_if_fail(context->ref_count > 0);

   ENTRY;
   if (g_atomic_int_dec_and_test(&context->ref_count)) {
      mongo_client_context_dispose(context);
      g_slice_free(MongoClientContext, context);
   }
   EXIT;
}

GType
mongo_client_context_get_type (void)
{
   static gsize initialized;
   static GType type_id;

   if (g_once_init_enter(&initialized)) {
      type_id = g_boxed_type_register_static(
            "MongoClientContext",
            (GBoxedCopyFunc)mongo_client_context_ref,
            (GBoxedFreeFunc)mongo_client_context_unref);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}
