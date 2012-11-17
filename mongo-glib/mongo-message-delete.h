/* mongo-message-delete.h
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

#ifndef MONGO_MESSAGE_DELETE_H
#define MONGO_MESSAGE_DELETE_H

#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_MESSAGE_DELETE            (mongo_message_delete_get_type())
#define MONGO_MESSAGE_DELETE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_DELETE, MongoMessageDelete))
#define MONGO_MESSAGE_DELETE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_DELETE, MongoMessageDelete const))
#define MONGO_MESSAGE_DELETE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_MESSAGE_DELETE, MongoMessageDeleteClass))
#define MONGO_IS_MESSAGE_DELETE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_MESSAGE_DELETE))
#define MONGO_IS_MESSAGE_DELETE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_MESSAGE_DELETE))
#define MONGO_MESSAGE_DELETE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_MESSAGE_DELETE, MongoMessageDeleteClass))

typedef struct _MongoMessageDelete        MongoMessageDelete;
typedef struct _MongoMessageDeleteClass   MongoMessageDeleteClass;
typedef struct _MongoMessageDeletePrivate MongoMessageDeletePrivate;

struct _MongoMessageDelete
{
   MongoMessage parent;

   /*< private >*/
   MongoMessageDeletePrivate *priv;
};

struct _MongoMessageDeleteClass
{
   MongoMessageClass parent_class;
};

GType mongo_message_delete_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MONGO_MESSAGE_DELETE_H */
