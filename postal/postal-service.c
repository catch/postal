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
#include <push-glib/push-glib.h>

#include "postal-debug.h"
#ifdef ENABLE_REDIS
#include "postal-redis.h"
#endif
#include "postal-service.h"

G_DEFINE_TYPE(PostalService, postal_service, G_TYPE_OBJECT)

struct _PostalServicePrivate
{
   PushApsClient   *aps;
   PushC2dmClient  *c2dm;
   PushGcmClient   *gcm;
   GKeyFile        *config;
   gchar           *db_and_collection;
   MongoConnection *mongo;
};

enum
{
   PROP_0,
   PROP_CONFIG,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * postal_service_get_default:
 *
 * Fetches the default instance of #PostalService. This is the singleton
 * instance that should be used throughout the life of the process.
 *
 * Returns: (transfer none): A #PostalService.
 */
PostalService *
postal_service_get_default (void)
{
   static PostalService *service;
   PostalService *instance;

   if (g_once_init_enter(&service)) {
      instance = g_object_new(POSTAL_TYPE_SERVICE, NULL);
      g_once_init_leave(&service, instance);
   }

   return service;
}

/**
 * postal_service_get_config:
 * @service: (in): A #PostalService.
 *
 * Fetch the configuration being used by the #PostalService instance.
 *
 * Returns: (transfer none): A #GKeyFile.
 */
GKeyFile *
postal_service_get_config (PostalService *service)
{
   g_return_val_if_fail(POSTAL_IS_SERVICE(service), NULL);
   return service->priv->config;
}

static void
postal_service_add_device_cb (GObject      *object,
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

   if (!(ret = mongo_connection_insert_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
#ifdef ENABLE_REDIS
   } else {
      PostalDevice *device;

      device = POSTAL_DEVICE(g_object_get_data(G_OBJECT(simple), "device"));
      postal_redis_device_added(device);
#endif
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
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
   MongoBsonIter iter;
   MongoBson *bson;
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

   /*
    * Asynchronously insert the document to Mongo.
    */
   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_add_device);
   g_object_set_data_full(G_OBJECT(simple),
                          "device",
                          g_object_ref(device),
                          g_object_unref);
   mongo_connection_insert_async(priv->mongo,
                                 priv->db_and_collection,
                                 MONGO_INSERT_NONE,
                                 &bson,
                                 1,
                                 cancellable,
                                 postal_service_add_device_cb,
                                 simple);
   mongo_bson_unref(bson);

   EXIT;
}

/**
 * postal_service_add_device_finish:
 * @service: (in): A #PostalService.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to postal_service_add_device().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
postal_service_add_device_finish (PostalService  *service,
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

   if (!(ret = mongo_connection_update_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
#ifdef ENABLE_REDIS
   } else {
      PostalDevice *device;

      device = POSTAL_DEVICE(g_object_get_data(G_OBJECT(simple), "device"));
      postal_redis_device_removed(device);
#endif
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
   const MongoObjectId *id;
   GSimpleAsyncResult *simple;
   MongoObjectId *user_id;
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
   if (!(id = postal_device_get_id(device))) {
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
   mongo_bson_append_object_id(q, "_id", id);
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
   MongoMessageReply *reply;
   MongoConnection *connection = (MongoConnection *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_connection_query_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      GOTO(cleanup);
   }

   g_simple_async_result_set_op_res_gpointer(simple, reply, g_object_unref);

cleanup:
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
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

   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_find_devices);
   g_simple_async_result_set_check_cancellable(simple, cancellable);
   mongo_connection_query_async(priv->mongo,
                                priv->db_and_collection,
                                MONGO_QUERY_NONE,
                                offset,
                                limit,
                                q,
                                NULL,
                                cancellable,
                                postal_service_find_devices_cb,
                                simple);

   mongo_bson_unref(q);

   EXIT;
}

/**
 * postal_service_find_devices_finish:
 * @service: (in): A #PostalService.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to postal_service_find_devices().
 * The resulting #GList of #MongoBson is owned by the caller and should be
 * freed using the following:
 *
 * <informalexample>
 *   <programlisting>
 * g_list_foreach(list, (GFunc)mongo_bson_unref, NULL);
 * g_list_free(list);
 *   </programlisting>
 * </informalexample>
 *
 * Returns: (transfer full) (element-type MongoBson*): A #GList of #MongoBson.
 */
GList *
postal_service_find_devices_finish (PostalService  *service,
                                    GAsyncResult   *result,
                                    GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *reply;
   GList *list;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), NULL);
   g_return_val_if_fail(!error || !*error, NULL);

   if (!(reply = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
      RETURN(NULL);
   }

   list = mongo_message_reply_get_documents(reply);
   list = g_list_copy(list);
   g_list_foreach(list, (GFunc)mongo_bson_ref, NULL);

   RETURN(list);
}

static void
postal_service_find_device_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoMessageReply *reply;
   MongoConnection *connection = (MongoConnection *)object;
   GError *error = NULL;
   GList *list;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_connection_query_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      GOTO(failure);
   }

   if ((list = mongo_message_reply_get_documents(reply))) {
      g_simple_async_result_set_op_res_gpointer(simple, mongo_bson_ref(list->data),
                                                (GDestroyNotify)mongo_bson_unref);
   } else {
      g_simple_async_result_set_error(simple,
                                      POSTAL_DEVICE_ERROR,
                                      POSTAL_DEVICE_ERROR_NOT_FOUND,
                                      _("The device could not be found."));
   }

