/* mongo-manager.c
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

#include "mongo-manager.h"

/**
 * SECTION:mongo-manager
 * @title: MongoManager
 * @short_description: Replica set node management.
 *
 * #MongoManager encapsulates the logic required to know who to connect to
 * upon failure of a replica set. It tracks seeded replica servers as well
 * as servers that were discovered up connecting to a Mongo server.
 */

#define MAX_DELAY (1000 * 60)

struct _MongoManager
{
   volatile gint ref_count;
   GPtrArray *seeds;
   GPtrArray *hosts;
   guint offset;
   guint delay;
};

/**
 * mongo_manager_new:
 *
 * Creates a new instance of #MongoManager.
 *
 * Returns: (transfer full): A newly created #MongoManager.
 */
MongoManager *
mongo_manager_new (void)
{
   MongoManager *mgr;

   mgr = g_slice_new0(MongoManager);
   mgr->ref_count = 1;
   mgr->hosts = g_ptr_array_new_with_free_func(g_free);
   mgr->seeds = g_ptr_array_new_with_free_func(g_free);

   return mgr;
}

/**
 * mongo_manager_add_host:
 * @manager: (in): A #MongoManager.
 * @host: (in): A "host:port" string.
 *
 * Adds a host to @manager. This is a node that was discovered by performing
 * an "ismaster" command to the mongod instance.
 */
void
mongo_manager_add_host (MongoManager *manager,
                        const gchar  *host)
{
   guint i;

   g_return_if_fail(manager);
   g_return_if_fail(host);

   for (i = 0; i < manager->hosts->len; i++) {
      if (!g_strcmp0(host, manager->hosts->pdata[i])) {
         return;
      }
   }

   g_ptr_array_add(manager->hosts, g_strdup(host));
}

/**
 * mongo_manager_add_seed:
 * @manager: (in): A #MongoManager.
 * @seed: (in): A "host:port" string.
 *
 * Adds a seed to @manager. This is a node that was provided in the
 * mongodb:// style connection string.
 */
void
mongo_manager_add_seed (MongoManager *manager,
                        const gchar  *seed)
{
   guint i;

   g_return_if_fail(manager);
   g_return_if_fail(seed);

   for (i = 0; i < manager->seeds->len; i++) {
      if (!g_strcmp0(seed, manager->seeds->pdata[i])) {
         return;
      }
   }

   g_ptr_array_add(manager->seeds, g_strdup(seed));
}

/**
 * mongo_manager_clear_hosts:
 * @manager: (in): A #MongoManager.
 *
 * Clear all hosts that were "discovered" in the replica set.
 */
void
mongo_manager_clear_hosts (MongoManager *manager)
{
   g_return_if_fail(manager);

   if (manager->hosts->len) {
      g_ptr_array_remove_range(manager->hosts, 0, manager->hosts->len);
   }
}

/**
 * mongo_manager_clear_seeds:
 * @manager: (in): A #MongoManager.
 *
 * Clear all known seeds.
 */
void
mongo_manager_clear_seeds (MongoManager *manager)
{
   g_return_if_fail(manager);

   if (manager->seeds->len) {
      g_ptr_array_remove_range(manager->seeds, 0, manager->seeds->len);
   }
}

/**
 * mongo_manager_get_hosts:
 * @manager: (in): A #MongoManager.
 *
 * Retrieves the array of hosts.
 *
 * Returns: (transfer full): An array of hosts.
 */
gchar **
mongo_manager_get_hosts (MongoManager *manager)
{
   gchar **ret;
   guint i;

   g_return_val_if_fail(manager, NULL);

   ret = g_new0(gchar *, manager->hosts->len + 1);
   for (i = 0; i < manager->hosts->len; i++) {
      ret[i] = g_strdup(g_ptr_array_index(manager->hosts, i));
   }

   return ret;
}

/**
 * mongo_manager_get_seeds:
 * @manager: (in): A #MongoManager.
 *
 * Retrieves the array of seeds.
 *
 * Returns: (transfer full): An array of seeds.
 */
gchar **
mongo_manager_get_seeds (MongoManager *manager)
{
   gchar **ret;
   guint i;

   g_return_val_if_fail(manager, NULL);

   ret = g_new0(gchar *, manager->seeds->len + 1);
   for (i = 0; i < manager->seeds->len; i++) {
      ret[i] = g_strdup(g_ptr_array_index(manager->seeds, i));
   }

   return ret;
}

