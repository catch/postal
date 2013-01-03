/* mongo-database.h
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

#if !defined (MONGO_INSIDE) && !defined (MONGO_COMPILATION)
#error "Only <mongo-glib/mongo-glib.h> can be included directly."
#endif

#ifndef MONGO_DATABASE_H
#define MONGO_DATABASE_H

#include <glib-object.h>
#include <gio/gio.h>

#include "mongo-collection.h"

G_BEGIN_DECLS

#define MONGO_TYPE_DATABASE            (mongo_database_get_type())
#define MONGO_DATABASE_ERROR           (mongo_database_error_quark())
#define MONGO_DATABASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_DATABASE, MongoDatabase))
#define MONGO_DATABASE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_DATABASE, MongoDatabase const))
#define MONGO_DATABASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_DATABASE, MongoDatabaseClass))
#define MONGO_IS_DATABASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_DATABASE))
#define MONGO_IS_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_DATABASE))
#define MONGO_DATABASE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_DATABASE, MongoDatabaseClass))

typedef struct _MongoDatabase        MongoDatabase;
typedef struct _MongoDatabaseClass   MongoDatabaseClass;
typedef enum   _MongoDatabaseError   MongoDatabaseError;
typedef struct _MongoDatabasePrivate MongoDatabasePrivate;

enum _MongoDatabaseError
{
   MONGO_DATABASE_ERROR_NO_CONNECTION = 1,
};

struct _MongoDatabase
{
   GObject parent;

   /*< private >*/
   MongoDatabasePrivate *priv;
};

/**
 * MongoDatabaseClass:
 * @parent_class: The parent #GObjectClass.
 *
 */
struct _MongoDatabaseClass
{
   GObjectClass parent_class;
};

GQuark           mongo_database_error_quark    (void) G_GNUC_CONST;
MongoCollection *mongo_database_get_collection (MongoDatabase        *database,
                                                const gchar          *name);
const gchar     *mongo_database_get_name       (MongoDatabase        *database);
GType            mongo_database_get_type       (void) G_GNUC_CONST;
void             mongo_database_drop_async     (MongoDatabase        *database,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean         mongo_database_drop_finish    (MongoDatabase        *database,
                                                GAsyncResult         *result,
                                                GError              **error);

MongoDatabase   *mongo_collection_get_database (MongoCollection      *collection);

G_END_DECLS

#endif /* MONGO_DATABASE_H */
