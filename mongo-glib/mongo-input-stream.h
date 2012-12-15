/* mongo-input-stream.h
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

#ifndef MONGO_INPUT_STREAM_H
#define MONGO_INPUT_STREAM_H

#include <gio/gio.h>

#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_INPUT_STREAM            (mongo_input_stream_get_type())
#define MONGO_INPUT_STREAM_ERROR           (mongo_input_stream_error_quark())
#define MONGO_INPUT_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_INPUT_STREAM, MongoInputStream))
#define MONGO_INPUT_STREAM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_INPUT_STREAM, MongoInputStream const))
#define MONGO_INPUT_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_INPUT_STREAM, MongoInputStreamClass))
#define MONGO_IS_INPUT_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_INPUT_STREAM))
#define MONGO_IS_INPUT_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_INPUT_STREAM))
#define MONGO_INPUT_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_INPUT_STREAM, MongoInputStreamClass))

typedef struct _MongoInputStream        MongoInputStream;
typedef struct _MongoInputStreamClass   MongoInputStreamClass;
typedef struct _MongoInputStreamPrivate MongoInputStreamPrivate;
typedef enum   _MongoInputStreamError   MongoInputStreamError;

enum _MongoInputStreamError
{
   MONGO_INPUT_STREAM_ERROR_INVALID_MESSAGE = 1,
   MONGO_INPUT_STREAM_ERROR_UNKNOWN_OPERATION,
   MONGO_INPUT_STREAM_ERROR_INSUFFICIENT_DATA,
};

struct _MongoInputStream
{
   GFilterInputStream parent;

   /*< private >*/
   MongoInputStreamPrivate *priv;
};

struct _MongoInputStreamClass
{
   GFilterInputStreamClass parent_class;
};

GQuark            mongo_input_stream_error_quark        (void) G_GNUC_CONST;
GType             mongo_input_stream_get_type           (void) G_GNUC_CONST;
MongoInputStream *mongo_input_stream_new                (GInputStream          *base_stream);
MongoMessage     *mongo_input_stream_read_message       (MongoInputStream      *stream,
                                                         GCancellable          *cancellable,
                                                         GError               **error);
void              mongo_input_stream_read_message_async (MongoInputStream      *stream,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
MongoMessage     *mongo_input_stream_read_message_finish (MongoInputStream     *stream,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS

#endif /* MONGO_INPUT_STREAM_H */
