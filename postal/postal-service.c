/* postal-service.c
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

#include <glib/gi18n.h>
#include <push-glib.h>

#include "postal-debug.h"
#include "postal-dm-cache.h"
#include "postal-metrics.h"
#include "postal-service.h"

#ifndef POSTAL_SERVICE_DM_CACHES
#define POSTAL_SERVICE_DM_CACHES 20
#endif

#ifndef POSTAL_SERVICE_DM_CACHE_ENTRIES
#define POSTAL_SERVICE_DM_CACHE_ENTRIES 16384
#endif

G_DEFINE_TYPE(PostalService, postal_service, NEO_TYPE_SERVICE_BASE)

struct _PostalServicePrivate
{
   PushApsClient    *aps;
   PushC2dmClient   *c2dm;
   PushGcmClient    *gcm;
   gchar            *db_and_collection;
   gchar            *db_and_cmd;
   gchar            *db;
   gchar            *collection;
   PostalMetrics    *metrics;
   MongoConnection  *mongo;
   PostalDmCache   **caches;
   guint             n_caches;
};

PostalService *
postal_service_new (void)
{
   return g_object_new(POSTAL_TYPE_SERVICE,
                       "name", "service",
                       NULL);
}

static gboolean
postal_service_should_ignore (PostalService      *service,
                              PostalDevice       *device,
                              PostalNotification *notif)
{
   PostalServicePrivate *priv;
   gboolean ret;
   guint i;
   guint idx;
   guint oldest;
   gchar *key;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), FALSE);
   g_return_val_if_fail(POSTAL_IS_DEVICE(device), FALSE);
   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notif), FALSE);

   priv = service->priv;

   /*
    * TODO: Some of this could be optimized fairly easily. Like doing the
    *       hash once and performing a lookup using that.
    *       Additionally, the call to g_get_monotonic_time() for each
    *       message that is getting checked. Perhaps we could have an
    *       input parameter for that.
    */

   /*
    * Build our collapse key for this device:message pair.
    */
   key = g_strdup_printf("%s:%s",
                         postal_device_get_device_token(device),
                         postal_notification_get_collapse_key(notif));

   /*
    * Figure out the bucket for our active cache.
    */
   idx = g_get_monotonic_time() / G_USEC_PER_SEC / priv->n_caches;
   oldest = (idx + 1) % priv->n_caches;

   /*
    * Cleanup the oldest cache.
    */
   if (!postal_dm_cache_is_empty(priv->caches[oldest])) {
      postal_dm_cache_remove_all(priv->caches[oldest]);
   }

   /*
    * See if any of the recent caches have this item.
    */
   for (i = 0; i < priv->n_caches; i++) {
      if ((ret = postal_dm_cache_contains(priv->caches[i], key))) {
         g_free(key);
         RETURN(TRUE);
      }
   }

   /*
    * Insert the key into the current cache.
    */
   ret = postal_dm_cache_insert(priv->caches[idx], key);
   RETURN(ret);
}

