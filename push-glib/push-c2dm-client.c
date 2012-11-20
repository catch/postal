/* push-c2dm-client.c
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

#include "push-c2dm-client.h"
#include "push-debug.h"

#define PUSH_C2DM_CLIENT_URL "https://android.apis.google.com/c2dm/send"

/**
 * SECTION:push-c2dm-client
 * @title: PushC2dmClient
 * @short_description: Client to send notifications to C2DM (Android) devices.
 *
 * #PushC2dmClient provides a convenient way to send a notification to
 * an Android device via Google's C2DM service. Simply create an instance
 * of the client and start delivering messages using
 * push_c2dm_client_deliver_async(). The notification will be delivered
 * asynchronously and the caller will be notified via the specified
 * callback.
 *
 * If it has been determined that a device has been removed, the
 * "identity-removed" signal will be emitted. It is important that
 * consumers connect to this signal and remove the device from their
 * database to prevent further communication with the service.
 *
 * Authentication is currently only provided via "Google Client Login".
 * As of April 2012, this has been deprecated. OAuth2 support will be
 * added sometime in the near future.
 */

G_DEFINE_TYPE(PushC2dmClient, push_c2dm_client, SOUP_TYPE_SESSION_ASYNC)

struct _PushC2dmClientPrivate
{
   gchar *auth_token;
};

enum
{
   PROP_0,
   PROP_AUTH_TOKEN,
   LAST_PROP
};

enum
{
   IDENTITY_REMOVED,
   LAST_SIGNAL
};

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];

/**
 * push_c2dm_client_get_auth_token:
 * @client: (in): A #PushC2dmClient.
 *
 * Retrieves the "auth-token" property. The "auth-token" is used when sending
 * requests to Google using the "Authorization:" header as required by C2DM.
 *
 * Returns: A string containing the "auth-token".
 */
const gchar *
push_c2dm_client_get_auth_token (PushC2dmClient *client)
{
   g_return_val_if_fail(PUSH_IS_C2DM_CLIENT(client), NULL);
   return client->priv->auth_token;
}

/**
 * push_c2dm_client_set_auth_token:
 * @client: (in): A #PushC2dmClient.
 * @auth_token: (in): A string.
 *
 * Sets the "auth-token" property. This should be a Google ClientLogin style
 * auth token that may be used in the "Authorization:" header of an HTTP
 * request.
 */
void
push_c2dm_client_set_auth_token (PushC2dmClient *client,
                                 const gchar    *auth_token)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_C2DM_CLIENT(client));
   g_free(client->priv->auth_token);
   client->priv->auth_token = g_strdup(auth_token);
   g_object_notify_by_pspec(G_OBJECT(client), gParamSpecs[PROP_AUTH_TOKEN]);
   EXIT;
}

static void
push_c2dm_client_message_cb (SoupSession *session,
                             SoupMessage *message,
                             gpointer     user_data)
{
   GSimpleAsyncResult *simple = user_data;
   PushC2dmIdentity *identity;
   PushC2dmClient *client = (PushC2dmClient *)session;
   const guint8 *data;
   const gchar *code_str;
   const gchar *registration_id;
   SoupBuffer *buffer;
   gboolean removed = FALSE;
   gsize length;
   guint code;

   ENTRY;

   g_return_if_fail(PUSH_IS_C2DM_CLIENT(client));
   g_return_if_fail(SOUP_IS_MESSAGE(message));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   buffer = soup_message_body_flatten(message->response_body);
   soup_buffer_get_data(buffer, &data, &length);

   if (!g_utf8_validate((gchar *)data, length, NULL)) {
      g_simple_async_result_set_error(simple,
                                      PUSH_C2DM_CLIENT_ERROR,
                                      PUSH_C2DM_CLIENT_ERROR_UNKNOWN,
                                      _("Received invalid result from C2DM."));
      GOTO(failure);
   }

   if (g_str_has_prefix((gchar *)data, "id=")) {
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
   } else {
      if (g_str_equal(data, "Error=QuotaExceeded")) {
         code = PUSH_C2DM_CLIENT_ERROR_QUOTA_EXCEEDED;
         code_str = _("Quota exceeded.");
      } else if (g_str_equal(data, "Error=DeviceQuotaExceeded")) {
         code = PUSH_C2DM_CLIENT_ERROR_DEVICE_QUOTA_EXCEEDED;
         code_str = _("Device quota exceeded.");
      } else if (g_str_equal(data, "Error=MissingRegistration")) {
         code = PUSH_C2DM_CLIENT_ERROR_MISSING_REGISTRATION;
         code_str = _("Missing registration.");
         removed = TRUE;
      } else if (g_str_equal(data, "Error=InvalidRegistration")) {
         code = PUSH_C2DM_CLIENT_ERROR_INVALID_REGISTRATION;
         code_str = _("Invalid registration.");
         removed = TRUE;
      } else if (g_str_equal(data, "Error=MismatchSenderId")) {
         code = PUSH_C2DM_CLIENT_ERROR_MISMATCH_SENDER_ID;
         code_str = _("Mismatch sender id.");
      } else if (g_str_equal(data, "Error=NotRegistered")) {
         code = PUSH_C2DM_CLIENT_ERROR_NOT_REGISTERED;
         code_str = _("Not registered.");
         removed = TRUE;
      } else if (g_str_equal(data, "Error=MessageTooBig")) {
         code = PUSH_C2DM_CLIENT_ERROR_MESSAGE_TOO_BIG;
         code_str = _("Message too big.");
      } else if (g_str_equal(data, "Error=MissingCollapseKey")) {
         code = PUSH_C2DM_CLIENT_ERROR_MISSING_COLLAPSE_KEY;
         code_str = _("Missing collapse key.");
      } else {
         code = PUSH_C2DM_CLIENT_ERROR_UNKNOWN;
         code_str = _("An unknown error occurred.");
      }
      g_simple_async_result_set_error(simple,
                                      PUSH_C2DM_CLIENT_ERROR,
                                      code, "%s", code_str);
      if (removed) {
         registration_id = g_object_get_data(G_OBJECT(simple),
                                             "registration-id");
         identity = g_object_new(PUSH_TYPE_C2DM_IDENTITY,
                                 "registration-id", registration_id,
                                 NULL);
         g_signal_emit(client, gSignals[IDENTITY_REMOVED], 0, identity);
         g_object_unref(identity);
      }
   }

failure:
   soup_buffer_free(buffer);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

/**
 * push_c2dm_client_deliver_async:
 * @client: A #PushC2dmClient.
 * @identity: A #PushC2dmIdentity.
 * @message: A #PushC2dmMessage.
 * @cancellable: (allow-none): A #GCancellable, or %NULL.
 * @callback: A callback to execute upon completion.
 * @user_data: User data for @callback.
 *
 * Requests that @message is pushed to the device identified by @identity.
 * Upon completion, @callback will be executed and is expected to call
 * push_c2dm_client_deliver_finish() to retrieve the result.
 */
void
push_c2dm_client_deliver_async (PushC2dmClient      *client,
                                PushC2dmIdentity    *identity,
                                PushC2dmMessage     *message,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
   PushC2dmClientPrivate *priv;
   GSimpleAsyncResult *simple;
   const gchar *auth_header;
   const gchar *registration_id;
   SoupMessage *request;
   GHashTable *params;

   ENTRY;

   g_return_if_fail(PUSH_IS_C2DM_CLIENT(client));
   g_return_if_fail(PUSH_IS_C2DM_IDENTITY(identity));
   g_return_if_fail(PUSH_IS_C2DM_MESSAGE(message));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = client->priv;

   registration_id = push_c2dm_identity_get_registration_id(identity);
   params = push_c2dm_message_build_params(message);
   g_hash_table_insert(params,
                       g_strdup("registration_id"),
                       g_strdup(registration_id));
   request = soup_form_request_new_from_hash(SOUP_METHOD_POST,
                                             PUSH_C2DM_CLIENT_URL,
                                             params);
   auth_header = g_strdup_printf("GoogleLogin auth=%s", priv->auth_token);
   soup_message_headers_append(request->request_headers,
                               "Authorization",
                               auth_header);
   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      push_c2dm_client_deliver_async);
   g_simple_async_result_set_check_cancellable(simple, cancellable);
   g_object_set_data_full(G_OBJECT(simple), "registration-id",
                          g_strdup(registration_id), g_free);
   soup_session_queue_message(SOUP_SESSION(client),
                              request,
                              push_c2dm_client_message_cb,
                              simple);
   g_hash_table_unref(params);

   EXIT;
}

