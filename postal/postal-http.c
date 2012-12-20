/* postal-http.c
 *
 * Copyright (C) 2012 Catch.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "postal-debug.h"
#include "postal-http.h"
#include "postal-metrics.h"
#include "postal-service.h"

#include "neo-logger.h"
#include "neo-logger-daily.h"

#include "url-router.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "http"

G_DEFINE_TYPE(PostalHttp, postal_http, NEO_TYPE_SERVICE_BASE)

struct _PostalHttpPrivate
{
   gboolean       started;
   NeoLogger     *logger;
   PostalMetrics *metrics;
   UrlRouter     *router;
   SoupServer    *server;
   PostalService *service;
};

PostalHttp *
postal_http_new (void)
{
   return g_object_new(POSTAL_TYPE_HTTP,
                       "name", "http",
                       NULL);
}

static guint
get_int_param (GHashTable  *hashtable,
               const gchar *name)
{
   const gchar *str;

   if (hashtable) {
      if ((str = g_hash_table_lookup(hashtable, name))) {
         return g_ascii_strtoll(str, NULL, 10);
      }
   }

   return 0;
}

GQuark
postal_json_error_quark (void)
{
   return g_quark_from_static_string("PostalJsonError");
}

static JsonNode *
postal_http_parse_body (SoupMessage  *message,
                        GError      **error)
{
   JsonParser *p;
   JsonNode *ret;

   g_assert(SOUP_IS_MESSAGE(message));

   p = json_parser_new();

   if (!json_parser_load_from_data(p,
                                   message->request_body->data,
                                   message->request_body->length,
                                   error)) {
      g_object_unref(p);
      return NULL;
   }

   if ((ret = json_parser_get_root(p))) {
      ret = json_node_copy(ret);
   }

   g_object_unref(p);

   if (!ret) {
      g_set_error(error,
                  postal_json_error_quark(),
                  0,
                  "Missing JSON payload.");
   }

   return ret;
}

static void
postal_http_reply_devices (PostalHttp  *http,
                           SoupMessage *message,
                           guint        status,
                           GPtrArray   *devices)
{
   JsonGenerator *g;
   PostalDevice *device;
   JsonArray *ar;
   JsonNode *node;
   JsonNode *child;
   gchar *json_buf;
   gsize length;
   guint i;

   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(devices);

   ar = json_array_new();
   node = json_node_new(JSON_NODE_ARRAY);

   for (i = 0; i < devices->len; i++) {
      device = g_ptr_array_index(devices, i);
      if ((child = postal_device_save_to_json(device, NULL))) {
         json_array_add_element(ar, child);
      }
   }

   json_node_set_array(node, ar);
   json_array_unref(ar);

   g = json_generator_new();
   json_generator_set_root(g, node);
   json_node_free(node);
   json_generator_set_indent(g, 2);
   json_generator_set_pretty(g, TRUE);
   json_buf = json_generator_to_data(g, &length);
   g_object_unref(g);

   soup_message_set_response(message,
                             "application/json",
                             SOUP_MEMORY_TAKE,
                             json_buf,
                             length);
   soup_message_set_status(message, status ?: SOUP_STATUS_OK);
   soup_server_unpause_message(http->priv->server, message);
}

static guint
get_status_code (const GError *error)
{
   guint code = SOUP_STATUS_INTERNAL_SERVER_ERROR;

   if (error->domain == POSTAL_DEVICE_ERROR) {
      switch (error->code) {
      case POSTAL_DEVICE_ERROR_MISSING_USER:
      case POSTAL_DEVICE_ERROR_MISSING_ID:
      case POSTAL_DEVICE_ERROR_INVALID_ID:
      case POSTAL_DEVICE_ERROR_NOT_FOUND:
         code = SOUP_STATUS_NOT_FOUND;
         break;
      case POSTAL_DEVICE_ERROR_INVALID_JSON:
      case POSTAL_DEVICE_ERROR_UNSUPPORTED_TYPE:
         code = SOUP_STATUS_BAD_REQUEST;
         break;
      default:
         break;
      }
   } else if (error->domain == postal_json_error_quark()) {
      code = SOUP_STATUS_BAD_REQUEST;
   }

   return code;
}

static void
postal_http_reply_error (PostalHttp   *http,
                         SoupMessage  *message,
                         const GError *error)
{
   JsonGenerator *g;
   JsonObject *obj;
   JsonNode *node;
   gchar *json_buf;
   gsize length;

   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(error);

   obj = json_object_new();
   json_object_set_string_member(obj, "message", error->message);
   json_object_set_string_member(obj, "domain", g_quark_to_string(error->domain));
   json_object_set_int_member(obj, "code", error->code);

   node = json_node_new(JSON_NODE_OBJECT);
   json_node_set_object(node, obj);
   json_object_unref(obj);

   g = json_generator_new();
   json_generator_set_indent(g, 2);
   json_generator_set_pretty(g, TRUE);
   json_generator_set_root(g, node);
   json_node_free(node);
   json_buf = json_generator_to_data(g, &length);
   g_object_unref(g);

   soup_message_set_response(message,
                             "application/json",
                             SOUP_MEMORY_TAKE,
                             json_buf,
                             length);
   soup_message_set_status(message, get_status_code(error));
   soup_server_unpause_message(http->priv->server, message);
}

static void
postal_http_reply_device (PostalHttp   *http,
                          SoupMessage  *message,
                          guint         status,
                          PostalDevice *device)
{
   PostalHttpPrivate *priv;
   JsonGenerator *g;
   JsonNode *node;
   GError *error = NULL;
   gchar *json_buf;
   gsize length;

   ENTRY;

   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(POSTAL_IS_DEVICE(device));
   g_assert(POSTAL_IS_HTTP(http));

   priv = http->priv;

   if (!(node = postal_device_save_to_json(device, &error))) {
      postal_http_reply_error(http, message, error);
      soup_server_unpause_message(priv->server, message);
      g_error_free(error);
      EXIT;
   }

   g = json_generator_new();
   json_generator_set_indent(g, 2);
   json_generator_set_pretty(g, TRUE);
   json_generator_set_root(g, node);
   json_node_free(node);
   if ((json_buf = json_generator_to_data(g, &length))) {
      soup_message_set_response(message,
                                "application/json",
                                SOUP_MEMORY_TAKE,
                                json_buf,
                                length);
   }
   soup_message_set_status(message, status);
   soup_server_unpause_message(priv->server, message);
   g_object_unref(g);

   EXIT;
}

static void
postal_http_find_device_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   PostalDevice *device;
   SoupMessage *message = user_data;
   PostalHttp *http;
   GError *error = NULL;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(SOUP_IS_MESSAGE(message));
   http = g_object_get_data(user_data, "http");
   g_assert(POSTAL_IS_HTTP(http));

   if (!(device = postal_service_find_device_finish(service, result, &error))) {
      postal_http_reply_error(http, message, error);
      g_error_free(error);
      GOTO(failure);
   }

   postal_http_reply_device(http, message, SOUP_STATUS_OK, device);
   g_object_unref(device);

failure:
   g_object_unref(message);

   EXIT;
}

static void
postal_http_remove_device_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   SoupMessage *message = user_data;
   PostalHttp *http;
   GError *error = NULL;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(SOUP_IS_MESSAGE(message));
   http = g_object_get_data(user_data, "http");
   g_assert(POSTAL_IS_HTTP(http));

   if (!postal_service_remove_device_finish(service, result, &error)) {
      postal_http_reply_error(http, message, error);
      g_error_free(error);
      GOTO(failure);
   } else {
      soup_message_set_status(message, SOUP_STATUS_NO_CONTENT);
      soup_message_headers_append(message->response_headers,
                                  "Content-Type",
                                  "application/json");
      soup_server_unpause_message(http->priv->server, message);
   }

failure:
   g_object_unref(message);

   EXIT;
}

static void
postal_http_add_device_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   PostalDevice *device;
   SoupMessage *message = user_data;
   PostalHttp *http;
   gboolean updated_existing = FALSE;
   GError *error = NULL;
   gchar *str;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(SOUP_IS_MESSAGE(message));
   http = g_object_get_data(user_data, "http");
   g_assert(POSTAL_IS_HTTP(http));

   if (!postal_service_add_device_finish(service, result, &updated_existing, &error)) {
      postal_http_reply_error(http, message, error);
      g_error_free(error);
      GOTO(failure);
   }

   if ((device = POSTAL_DEVICE(g_object_get_data(G_OBJECT(message), "device")))) {
      str = g_strdup_printf("/v1/users/%s/devices/%s",
                            postal_device_get_user(device),
                            postal_device_get_device_token(device));
      soup_message_headers_append(message->response_headers,
                                  "Location",
                                  str);
      g_free(str);
      postal_http_reply_device(http,
                               message,
                               updated_existing ? SOUP_STATUS_OK : SOUP_STATUS_CREATED,
                               device);
   } else {
      g_assert_not_reached();
   }

failure:
   g_object_unref(message);

   EXIT;
}


static void
postal_http_handle_v1_users_user_devices_device (UrlRouter         *router,
                                                 SoupServer        *server,
                                                 SoupMessage       *message,
                                                 const gchar       *path,
                                                 GHashTable        *params,
                                                 GHashTable        *query,
                                                 SoupClientContext *client,
                                                 gpointer           user_data)
{
   PostalDevice *pdev;
   const gchar *user;
   const gchar *device;
   PostalHttp *http = user_data;
   GError *error = NULL;
   JsonNode *node;

   ENTRY;

   g_assert(router);
   g_assert(SOUP_IS_SERVER(server));
   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(path);
   g_assert(params);
   g_assert(g_hash_table_contains(params, "device"));
   g_assert(g_hash_table_contains(params, "user"));
   g_assert(client);
   g_assert(POSTAL_IS_HTTP(http));

   device = g_hash_table_lookup(params, "device");
   user = g_hash_table_lookup(params, "user");

   if (message->method == SOUP_METHOD_GET) {
      postal_service_find_device(http->priv->service,
                                 user,
                                 device,
                                 NULL, /* TODO */
                                 postal_http_find_device_cb,
                                 g_object_ref(message));
      soup_server_pause_message(server, message);
      EXIT;
   } else if (message->method == SOUP_METHOD_DELETE) {
      pdev = g_object_new(POSTAL_TYPE_DEVICE,
                          "device-token", device,
                          "user", user,
                          NULL);
      postal_service_remove_device(http->priv->service,
                                   pdev,
                                   NULL, /* TODO */
                                   postal_http_remove_device_cb,
                                   g_object_ref(message));
      soup_server_pause_message(server, message);
      g_object_unref(pdev);
      EXIT;
   } else if (message->method == SOUP_METHOD_PUT) {
      if (!(node = postal_http_parse_body(message, &error))) {
         postal_http_reply_error(http, message, error);
         soup_server_unpause_message(server, message);
         g_error_free(error);
         EXIT;
      }
      pdev = postal_device_new();
      if (!postal_device_load_from_json(pdev, node, &error)) {
         postal_http_reply_error(http, message, error);
         soup_server_unpause_message(server, message);
         json_node_free(node);
         g_error_free(error);
         g_object_unref(pdev);
         EXIT;
      }
      postal_device_set_device_token(pdev, device);
      postal_device_set_user(pdev, user);
      json_node_free(node);
      g_object_set_data_full(G_OBJECT(message),
                             "device",
                             g_object_ref(pdev),
                             g_object_unref);
      postal_service_add_device(http->priv->service,
                                pdev,
                                NULL, /* TODO */
                                postal_http_add_device_cb,
                                g_object_ref(message));
      soup_server_pause_message(server, message);
      g_object_unref(pdev);
      EXIT;
   } else {
      soup_message_set_status(message, SOUP_STATUS_METHOD_NOT_ALLOWED);
      EXIT;
   }

   g_assert_not_reached();
}