static void
postal_service_add_device_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoMessageReply *reply = NULL;
   MongoConnection *connection = (MongoConnection *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_connection_query_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      GOTO(failure);
   } else {
      PostalService *service;
      MongoBsonIter iter;
      MongoBsonIter citer;
      PostalDevice *device;
      MongoBson *bson = NULL;
      MongoBson *value;
      gboolean updated_existing;
      GList *list;

      device = POSTAL_DEVICE(g_object_get_data(G_OBJECT(simple), "device"));
      service = POSTAL_SERVICE(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
      updated_existing = FALSE;

      if (!(list = mongo_message_reply_get_documents(reply))) {
         g_object_unref(reply);
         GOTO(failure);
      }

      bson = list->data;

      if (mongo_bson_iter_init_find(&iter, bson, "lastErrorObject") &&
          MONGO_BSON_ITER_HOLDS_DOCUMENT(&iter) &&
          mongo_bson_iter_recurse(&iter, &citer) &&
          mongo_bson_iter_find(&citer, "updatedExisting") &&
          MONGO_BSON_ITER_HOLDS_BOOLEAN(&citer)) {
         updated_existing = mongo_bson_iter_get_value_boolean(&citer);
         g_object_set_data(G_OBJECT(simple),
                           "updated-existing",
                           GINT_TO_POINTER(updated_existing));
      }

      if (mongo_bson_iter_init_find(&iter, bson, "value") &&
          MONGO_BSON_ITER_HOLDS_DOCUMENT(&iter) &&
          (value = mongo_bson_iter_get_value_bson(&iter))) {
         if (!postal_device_load_from_bson(device, value, &error)) {
            mongo_bson_unref(value);
            g_object_unref(reply);
            GOTO(failure);
         }
         mongo_bson_unref(value);
      }

      if (service->priv->metrics) {
         if (updated_existing) {
            postal_metrics_device_added(service->priv->metrics, device);
         } else {
            postal_metrics_device_updated(service->priv->metrics, device);
         }
      }

      g_object_unref(reply);
      g_object_unref(service);
   }

failure:
   g_simple_async_result_set_op_res_gboolean(simple, !!reply);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

/**
 * postal_service_add_device:
 * @service: (in): A #PostalService.
 * @device: (in): A #PostalDevice.
 * @cancellable: (in) (allow-none): A #GCancellable or %NULL.
 * @callback: (in): A callback to execute upon completion.
 * @user_data: (in): User data for @callback.
 *
 * Asynchronously adds @device to the configured data store. @callback
 * must call postal_service_add_device_finish() to complete the asynchronous
 * request.
 */
void
postal_service_add_device (PostalService       *service,
                           PostalDevice        *device,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
   PostalServicePrivate *priv;
   GSimpleAsyncResult *simple;
   MongoObjectId *oid;
   MongoBsonIter iter;
   MongoBson *bson;
   MongoBson *cmd;
   MongoBson *q;
   MongoBson *set;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   /*
    * Serialize the device to a BSON document.
    */
   if (!(bson = postal_device_save_to_bson(device, &error))) {
      g_simple_async_report_take_gerror_in_idle(G_OBJECT(service),
                                                callback,
                                                user_data,
                                                error);
      EXIT;
   }

   /*
    * Make sure we have a removed_at field for querying active devices.
    */
   if (!mongo_bson_iter_init_find(&iter, bson, "removed_at")) {
      mongo_bson_append_null(bson, "removed_at");
   }

   set = mongo_bson_new_empty();
   mongo_bson_append_bson(set, "$set", bson);

   /*
    * Build query so we can upsert the previous item if it exists.
    */
   q = mongo_bson_new_empty();
   mongo_bson_append_string(q, "device_token",
                            postal_device_get_device_token(device));
   if (mongo_bson_iter_init_find(&iter, bson, "user")) {
      if (mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_OBJECT_ID) {
         oid = mongo_bson_iter_get_value_object_id(&iter);
         mongo_bson_append_object_id(q, "user", oid);
         mongo_object_id_free(oid);
      } else if (mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_UTF8) {
         mongo_bson_append_string(q, "user",
               mongo_bson_iter_get_value_string(&iter, NULL));
      } else {
         g_assert_not_reached();
      }
   }

   cmd = mongo_bson_new_empty();
   mongo_bson_append_string(cmd, "findAndModify", priv->collection);
   mongo_bson_append_bson(cmd, "query", q);
   mongo_bson_append_bson(cmd, "update", set);
   mongo_bson_append_boolean(cmd, "new", TRUE);
   mongo_bson_append_boolean(cmd, "upsert", TRUE);

   /*
    * Asynchronously upsert the document to Mongo.
    */
   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_add_device);
   g_simple_async_result_set_check_cancellable(simple, cancellable);
   g_object_set_data_full(G_OBJECT(simple),
                          "device",
                          g_object_ref(device),
                          g_object_unref);

   mongo_connection_query_async(priv->mongo,
                                priv->db_and_cmd,
                                MONGO_QUERY_EXHAUST,
                                0,
                                1,
                                cmd,
                                NULL,
                                NULL, /* TODO: */
                                postal_service_add_device_cb,
                                simple);

   mongo_bson_unref(cmd);
   mongo_bson_unref(bson);
   mongo_bson_unref(q);
   mongo_bson_unref(set);

   EXIT;
}

/**
 * postal_service_add_device_finish:
 * @service: (in): A #PostalService.
 * @result: (in): A #GAsyncResult.
 * @updated_existing: (out) (allow-none): A location for a #gboolean.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to postal_service_add_device().
 *
 * If an existing device was updated, then @updated_existing is set to %TRUE.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
postal_service_add_device_finish (PostalService  *service,
                                  GAsyncResult   *result,
                                  gboolean       *updated_existing,
                                  GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   if (updated_existing) {
      *updated_existing =
            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(result),
                                              "updated-existing"));
   }

   RETURN(ret);
}

static void
postal_service_remove_device_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoConnection *connection = (MongoConnection *)object;
   gboolean ret;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(ret = mongo_connection_update_finish(connection,
                                              result,
                                              NULL,
                                              &error))) {
      g_simple_async_result_take_error(simple, error);
   } else {
      PostalService *service;
      PostalDevice *device;

      device = POSTAL_DEVICE(g_object_get_data(G_OBJECT(simple), "device"));
      service = POSTAL_SERVICE(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
      if (service->priv->metrics) {
         postal_metrics_device_removed(service->priv->metrics, device);
      }
      g_object_unref(service);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

/**
 * postal_service_remove_device:
 * @service: (in): A #PostalService.
 * @device: (in): A #PostalDevice.
 * @cancellable: (in) (allow-none): A #GCancellable or %NULL.
 * @callback: (in): A callback to execute upon completion.
 * @user_data: (in): User data for callback.
 *
 * Removes a device from the underlying storage (MongoDB). @callback will
 * be executed upon completion of the request on the applications main
 * thread. @callback MUST call postal_service_remove_device_finish()
 * to complete the request.
 */
