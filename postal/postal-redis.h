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

#include <glib.h>

#include "postal-device.h"

G_BEGIN_DECLS

void postal_redis_init            (GKeyFile     *key_file);
void postal_redis_device_added    (PostalDevice *device);
void postal_redis_device_removed  (PostalDevice *device);
void postal_redis_device_updated  (PostalDevice *device);
void postal_redis_device_notified (PostalDevice *device);

G_END_DECLS

#endif /* POSTAL_REDIS_H */
