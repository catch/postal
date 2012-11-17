/* mongo-bson-stream.h
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

#ifndef MONGO_BSON_STREAM_H
#define MONGO_BSON_STREAM_H

#include <gio/gio.h>

#include "mongo-bson.h"

G_BEGIN_DECLS

#define MONGO_TYPE_BSON_STREAM            (mongo_bson_stream_get_type())
#define MONGO_BSON_STREAM_ERROR           (mongo_bson_stream_error_quark())
#define MONGO_BSON_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_BSON_STREAM, MongoBsonStream))
#define MONGO_BSON_STREAM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_BSON_STREAM, MongoBsonStream const))
#define MONGO_BSON_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_BSON_STREAM, MongoBsonStreamClass))
#define MONGO_IS_BSON_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_BSON_STREAM))
#define MONGO_IS_BSON_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_BSON_STREAM))
#define MONGO_BSON_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_BSON_STREAM, MongoBsonStreamClass))

typedef struct _MongoBsonStream        MongoBsonStream;
typedef struct _MongoBsonStreamClass   MongoBsonStreamClass;
typedef struct _MongoBsonStreamPrivate MongoBsonStreamPrivate;
typedef enum   _MongoBsonStreamError   MongoBsonStreamError;

enum _MongoBsonStreamError
{
   MONGO_BSON_STREAM_ERROR_ALREADY_LOADED = 1,
};

struct _MongoBsonStream
{
   GObject parent;

   /*< private >*/
   MongoBsonStreamPrivate *priv;
};

/**
 * MongoBsonStreamClass:
 * @parent_class: GObject parent class.
 */
struct _MongoBsonStreamClass
{
   GObjectClass parent_class;
};

MongoBsonStream *mongo_bson_stream_new               (void);
GQuark           mongo_bson_stream_error_quark       (void) G_GNUC_CONST;
GType            mongo_bson_stream_get_type          (void) G_GNUC_CONST;
gboolean         mongo_bson_stream_load_from_file    (MongoBsonStream  *stream,
                                                      GFile            *file,
                                                      GCancellable     *cancellable,
                                                      GError          **error);
gboolean         mongo_bson_stream_load_from_channel (MongoBsonStream  *stream,
                                                      GIOChannel       *channel,
                                                      GError          **error);
MongoBson       *mongo_bson_stream_next              (MongoBsonStream  *stream);

G_END_DECLS

#endif /* MONGO_BSON_STREAM_H */