static void
devices_get_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   SoupMessage *message = user_data;
   PostalHttp *http;
   GPtrArray *devices;
   GError *error = NULL;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   http = g_object_get_data(user_data, "http");
   g_assert(POSTAL_IS_HTTP(http));

   if (!(devices = postal_service_find_devices_finish(service,
                                                      result,
                                                      &error))) {
      postal_http_reply_error(http, message, error);
      g_error_free(error);
      GOTO(failure);
   }

   postal_http_reply_devices(http, message, SOUP_STATUS_OK, devices);
   g_ptr_array_unref(devices);

failure:
   g_object_unref(message);

   EXIT;
}

static void
postal_http_handle_v1_users_user_devices (UrlRouter         *router,
                                          SoupServer        *server,
                                          SoupMessage       *message,
                                          const gchar       *path,
                                          GHashTable        *params,
                                          GHashTable        *query,
                                          SoupClientContext *client,
                                          gpointer           user_data)
{
   const gchar *user;
   PostalHttp *http = user_data;

   ENTRY;

   g_assert(router);
   g_assert(SOUP_IS_SERVER(server));
   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(path);
   g_assert(params);
   g_assert(g_hash_table_contains(params, "user"));
   g_assert(client);
   g_assert(POSTAL_IS_HTTP(http));

   user = g_hash_table_lookup(params, "user");
   soup_server_pause_message(server, message);

   if (message->method == SOUP_METHOD_GET) {
      postal_service_find_devices(http->priv->service,
                                  user,
                                  get_int_param(query, "offset"),
                                  get_int_param(query, "limit"),
                                  NULL, /* TODO */
                                  devices_get_cb,
                                  g_object_ref(message));
      EXIT;
   }

   soup_message_set_status(message, SOUP_STATUS_METHOD_NOT_ALLOWED);
   soup_server_unpause_message(server, message);

   EXIT;
}

