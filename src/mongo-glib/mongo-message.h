/* mongo-message.h
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
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

#ifndef MONGO_MESSAGE_H
#define MONGO_MESSAGE_H

#include <glib-object.h>

#include "mongo-bson.h"
#include "mongo-flags.h"
#include "mongo-operation.h"

G_BEGIN_DECLS

#define MONGO_TYPE_MESSAGE            (mongo_message_get_type())
#define MONGO_MESSAGE_ERROR           (mongo_message_error_quark())
#define MONGO_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE, MongoMessage))
#define MONGO_MESSAGE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE, MongoMessage const))
#define MONGO_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_MESSAGE, MongoMessageClass))
#define MONGO_IS_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_MESSAGE))
#define MONGO_IS_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_MESSAGE))
#define MONGO_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_MESSAGE, MongoMessageClass))

typedef struct _MongoMessage        MongoMessage;
typedef struct _MongoMessageClass   MongoMessageClass;
typedef struct _MongoMessagePrivate MongoMessagePrivate;
typedef enum   _MongoMessageError   MongoMessageError;

enum _MongoMessageError
{
   MONGO_MESSAGE_ERROR_INVALID_MESSAGE = 1,
};

struct _MongoMessage
{
   GObject parent;

   /*< private >*/
   MongoMessagePrivate *priv;
};

struct _MongoMessageClass
{
   GObjectClass parent_class;

   MongoOperation operation;

   gboolean  (*load_from_data) (MongoMessage *message,
                                const guint8 *data,
                                gsize         length);

   guint8   *(*save_to_data)   (MongoMessage *message,
                                gsize        *length);

   GBytes   *(*save_to_bytes)  (MongoMessage  *message,
                                GError       **error);

   gpointer _reserved1;
   gpointer _reserved2;
   gpointer _reserved3;
   gpointer _reserved4;
   gpointer _reserved5;
   gpointer _reserved6;
   gpointer _reserved7;
   gpointer _reserved8;
};

GQuark        mongo_message_error_quark     (void) G_GNUC_CONST;
GType         mongo_message_get_type        (void) G_GNUC_CONST;
gint          mongo_message_get_request_id  (MongoMessage    *message);
MongoMessage *mongo_message_get_reply       (MongoMessage    *message);
gint          mongo_message_get_response_to (MongoMessage    *message);
gboolean      mongo_message_load_from_data  (MongoMessage    *message,
                                             const guint8    *data,
                                             gsize            length);
guint8       *mongo_message_save_to_data    (MongoMessage    *message,
                                             gsize           *length);
GBytes       *mongo_message_save_to_bytes   (MongoMessage    *message,
                                             GError          **error);
void          mongo_message_set_request_id  (MongoMessage    *message,
                                             gint             request_id);
void          mongo_message_set_reply       (MongoMessage    *message,
                                             MongoMessage    *reply);
void          mongo_message_set_reply_bson  (MongoMessage    *message,
                                             MongoReplyFlags  flags,
                                             MongoBson       *bson);
void          mongo_message_set_response_to (MongoMessage    *message,
                                             gint             response_to);

G_END_DECLS

#endif /* MONGO_MESSAGE_H */
