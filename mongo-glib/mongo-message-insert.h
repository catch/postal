/* mongo-message-insert.h
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

#ifndef MONGO_MESSAGE_INSERT_H
#define MONGO_MESSAGE_INSERT_H

#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_MESSAGE_INSERT            (mongo_message_insert_get_type())
#define MONGO_MESSAGE_INSERT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_INSERT, MongoMessageInsert))
#define MONGO_MESSAGE_INSERT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_INSERT, MongoMessageInsert const))
#define MONGO_MESSAGE_INSERT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_MESSAGE_INSERT, MongoMessageInsertClass))
#define MONGO_IS_MESSAGE_INSERT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_MESSAGE_INSERT))
#define MONGO_IS_MESSAGE_INSERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_MESSAGE_INSERT))
#define MONGO_MESSAGE_INSERT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_MESSAGE_INSERT, MongoMessageInsertClass))

typedef struct _MongoMessageInsert        MongoMessageInsert;
typedef struct _MongoMessageInsertClass   MongoMessageInsertClass;
typedef struct _MongoMessageInsertPrivate MongoMessageInsertPrivate;

struct _MongoMessageInsert
{
   MongoMessage parent;

   /*< private >*/
   MongoMessageInsertPrivate *priv;
};

struct _MongoMessageInsertClass
{
   MongoMessageClass parent_class;
};

const gchar      *mongo_message_insert_get_collection (MongoMessageInsert  *insert);
GList            *mongo_message_insert_get_documents  (MongoMessageInsert  *insert);
MongoInsertFlags  mongo_message_insert_get_flags      (MongoMessageInsert  *insert);
GType             mongo_message_insert_get_type       (void) G_GNUC_CONST;
void              mongo_message_insert_set_collection (MongoMessageInsert  *insert,
                                                       const gchar         *collection);
void              mongo_message_insert_set_documents  (MongoMessageInsert  *insert,
                                                       GList               *documents);
void              mongo_message_insert_set_flags      (MongoMessageInsert  *insert,
                                                       MongoInsertFlags     flags);

G_END_DECLS

#endif /* MONGO_MESSAGE_INSERT_H */