/**
 * mongo_manager_remove_host:
 * @manager: (in): A #MongoManager.
 * @host: (in): A "host:port" string.
 *
 * Remove a discovered host from the manager.
 */
void
mongo_manager_remove_host (MongoManager *manager,
                           const gchar  *host)
{
   guint i;

   g_return_if_fail(manager);
   g_return_if_fail(host);

   for (i = 0; i < manager->hosts->len; i++) {
      if (!g_strcmp0(manager->hosts->pdata[i], host)) {
         g_ptr_array_remove_index(manager->hosts, i);
         break;
      }
   }
}

/**
 * mongo_manager_remove_seed:
 * @manager: (in): A #MongoManager.
 * @seed: (in): A "host:port" string.
 *
 * Remove a seed from @manager.
 */
void
mongo_manager_remove_seed (MongoManager *manager,
                           const gchar  *seed)
{
   guint i;

   g_return_if_fail(manager);
   g_return_if_fail(seed);

   for (i = 0; i < manager->seeds->len; i++) {
      if (!g_strcmp0(manager->seeds->pdata[i], seed)) {
         g_ptr_array_remove_index(manager->seeds, i);
         break;
      }
   }
}

/**
 * mongo_manager_reset_delay:
 * @manager: (in): A #MongoManager.
 *
 * Resets the delay that will be provided when iterating nodes to connect to.
 * This should be called after successfully connecting to a PRIMARY node.
 */
void
mongo_manager_reset_delay (MongoManager *manager)
{
   g_return_if_fail(manager);
   manager->delay = 0;
}

/**
 * mongo_manager_next:
 * @manager: (in): A #MongoManager.
 * @delay: (out): A location for the requested delay.
 *
 * Retrieves the next host that should be connected to. If another host
 * does not exist, %NULL is returned and @delay is set. The caller should
 * delay that many milliseconds before calling mongo_manager_next() again.
 *
 * Returns: A "host:port" to connect to, or %NULL and @delay is set.
 */
const gchar *
mongo_manager_next (MongoManager *manager,
                    guint        *delay)
{
   gchar *next;
   guint offset;

   g_return_val_if_fail(manager, NULL);
   g_return_val_if_fail(delay, NULL);

   *delay = 0;

   offset = manager->offset;

   if (offset < manager->seeds->len) {
      next = g_ptr_array_index(manager->seeds, offset);
      manager->offset++;
      return next;
   }

   offset -= manager->seeds->len;

   if (offset < manager->hosts->len) {
      next = g_ptr_array_index(manager->hosts, offset);
      manager->offset++;
      return next;
   }

   manager->offset = 0;

   if (!manager->delay) {
      manager->delay = g_random_int_range(200, 1000);
   } else {
      manager->delay = CLAMP(manager->delay << 1, 1, MAX_DELAY);
   }

   *delay = manager->delay;

   return NULL;
}

static void
mongo_manager_dispose (MongoManager *manager)
{
   g_assert(manager);
   g_ptr_array_unref(manager->hosts);
   g_ptr_array_unref(manager->seeds);
}

/**
 * mongo_manager_ref:
 * @manager: (in): A #MongoManager.
 *
 * Increment the reference count of @manager by one.
 *
 * Returns: @manager.
 */
MongoManager *
mongo_manager_ref (MongoManager *manager)
{
   g_return_val_if_fail(manager, NULL);
   g_return_val_if_fail(manager->ref_count > 0, NULL);
   g_atomic_int_inc(&manager->ref_count);
   return manager;
}

/**
 * mongo_manager_unref:
 * @manager: (in): A #MongoManager.
 *
 * Decrement the reference count of @manager by one. Upon reaching zero, the
 * structure will be freed and resources released.
 */
void
mongo_manager_unref (MongoManager *manager)
{
   g_return_if_fail(manager);
   g_return_if_fail(manager->ref_count > 0);
   if (g_atomic_int_dec_and_test(&manager->ref_count)) {
      mongo_manager_dispose(manager);
      g_slice_free(MongoManager, manager);
   }
}

/**
 * mongo_manager_get_type:
 *
 * Fetches the boxed #GType for #MongoManager.
 *
 * Returns: A #GType.
 */
GType
mongo_manager_get_type (void)
{
   static gsize initialized;
   static GType type_id;

   if (g_once_init_enter(&initialized)) {
      type_id = g_boxed_type_register_static(
            "MongoManager",
            (GBoxedCopyFunc)mongo_manager_ref,
            (GBoxedFreeFunc)mongo_manager_unref);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}
