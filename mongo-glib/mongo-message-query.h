/* mongo-message-query.h
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

#ifndef MONGO_MESSAGE_QUERY_H
#define MONGO_MESSAGE_QUERY_H

#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_MESSAGE_QUERY            (mongo_message_query_get_type())
#define MONGO_MESSAGE_QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_QUERY, MongoMessageQuery))
#define MONGO_MESSAGE_QUERY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_MESSAGE_QUERY, MongoMessageQuery const))
#define MONGO_MESSAGE_QUERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_MESSAGE_QUERY, MongoMessageQueryClass))
#define MONGO_IS_MESSAGE_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_MESSAGE_QUERY))
#define MONGO_IS_MESSAGE_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_MESSAGE_QUERY))
#define MONGO_MESSAGE_QUERY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_MESSAGE_QUERY, MongoMessageQueryClass))

typedef struct _MongoMessageQuery        MongoMessageQuery;
typedef struct _MongoMessageQueryClass   MongoMessageQueryClass;
typedef struct _MongoMessageQueryPrivate MongoMessageQueryPrivate;

struct _MongoMessageQuery
{
   MongoMessage parent;

   /*< private >*/
   MongoMessageQueryPrivate *priv;
};

struct _MongoMessageQueryClass
{
   MongoMessageClass parent_class;
};

const gchar     *mongo_message_query_get_collection   (MongoMessageQuery *query);
const gchar     *mongo_message_query_get_command_name (MongoMessageQuery *query);
MongoQueryFlags  mongo_message_query_get_flags        (MongoMessageQuery *query);
guint            mongo_message_query_get_limit        (MongoMessageQuery *query);
const MongoBson *mongo_message_query_get_query        (MongoMessageQuery *query);
const MongoBson *mongo_message_query_get_selector     (MongoMessageQuery *query);
guint            mongo_message_query_get_skip         (MongoMessageQuery *query);
GType            mongo_message_query_get_type         (void) G_GNUC_CONST;
gboolean         mongo_message_query_is_command       (MongoMessageQuery *query);
void             mongo_message_query_set_collection   (MongoMessageQuery *query,
                                                       const gchar       *collection);
void             mongo_message_query_set_flags        (MongoMessageQuery *query,
                                                       MongoQueryFlags    flags);
void             mongo_message_query_set_limit        (MongoMessageQuery *query,
                                                       guint              limit);
void             mongo_message_query_set_query        (MongoMessageQuery *query,
                                                       MongoBson         *bson);
void             mongo_message_query_set_fields       (MongoMessageQuery *query,
                                                       MongoBson         *fields);
void             mongo_message_query_set_skip         (MongoMessageQuery *query,
                                                       guint              skip);
void             mongo_message_query_take_query       (MongoMessageQuery *query,
                                                       MongoBson         *bson);
void             mongo_message_query_take_fields      (MongoMessageQuery *query,
                                                       MongoBson         *fields);

G_END_DECLS

#endif /* MONGO_MESSAGE_QUERY_H */
