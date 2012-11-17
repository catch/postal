/* redis-client.c
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
#include <string.h>

#include "redis-adapter.h"
#include "redis-client.h"

G_DEFINE_TYPE(RedisClient, redis_client, G_TYPE_OBJECT)

typedef struct
{
   guint id;
   gchar *channel;
   RedisPubsubCallback callback;
   gpointer user_data;
   GDestroyNotify notify;
} RedisClientSub;

struct _RedisClientPrivate
{
   GSimpleAsyncResult *async_connect;
   guint context_source;
   redisAsyncContext *context;
   GMainContext *main_context;

   /* Pubsub datastructures */
   guint next_sub;
   GHashTable *sub_by_id;
   GHashTable *subs_by_channel;
   gboolean dispatching;
};

RedisClient *
redis_client_new (void)
{
   return g_object_new(REDIS_TYPE_CLIENT, NULL);
}

static GVariant *
redis_client_build_variant (redisReply  *reply,
                            GError     **error)
{
   g_assert(reply);

   switch (reply->type) {
   case REDIS_REPLY_STATUS:
      return g_variant_new_string(reply->str);
   case REDIS_REPLY_STRING:
      return g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                       reply->str,
                                       reply->len,
                                       sizeof(guint8));
   case REDIS_REPLY_ARRAY: {
      GVariantBuilder b;
      GVariant *v;
      guint i;

      g_variant_builder_init(&b, G_VARIANT_TYPE_ARRAY);
      for (i = 0; i < reply->elements; i++) {
         if (!(v = redis_client_build_variant(reply->element[i], error))) {
            g_variant_builder_clear(&b);
            return NULL;
         }
         g_variant_builder_add_value(&b, v);
      }
      return g_variant_builder_end(&b);
   }
   case REDIS_REPLY_INTEGER:
      return g_variant_new_int64(reply->integer);
   case REDIS_REPLY_NIL:
      return g_variant_new_maybe(G_VARIANT_TYPE_STRING, NULL);
   case REDIS_REPLY_ERROR:
      g_set_error(error,
                  REDIS_CLIENT_ERROR,
                  REDIS_CLIENT_ERROR_HIREDIS,
                  "%s", reply->str);
      return NULL;
   default:
      break;
   }

   g_set_error(error,
               REDIS_CLIENT_ERROR,
               REDIS_CLIENT_ERROR_HIREDIS,
               _("Invalid reply from redis."));
   return NULL;
}

static void
redis_client_command_cb (redisAsyncContext *context,
                         gpointer           r,
                         gpointer           user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GVariant *variant;
   GError *error = NULL;

   g_return_if_fail(context);
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (r) {
      if (!(variant = redis_client_build_variant(r, &error))) {
         g_simple_async_result_take_error(simple, error);
      } else {
         g_simple_async_result_set_op_res_gpointer(
               simple, variant, (GDestroyNotify)g_variant_unref);
      }
   } else {
      g_simple_async_result_set_error(simple,
                                      REDIS_CLIENT_ERROR,
                                      REDIS_CLIENT_ERROR_HIREDIS,
                                      _("Missing reply from redis."));
   }

   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
}

void
redis_client_command_async (RedisClient         *client,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data,
                            const gchar         *format,
                            ...)
{
   RedisClientPrivate *priv;
   GSimpleAsyncResult *simple;
   va_list args;

   g_return_if_fail(REDIS_IS_CLIENT(client));
   g_return_if_fail(callback);
   g_return_if_fail(format);

   priv = client->priv;

   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      redis_client_command_async);

   va_start(args, format);
   redisvAsyncCommand(priv->context,
                      redis_client_command_cb,
                      simple,
                      format,
                      args);
   va_end(args);
}

GVariant *
redis_client_command_finish (RedisClient   *client,
                             GAsyncResult  *result,
                             GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GVariant *ret;

   g_return_val_if_fail(REDIS_IS_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret ? g_variant_ref(ret) : NULL;
}

static void
redis_client_publish_cb (redisAsyncContext *context,
                         gpointer           r,
                         gpointer           user_data)
{
   GSimpleAsyncResult *simple = user_data;

   g_assert(context);
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   /*
    * TODO: Is there anything to check for failure/error?
    */

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
}

void
redis_client_publish_async (RedisClient         *client,
                            const gchar         *channel,
                            const gchar         *message,
                            gssize               length,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
   RedisClientPrivate *priv;
   GSimpleAsyncResult *simple;

   g_return_if_fail(REDIS_IS_CLIENT(client));
   g_return_if_fail(channel);
   g_return_if_fail(message);
   g_return_if_fail(length >= -1);

   priv = client->priv;

   if (length < 0) {
      length = strlen(message);
   }

   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      redis_client_publish_async);

   redisAsyncCommand(priv->context,
                     redis_client_publish_cb,
                     simple,
                     "PUBLISH %s %b", channel, message, length);
}