void
postal_service_remove_device (PostalService       *service,
                              PostalDevice        *device,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
   PostalServicePrivate *priv;
   GSimpleAsyncResult *simple;
   MongoObjectId *user_id;
   const gchar *device_token;
   const gchar *user;
   MongoBson *q;
   MongoBson *u;
   MongoBson *s;
   GTimeVal tv;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   /*
    * Make sure we have an _id field to remove.
    */
   if (!(device_token = postal_device_get_device_token(device))) {
      g_simple_async_report_error_in_idle(G_OBJECT(service),
                                          callback,
                                          user_data,
                                          POSTAL_DEVICE_ERROR,
                                          POSTAL_DEVICE_ERROR_MISSING_ID,
                                          _("id is missing from device."));
      EXIT;
   }

   /*
    * Make sure we have the user so that we can enforce the device
    * was created by a particular user. This isn't meant to be security
    * enforcement, but to help prevent API consumers from deleting the wrong
    * data.
    */
   if (!(user = postal_device_get_user(device))) {
      g_simple_async_report_error_in_idle(G_OBJECT(service),
                                          callback,
                                          user_data,
                                          POSTAL_DEVICE_ERROR,
                                          POSTAL_DEVICE_ERROR_MISSING_USER,
                                          _("user is missing from device."));
      EXIT;
   }

   /*
    * Build our query for the device to remove.
    */
   q = mongo_bson_new_empty();
   mongo_bson_append_string(q, "device_token", device_token);
   if ((user_id = mongo_object_id_new_from_string(user))) {
      mongo_bson_append_object_id(q, "user", user_id);
      mongo_object_id_free(user_id);
   } else {
      mongo_bson_append_string(q, "user", user);
   }

   /*
    * Build our update document of {"$set": {"removed_at": now}}.
    */
   s = mongo_bson_new_empty();
   g_get_current_time(&tv);
   mongo_bson_append_timeval(s, "removed_at", &tv);
   u = mongo_bson_new_empty();
   mongo_bson_append_bson(u, "$set", s);

   /*
    * Asynchronously update the document.
    */
   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_remove_device);
   g_object_set_data_full(G_OBJECT(simple),
                          "device",
                          g_object_ref(device),
                          g_object_unref);
   mongo_connection_update_async(priv->mongo,
                                 priv->db_and_collection,
                                 MONGO_UPDATE_NONE,
                                 q,
                                 u,
                                 cancellable,
                                 postal_service_remove_device_cb,
                                 simple);

   /*
    * Cleanup our allocations.
    */
   mongo_bson_unref(q);
   mongo_bson_unref(s);
   mongo_bson_unref(u);

   EXIT;
}

/**
 * postal_service_remove_device_finish:
 * @service: (in): A #PostalService.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to postal_service_remove_device().
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 */
gboolean
postal_service_remove_device_finish (PostalService  *service,
                                     GAsyncResult   *result,
                                     GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

static void
postal_service_find_devices_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoCursor *cursor = (MongoCursor *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_CURSOR(cursor));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!mongo_cursor_foreach_finish(cursor, result, &error)) {
      g_simple_async_result_set_op_res_gpointer(simple, NULL, NULL);
      g_simple_async_result_take_error(simple, error);
      GOTO(cleanup);
   }

cleanup:
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static gboolean
postal_service_find_devices_foreach (MongoCursor *cursor,
                                     MongoBson   *bson,
                                     gpointer     user_data)
{
   PostalDevice *device;
   GPtrArray *devices = user_data;
   GError *error = NULL;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CURSOR(cursor), TRUE);
   g_return_val_if_fail(bson, TRUE);
   g_return_val_if_fail(devices, TRUE);

   device = postal_device_new();

   if (!postal_device_load_from_bson(device, bson, &error)) {
      g_message("Failed to load device from BSON: %s", error->message);
      g_object_unref(device);
      RETURN(TRUE);
   }

   g_ptr_array_add(devices, device);

   RETURN(TRUE);
}

/**
 * postal_service_find_devices:
 * @service: (in): A #PostalService.
 * @user: (in): A string.
 * @offset: (in): The offset in the result query for pagination.
 * @limit: (in): The maximum number of documents to return.
 * @callback: (in): A callback to execute upon completion.
 * @user_data: (in): User data for callback.
 *
 * Asynchronously requests the devices that were created by @user.
 * @offset and @limit will be respected to paginate through the result set.
 * @callback will be executed upon completion of the query. @callback MUST
 * call postal_service_find_devices_finish() to complete the asynchronous
 * request.
 */
void
postal_service_find_devices (PostalService       *service,
                             const gchar         *user,
                             gsize                offset,
                             gsize                limit,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
   PostalServicePrivate *priv;
   GSimpleAsyncResult *simple;
   MongoObjectId *id;
   MongoCursor *cursor;
   GPtrArray *devices;
   MongoBson *q;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(user);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   q = mongo_bson_new_empty();

   if ((id = mongo_object_id_new_from_string(user))) {
      mongo_bson_append_object_id(q, "user", id);
      mongo_object_id_free(id);
   } else {
      mongo_bson_append_string(q, "user", user);
   }

   cursor = g_object_new(MONGO_TYPE_CURSOR,
                         "database", priv->db,
                         "collection", priv->collection,
                         "connection", priv->mongo,
                         "limit", (gint)limit,
                         "query", q,
                         "skip", (gint)offset,
                         NULL);

   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_find_devices);
   g_simple_async_result_set_check_cancellable(simple, cancellable);

   devices = g_ptr_array_new_with_free_func(g_object_unref);
   g_simple_async_result_set_op_res_gpointer(simple, devices,
                                             (GDestroyNotify)g_ptr_array_unref);

   mongo_cursor_foreach_async(cursor,
                              postal_service_find_devices_foreach,
                              g_ptr_array_ref(devices),
                              (GDestroyNotify)g_ptr_array_unref,
                              cancellable,
                              postal_service_find_devices_cb,
                              simple);

   mongo_bson_unref(q);
   g_object_unref(cursor);

   EXIT;
}