static void
postal_http_notify_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   SoupMessage *message = user_data;
   PostalHttp *http;
   GError *error = NULL;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(SOUP_IS_MESSAGE(message));
   http = g_object_get_data(user_data, "http");
   g_assert(POSTAL_IS_HTTP(http));

   if (!postal_service_notify_finish(service, result, &error)) {
      postal_http_reply_error(http, message, error);
      g_error_free(error);
      g_object_unref(message);
      EXIT;
   }

   soup_message_set_status(message, SOUP_STATUS_OK);
   soup_server_unpause_message(http->priv->server, message);
   soup_message_headers_append(message->response_headers,
                               "Content-Type",
                               "application/json");
   g_object_unref(message);

   EXIT;
}

static void
postal_http_handle_v1_notify (UrlRouter         *router,
                              SoupServer        *server,
                              SoupMessage       *message,
                              const gchar       *path,
                              GHashTable        *params,
                              GHashTable        *query,
                              SoupClientContext *client,
                              gpointer           user_data)
{
   PostalNotification *notif;
   const gchar *collapse_key = NULL;
   const gchar *str;
   PostalHttp *http = user_data;
   JsonObject *aps;
   JsonObject *c2dm;
   JsonObject *gcm;
   JsonObject *object;
   JsonArray *devices;
   JsonArray *users;
   GPtrArray *devices_ptr;
   GPtrArray *users_ptr;
   JsonNode *node;
   GError *error = NULL;
   guint count;
   guint i;

   g_assert(SOUP_IS_SERVER(server));
   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(path);
   g_assert(client);
   g_assert(POSTAL_IS_HTTP(http));

   if (message->method != SOUP_METHOD_POST) {
      soup_message_set_status(message, SOUP_STATUS_METHOD_NOT_ALLOWED);
      return;
   }

   soup_server_pause_message(server, message);

   if (!(node = postal_http_parse_body(message, &error))) {
      postal_http_reply_error(http, message, error);
      g_error_free(error);
      return;
   }

   if (!JSON_NODE_HOLDS_OBJECT(node) ||
       !(object = json_node_get_object(node)) ||
       !json_object_has_member(object, "aps") ||
       !(node = json_object_get_member(object, "aps")) ||
       !JSON_NODE_HOLDS_OBJECT(node) ||
       !(aps = json_object_get_object_member(object, "aps")) ||
       !json_object_has_member(object, "c2dm") ||
       !(node = json_object_get_member(object, "c2dm")) ||
       !JSON_NODE_HOLDS_OBJECT(node) ||
       !(c2dm = json_object_get_object_member(object, "c2dm")) ||
       !json_object_has_member(object, "gcm") ||
       !(node = json_object_get_member(object, "gcm")) ||
       !JSON_NODE_HOLDS_OBJECT(node) ||
       !(gcm = json_object_get_object_member(object, "gcm")) ||
       !json_object_has_member(object, "users") ||
       !(node = json_object_get_member(object, "users")) ||
       !JSON_NODE_HOLDS_ARRAY(node) ||
       !(users = json_object_get_array_member(object, "users")) ||
       !json_object_has_member(object, "devices") ||
       !(node = json_object_get_member(object, "devices")) ||
       !JSON_NODE_HOLDS_ARRAY(node) ||
       !(devices = json_object_get_array_member(object, "devices"))) {
      error = g_error_new(postal_json_error_quark(), 0,
                          "Missing or invalid fields in JSON payload.");
      postal_http_reply_error(http, message, error);
      json_node_free(node);
      g_error_free(error);
      return;
   }

   if (json_object_has_member(object, "collapse_key") &&
       (node = json_object_get_member(object, "collapse_key")) &&
       JSON_NODE_HOLDS_VALUE(node)) {
      collapse_key = json_node_get_string(node);
   }

   notif = g_object_new(POSTAL_TYPE_NOTIFICATION,
                        "aps", aps,
                        "c2dm", c2dm,
                        "collapse-key", collapse_key,
                        "gcm", gcm,
                        NULL);

   count = json_array_get_length(users);
   users_ptr = g_ptr_array_sized_new(count);
   for (i = 0; i < count; i++) {
      node = json_array_get_element(users, i);
      if (json_node_get_value_type(node) == G_TYPE_STRING) {
         str = json_node_get_string(node);
         g_ptr_array_add(users_ptr, (gchar *)str);
      }
   }
   g_ptr_array_add(users_ptr, NULL);

   count = json_array_get_length(devices);
   devices_ptr = g_ptr_array_sized_new(count);
   g_ptr_array_set_free_func(devices_ptr, g_free);
   for (i = 0; i < count; i++) {
      node = json_array_get_element(devices, i);
      if (json_node_get_value_type(node) == G_TYPE_STRING) {
         str = json_node_get_string(node);
         g_ptr_array_add(devices_ptr, g_strdup(str));
      }
   }
   g_ptr_array_add(devices_ptr, NULL);

   postal_service_notify(http->priv->service,
                         notif,
                         (gchar **)users_ptr->pdata,
                         (gchar **)devices_ptr->pdata,
                         NULL, /* TODO: Cancellable/Timeout? */
                         postal_http_notify_cb,
                         g_object_ref(message));

   g_ptr_array_unref(devices_ptr);
   g_ptr_array_unref(users_ptr);
   json_node_free(node);
   g_object_unref(notif);
}

