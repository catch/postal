/* redis-client.h
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

#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define REDIS_TYPE_CLIENT            (redis_client_get_type())
#define REDIS_TYPE_CLIENT_ERROR      (redis_client_error_get_type())
#define REDIS_CLIENT_ERROR           (redis_client_error_quark())
#define REDIS_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REDIS_TYPE_CLIENT, RedisClient))
#define REDIS_CLIENT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), REDIS_TYPE_CLIENT, RedisClient const))
#define REDIS_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  REDIS_TYPE_CLIENT, RedisClientClass))
#define REDIS_IS_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REDIS_TYPE_CLIENT))
#define REDIS_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  REDIS_TYPE_CLIENT))
#define REDIS_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  REDIS_TYPE_CLIENT, RedisClientClass))

typedef struct _RedisClient        RedisClient;
typedef struct _RedisClientClass   RedisClientClass;
typedef struct _RedisClientPrivate RedisClientPrivate;
typedef enum   _RedisClientError   RedisClientError;

typedef void (*RedisPubsubCallback) (RedisClient *client,
                                     GVariant    *variant,
                                     gpointer     user_data);

enum _RedisClientError
{
   REDIS_CLIENT_ERROR_INVALID_STATE = 1,
   REDIS_CLIENT_ERROR_HIREDIS,
};

struct _RedisClient
{
   GObject parent;

   /*< private >*/
   RedisClientPrivate *priv;
};

struct _RedisClientClass
{
   GObjectClass parent_class;
};

void         redis_client_command_async  (RedisClient          *client,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data,
                                          const gchar          *format,
                                          ...);
GVariant    *redis_client_command_finish (RedisClient          *client,
                                          GAsyncResult         *result,
                                          GError              **error);
void         redis_client_connect_async  (RedisClient          *client,
                                          const gchar          *hostname,
                                          guint16               port,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean     redis_client_connect_finish (RedisClient          *client,
                                          GAsyncResult         *result,
                                          GError              **error);
void         redis_client_publish_async  (RedisClient          *client,
                                          const gchar          *channel,
                                          const gchar          *message,
                                          gssize                length,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean     redis_client_publish_finish (RedisClient          *client,
                                          GAsyncResult         *result,
                                          GError              **error);
guint        redis_client_subscribe      (RedisClient          *client,
                                          const gchar          *channel,
                                          RedisPubsubCallback   callback,
                                          gpointer              data,
                                          GDestroyNotify        notify);
void         redis_client_unsubscribe    (RedisClient          *client,
                                          guint                 handler_id);
GQuark       redis_client_error_quark    (void) G_GNUC_CONST;
GType        redis_client_error_get_type (void) G_GNUC_CONST;
GType        redis_client_get_type       (void) G_GNUC_CONST;
RedisClient *redis_client_new            (void);

G_END_DECLS

#endif /* REDIS_CLIENT_H */