/**
 * postal_service_find_devices_finish:
 * @service: (in): A #PostalService.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to postal_service_find_devices().
 * The result is a #GPtrArray of #PostalDevice instances. The resulting
 * #GPtrArray should be released using g_ptr_array_unref() when you are
 * finished.
 *
 * Returns: (transfer container) (element-type PostalDevice*):
 *   A #GList of #PostalDevice.
 */
GPtrArray *
postal_service_find_devices_finish (PostalService  *service,
                                    GAsyncResult   *result,
                                    GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GPtrArray *devices;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), NULL);
   g_return_val_if_fail(!error || !*error, NULL);

   if (!(devices = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
      RETURN(NULL);
   }

   RETURN(devices);
}

static void
postal_service_find_device_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoMessageReply *reply;
   MongoConnection *connection = (MongoConnection *)object;
   PostalDevice *device;
   GError *error = NULL;
   GList *list = NULL;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_connection_query_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      GOTO(failure);
   }

   if ((list = mongo_message_reply_get_documents(reply))) {
      device = postal_device_new();
      if (!postal_device_load_from_bson(device, list->data, &error)) {
         g_simple_async_result_take_error(simple, error);
         g_object_unref(device);
         GOTO(failure);
      }
      g_simple_async_result_set_op_res_gpointer(simple, device, g_object_unref);
   } else {
      g_simple_async_result_set_error(simple,
                                      POSTAL_DEVICE_ERROR,
                                      POSTAL_DEVICE_ERROR_NOT_FOUND,
                                      _("The device could not be found."));
   }

failure:
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
   g_list_foreach(list, (GFunc)mongo_bson_unref, NULL);
   g_list_free(list);

   EXIT;
}

void
postal_service_find_device (PostalService       *service,
                            const gchar         *user,
                            const gchar         *device,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
   PostalServicePrivate *priv;
   GSimpleAsyncResult *simple;
   MongoObjectId *oid;
   MongoBson *q;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(user);
   g_return_if_fail(device);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   q = mongo_bson_new_empty();

   mongo_bson_append_string(q, "device_token", device);

   if ((oid = mongo_object_id_new_from_string(user))) {
      mongo_bson_append_object_id(q, "user", oid);
      mongo_object_id_free(oid);
   } else {
      mongo_bson_append_string(q, "user", user);
   }

   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_find_device);
   mongo_connection_query_async(priv->mongo,
                                priv->db_and_collection,
                                0,
                                0,
                                MONGO_QUERY_NONE,
                                q,
                                NULL,
                                cancellable,
                                postal_service_find_device_cb,
                                simple);

   mongo_bson_unref(q);

   EXIT;
}

PostalDevice *
postal_service_find_device_finish (PostalService  *service,
                                   GAsyncResult   *result,
                                   GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   PostalDevice *ret;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      ret = g_object_ref(ret);
   }

   RETURN(ret);
}

static PushC2dmMessage *
postal_service_build_c2dm (PostalNotification *notification)
{
   PushC2dmMessage *message;
   const gchar *collapse_key;
   JsonObject *obj;
   JsonNode *node;
   gchar str[32];
   GList *list;
   GList *iter;
   GType type_id;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);

   collapse_key = postal_notification_get_collapse_key(notification);
   message = g_object_new(PUSH_TYPE_C2DM_MESSAGE,
                          "collapse-key", collapse_key,
                          NULL);


   if ((obj = postal_notification_get_c2dm(notification))) {
      list = json_object_get_members(obj);
      for (iter = list; iter; iter = iter->next) {
         node = json_object_get_member(obj, iter->data);
         type_id = json_node_get_value_type(node);

         switch (type_id) {
         case G_TYPE_BOOLEAN:
            push_c2dm_message_add_param(message,
                                        iter->data,
                                        json_node_get_boolean(node) ? "1" : "0");
            break;
         case G_TYPE_STRING:
            push_c2dm_message_add_param(message,
                                        iter->data,
                                        json_node_get_string(node));
            break;
         case G_TYPE_INT64:
            g_snprintf(str, sizeof str, "%"G_GINT64_FORMAT, json_node_get_int(node));
            push_c2dm_message_add_param(message, iter->data, str);
            break;
         case G_TYPE_DOUBLE:
            g_snprintf(str, sizeof str, "%lf", json_node_get_double(node));
            push_c2dm_message_add_param(message, iter->data, str);
            break;
         default:
            if (json_node_is_null(node)) {
               push_c2dm_message_add_param(message, iter->data, NULL);
            } else {
               g_warning("Unsupported JSON field type: %s", g_type_name(type_id));
            }
            break;
         }

         if (!g_strcmp0(iter->data, "delay_while_idle")) {
            if (json_node_get_value_type(node) == G_TYPE_BOOLEAN) {
               g_object_set(message,
                            "delay-while-idle", json_node_get_boolean(node),
                            NULL);
            }
         }
      }
      g_list_free(list);
   }

   RETURN(message);
}

