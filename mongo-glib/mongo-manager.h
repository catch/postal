/* mongo-manager.h
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

#ifndef MONGO_MANAGER_H
#define MONGO_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MONGO_TYPE_MANAGER (mongo_manager_get_type())

typedef struct _MongoManager MongoManager;

void           mongo_manager_add_seed    (MongoManager *manager,
                                          const gchar  *seed);
void           mongo_manager_add_host    (MongoManager *manager,
                                          const gchar  *host);
void           mongo_manager_clear_hosts (MongoManager *manager);
void           mongo_manager_clear_seeds (MongoManager *manager);
gchar        **mongo_manager_get_hosts   (MongoManager *manager);
gchar        **mongo_manager_get_seeds   (MongoManager *manager);
GType          mongo_manager_get_type    (void) G_GNUC_CONST;
MongoManager  *mongo_manager_new         (void);
const gchar   *mongo_manager_next        (MongoManager *manager,
                                          guint        *delay);
MongoManager  *mongo_manager_ref         (MongoManager *manager);
void           mongo_manager_remove_host (MongoManager *manager,
                                          const gchar  *host);
void           mongo_manager_remove_seed (MongoManager *manager,
                                          const gchar  *seed);
void           mongo_manager_reset_delay (MongoManager *manager);
void           mongo_manager_unref       (MongoManager *manager);

G_END_DECLS

#endif /* MONGO_MANAGER_H */

