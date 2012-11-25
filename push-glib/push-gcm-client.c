/* push-gcm-client.h
 *
 * Copyright (C) 2012 Catch.com
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

#include "push-debug.h"
#include "push-gcm-client.h"

#define PUSH_GCM_CLIENT_URL "https://android.googleapis.com/gcm/send"

/**
 * SECTION:push-gcm-client
 * @title: PushGcmClient
 * @short_description: Client to send notifications to GCM (Android) devices.
 *
 * #PushGcmClient provides a convenient way to send notifications to one or
 * more Android devices via Google's GCM service. Simply create an instance
 * of the client and start delivering messages using
 * push_gcm_client_deliver_async(). The notification will be delivered
 * asynchronously and the caller will be notified via the specified callback
 * upon delivery of the message to GCM.
 *
 * If it has been determined that a device has been removed, the
 * :identity-removed single will be emitted. It is important that consumers
 * connect to this signal and remove the device from their database to
 * prevent further communication with the service.
 */

G_DEFINE_TYPE(PushGcmClient, push_gcm_client, SOUP_TYPE_SESSION_ASYNC)

struct _PushGcmClientPrivate
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

PushGcmClient *
push_gcm_client_new (const gchar *auth_token)
{
   PushGcmClient *client;

   ENTRY;
   client = g_object_new(PUSH_TYPE_GCM_CLIENT,
                         "auth-token", auth_token,
                         NULL);
   RETURN(client);
}

const gchar *
push_gcm_client_get_auth_token (PushGcmClient *client)
{
   g_return_val_if_fail(PUSH_IS_GCM_CLIENT(client), NULL);
   return client->priv->auth_token;
}

void
push_gcm_client_set_auth_token (PushGcmClient *client,
                                const gchar   *auth_token)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_GCM_CLIENT(client));
   g_free(client->priv->auth_token);
   client->priv->auth_token = g_strdup(auth_token);
   g_object_notify_by_pspec(G_OBJECT(client), gParamSpecs[PROP_AUTH_TOKEN]);
   EXIT;
}

static void
_push_gcm_identities_free (gpointer data)
{
   ENTRY;
   g_list_foreach(data, (GFunc)g_object_unref, NULL);
   g_list_free(data);
   EXIT;
}