static PushGcmMessage *
postal_service_build_gcm (PostalNotification *notification)
{
   PushGcmMessage *message;
   const gchar *collapse_key;
   JsonObject *obj;
   JsonNode *node;
   GList *list;
   GList *iter;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);

   collapse_key = postal_notification_get_collapse_key(notification);
   message = g_object_new(PUSH_TYPE_GCM_MESSAGE,
                          "collapse-key", collapse_key,
                          NULL);

   if ((obj = postal_notification_get_gcm(notification))) {
      list = json_object_get_members(obj);
      for (iter = list; iter; iter = iter->next) {
         node = json_object_get_member(obj, iter->data);

         if (!g_strcmp0(iter->data, "data") && JSON_NODE_HOLDS_OBJECT(node)) {
            push_gcm_message_set_data(message, json_node_get_object(node));
         } else if (!g_strcmp0(iter->data, "delay_while_idle")) {
            if (json_node_get_value_type(node) == G_TYPE_BOOLEAN) {
               push_gcm_message_set_delay_while_idle(
                     message, json_node_get_boolean(node));
            }
         } else if (!g_strcmp0(iter->data, "dry_run")) {
            if (json_node_get_value_type(node) == G_TYPE_BOOLEAN) {
               push_gcm_message_set_dry_run(message,
                                            json_node_get_boolean(node));
            }
         } else if (!g_strcmp0(iter->data, "time_to_live")) {
            if (json_node_get_value_type(node) == G_TYPE_INT64) {
               push_gcm_message_set_time_to_live(message,
                                                 json_node_get_int(node));
            }
         }
      }
      g_list_free(list);
   }

   RETURN(message);
}

static PushApsMessage *
postal_service_build_aps (PostalNotification *notification)
{
   PushApsMessage *message;
   JsonObject *obj;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);

   if ((obj = postal_notification_get_aps(notification))) {
      message = push_aps_message_new_from_json(obj);
   } else {
      message = push_aps_message_new();
   }

   RETURN(message);
}

static void
postal_service_notify_c2dm_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
   PushC2dmClient *client = (PushC2dmClient *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(PUSH_IS_C2DM_CLIENT(client));

   if (!push_c2dm_client_deliver_finish(client, result, &error)) {
      g_message("%s", error->message);
      g_error_free(error);
   }

   EXIT;
}

static void
postal_service_notify_gcm_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   PushGcmClient *client = (PushGcmClient *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(PUSH_IS_GCM_CLIENT(client));

   if (!push_gcm_client_deliver_finish(client, result, &error)) {
      g_message("%s", error->message);
      g_error_free(error);
   }

   EXIT;
}

static void
postal_service_notify_aps_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   PushApsClient *client = (PushApsClient *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(PUSH_IS_APS_CLIENT(client));

   if (!push_aps_client_deliver_finish(client, result, &error)) {
      g_message("%s", error->message);
      g_error_free(error);
   }

   EXIT;
}

static void
postal_service_notify_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
   PostalServicePrivate *priv;
   PostalNotification *notif;
   GSimpleAsyncResult *simple = user_data;
   MongoMessageReply *reply;
   PushC2dmIdentity *c2dm;
   PostalDeviceType device_type;
   PushC2dmMessage *c2dm_message;
   PushGcmIdentity *gcm;
   PushGcmMessage *gcm_message;
   MongoConnection *connection = (MongoConnection *)object;
   PushApsIdentity *aps;
   PushApsMessage *aps_message;
   PostalService *service;
   PostalDevice *device;
   const gchar *device_token;
   GObject *source;
   GError *error = NULL;
   GList *gcm_devices = NULL;
   GList *list;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   source = g_async_result_get_source_object(user_data);
   service = POSTAL_SERVICE(source);
   priv = service->priv;
   g_object_unref(source);

   if (!(reply = mongo_connection_query_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      GOTO(failure);
   }

   notif = g_object_get_data(G_OBJECT(simple), "notification");
   aps_message = postal_service_build_aps(notif);
   c2dm_message = postal_service_build_c2dm(notif);
   gcm_message = postal_service_build_gcm(notif);

   /*
    * TODO: Right now, this is just sending a message to all of these users.
    *       It would be nice if we could add a little delay so we can collapse
    *       multiple messages into one.
    *
    *       The trivial way to do this would be to keep track per user what
    *       they have and only send one. However, that would introduce delay
    *       and require more memory.
    *
    *       I think we can simply keep the device+collapse_key and ignore
    *       sending the second if the first was sent within the last 10
    *       seconds or so. (Then evict that message).
    */

   list = mongo_message_reply_get_documents(reply);
   for (; list; list = list->next) {
      /*
       * Inflate a PostalDevice for this MongoBson.
       */
      device = postal_device_new();
      if (!postal_device_load_from_bson(device, list->data, NULL)) {
         g_object_unref(device);
         continue;
      }

      /*
       * Fetch the device type and device token.
       */
      device_type = postal_device_get_device_type(device);
      device_token = postal_device_get_device_token(device);
      if (!device_type || !device_token) {
         g_object_unref(device);
         continue;
      }

      /*
       * See if we can ignore this message.
       */
      if (!postal_service_should_ignore(service, device, notif)) {
         /*
          * Build the provider specific message.
          */
         switch (device_type) {
         case POSTAL_DEVICE_APS:
            gcm = push_gcm_identity_new(device_token);
            gcm_devices = g_list_append(gcm_devices, gcm);
            postal_metrics_device_notified(priv->metrics, device);
            break;
         case POSTAL_DEVICE_C2DM:
            c2dm = g_object_new(PUSH_TYPE_C2DM_IDENTITY,
                                "registration-id", device_token,
                                NULL);
            push_c2dm_client_deliver_async(priv->c2dm,
                                           c2dm,
                                           c2dm_message,
                                           NULL, /* TODO: */
                                           postal_service_notify_c2dm_cb,
                                           NULL);
            postal_metrics_device_notified(priv->metrics, device);
            g_object_unref(c2dm);
            break;
         case POSTAL_DEVICE_GCM:
            aps = g_object_new(PUSH_TYPE_APS_IDENTITY,
                               "device-token", device_token,
                               NULL);
            push_aps_client_deliver_async(priv->aps,
                                          aps,
                                          aps_message,
                                          NULL, /* TODO: */
                                          postal_service_notify_aps_cb,
                                          NULL);
            postal_metrics_device_notified(priv->metrics, device);
            g_object_unref(aps);
            break;
         default:
            g_assert_not_reached();
            break;
         }
      }

      g_object_unref(device);
   }

   if (gcm_devices) {
      push_gcm_client_deliver_async(priv->gcm,
                                    gcm_devices,
                                    gcm_message,
                                    NULL, /* TODO: */
                                    postal_service_notify_gcm_cb,
                                    NULL);
      g_list_foreach(gcm_devices, (GFunc)g_object_unref, NULL);
      g_list_free(gcm_devices);
   }

   g_object_unref(aps_message);
   g_object_unref(c2dm_message);
   g_object_unref(gcm_message);

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);

