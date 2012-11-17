/* mongo-connection.h
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

#ifndef MONGO_CONNECTION_H
#define MONGO_CONNECTION_H

#include <glib-object.h>
#include <gio/gio.h>

#include "mongo-collection.h"
#include "mongo-database.h"
#include "mongo-protocol.h"

G_BEGIN_DECLS

#define MONGO_TYPE_CONNECTION            (mongo_connection_get_type())
#define MONGO_CONNECTION_ERROR           (mongo_connection_error_quark())
#define MONGO_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_CONNECTION, MongoConnection))
#define MONGO_CONNECTION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_CONNECTION, MongoConnection const))
#define MONGO_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_CONNECTION, MongoConnectionClass))
#define MONGO_IS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_CONNECTION))
#define MONGO_IS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_CONNECTION))
#define MONGO_CONNECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_CONNECTION, MongoConnectionClass))

typedef struct _MongoConnection        MongoConnection;
typedef struct _MongoConnectionClass   MongoConnectionClass;
typedef enum   _MongoConnectionError   MongoConnectionError;
typedef struct _MongoConnectionPrivate MongoConnectionPrivate;

enum _MongoConnectionError
{
   MONGO_CONNECTION_ERROR_NO_SEEDS = 1,
   MONGO_CONNECTION_ERROR_NOT_CONNECTED,
   MONGO_CONNECTION_ERROR_COMMAND_FAILED,
   MONGO_CONNECTION_ERROR_INVALID_REPLY,
   MONGO_CONNECTION_ERROR_NOT_MASTER,
   MONGO_CONNECTION_ERROR_CONNECT_FAILED,
};

struct _MongoConnection
{
   GObject parent;

   /*< private >*/
   MongoConnectionPrivate *priv;
};

/**
 * MongoConnectionClass:
 * @parent_class: The parent GObject class.
 *
 */
struct _MongoConnectionClass
{
   GObjectClass parent_class;
};

void               mongo_connection_command_async       (MongoConnection      *connection,
                                                         const gchar          *db,
                                                         const MongoBson      *command,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
MongoMessageReply *mongo_connection_command_finish      (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               mongo_connection_getmore_async       (MongoConnection      *connection,
                                                         const gchar          *db_and_collection,
                                                         guint32               limit,
                                                         guint64               cursor_id,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
MongoMessageReply *mongo_connection_getmore_finish      (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               mongo_connection_insert_async        (MongoConnection      *connection,
                                                         const gchar          *db_and_collection,
                                                         MongoInsertFlags      flags,
                                                         MongoBson           **documents,
                                                         gsize                 n_documents,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean           mongo_connection_insert_finish       (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               mongo_connection_delete_async        (MongoConnection      *connection,
                                                         const gchar          *db_and_collection,
                                                         MongoDeleteFlags      flags,
                                                         const MongoBson      *selector,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean           mongo_connection_delete_finish       (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               mongo_connection_update_async        (MongoConnection      *connection,
                                                         const gchar          *db_and_collection,
                                                         MongoUpdateFlags      flags,
                                                         const MongoBson      *selector,
                                                         const MongoBson      *update,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean           mongo_connection_update_finish       (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               mongo_connection_kill_cursors_async  (MongoConnection      *connection,
                                                         guint64              *cursors,
                                                         gsize                 n_cursors,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean           mongo_connection_kill_cursors_finish (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               mongo_connection_query_async         (MongoConnection      *connection,
                                                         const gchar          *db_and_collection,
                                                         MongoQueryFlags       flags,
                                                         guint32               skip,
                                                         guint32               limit,
                                                         const MongoBson      *query,
                                                         const MongoBson      *field_selector,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
MongoMessageReply *mongo_connection_query_finish        (MongoConnection      *connection,
                                                         GAsyncResult         *result,
                                                         GError              **error);
MongoDatabase     *mongo_connection_get_database        (MongoConnection      *connection,
                                                         const gchar          *name);
GType              mongo_connection_get_type            (void) G_GNUC_CONST;
GQuark             mongo_connection_error_quark         (void) G_GNUC_CONST;
MongoConnection   *mongo_connection_new                 (void);
MongoConnection   *mongo_connection_new_from_uri        (const gchar          *uri);
gboolean           mongo_connection_get_slave_okay      (MongoConnection      *connection);
void               mongo_connection_set_slave_okay      (MongoConnection      *connection,
                                                         gboolean              slave_okay);
MongoConnection   *mongo_database_get_connection        (MongoDatabase        *database);
MongoConnection   *mongo_collection_get_connection      (MongoCollection      *collection);

G_END_DECLS

#endif /* MONGO_CONNECTION_H */