static void
push_gcm_client_deliver_cb (SoupSession *session,
                            SoupMessage *message,
                            gpointer     user_data)
{
   GSimpleAsyncResult *simple = user_data;
   const gchar *str;
   JsonObject *obj;
   JsonParser *p = NULL;
   JsonArray *ar;
   JsonNode *root;
   JsonNode *node;
   gboolean removed;
   GError *error = NULL;
   GList *list;
   gsize length;
   guint i;

   ENTRY;

   g_assert(SOUP_IS_SESSION(session));
   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   switch (message->status_code) {
   case SOUP_STATUS_OK:
      break;
   case SOUP_STATUS_BAD_REQUEST:
      /*
       * TODO: Log that there was a JSON encoding error likely.
       */
      break;
   case SOUP_STATUS_UNAUTHORIZED:
      g_simple_async_result_set_error(simple,
                                      SOUP_HTTP_ERROR,
                                      message->status_code,
                                      _("GCM request unauthorized."));
      GOTO(failure);
   default:
      if (SOUP_STATUS_IS_SERVER_ERROR(message->status_code) &&
          (str = soup_message_headers_get_one(message->response_headers,
                                              "Retry-After"))) {
         /*
          * TODO: Implement exponential back-off.
          */
      }
      g_simple_async_result_set_error(simple,
                                      SOUP_HTTP_ERROR,
                                      message->status_code,
                                      _("Unknown failure occurred."));
      break;
   }

   if (!message->response_body->data || !message->response_body->length) {
      g_simple_async_result_set_error(simple,
                                      SOUP_HTTP_ERROR,
                                      SOUP_STATUS_IO_ERROR,
                                      _("No data was received from GCM."));
      GOTO(failure);
   }

   p = json_parser_new();

   if (!json_parser_load_from_data(p,
                                   message->response_body->data,
                                   message->response_body->length,
                                   &error)) {
      g_simple_async_result_take_error(simple, error);
      GOTO(failure);
   }

   list = g_object_get_data(G_OBJECT(simple), "identities");

   if ((root = json_parser_get_root(p)) &&
       JSON_NODE_HOLDS_OBJECT(root) &&
       (obj = json_node_get_object(root)) &&
       json_object_has_member(obj, "results") &&
       (node = json_object_get_member(obj, "results")) &&
       JSON_NODE_HOLDS_ARRAY(node) &&
       (ar = json_node_get_array(node))) {
      length = json_array_get_length(ar);
      for (i = 0; i < length && list; i++, list = list->next) {
         /*
          * TODO: Handle the case that the device_token has been renamed.
          */
         removed = FALSE;
         if ((obj = json_array_get_object_element(ar, i)) &&
             json_object_has_member(obj, "error") &&
             (node = json_object_get_member(obj, "error")) &&
             JSON_NODE_HOLDS_VALUE(node) &&
             (str = json_node_get_string(node))) {
            if (!g_strcmp0(str, "MissingRegistration")) {
               removed = TRUE;
            } else if (!g_strcmp0(str, "InvalidRegistration")) {
               removed = TRUE;
            } else if (!g_strcmp0(str, "MismatchSenderId")) {
            } else if (!g_strcmp0(str, "NotRegistered")) {
               removed = TRUE;
            } else if (!g_strcmp0(str, "MessageTooBig")) {
            } else if (!g_strcmp0(str, "InvalidDataKey")) {
            } else if (!g_strcmp0(str, "InvalidTtl")) {
            }

            if (removed) {
               g_assert(PUSH_IS_GCM_IDENTITY(list->data));
               g_signal_emit(session, gSignals[IDENTITY_REMOVED], 0, list->data);
            }
         }
      }
   }

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);

failure:
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
   if (p) {
      g_object_unref(p);
   }

   EXIT;
}

/**
 * push_gcm_client_deliver_async:
 * @client: (in): A #PushGcmClient.
 * @identities: (element-type PushGcmIdentity*): A #GList of #PushGcmIdentity.
 * @message: A #PushGcmMessage.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: User data for @callback.
 *
 * Asynchronously deliver a #PushGcmMessage to one or more GCM enabled
 * devices.
 */
