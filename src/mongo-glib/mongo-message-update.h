/* mongo-message-update.h
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

#ifndef MONGO_MESSAGE_UPDATE_H
#define MONGO_MESSAGE_UPDATE_H

#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_MESSAGE_UPDATE            (mongo_message_update_get_type())
#define MONGO_MESSAGE_UPDATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_UPDATE, MongoMessageUpdate))
#define MONGO_MESSAGE_UPDATE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_UPDATE, MongoMessageUpdate const))
#define MONGO_MESSAGE_UPDATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_MESSAGE_UPDATE, MongoMessageUpdateClass))
#define MONGO_IS_MESSAGE_UPDATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_MESSAGE_UPDATE))
#define MONGO_IS_MESSAGE_UPDATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_MESSAGE_UPDATE))
#define MONGO_MESSAGE_UPDATE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_MESSAGE_UPDATE, MongoMessageUpdateClass))

typedef struct _MongoMessageUpdate        MongoMessageUpdate;
typedef struct _MongoMessageUpdateClass   MongoMessageUpdateClass;
typedef struct _MongoMessageUpdatePrivate MongoMessageUpdatePrivate;

struct _MongoMessageUpdate
{
   MongoMessage parent;

   /*< private >*/
   MongoMessageUpdatePrivate *priv;
};

struct _MongoMessageUpdateClass
{
   MongoMessageClass parent_class;
};

GType mongo_message_update_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MONGO_MESSAGE_UPDATE_H */