failure:
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

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
   MongoObjectId *_id;
   MongoBson *q;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(user);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   q = mongo_bson_new_empty();

   if (!(_id = mongo_object_id_new_from_string(device))) {
      g_simple_async_report_error_in_idle(G_OBJECT(service),
                                          callback,
                                          user_data,
                                          POSTAL_DEVICE_ERROR,
                                          POSTAL_DEVICE_ERROR_INVALID_ID,
                                          _("device id is invalid."));
      GOTO(failure);
   }

   mongo_bson_append_object_id(q, "_id", _id);
   mongo_object_id_free(_id);

   if ((_id = mongo_object_id_new_from_string(user))) {
      mongo_bson_append_object_id(q, "user", _id);
      mongo_object_id_free(_id);
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

failure:
   mongo_bson_unref(q);

   EXIT;
}

MongoBson *
postal_service_find_device_finish (PostalService  *service,
                                   GAsyncResult   *result,
                                   GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoBson *ret;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_SERVICE(service), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      ret = mongo_bson_ref(ret);
   }

   RETURN(ret);
}

static void
postal_service_update_device_cb (GObject      *object,
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

   if (!(ret = mongo_connection_update_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
#ifdef ENABLE_REDIS
   } else {
      PostalDevice *device;

      device = POSTAL_DEVICE(g_object_get_data(G_OBJECT(simple), "device"));
      postal_redis_device_updated(device);
#endif
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

void
postal_service_update_device (PostalService       *service,
                              PostalDevice        *device,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
   PostalServicePrivate *priv;
   const MongoObjectId *id;
   GSimpleAsyncResult *simple;
   MongoObjectId *user_id;
   const gchar *user;
   MongoBson *q;
   MongoBson *u;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = service->priv;

   /*
    * Make sure we have an _id field to remove.
    */
   if (!(id = postal_device_get_id(device))) {
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
    * Build our update document.
    */
   if (!(u = postal_device_save_to_bson(device, &error))) {
      g_simple_async_report_take_gerror_in_idle(G_OBJECT(service),
                                                callback,
                                                user_data,
                                                error);
      EXIT;
   }

   /*
    * Build our query for the device to update.
    */
   q = mongo_bson_new_empty();
   mongo_bson_append_object_id(q, "_id", id);
   if ((user_id = mongo_object_id_new_from_string(user))) {
      mongo_bson_append_object_id(q, "user", user_id);
      mongo_object_id_free(user_id);
   } else {
      mongo_bson_append_string(q, "user", user);
   }
   mongo_bson_append_null(q, "removed_at");

   /*
    * Asynchronously update the document.
    */
   simple = g_simple_async_result_new(G_OBJECT(service), callback, user_data,
                                      postal_service_update_device);
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
                                 postal_service_update_device_cb,
                                 simple);

   mongo_bson_unref(q);
   mongo_bson_unref(u);

   EXIT;
}

gboolean
postal_service_update_device_finish (PostalService  *service,
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
   PushC2dmMessage *c2dm_message;
   PushGcmMessage *gcm_message;
   MongoConnection *connection = (MongoConnection *)object;
   PushApsIdentity *aps;
   PushApsMessage *aps_message;
   MongoBsonIter iter;
   const gchar *device_type;
   const gchar *device_token;
   GError *error = NULL;
   GList *list;
   GList item;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   priv = POSTAL_SERVICE_DEFAULT->priv;

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

   /*
    * TODO: We need to inflate all of the GCM identities together so that
    *       we can send them a message via a single API call to Google.
    */

   list = mongo_message_reply_get_documents(reply);
   for (; list; list = list->next) {
      if (mongo_bson_iter_init_find(&iter, list->data, "device_type")) {
         device_type = mongo_bson_iter_get_value_string(&iter, NULL);
         if (!g_strcmp0(device_type, "gcm")) {
            if (mongo_bson_iter_init_find(&iter, list->data, "device_token")) {
               memset(&item, 0, sizeof item);
               device_token = mongo_bson_iter_get_value_string(&iter, NULL);
               item.data = push_gcm_identity_new(device_token);
               push_gcm_client_deliver_async(priv->gcm,
                                             &item,
                                             gcm_message,
                                             NULL, /* TODO: */
                                             postal_service_notify_gcm_cb,
                                             NULL);
               g_object_unref(item.data);
            }
         } else if (!g_strcmp0(device_type, "c2dm")) {
            if (mongo_bson_iter_init_find(&iter, list->data, "device_token")) {
               device_token = mongo_bson_iter_get_value_string(&iter, NULL);
               c2dm = g_object_new(PUSH_TYPE_C2DM_IDENTITY,
                                   "registration-id", device_token,
                                   NULL);
               push_c2dm_client_deliver_async(priv->c2dm,
                                              c2dm,
                                              c2dm_message,
                                              NULL, /* TODO: */
                                              postal_service_notify_c2dm_cb,
                                              NULL);
               g_object_unref(c2dm);
            }
         } else if (!g_strcmp0(device_type, "aps")) {
            if (mongo_bson_iter_init_find(&iter, list->data, "device_token")) {
               device_token = mongo_bson_iter_get_value_string(&iter, NULL);
               aps = g_object_new(PUSH_TYPE_APS_IDENTITY,
                                  "device-token", device_token,
                                  NULL);
               push_aps_client_deliver_async(priv->aps,
                                             aps,
                                             aps_message,
                                             NULL, /* TODO: */
                                             postal_service_notify_aps_cb,
                                             NULL);
               g_object_unref(aps);
            }
         } else {
            g_warning("Unknown device_type %s", device_type);
         }
      }
   }

   g_object_unref(aps_message);
   g_object_unref(c2dm_message);

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

   if (!mongo_connection_update_finish(connection, result, &error)) {
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

   if (!mongo_connection_update_finish(connection, result, &error)) {
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

   if (!mongo_connection_update_finish(connection, result, &error)) {
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
 * postal_service_set_config:
 * @service: (in): A #PostalService.
 * @config: (in): A #GKeyFile.
 *
 * Sets the #GKeyFile to use for configuration of the service.
 */
void
postal_service_set_config (PostalService *service,
                           GKeyFile      *config)
{
   PostalServicePrivate *priv;

   ENTRY;

   g_return_if_fail(POSTAL_IS_SERVICE(service));

   priv = service->priv;

   if (priv->config) {
      g_key_file_unref(priv->config);
      priv->config = NULL;
   }
   if (config) {
      priv->config = g_key_file_ref(config);
   }
   g_object_notify_by_pspec(G_OBJECT(service), gParamSpecs[PROP_CONFIG]);

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
postal_service_start (PostalService *service)
{
   PostalServicePrivate *priv;
   PushApsClientMode aps_mode;
   gchar *c2dm_auth_token = NULL;
   gchar *gcm_auth_token = NULL;
   gchar *collection = NULL;
   gchar *db = NULL;
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

#define GET_STRING_KEY(g,n) g_key_file_get_string(priv->config, g, n, NULL)
   /*
    * Apply configuration.
    */
   if (priv->config) {
      if (g_key_file_get_boolean(priv->config, "aps", "sandbox", NULL)) {
         aps_mode = PUSH_APS_CLIENT_SANDBOX;
      }

      ssl_cert_file = GET_STRING_KEY("aps", "ssl-cert-file");
      ssl_key_file = GET_STRING_KEY("aps", "ssl-key-file");
      c2dm_auth_token = GET_STRING_KEY("c2dm", "auth-token");
      gcm_auth_token = GET_STRING_KEY("gcm", "auth-token");
      collection = GET_STRING_KEY("mongo", "collection");
      db = GET_STRING_KEY("mongo", "db");
      uri = GET_STRING_KEY("mongo", "uri");

      g_free(priv->db_and_collection);
      priv->db_and_collection = g_strdup_printf("%s.%s", db, collection);
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
   g_free(collection);
   g_free(db);
   g_free(uri);

   EXIT;
}

static void
postal_service_finalize (GObject *object)
{
   PostalServicePrivate *priv;

   ENTRY;

   priv = POSTAL_SERVICE(object)->priv;

   g_clear_object(&priv->aps);
   g_clear_object(&priv->c2dm);
   g_clear_object(&priv->mongo);

   if (priv->config) {
      g_key_file_unref(priv->config);
      priv->config = NULL;
   }

   G_OBJECT_CLASS(postal_service_parent_class)->finalize(object);

   EXIT;
}

static void
postal_service_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
   PostalService *service = POSTAL_SERVICE(object);

   switch (prop_id) {
   case PROP_CONFIG:
      g_value_set_boxed(value, postal_service_get_config(service));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_service_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
   PostalService *service = POSTAL_SERVICE(object);

   switch (prop_id) {
   case PROP_CONFIG:
      postal_service_set_config(service, g_value_get_boxed(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_service_class_init (PostalServiceClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_service_finalize;
   object_class->get_property = postal_service_get_property;
   object_class->set_property = postal_service_set_property;
   g_type_class_add_private(object_class, sizeof(PostalServicePrivate));

   gParamSpecs[PROP_CONFIG] =
      g_param_spec_boxed("config",
                         _("Config"),
                         _("A GKeyFile containing the configuration."),
                         G_TYPE_KEY_FILE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_CONFIG,
                                   gParamSpecs[PROP_CONFIG]);

   EXIT;
}

static void
postal_service_init (PostalService *service)
{
   ENTRY;

   service->priv = G_TYPE_INSTANCE_GET_PRIVATE(service,
                                               POSTAL_TYPE_SERVICE,
                                               PostalServicePrivate);
   service->priv->db_and_collection = g_strdup("test.devices");

   EXIT;
}
