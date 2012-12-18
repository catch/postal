/* postal-dm-cache.h
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

#ifndef POSTAL_DM_CACHE_H
#define POSTAL_DM_CACHE_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _PostalDmCache PostalDmCache;

gboolean       postal_dm_cache_contains (PostalDmCache  *cache,
                                         gconstpointer   key);
GType          postal_dm_cache_get_type (void) G_GNUC_CONST;
gboolean       postal_dm_cache_insert   (PostalDmCache  *cache,
                                         gpointer        key);
PostalDmCache *postal_dm_cache_new      (guint           size,
                                         GHashFunc       hash_func,
                                         GEqualFunc      equal_func,
                                         GDestroyNotify  free_func);
PostalDmCache *postal_dm_cache_ref      (PostalDmCache  *cache);
void           postal_dm_cache_unref    (PostalDmCache  *cache);

G_END_DECLS

#endif /* POSTAL_DM_CACHE_H */