static void
postal_http_log_message (PostalHttp        *http,
                         SoupMessage       *message,
                         const gchar       *path,
                         SoupClientContext *client)
{
   const gchar *ip_addr;
   const gchar *version;
   GTimeVal event_time;
   struct tm tt;
   time_t t;
   gchar *epath;
   gchar *formatted = NULL;
   gchar *referrer;
   gchar *user_agent;
   gchar ftime[32];

   g_assert(POSTAL_IS_HTTP(http));
   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(client);

   g_get_current_time(&event_time);
   t = (time_t)event_time.tv_sec;
   localtime_r(&t, &tt);
   strftime(ftime, sizeof ftime, "%b %d %H:%M:%S", &tt);
   ftime[31] = '\0';

   switch (soup_message_get_http_version(message)) {
   case SOUP_HTTP_1_0:
      version = "HTTP/1.0";
      break;
   case SOUP_HTTP_1_1:
      version = "HTTP/1.1";
      break;
   default:
      g_assert_not_reached();
      version = NULL;
      break;
   }

   ip_addr = soup_client_context_get_host(client);
   referrer = (gchar *)soup_message_headers_get_one(message->request_headers, "Referrer");
   user_agent = (gchar *)soup_message_headers_get_one(message->request_headers, "User-Agent");

   epath = g_strescape(path, NULL);
   referrer = g_strescape(referrer ?: "", NULL);
   user_agent = g_strescape(user_agent ?: "", NULL);

   formatted = g_strdup_printf("%s %s \"%s %s %s\" %u %"G_GSIZE_FORMAT" \"%s\" \"%s\"\n",
                               ip_addr,
                               ftime,
                               message->method,
                               epath,
                               version,
                               message->status_code,
                               message->response_body->length,
                               referrer,
                               user_agent);

   neo_logger_log(http->priv->logger,
                  &event_time,
                  NULL,
                  NULL,
                  0,
                  0,
                  0,
                  NULL,
                  formatted);

   g_free(epath);
   g_free(referrer);
   g_free(user_agent);
   g_free(formatted);
}