gboolean
redis_client_publish_finish (RedisClient   *client,
                             GAsyncResult  *result,
                             GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(REDIS_IS_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret;
}

static void
#if (HIREDIS_MAJOR == 0) && (HIREDIS_MINOR <= 10)
redis_client_connect_cb (const redisAsyncContext *ac)
#else
redis_client_connect_cb (const redisAsyncContext *ac,
                         int                      status)
#endif
{
   RedisClientPrivate *priv;
   RedisSource *source;
   gboolean success;

   g_return_if_fail(ac);
   g_return_if_fail(ac->ev.data);

   source = ac->ev.data;
   priv = REDIS_CLIENT(source->user_data)->priv;

   if (priv->async_connect) {
#if (HIREDIS_MAJOR == 0) && (HIREDIS_MINOR <= 10)
      success = (ac->err == REDIS_OK);
#else
      success = (status == REDIS_OK);
#endif
      g_simple_async_result_set_op_res_gboolean(priv->async_connect, success);
      if (!success) {
         g_simple_async_result_set_error(priv->async_connect,
                                         REDIS_CLIENT_ERROR,
                                         REDIS_CLIENT_ERROR_HIREDIS,
                                         "%s", ac->errstr);
      }
      g_simple_async_result_complete_in_idle(priv->async_connect);
      g_clear_object(&priv->async_connect);
   }
}

static void
redis_client_sub_free (gpointer data)
{
   RedisClientSub *sub = data;

   if (sub) {
      g_free(sub->channel);
      g_free(sub);
   }
}

static void
redis_client_subscribe_cb (redisAsyncContext *context,
                           gpointer           reply,
                           gpointer           user_data)
{
   RedisClientPrivate *priv;
   RedisClientSub *sub;
   RedisClient *client = user_data;
   const gchar *channel;
   redisReply *r = reply;
   GVariant *v;
   GSList *list;

   g_assert(context);
   g_assert(REDIS_IS_CLIENT(client));

   priv = client->priv;

   if (r &&
       (r->type == REDIS_REPLY_ARRAY) &&
       (r->elements == 3) &&
       (r->element[0]->type == REDIS_REPLY_STRING) &&
       !g_strcmp0(r->element[0]->str, "message") &&
       (r->element[1]->type == REDIS_REPLY_STRING) &&
       (channel = r->element[1]->str) &&
       (v = redis_client_build_variant(r->element[2], NULL))) {
      g_variant_ref_sink(v);
      priv->dispatching = TRUE;
      list = g_hash_table_lookup(priv->subs_by_channel, channel);
      for (; list; list = list->next) {
         sub = list->data;
         sub->callback(client, v, sub->user_data);
      }
      priv->dispatching = FALSE;
      g_variant_unref(v);
   }
}

static void
redis_client_add_sub (RedisClient    *client,
                      RedisClientSub *sub)
{
   RedisClientPrivate *priv;
   GSList *list;

   g_return_if_fail(REDIS_IS_CLIENT(client));
   g_return_if_fail(sub);

   priv = client->priv;

   /*
    * Get the next subscription identifier.
    */
   sub->id = priv->next_sub++;

   /*
    * Add the subscription to the hashtable indexed by subscription id.
    */
   g_hash_table_insert(priv->sub_by_id, &sub->id, sub);

   /*
    * Add the subscription to the channels hashtable. Remotely subscribe
    * to the channel if necessary.
    */
   if (!(list = g_hash_table_lookup(priv->subs_by_channel, sub->channel))) {
      /*
       * Add this subscription to the hashtable.
       */
      list = g_slist_append(NULL, sub);
      g_hash_table_insert(priv->subs_by_channel,
                          g_strdup(sub->channel),
                          list);

      /*
       * If nothing is in the channels hashtable yet, then we need to
       * actually subscribe using the redis context.
       */
      redisAsyncCommand(priv->context,
                        redis_client_subscribe_cb,
                        client,
                        "SUBSCRIBE %s", sub->channel);
   } else {
      /*
       * Since there is already an item, the first value wont change.
       * Therefore we don't need to store it back to the hashtable.
       */
      list = g_slist_append(list, sub);
   }
}

static void
redis_client_remove_sub (RedisClient    *client,
                         RedisClientSub *sub)
{
   RedisClientPrivate *priv;
   GSList *list;

   g_return_if_fail(REDIS_IS_CLIENT(client));
   g_return_if_fail(sub);

   priv = client->priv;

   if ((list = g_hash_table_lookup(priv->subs_by_channel, sub->channel))) {
      list = g_slist_remove(list, sub);
      if (!list) {
         g_hash_table_remove(priv->subs_by_channel, sub->channel);
         redisAsyncCommand(priv->context, NULL, NULL,
                           "UNSUBSCRIBE %s", sub->channel);
      } else {
         g_hash_table_replace(priv->subs_by_channel,
                              g_strdup(sub->channel),
                              list);
      }
   }

   g_hash_table_remove(priv->sub_by_id, &sub->id);
}

guint
redis_client_subscribe (RedisClient         *client,
                        const gchar         *channel,
                        RedisPubsubCallback  callback,
                        gpointer             user_data,
                        GDestroyNotify       notify)
{
   RedisClientSub *sub;

   g_return_val_if_fail(REDIS_IS_CLIENT(client), 0);
   g_return_val_if_fail(channel, 0);
   g_return_val_if_fail(g_utf8_validate(channel, -1, NULL), 0);
   g_return_val_if_fail(callback, 0);

   sub = g_new0(RedisClientSub, 1);
   sub->channel = g_strdup(channel);
   sub->callback = callback;
   sub->user_data = user_data;
   sub->notify = notify;

   redis_client_add_sub(client, sub);

   return sub->id;
}

void
redis_client_unsubscribe (RedisClient *client,
                          guint        handler_id)
{
   RedisClientPrivate *priv;
   RedisClientSub *sub;

   g_return_if_fail(REDIS_IS_CLIENT(client));
   g_return_if_fail(handler_id > 0);

   priv = client->priv;

   if (priv->dispatching) {
      g_warning("Request to unsubscribe from subscription callback! "
                "Re-entrancy is not allowed. Ignoring request.");
      return;
   }

   if (!(sub = g_hash_table_lookup(priv->sub_by_id, &handler_id))) {
      g_warning("No subscription matching %u found.", handler_id);
      return;
   }

   redis_client_remove_sub(client, sub);
}

static void
redis_client_disconnect_cb (const redisAsyncContext *ac,
                            int                      status)
{
   g_printerr("%s\n", G_STRFUNC);
}

void
redis_client_connect_async (RedisClient         *client,
                            const gchar         *hostname,
                            guint16              port,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
   RedisClientPrivate *priv;
   GSimpleAsyncResult *simple;
   GSource *source;

   g_return_if_fail(REDIS_IS_CLIENT(client));
   g_return_if_fail(hostname);
   g_return_if_fail(callback);

   priv = client->priv;

   if (!port) {
      port = 6379;
   }

   /*
    * Make sure we haven't already connected.
    */
   if (priv->context) {
      simple =
         g_simple_async_result_new_error(G_OBJECT(client),
                                         callback,
                                         user_data,
                                         REDIS_CLIENT_ERROR,
                                         REDIS_CLIENT_ERROR_INVALID_STATE,
                                         _("%s() has already been called."),
                                         G_STRFUNC);
      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      return;
   }

   /*
    * Start connecting asynchronously.
    */
   priv->context = redisAsyncConnect(hostname, port);
   if (priv->context->err) {
      simple =
         g_simple_async_result_new_error(G_OBJECT(client),
                                         callback,
                                         user_data,
                                         REDIS_CLIENT_ERROR,
                                         REDIS_CLIENT_ERROR_HIREDIS,
                                         "%s", priv->context->errstr);
      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      redisAsyncFree(priv->context);
      priv->context = NULL;
      return;
   }

   /*
    * Save our GSimpleAsyncResult for later callback.
    */
   g_assert(!priv->async_connect);
   priv->async_connect =
      g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                redis_client_connect_async);

   /*
    * Create an event source so that we get our callbacks.
    */
   source = redis_source_new(priv->context, client);

   /*
    * This needs to come after the call redis_source_new().
    */
   redisAsyncSetConnectCallback(priv->context, redis_client_connect_cb);
   redisAsyncSetDisconnectCallback(priv->context, redis_client_disconnect_cb);

   /*
    * Start processing events using our GSource.
    */
   priv->context_source = g_source_attach(source, priv->main_context);

#if 0
   g_source_unref(source);
#endif
}