failure:
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

void
postal_service_notify (PostalService        *service,
                       PostalNotification   *notification,
                       gchar               **users,
                       MongoObjectId       **devices,
                       gsize                 n_devices,
                       GCancellable         *cancellable,
                       GAsyncReadyCallback   callback,
                       gpointer              user_data)
{
   PostalServicePrivate *priv;
   GSimpleAsyncResult *simple;
   MongoObjectId *oid;
   MongoBson *ar;
   MongoBson *in;
   MongoBson *or;
   MongoBson *q;
   gchar idxstr[12];
   guint i;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(POSTAL_IS_NOTIFICATION(notification));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   or = mongo_bson_new_empty();
   q = mongo_bson_new_empty();
   ar = mongo_bson_new_empty();

   for (i = 0; i < n_devices; i++) {
      g_snprintf(idxstr, sizeof idxstr, "%u", i);
      idxstr[sizeof idxstr - 1] = '\0';
      mongo_bson_append_object_id(ar, idxstr, devices[i]);
   }

   in = mongo_bson_new_empty();
   mongo_bson_append_array(in, "$in", ar);
   mongo_bson_append_bson(q, "devices", in);
   mongo_bson_append_bson(or, "0", q);
   mongo_bson_unref(q);
   mongo_bson_unref(ar);
   mongo_bson_unref(in);

   q = mongo_bson_new_empty();
   ar = mongo_bson_new_empty();

   for (i = 0; users[i]; i++) {
      g_snprintf(idxstr, sizeof idxstr, "%u", i);
      idxstr[sizeof idxstr - 1] = '\0';
      if ((oid = mongo_object_id_new_from_string(users[i]))) {
         mongo_bson_append_object_id(ar, idxstr, oid);
         mongo_object_id_free(oid);
      } else {
         mongo_bson_append_string(ar, idxstr, users[i]);
      }
   }

   in = mongo_bson_new_empty();
   mongo_bson_append_array(in, "$in", ar);
   mongo_bson_append_bson(q, "user", in);
   mongo_bson_append_bson(or, "1", q);

   mongo_bson_unref(ar);
   mongo_bson_unref(in);
   mongo_bson_unref(q);

   q = mongo_bson_new_empty();
   mongo_bson_append_array(q, "$or", or);
   mongo_bson_append_null(q, "removed_at");
   mongo_bson_unref(or);

   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_notify);
   g_simple_async_result_set_check_cancellable(simple, cancellable);
   g_object_set_data_full(G_OBJECT(simple),
                          "notification",
                          g_object_ref(notification),
                          g_object_unref);

   mongo_connection_query_async(priv->mongo,
                                priv->db_and_collection,
                                MONGO_QUERY_NONE,
                                0,
                                100,
                                q,
                                NULL,
                                cancellable,
                                postal_service_notify_cb,
                                simple);

   mongo_bson_unref(q);

   EXIT;
}

gboolean
postal_service_notify_finish (PostalService  *service,
                              GAsyncResult   *result,
                              GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret;
}

static void
postal_service_aps_identity_removed_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
   MongoConnection *connection = (MongoConnection *)object;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));

   if (!mongo_connection_update_finish(connection,
                                       result,
                                       NULL,
                                       &error)) {
      g_message("Device removal failed: %s\n", error->message);
      g_error_free(error);
   }

   EXIT;
}