static void
postal_http_handle_status (UrlRouter         *router,
                           SoupServer        *server,
                           SoupMessage       *message,
                           const gchar       *path,
                           GHashTable        *params,
                           GHashTable        *query,
                           SoupClientContext *client,
                           gpointer           user_data)
{
   PostalHttp *http = user_data;
   guint64 devices_added;
   guint64 devices_removed;
   guint64 devices_updated;
   guint64 aps_notified;
   guint64 c2dm_notified;
   guint64 gcm_notified;
   gchar *str;

   g_assert(POSTAL_IS_HTTP(http));

   g_object_get(http->priv->metrics,
                "aps-notified", &aps_notified,
                "c2dm-notified", &c2dm_notified,
                "devices-added", &devices_added,
                "devices-removed", &devices_removed,
                "devices-updated", &devices_updated,
                "gcm-notified", &gcm_notified,
                NULL);
   /*
    * XXX: This technically isn't valid JSON since it is limited to
    *      56-bit integers. But that's okay, I'm not terribly worried.
    */
   str = g_strdup_printf("{\n"
                         "  \"devices_added\": %"G_GUINT64_FORMAT",\n"
                         "  \"devices_removed\": %"G_GUINT64_FORMAT",\n"
                         "  \"devices_updated\": %"G_GUINT64_FORMAT",\n"
                         "  \"devices_notified\": {\n"
                         "    \"aps\": %"G_GUINT64_FORMAT",\n"
                         "    \"c2dm\": %"G_GUINT64_FORMAT",\n"
                         "    \"gcm\": %"G_GUINT64_FORMAT"\n"
                         "  }\n"
                         "}\n",
                         devices_added,
                         devices_removed,
                         devices_updated,
                         aps_notified,
                         c2dm_notified,
                         gcm_notified);
   soup_message_set_status(message, SOUP_STATUS_OK);
   soup_message_set_response(message,
                             "application/json",
                             SOUP_MEMORY_TAKE,
                             str,
                             strlen(str));
}

