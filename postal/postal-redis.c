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

#include <redis-glib/redis-glib.h>

#include "postal-debug.h"
#include "postal-device.h"
#include "postal-redis.h"

static gchar       *gRedisChannel;
static RedisClient *gRedisClient;
static gchar       *gRedisHost;
static guint        gRedisPort;

static void
postal_redis_connect_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
   RedisClient *client = (RedisClient *)object;
   GError *error = NULL;

   ENTRY;

   if (!redis_client_connect_finish(client, result, &error)) {
      g_warning("Failed to connect to Redis: %s", error->message);
      g_error_free(error);
      /*
       * TODO: Try again with exponential timeout or patch redis-glib
       *       to handle connecting on its own.
       */
      EXIT;
   }

   g_message("Connected to redis at %s:%u", gRedisHost, gRedisPort);

   EXIT;
}

void
postal_redis_init (GKeyFile *key_file)
{
   gchar *host;
   gchar *channel;
   guint port;

   ENTRY;

   if (key_file) {
      if (g_key_file_get_boolean(key_file, "redis", "enabled", NULL)) {
         port = g_key_file_get_integer(key_file, "redis", "port", NULL);
         host = g_key_file_get_string(key_file, "redis", "host", NULL);
         channel = g_key_file_get_string(key_file, "redis", "channel", NULL);

         gRedisChannel = channel ?: g_strdup("events");
         gRedisHost = host ?: g_strdup("localhost");
         gRedisPort = port ?: 6379;

         gRedisClient = redis_client_new();
         redis_client_connect_async(gRedisClient,
                                    host,
                                    port,
                                    postal_redis_connect_cb,
                                    NULL);
      }
   }

   EXIT;
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

void
postal_redis_device_added (PostalDevice *device)
{
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_DEVICE(device));

   if (gRedisClient) {
      message = postal_redis_build_message(device, "device-added");
      redis_client_publish_async(gRedisClient,
                                 gRedisChannel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

void
postal_redis_device_removed (PostalDevice *device)
{
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_DEVICE(device));

   if (gRedisClient) {
      message = postal_redis_build_message(device, "device-removed");
      redis_client_publish_async(gRedisClient,
                                 gRedisChannel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

void
postal_redis_device_updated (PostalDevice *device)
{
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_DEVICE(device));

   if (gRedisClient) {
      message = postal_redis_build_message(device, "device-updated");
      redis_client_publish_async(gRedisClient,
                                 gRedisChannel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}

void
postal_redis_device_notified (PostalDevice *device)
{
   gchar *message = NULL;

   ENTRY;

   g_return_if_fail(POSTAL_IS_DEVICE(device));

   if (gRedisClient) {
      message = postal_redis_build_message(device, "device-notified");
      redis_client_publish_async(gRedisClient,
                                 gRedisChannel,
                                 message,
                                 -1,
                                 postal_redis_publish_cb,
                                 NULL);
      g_free(message);
   }

   EXIT;
}