static void
postal_service_aps_identity_removed (PostalService   *service,
                                     PushApsIdentity *identity,
                                     PushApsClient   *client)
{
   PostalServicePrivate *priv;
   MongoBson *q;
   MongoBson *set;
   MongoBson *u;
   GTimeVal tv;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(PUSH_IS_APS_IDENTITY(identity));

   priv = service->priv;

   q = mongo_bson_new_empty();
   mongo_bson_append_string(q, "device_type", "aps");
   mongo_bson_append_string(q, "device_token",
                            push_aps_identity_get_device_token(identity));
   mongo_bson_append_null(q, "removed_at");

   g_get_current_time(&tv);
   set = mongo_bson_new_empty();
   mongo_bson_append_timeval(set, "removed_at", &tv);

   u = mongo_bson_new_empty();
   mongo_bson_append_bson(u, "$set", set);

   mongo_connection_update_async(priv->mongo,
                                 priv->db_and_collection,
                                 MONGO_UPDATE_MULTI_UPDATE,
                                 q,
                                 u,
                                 NULL,
                                 postal_service_aps_identity_removed_cb,
                                 NULL);

   mongo_bson_unref(q);
   mongo_bson_unref(set);
   mongo_bson_unref(u);

   EXIT;
}

static void
postal_service_c2dm_identity_removed_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
   MongoConnection *connection = (MongoConnection *)object;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));

   if (!mongo_connection_update_finish(connection,
                                       result,
                                       NULL,
                                       &error)) {
      g_message("Device removal failed: %s\n", error->message);
      g_error_free(error);
   }

   EXIT;
}

static void
postal_service_c2dm_identity_removed (PostalService    *service,
                                      PushC2dmIdentity *identity,
                                      PushC2dmClient   *client)
{
   PostalServicePrivate *priv;
   MongoBson *q;
   MongoBson *set;
   MongoBson *u;
   GTimeVal tv;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(PUSH_IS_C2DM_IDENTITY(identity));

   priv = service->priv;

   q = mongo_bson_new_empty();
   mongo_bson_append_string(q, "device_type", "c2dm");
   mongo_bson_append_string(q, "device_token",
                            push_c2dm_identity_get_registration_id(identity));
   mongo_bson_append_null(q, "removed_at");

   g_get_current_time(&tv);
   set = mongo_bson_new_empty();
   mongo_bson_append_timeval(set, "removed_at", &tv);

   u = mongo_bson_new_empty();
   mongo_bson_append_bson(u, "$set", set);

   mongo_connection_update_async(priv->mongo,
                                 priv->db_and_collection,
                                 MONGO_UPDATE_MULTI_UPDATE,
                                 q,
                                 u,
                                 NULL,
                                 postal_service_c2dm_identity_removed_cb,
                                 NULL);

   mongo_bson_unref(q);
   mongo_bson_unref(set);
   mongo_bson_unref(u);

   EXIT;
}

static void
postal_service_gcm_identity_removed_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
   MongoConnection *connection = (MongoConnection *)object;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));

   if (!mongo_connection_update_finish(connection,
                                       result,
                                       NULL,
                                       &error)) {
      g_message("Device removal failed: %s\n", error->message);
      g_error_free(error);
   }

   EXIT;
}

static void
postal_service_gcm_identity_removed (PostalService   *service,
                                     PushGcmIdentity *identity,
                                     PushGcmClient   *client)
{
   PostalServicePrivate *priv;
   MongoBson *q;
   MongoBson *set;
   MongoBson *u;
   GTimeVal tv;

   ENTRY;

   g_assert(POSTAL_IS_SERVICE(service));
   g_assert(PUSH_IS_GCM_IDENTITY(identity));

   priv = service->priv;

   q = mongo_bson_new_empty();
   mongo_bson_append_string(q, "device_type", "gcm");
   mongo_bson_append_string(q, "device_token",
                            push_gcm_identity_get_registration_id(identity));
   mongo_bson_append_null(q, "removed_at");

   g_get_current_time(&tv);
   set = mongo_bson_new_empty();
   mongo_bson_append_timeval(set, "removed_at", &tv);

   u = mongo_bson_new_empty();
   mongo_bson_append_bson(u, "$set", set);

   mongo_connection_update_async(priv->mongo,
                                 priv->db_and_collection,
                                 MONGO_UPDATE_MULTI_UPDATE,
                                 q,
                                 u,
                                 NULL,
                                 postal_service_gcm_identity_removed_cb,
                                 NULL);

   mongo_bson_unref(q);
   mongo_bson_unref(set);
   mongo_bson_unref(u);

   EXIT;
}

/**
 * postal_service_start:
 * @service: A #PostalService.
 *
 * Starts the #PostalService instance. This configures MongoDB access as well
 * as communication to Apple's push service.
 */
void
postal_service_start (NeoServiceBase *base,
                      GKeyFile       *config)
{
   PostalServicePrivate *priv;
   PushApsClientMode aps_mode;
   PostalService *service = (PostalService *)base;
   NeoService *peer;
   gchar *c2dm_auth_token = NULL;
   gchar *gcm_auth_token = NULL;
   gchar *ssl_cert_file = NULL;
   gchar *ssl_key_file = NULL;
   gchar *uri = NULL;
   guint feedback_interval_sec;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));

   priv = service->priv;

   /*
    * Set default values.
    */
   aps_mode = PUSH_APS_CLIENT_PRODUCTION;
   c2dm_auth_token = NULL;
   gcm_auth_token = NULL;
   ssl_cert_file = NULL;
   ssl_key_file = NULL;
   feedback_interval_sec = 10;