static void
postal_http_router (SoupServer        *server,
                    SoupMessage       *message,
                    const gchar       *path,
                    GHashTable        *query,
                    SoupClientContext *client,
                    gpointer           user_data)
{
   PostalHttpPrivate *priv;
   PostalHttp *http = user_data;

   ENTRY;

   g_assert(SOUP_IS_SERVER(server));
   g_assert(SOUP_IS_MESSAGE(message));
   g_assert(path);
   g_assert(client);
   g_assert(POSTAL_IS_HTTP(http));

   priv = http->priv;

   g_object_set_data_full(G_OBJECT(message),
                          "http",
                          g_object_ref(http),
                          g_object_unref);

   if (!url_router_route(priv->router, server, message, path, query, client)) {
      soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
   }

   EXIT;
}

static void
postal_http_request_finished (SoupServer        *server,
                              SoupMessage       *message,
                              SoupClientContext *client,
                              gpointer           user_data)
{
   const gchar *path;
   PostalHttp *http = user_data;
   SoupURI *uri;

   g_assert(POSTAL_IS_HTTP(http));

   if (message && (uri = soup_message_get_uri(message))) {
      path = soup_uri_get_path(uri);
      postal_http_log_message(http, message, path, client);
   }
}

static void
postal_http_start (NeoServiceBase *base,
                   GKeyFile       *config)
{
   PostalHttpPrivate *priv;
   NeoService *peer;
   gboolean nologging = FALSE;
   gchar *logfile = NULL;
   guint port = 0;

   ENTRY;

   g_assert(POSTAL_IS_HTTP(base));

   priv = POSTAL_HTTP(base)->priv;

   if (config) {
      port = g_key_file_get_integer(config, "http", "port", NULL);
      logfile = g_key_file_get_string(config, "http", "logfile", NULL);
      nologging = g_key_file_get_boolean(config, "http", "nologging", NULL);
   }

   if (!(peer = neo_service_get_peer(NEO_SERVICE(base), "metrics"))) {
      g_error("Failed to discover PostalMetrics!");
   }

   g_assert(POSTAL_IS_METRICS(peer));
   priv->metrics = g_object_ref(peer);

   if (!(peer = neo_service_get_peer(NEO_SERVICE(base), "service"))) {
      g_error("Failed to discover PostalService!");
   }

   g_assert(POSTAL_IS_SERVICE(peer));
   priv->service = g_object_ref(peer);

   priv->server = soup_server_new(SOUP_SERVER_PORT, port ?: 5300,
                                  SOUP_SERVER_SERVER_HEADER, "Postal/"VERSION,
                                  NULL);

   if (!nologging) {
      logfile = logfile ?: g_strdup("postal.log");
      priv->logger = neo_logger_daily_new(logfile);
      g_signal_connect(priv->server,
                       "request-finished",
                       G_CALLBACK(postal_http_request_finished),
                       base);
   }

   soup_server_add_handler(priv->server,
                           NULL,
                           postal_http_router,
                           base,
                           NULL);

   soup_server_run_async(priv->server);

   g_free(logfile);

   EXIT;
}