void
push_gcm_client_deliver_async (PushGcmClient       *client,
                               GList               *identities,
                               PushGcmMessage      *message,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
   PushGcmClientPrivate *priv;
   GSimpleAsyncResult *simple;
   SoupMessage *request;
   const gchar *registration_id;
   const gchar *collapse_key;
   JsonGenerator *g;
   JsonObject *obj;
   JsonObject *data;
   JsonObject *mdata;
   JsonArray *ar;
   JsonNode *node;
   GList *iter;
   GList *list;
   gchar *str;
   gsize length;
   guint time_to_live;

   ENTRY;

   g_return_if_fail(PUSH_IS_GCM_CLIENT(client));
   g_return_if_fail(identities);
   g_return_if_fail(PUSH_IS_GCM_MESSAGE(message));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = client->priv;

   request = soup_message_new("POST", PUSH_GCM_CLIENT_URL);
   ar = json_array_new();

   for (iter = identities; iter; iter = iter->next) {
      g_assert(PUSH_IS_GCM_IDENTITY(iter->data));
      registration_id = push_gcm_identity_get_registration_id(iter->data);
      json_array_add_string_element(ar, registration_id);
   }

   str = g_strdup_printf("key=%s", priv->auth_token);
   soup_message_headers_append(request->request_headers, "Authorization", str);
   g_free(str);

   soup_message_headers_append(request->request_headers,
                               "Accept",
                               "application/json");

   data = json_object_new();

   if ((collapse_key = push_gcm_message_get_collapse_key(message))) {
      json_object_set_string_member(data, "collapse_key", collapse_key);
   }

   json_object_set_boolean_member(data,
                                  "delay_while_idle",
                                  push_gcm_message_get_delay_while_idle(message));

   json_object_set_boolean_member(data,
                                  "dry_run",
                                  push_gcm_message_get_dry_run(message));

   if ((time_to_live = push_gcm_message_get_time_to_live(message))) {
      json_object_set_int_member(data, "time_to_live", time_to_live);
   }

   if ((mdata = push_gcm_message_get_data(message))) {
      json_object_set_object_member(data, "data", mdata);
   }

   obj = json_object_new();
   json_object_set_array_member(obj, "registration_ids", ar);
   json_object_set_object_member(obj, "data", data);

   node = json_node_new(JSON_NODE_OBJECT);
   json_node_set_object(node, obj);
   json_object_unref(obj);

   g = json_generator_new();
   json_generator_set_pretty(g, TRUE);
   json_generator_set_indent(g, 2);
   json_generator_set_root(g, node);
   str = json_generator_to_data(g, &length);
   json_node_free(node);
   g_object_unref(g);

   g_print("REQUEST: \"%s\"\n", str);

   soup_message_set_request(request,
                            "application/json",
                            SOUP_MEMORY_TAKE,
                            str,
                            length);

   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      push_gcm_client_deliver_async);

   /*
    * Keep the list of identities around until we receive our result.
    * We need them to key with the resulting array.
    */
   list = g_list_copy(identities);
   g_list_foreach(list, (GFunc)g_object_ref, NULL);
   g_object_set_data_full(G_OBJECT(simple),
                          "identities",
                          list,
                          _push_gcm_identities_free);

   soup_session_queue_message(SOUP_SESSION(client),
                              request,
                              push_gcm_client_deliver_cb,
                              simple);

   EXIT;
}

gboolean
push_gcm_client_deliver_finish (PushGcmClient  *client,
                                GAsyncResult   *result,
                                GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(PUSH_IS_GCM_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

static void
push_gcm_client_finalize (GObject *object)
{
   PushGcmClientPrivate *priv;

   ENTRY;
   priv = PUSH_GCM_CLIENT(object)->priv;
   g_free(priv->auth_token);
   G_OBJECT_CLASS(push_gcm_client_parent_class)->finalize(object);
   EXIT;
}

static void
push_gcm_client_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
   PushGcmClient *client = PUSH_GCM_CLIENT(object);

   switch (prop_id) {
   case PROP_AUTH_TOKEN:
      g_value_set_string(value, push_gcm_client_get_auth_token(client));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_client_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
   PushGcmClient *client = PUSH_GCM_CLIENT(object);

   switch (prop_id) {
   case PROP_AUTH_TOKEN:
      push_gcm_client_set_auth_token(client, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_client_class_init (PushGcmClientClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_gcm_client_finalize;
   object_class->get_property = push_gcm_client_get_property;
   object_class->set_property = push_gcm_client_set_property;
   g_type_class_add_private(object_class, sizeof(PushGcmClientPrivate));

   gParamSpecs[PROP_AUTH_TOKEN] =
      g_param_spec_string("auth-token",
                          _("Auth Token"),
                          _("The authorization token to use to authenticate "
                            "with Google's GCM service."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_AUTH_TOKEN,
                                   gParamSpecs[PROP_AUTH_TOKEN]);

   gSignals[IDENTITY_REMOVED] = g_signal_new("identity-removed",
                                             PUSH_TYPE_GCM_CLIENT,
                                             G_SIGNAL_RUN_FIRST,
                                             0,
                                             NULL,
                                             NULL,
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE,
                                             1,
                                             PUSH_TYPE_GCM_IDENTITY);

   EXIT;
}

static void
push_gcm_client_init (PushGcmClient *client)
{
   ENTRY;
   client->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(client,
                                  PUSH_TYPE_GCM_CLIENT,
                                  PushGcmClientPrivate);
   EXIT;
}
