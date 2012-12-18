/* postal-dm-cache.c
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

#include "postal-dm-cache.h"

struct _PostalDmCache
{
   gint           ref_count;
   guint          size;
   GHashFunc      hash_func;
   GEqualFunc     equal_func;
   GDestroyNotify free_func;
   gpointer       data[0];
};

/**
 * postal_dm_cache_contains:
 * @cache: A #PostalDmCache.
 *
 * Performs a lookup to determine if @key is currently contained in @cache.
 *
 * Returns: %TRUE if @key was found; otherwise %FALSE.
 */
gboolean
postal_dm_cache_contains (PostalDmCache *cache,
                          gconstpointer  key)
{
   gpointer prev;
   guint hash_code;

   g_return_val_if_fail(cache, FALSE);
   g_return_val_if_fail(key, FALSE);

   hash_code = cache->hash_func(key);
   if ((prev = cache->data[hash_code % cache->size])) {
      return cache->equal_func(key, prev);
   }

   return FALSE;
}

/**
 * postal_dm_cache_insert:
 * @cache: A #PostalDmCache.
 * @key: A pointer to the key to insert.
 *
 * Inserts a new key into @cache. If the key already exists, it will be
 * freed before inserting the new key and %TRUE will be returned. Otherwise,
 * the new key is inserted and %FALSE is returned.
 *
 * Returns: %TRUE if the key already existed in @cache.
 */
gboolean
postal_dm_cache_insert (PostalDmCache *cache,
                        gpointer       key)
{
   gpointer prev;
   gboolean ret = FALSE;
   guint hash_code;
   guint idx;

   g_return_val_if_fail(cache, FALSE);
   g_return_val_if_fail(key, FALSE);

   hash_code = cache->hash_func(key);
   idx = hash_code % cache->size;
   if ((prev = cache->data[idx])) {
      ret = cache->equal_func(prev, key);
      cache->free_func(prev);
   }
   cache->data[idx] = key;

   return ret;
}

/**
 * postal_dm_cache_new:
 * @size: The number of entries in the cache table.
 * @hash_func: A #GHashFunc to hash keys.
 * @equal_func: A #GEqualFunc to compare keys.
 * @free_func: A #GDestroyNotify to free expunged keys.
 *
 * Creates a new #PostalDmCache, a direct-mapped cache. This provides
 * semantics opposite of a bloom-filter. If #PostalDmCache tells you
 * an item was found in the cache, it definitely was. However, it may
 * tell you it was not when it actually has.
 *
 * You may check to see if an item is in the #PostalDmCache non-destructively
 * using postal_dm_cache_contains(). However, you can insert into the
 * #PostalDmCache and perform a contains in the same call using
 * postal_dm_cache_insert(). This is the typical use-case.
 *
 * Returns: (transfer full): A #PostalDmCache.
 */
PostalDmCache *
postal_dm_cache_new (guint          size,
                     GHashFunc      hash_func,
                     GEqualFunc     equal_func,
                     GDestroyNotify free_func)
{
   PostalDmCache *cache;

   g_return_val_if_fail(size, NULL);
   g_return_val_if_fail(size < (G_MAXUINT32 / sizeof(gpointer)), NULL);
   g_return_val_if_fail(hash_func, NULL);
   g_return_val_if_fail(equal_func, NULL);
   g_return_val_if_fail(free_func, NULL);

   cache = g_malloc0((sizeof *cache) + (sizeof(gpointer) * size));
   cache->ref_count = 1;
   cache->size = size;
   cache->hash_func = hash_func;
   cache->equal_func = equal_func;
   cache->free_func = free_func;

   return cache;
}

/**
 * postal_dm_cache_ref:
 * @cache: A #PostalDmCache.
 *
 * Increments the reference count of @cache by one.
 *
 * Returns: (transfer full): A #PostalDmCache.
 */
PostalDmCache *
postal_dm_cache_ref (PostalDmCache *cache)
{
   g_return_val_if_fail(cache, NULL);
   g_return_val_if_fail(cache->ref_count > 0, NULL);
   g_atomic_int_inc(&cache->ref_count);
   return cache;
}

/**
 * postal_dm_cache_unref:
 * @cache: A #PostalDmCache.
 *
 * Decrements the reference count of @cache by one. When the reference
 * count reaches zero, the structure will be freed.
 */
void
postal_dm_cache_unref (PostalDmCache *cache)
{
   guint i;

   g_return_if_fail(cache);
   g_return_if_fail(cache->ref_count > 0);

   if (g_atomic_int_dec_and_test(&cache->ref_count)) {
      for (i = 0; i < cache->size; i++) {
         cache->free_func(cache->data[i]);
      }
      g_free(cache);
   }
}

/**
 * postal_dm_cache_get_type:
 *
 * Fetches the #GType for #PostalDmCache to be used with the GObject
 * type system.
 *
 * Returns: The #GType for #PostalDmCache.
 */
GType
postal_dm_cache_get_type (void)
{
   static volatile GType type_id;

   if (g_once_init_enter(&type_id)) {
      GType registered;
      registered = g_boxed_type_register_static(
            "PostalDmCache",
            (GBoxedCopyFunc)postal_dm_cache_ref,
            (GBoxedFreeFunc)postal_dm_cache_unref);
      g_once_init_leave(&type_id, registered);
   }

   return type_id;
}
