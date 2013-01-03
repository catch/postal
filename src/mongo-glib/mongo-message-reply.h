/* mongo-message-reply.h
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

#ifndef MONGO_MESSAGE_REPLY_H
#define MONGO_MESSAGE_REPLY_H

#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_MESSAGE_REPLY            (mongo_message_reply_get_type())
#define MONGO_MESSAGE_REPLY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_REPLY, MongoMessageReply))
#define MONGO_MESSAGE_REPLY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_REPLY, MongoMessageReply const))
#define MONGO_MESSAGE_REPLY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_MESSAGE_REPLY, MongoMessageReplyClass))
#define MONGO_IS_MESSAGE_REPLY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_MESSAGE_REPLY))
#define MONGO_IS_MESSAGE_REPLY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_MESSAGE_REPLY))
#define MONGO_MESSAGE_REPLY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_MESSAGE_REPLY, MongoMessageReplyClass))

typedef struct _MongoMessageReply        MongoMessageReply;
typedef struct _MongoMessageReplyClass   MongoMessageReplyClass;
typedef struct _MongoMessageReplyPrivate MongoMessageReplyPrivate;

struct _MongoMessageReply
{
   MongoMessage parent;

   /*< private >*/
   MongoMessageReplyPrivate *priv;
};

struct _MongoMessageReplyClass
{
   MongoMessageClass parent_class;
};

gsize            mongo_message_reply_get_count      (MongoMessageReply   *reply);
guint64          mongo_message_reply_get_cursor_id  (MongoMessageReply   *reply);
GList           *mongo_message_reply_get_documents  (MongoMessageReply   *reply);
MongoReplyFlags  mongo_message_reply_get_flags      (MongoMessageReply   *reply);
guint            mongo_message_reply_get_offset     (MongoMessageReply   *reply);
GType            mongo_message_reply_get_type       (void) G_GNUC_CONST;
void             mongo_message_reply_set_cursor_id  (MongoMessageReply   *reply,
                                                     guint64              cursor_id);
void             mongo_message_reply_set_documents  (MongoMessageReply   *reply,
                                                     GList               *documents);
void             mongo_message_reply_set_flags      (MongoMessageReply  *reply,
                                                     MongoReplyFlags     flags);
void             mongo_message_reply_set_offset     (MongoMessageReply  *reply,
                                                     guint               offset);

G_END_DECLS

#endif /* MONGO_MESSAGE_REPLY_H */