#define GET_STRING_KEY(g,n) g_key_file_get_string(config, g, n, NULL)
   /*
    * Apply configuration.
    */
   if (config) {
      if (g_key_file_get_boolean(config, "aps", "sandbox", NULL)) {
         aps_mode = PUSH_APS_CLIENT_SANDBOX;
      }

      ssl_cert_file = GET_STRING_KEY("aps", "ssl-cert-file");
      ssl_key_file = GET_STRING_KEY("aps", "ssl-key-file");
      c2dm_auth_token = GET_STRING_KEY("c2dm", "auth-token");
      gcm_auth_token = GET_STRING_KEY("gcm", "auth-token");
      uri = GET_STRING_KEY("mongo", "uri");

      g_free(priv->collection);
      priv->collection = GET_STRING_KEY("mongo", "collection");

      g_free(priv->db);
      priv->db = GET_STRING_KEY("mongo", "db");

      g_free(priv->db_and_collection);
      priv->db_and_collection = g_strdup_printf("%s.%s",
                                                priv->db,
                                                priv->collection);

      g_free(priv->db_and_cmd);
      priv->db_and_cmd = g_strdup_printf("%s.$cmd", priv->db);
   }
#undef GET_STRING_KEY

   priv->aps = g_object_new(PUSH_TYPE_APS_CLIENT,
                            "feedback-interval", feedback_interval_sec,
                            "mode", aps_mode,
                            "ssl-cert-file", ssl_cert_file,
                            "ssl-key-file", ssl_key_file,
                            NULL);
   priv->c2dm = g_object_new(PUSH_TYPE_C2DM_CLIENT,
                             "auth-token", c2dm_auth_token,
                             NULL);
   priv->gcm = g_object_new(PUSH_TYPE_GCM_CLIENT,
                            "auth-token", gcm_auth_token,
                            NULL);
   priv->mongo = mongo_connection_new_from_uri(uri);

   if ((peer = neo_service_get_peer(NEO_SERVICE(base), "metrics"))) {
      priv->metrics = g_object_ref(peer);
   }

   g_signal_connect_swapped(priv->aps,
                            "identity-removed",
                            G_CALLBACK(postal_service_aps_identity_removed),
                            service);

   g_signal_connect_swapped(priv->c2dm,
                            "identity-removed",
                            G_CALLBACK(postal_service_c2dm_identity_removed),
                            service);

   g_signal_connect_swapped(priv->gcm,
                            "identity-removed",
                            G_CALLBACK(postal_service_gcm_identity_removed),
                            service);

   g_free(ssl_cert_file);
   g_free(ssl_key_file);
   g_free(c2dm_auth_token);
   g_free(gcm_auth_token);
   g_free(uri);

   EXIT;
}

static void
postal_service_finalize (GObject *object)
{
   PostalServicePrivate *priv;
   guint i;

   ENTRY;

   priv = POSTAL_SERVICE(object)->priv;

   g_clear_object(&priv->aps);
   g_clear_object(&priv->c2dm);
   g_clear_object(&priv->mongo);
   g_clear_object(&priv->metrics);

   g_free(priv->db);
   g_free(priv->collection);
   g_free(priv->db_and_cmd);
   g_free(priv->db_and_collection);

   for (i = 0; i < priv->n_caches; i++) {
      postal_dm_cache_unref(priv->caches[i]);
      priv->caches[i] = NULL;
   }

   g_free(priv->caches);

   priv->n_caches = 0;
   priv->caches = NULL;

   G_OBJECT_CLASS(postal_service_parent_class)->finalize(object);

   EXIT;
}

static void
postal_service_class_init (PostalServiceClass *klass)
{
   GObjectClass *object_class;
   NeoServiceBaseClass *service_base_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_service_finalize;
   g_type_class_add_private(object_class, sizeof(PostalServicePrivate));

   service_base_class = NEO_SERVICE_BASE_CLASS(klass);
   service_base_class->start = postal_service_start;

   EXIT;
}

static void
postal_service_init (PostalService *service)
{
   guint i;

   ENTRY;

   service->priv = G_TYPE_INSTANCE_GET_PRIVATE(service,
                                               POSTAL_TYPE_SERVICE,
                                               PostalServicePrivate);
   service->priv->db_and_cmd = g_strdup("test.$cmd");
   service->priv->db_and_collection = g_strdup("test.devices");
   service->priv->db = g_strdup("test");
   service->priv->collection = g_strdup("devices");

   service->priv->n_caches = POSTAL_SERVICE_DM_CACHES;
   service->priv->caches = g_new0(PostalDmCache*, service->priv->n_caches);
   for (i = 0; i < service->priv->n_caches; i++) {
      service->priv->caches[i] =
         postal_dm_cache_new(POSTAL_SERVICE_DM_CACHE_ENTRIES,
                             g_str_hash,
                             g_str_equal,
                             g_free);
   }

   EXIT;
}
