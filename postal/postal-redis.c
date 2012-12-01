/* postal-redis.c
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

#include <redis-glib.h>

#include "postal-debug.h"
#include "postal-device.h"
#include "postal-redis.h"

G_DEFINE_TYPE(PostalRedis, postal_redis, NEO_TYPE_SERVICE_BASE)

struct _PostalRedisPrivate
{
   gchar       *channel;
   RedisClient *redis;
   gchar       *host;
   guint        port;
};

PostalRedis *
postal_redis_new (void)
{
   return g_object_new(POSTAL_TYPE_REDIS,
                       "name", "redis",
                       NULL);
}

static void
postal_redis_set_channel (PostalRedis *redis,
                          const gchar *channel)
{
   g_free(redis->priv->channel);
   redis->priv->channel = g_strdup(channel);
}

static void
postal_redis_set_host (PostalRedis *redis,
                       const gchar *host)
{
   g_free(redis->priv->host);
   redis->priv->host = g_strdup(host);
}

static void
postal_redis_set_port (PostalRedis *redis,
                        guint        port)
{
   redis->priv->port = port;
}

static void
postal_redis_connect_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
   PostalRedisPrivate *priv;
   RedisClient *client = (RedisClient *)object;
   PostalRedis *redis = user_data;
   GError *error = NULL;

   ENTRY;

   g_assert(POSTAL_IS_REDIS(redis));

   priv = redis->priv;

   if (!redis_client_connect_finish(client, result, &error)) {
      g_warning("Failed to connect to Redis: %s", error->message);
      g_error_free(error);
      /*
       * TODO: Try again with exponential timeout or patch redis-glib
       *       to handle connecting on its own.
       */
      EXIT;
   }

   g_message("Connected to redis at %s:%u", priv->host, priv->port);

   EXIT;
}

static void
postal_redis_start (NeoServiceBase *service,
                    GKeyFile       *config)
{
   PostalRedisPrivate *priv;
   PostalRedis *redis = (PostalRedis *)service;
   gchar *str;
   guint port;

   ENTRY;

   g_assert(POSTAL_IS_REDIS(redis));

   priv = redis->priv;

   if (config) {
      if (g_key_file_get_boolean(config, "redis", "enabled", NULL)) {
         str = g_key_file_get_string(config, "redis", "host", NULL);
         postal_redis_set_host(redis, str);
         g_free(str);

         str = g_key_file_get_string(config, "redis", "channel", NULL);
         str = str ?: g_strdup("localhost");
         postal_redis_set_channel(redis, str);
         g_free(str);

         port = g_key_file_get_integer(config, "redis", "port", NULL);
         postal_redis_set_port(redis, port ?: 6379);

         g_clear_object(&priv->redis);

         if (priv->host && priv->port) {
            priv->redis = redis_client_new();
            redis_client_connect_async(priv->redis,
                                       priv->host,
                                       priv->port,
                                       postal_redis_connect_cb,
                                       NULL);
         }
      }
   }

   EXIT;
}

static void
postal_redis_stop (NeoServiceBase *service)
{
   ENTRY;

   EXIT;
}

static gchar *
postal_redis_build_message (PostalDevice *device,
                            const gchar  *action)
{
   const gchar *device_type;
   const gchar *device_token;
   const gchar *user;
   gchar *ret;

   g_assert(POSTAL_IS_DEVICE(device));
   g_assert(action);

   device_type = postal_device_get_device_type(device);
   device_token = postal_device_get_device_token(device);
   user = postal_device_get_user(device);

   ret = g_strdup_printf("{\n"
                         "  \"Action\": \"%s\",\n"
                         "  \"DeviceType\": \"%s\",\n"
                         "  \"DeviceToken\": \"%s\",\n"
                         "  \"User\": \"%s\"\n"
                         "}",
                         action,
                         device_type,
                         device_token,
                         user);

   return ret;
}

static void
postal_redis_publish_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
   RedisClient *client = (RedisClient *)object;
   GError *error = NULL;

   ENTRY;

   g_assert(REDIS_IS_CLIENT(client));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(!user_data);

   if (!redis_client_publish_finish(client, result, &error)) {
      g_warning("%s", error->message);
   }

   EXIT;
}

void
postal_redis_device_added (PostalRedis  *redis,
                           PostalDevice *device)
{
   PostalRedisPrivate *priv;
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_REDIS(redis));
   g_return_if_fail(POSTAL_IS_DEVICE(device));

   priv = redis->priv;

   if (priv->redis) {
      message = postal_redis_build_message(device, "device-added");
      redis_client_publish_async(priv->redis,
                                 priv->channel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

void
postal_redis_device_removed (PostalRedis  *redis,
                             PostalDevice *device)
{
   PostalRedisPrivate *priv;
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_REDIS(redis));
   g_return_if_fail(POSTAL_IS_DEVICE(device));

   priv = redis->priv;

   if (priv->redis) {
      message = postal_redis_build_message(device, "device-removed");
      redis_client_publish_async(priv->redis,
                                 priv->channel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

void
postal_redis_device_updated (PostalRedis  *redis,
                             PostalDevice *device)
{
   PostalRedisPrivate *priv;
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_REDIS(redis));
   g_return_if_fail(POSTAL_IS_DEVICE(device));

   priv = redis->priv;

   if (priv->redis) {
      message = postal_redis_build_message(device, "device-updated");
      redis_client_publish_async(priv->redis,
                                 priv->channel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

void
postal_redis_device_notified (PostalRedis  *redis,
                              PostalDevice *device)
{
   PostalRedisPrivate *priv;
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_REDIS(redis));
   g_return_if_fail(POSTAL_IS_DEVICE(device));

   priv = redis->priv;

   if (priv->redis) {
      message = postal_redis_build_message(device, "device-notified");
      redis_client_publish_async(priv->redis,
                                 priv->channel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

static void
postal_redis_finalize (GObject *object)
{
   PostalRedisPrivate *priv;

   priv = POSTAL_REDIS(object)->priv;

   g_clear_object(&priv->redis);
   g_free(priv->channel);
   g_free(priv->host);

   G_OBJECT_CLASS(postal_redis_parent_class)->finalize(object);
}

static void
postal_redis_class_init (PostalRedisClass *klass)
{
   GObjectClass *object_class;
   NeoServiceBaseClass *service_base_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_redis_finalize;
   g_type_class_add_private(object_class, sizeof(PostalRedisPrivate));

   service_base_class = NEO_SERVICE_BASE_CLASS(klass);
   service_base_class->start = postal_redis_start;
   service_base_class->stop = postal_redis_stop;

   EXIT;
}

static void
postal_redis_init (PostalRedis *redis)
{
   ENTRY;

   redis->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(redis,
                                  POSTAL_TYPE_REDIS,
                                  PostalRedisPrivate);
   g_object_set(redis, "name", "redis", NULL);

   EXIT;
}
