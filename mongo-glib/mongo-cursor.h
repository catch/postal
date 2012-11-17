/* mongo-cursor.h
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

#ifndef MONGO_CURSOR_H
#define MONGO_CURSOR_H

#include <glib-object.h>
#include <gio/gio.h>

#include "mongo-bson.h"
#include "mongo-protocol.h"

G_BEGIN_DECLS

#define MONGO_TYPE_CURSOR            (mongo_cursor_get_type())
#define MONGO_CURSOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_CURSOR, MongoCursor))
#define MONGO_CURSOR_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_CURSOR, MongoCursor const))
#define MONGO_CURSOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_CURSOR, MongoCursorClass))
#define MONGO_IS_CURSOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_CURSOR))
#define MONGO_IS_CURSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_CURSOR))
#define MONGO_CURSOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_CURSOR, MongoCursorClass))

typedef struct _MongoCursor        MongoCursor;
typedef struct _MongoCursorClass   MongoCursorClass;
typedef struct _MongoCursorPrivate MongoCursorPrivate;

/**
 * MongoCursorCallback:
 * @cursor: (in): A #MongoCursor.
 * @bson: (in): A #MongoBson.
 * @user_data: (in): User data provided to mongo_cursor_foreach_async().
 *
 * This function prototype is used by callbacks to
 * mongo_cursor_foreach_async(). It allows you to iterate through all
 * of the documents as they are received from the remote Mongo server.
 * There may be delay between successive calls to this function while
 * data is delivered from the Mongo server.
 *
 * Returns: %TRUE to continue processing, %FALSE to stop.
 */
typedef gboolean (*MongoCursorCallback) (MongoCursor *cursor,
                                         MongoBson   *bson,
                                         gpointer     user_data);

struct _MongoCursor
{
   GObject parent;

   /*< private >*/
   MongoCursorPrivate *priv;
};

/**
 * MongoCursorClass:
 * @parent_class: The parent #GObjectClass.
 *
 */
struct _MongoCursorClass
{
   GObjectClass parent_class;
};

const gchar     *mongo_cursor_get_collection (MongoCursor          *cursor);
guint            mongo_cursor_get_skip       (MongoCursor          *cursor);
guint            mongo_cursor_get_limit      (MongoCursor          *cursor);
MongoBson       *mongo_cursor_get_fields     (MongoCursor          *cursor);
MongoQueryFlags  mongo_cursor_get_flags      (MongoCursor          *cursor);
MongoBson       *mongo_cursor_get_query      (MongoCursor          *cursor);
void             mongo_cursor_close_async    (MongoCursor          *cursor,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
gboolean         mongo_cursor_close_finish   (MongoCursor          *cursor,
                                              GAsyncResult         *result,
                                              GError              **error);
void             mongo_cursor_count_async    (MongoCursor          *cursor,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
gboolean         mongo_cursor_count_finish   (MongoCursor          *cursor,
                                              GAsyncResult         *result,
                                              guint64              *count,
                                              GError              **error);
void             mongo_cursor_foreach_async  (MongoCursor          *cursor,
                                              MongoCursorCallback   foreach_func,
                                              gpointer              foreach_data,
                                              GDestroyNotify        foreach_notify,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
gboolean         mongo_cursor_foreach_finish (MongoCursor          *cursor,
                                              GAsyncResult         *result,
                                              GError              **error);
GType            mongo_cursor_get_type       (void) G_GNUC_CONST;
guint            mongo_cursor_get_batch_size (MongoCursor          *cursor);
void             mongo_cursor_set_batch_size (MongoCursor          *cursor,
                                              guint                 batch_size);

G_END_DECLS

#endif /* MONGO_CURSOR_H */
