/* postal-redis.h
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

#ifndef POSTAL_REDIS_H
#define POSTAL_REDIS_H

#include <neo.h>

#include "postal-device.h"

G_BEGIN_DECLS

#define POSTAL_TYPE_REDIS            (postal_redis_get_type())
#define POSTAL_REDIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_REDIS, PostalRedis))
#define POSTAL_REDIS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_REDIS, PostalRedis const))
#define POSTAL_REDIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_REDIS, PostalRedisClass))
#define POSTAL_IS_REDIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_REDIS))
#define POSTAL_IS_REDIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_REDIS))
#define POSTAL_REDIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_REDIS, PostalRedisClass))

typedef struct _PostalRedis        PostalRedis;
typedef struct _PostalRedisClass   PostalRedisClass;
typedef struct _PostalRedisPrivate PostalRedisPrivate;

struct _PostalRedis
{
   NeoServiceBase parent;

   /*< private >*/
   PostalRedisPrivate *priv;
};

struct _PostalRedisClass
{
   NeoServiceBaseClass parent_class;
};

GType        postal_redis_get_type        (void) G_GNUC_CONST;
void         postal_redis_device_added    (PostalRedis  *redis,
                                           PostalDevice *device);
void         postal_redis_device_removed  (PostalRedis  *redis,
                                           PostalDevice *device);
void         postal_redis_device_updated  (PostalRedis  *redis,
                                           PostalDevice *device);
void         postal_redis_device_notified (PostalRedis  *redis,
                                           PostalDevice *device);
PostalRedis *postal_redis_new             (void);



G_END_DECLS

#endif /* POSTAL_REDIS_H */
