/* mongo-output-stream.h
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

#ifndef MONGO_OUTPUT_STREAM_H
#define MONGO_OUTPUT_STREAM_H

#include <gio/gio.h>

#include "mongo-message.h"
#include "mongo-write-concern.h"

G_BEGIN_DECLS

#define MONGO_TYPE_OUTPUT_STREAM            (mongo_output_stream_get_type())
#define MONGO_OUTPUT_STREAM_ERROR           (mongo_output_stream_error_quark())
#define MONGO_OUTPUT_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_OUTPUT_STREAM, MongoOutputStream))
#define MONGO_OUTPUT_STREAM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_OUTPUT_STREAM, MongoOutputStream const))
#define MONGO_OUTPUT_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_OUTPUT_STREAM, MongoOutputStreamClass))
#define MONGO_IS_OUTPUT_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_OUTPUT_STREAM))
#define MONGO_IS_OUTPUT_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_OUTPUT_STREAM))
#define MONGO_OUTPUT_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_OUTPUT_STREAM, MongoOutputStreamClass))

typedef struct _MongoOutputStream        MongoOutputStream;
typedef struct _MongoOutputStreamClass   MongoOutputStreamClass;
typedef struct _MongoOutputStreamPrivate MongoOutputStreamPrivate;
typedef enum   _MongoOutputStreamError   MongoOutputStreamError;

enum _MongoOutputStreamError
{
   MONGO_OUTPUT_STREAM_ERROR_SHORT_WRITE = 1,
};

struct _MongoOutputStream
{
   GFilterOutputStream parent;

   /*< private >*/
   MongoOutputStreamPrivate *priv;
};

struct _MongoOutputStreamClass
{
   GFilterOutputStreamClass parent_class;
};

GQuark             mongo_output_stream_error_quark          (void) G_GNUC_CONST;
GType              mongo_output_stream_get_type             (void) G_GNUC_CONST;
MongoOutputStream *mongo_output_stream_new                  (GOutputStream        *base_stream);
gboolean           mongo_output_stream_write_message        (MongoOutputStream    *stream,
                                                             MongoMessage         *message,
                                                             MongoWriteConcern    *concern,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
gint32             mongo_output_stream_write_message_async  (MongoOutputStream    *stream,
                                                             MongoMessage         *message,
                                                             MongoWriteConcern    *concern,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              user_data);
gboolean           mongo_output_stream_write_message_finish (MongoOutputStream    *stream,
                                                             GAsyncResult         *result,
                                                             GError              **error);

G_END_DECLS

#endif /* MONGO_OUTPUT_STREAM_H */