gboolean
redis_client_connect_finish (RedisClient   *client,
                             GAsyncResult  *result,
                             GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(REDIS_IS_CLIENT(client), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret;
}

static void
redis_client_finalize (GObject *object)
{
   RedisClientPrivate *priv;

   priv = REDIS_CLIENT(object)->priv;

   g_hash_table_unref(priv->sub_by_id);
   priv->sub_by_id = NULL;

   g_hash_table_unref(priv->subs_by_channel);
   priv->subs_by_channel = NULL;

   G_OBJECT_CLASS(redis_client_parent_class)->finalize(object);
}

static void
redis_client_class_init (RedisClientClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = redis_client_finalize;
   g_type_class_add_private(object_class, sizeof(RedisClientPrivate));
}

static void
redis_client_init (RedisClient *client)
{
   client->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(client,
                                  REDIS_TYPE_CLIENT,
                                  RedisClientPrivate);
   client->priv->sub_by_id =
      g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
                            redis_client_sub_free);
   client->priv->subs_by_channel =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                            NULL);
   client->priv->next_sub = 1;
}

GQuark
redis_client_error_quark (void)
{
   return g_quark_from_string("redis-client-error-quark");
}

GType
redis_client_error_get_type (void)
{
   static gsize initialized;
   static GType type_id;
   static const GEnumValue values[] = {
      { REDIS_CLIENT_ERROR_INVALID_STATE, "REDIS_CLIENT_ERROR_INVALID_STATE", "INVALID_STATE" },
      { REDIS_CLIENT_ERROR_HIREDIS, "REDIS_CLIENT_ERROR_HIREDIS", "HIREDIS" },
      { 0 }
   };

   if (g_once_init_enter(&initialized)) {
      type_id = g_enum_register_static(
         g_intern_static_string("RedisClientError"),
         values);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}