/**
 * push_c2dm_client_deliver_finish:
 * @client: A #PushC2dmClient.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes a request to push_c2dm_client_deliver_async(). If successful,
 * %TRUE is returned. Otherwise, %FALSE is returned and @error is set to
 * the error code representing the remote failure.
 *
 * Callers may want to check the error domain and code of an error to
 * determine if they should remove the device from their records. Such
 * an error may happen if the device has unregistered from push notifications
 * but you have not yet been notified.
 *
 * Returns: %TRUE if successful.
 */
gboolean
push_c2dm_client_deliver_finish (PushC2dmClient  *client,
                                 GAsyncResult    *result,
                                 GError         **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(PUSH_IS_C2DM_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

static void
push_c2dm_client_finalize (GObject *object)
{
   PushC2dmClientPrivate *priv;

   ENTRY;

   priv = PUSH_C2DM_CLIENT(object)->priv;

   g_free(priv->auth_token);

   G_OBJECT_CLASS(push_c2dm_client_parent_class)->finalize(object);

   EXIT;
}

static void
push_c2dm_client_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
   PushC2dmClient *client = PUSH_C2DM_CLIENT(object);

   switch (prop_id) {
   case PROP_AUTH_TOKEN:
      g_value_set_string(value, push_c2dm_client_get_auth_token(client));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_c2dm_client_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
   PushC2dmClient *client = PUSH_C2DM_CLIENT(object);

   switch (prop_id) {
   case PROP_AUTH_TOKEN:
      push_c2dm_client_set_auth_token(client, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_c2dm_client_class_init (PushC2dmClientClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_c2dm_client_finalize;
   object_class->get_property = push_c2dm_client_get_property;
   object_class->set_property = push_c2dm_client_set_property;
   g_type_class_add_private(object_class, sizeof(PushC2dmClientPrivate));

   gParamSpecs[PROP_AUTH_TOKEN] =
      g_param_spec_string("auth-token",
                          _("Auth Token"),
                          _("The authentication token to use with C2DM."),
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_AUTH_TOKEN,
                                   gParamSpecs[PROP_AUTH_TOKEN]);

   gSignals[IDENTITY_REMOVED] = g_signal_new("identity-removed",
                                             PUSH_TYPE_C2DM_CLIENT,
                                             G_SIGNAL_RUN_FIRST,
                                             0,
                                             NULL,
                                             NULL,
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE,
                                             1,
                                             PUSH_TYPE_C2DM_IDENTITY);

   EXIT;
}

static void
push_c2dm_client_init (PushC2dmClient *client)
{
   ENTRY;
   client->priv = G_TYPE_INSTANCE_GET_PRIVATE(client,
                                              PUSH_TYPE_C2DM_CLIENT,
                                              PushC2dmClientPrivate);
   EXIT;
}

GQuark
push_c2dm_client_error_quark (void)
{
   return g_quark_from_static_string("PushC2dmClientError");
}