static void
postal_http_stop (NeoServiceBase *base)
{
   PostalHttpPrivate *priv;
   PostalHttp *http = (PostalHttp *)base;

   ENTRY;

   g_assert(POSTAL_IS_HTTP(http));

   priv = http->priv;

   if (priv->server) {
      soup_server_quit(priv->server);
      g_clear_object(&priv->server);
   }

   EXIT;
}

static void
postal_http_finalize (GObject *object)
{
   PostalHttpPrivate *priv;

   priv = POSTAL_HTTP(object)->priv;

   g_clear_object(&priv->logger);
   g_clear_object(&priv->server);

   url_router_free(priv->router);
   priv->router = NULL;

   G_OBJECT_CLASS(postal_http_parent_class)->finalize(object);
}

static void
postal_http_class_init (PostalHttpClass *klass)
{
   GObjectClass *object_class;
   NeoServiceBaseClass *base_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_http_finalize;
   g_type_class_add_private(object_class, sizeof(PostalHttpPrivate));

   base_class = NEO_SERVICE_BASE_CLASS(klass);
   base_class->start = postal_http_start;
   base_class->stop = postal_http_stop;
}

static void
postal_http_init (PostalHttp *http)
{
   http->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(http,
                                  POSTAL_TYPE_HTTP,
                                  PostalHttpPrivate);

   http->priv->router = url_router_new();
   url_router_add_handler(http->priv->router,
                          "/status",
                          postal_http_handle_status,
                          http);
   url_router_add_handler(http->priv->router,
                          "/v1/users/:user/devices",
                          postal_http_handle_v1_users_user_devices,
                          http);
   url_router_add_handler(http->priv->router,
                          "/v1/users/:user/devices/:device",
                          postal_http_handle_v1_users_user_devices_device,
                          http);
   url_router_add_handler(http->priv->router,
                          "/v1/notify",
                          postal_http_handle_v1_notify,
                          http);
}
