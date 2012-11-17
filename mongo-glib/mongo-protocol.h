/* mongo-protocol.h
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

#ifndef MONGO_PROTOCOL_H
#define MONGO_PROTOCOL_H

#include <glib-object.h>
#include <gio/gio.h>

#include "mongo-bson.h"
#include "mongo-flags.h"
#include "mongo-operation.h"
#include "mongo-message-reply.h"

G_BEGIN_DECLS

#define MONGO_TYPE_QUERY_FLAGS         (mongo_query_flags_get_type())
#define MONGO_TYPE_PROTOCOL            (mongo_protocol_get_type())
#define MONGO_PROTOCOL_ERROR           (mongo_protocol_error_quark())
#define MONGO_PROTOCOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_PROTOCOL, MongoProtocol))
#define MONGO_PROTOCOL_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_PROTOCOL, MongoProtocol const))
#define MONGO_PROTOCOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_PROTOCOL, MongoProtocolClass))
#define MONGO_IS_PROTOCOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_PROTOCOL))
#define MONGO_IS_PROTOCOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_PROTOCOL))
#define MONGO_PROTOCOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_PROTOCOL, MongoProtocolClass))

typedef struct _MongoProtocol        MongoProtocol;
typedef struct _MongoProtocolClass   MongoProtocolClass;
typedef enum   _MongoProtocolError   MongoProtocolError;
typedef struct _MongoProtocolPrivate MongoProtocolPrivate;

enum _MongoProtocolError
{
   MONGO_PROTOCOL_ERROR_UNEXPECTED = 1,
};

struct _MongoProtocol
{
   GObject parent;

   /*< private >*/
   MongoProtocolPrivate *priv;
};

/**
 * MongoProtocolClass:
 * @parent_class: The parent #GObjectClass.
 *
 */
struct _MongoProtocolClass
{
   GObjectClass parent_class;
};

GQuark      mongo_protocol_error_quark         (void) G_GNUC_CONST;
GType       mongo_protocol_get_type            (void) G_GNUC_CONST;
GIOStream  *mongo_protocol_get_io_stream       (MongoProtocol        *protocol);
void        mongo_protocol_fail                (MongoProtocol        *protocol,
                                                const GError         *error);
void        mongo_protocol_update_async        (MongoProtocol        *protocol,
                                                const gchar          *db_and_collection,
                                                MongoUpdateFlags      flags,
                                                const MongoBson      *selector,
                                                const MongoBson      *update,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean    mongo_protocol_update_finish       (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_insert_async        (MongoProtocol        *protocol,
                                                const gchar          *db_and_collection,
                                                MongoInsertFlags      flags,
                                                MongoBson           **documents,
                                                gsize                 n_documents,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean    mongo_protocol_insert_finish       (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_query_async         (MongoProtocol        *protocol,
                                                const gchar          *db_and_collection,
                                                MongoQueryFlags       flags,
                                                guint32               skip,
                                                guint32               limit,
                                                const MongoBson      *query,
                                                const MongoBson      *field_selector,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
MongoMessageReply *mongo_protocol_query_finish        (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_getmore_async       (MongoProtocol        *protocol,
                                                const gchar          *db_and_collection,
                                                guint32               limit,
                                                guint64               cursor_id,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
MongoMessageReply *mongo_protocol_getmore_finish      (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_delete_async        (MongoProtocol        *protocol,
                                                const gchar          *db_and_collection,
                                                MongoDeleteFlags      flags,
                                                const MongoBson      *selector,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean    mongo_protocol_delete_finish       (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_kill_cursors_async  (MongoProtocol        *protocol,
                                                guint64              *cursors,
                                                gsize                 n_cursors,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean    mongo_protocol_kill_cursors_finish (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_msg_async           (MongoProtocol        *protocol,
                                                const gchar          *message,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean    mongo_protocol_msg_finish          (MongoProtocol        *protocol,
                                                GAsyncResult         *result,
                                                GError              **error);
void        mongo_protocol_flush_sync          (MongoProtocol        *protocol);

G_END_DECLS

#endif /* MONGO_PROTOCOL_H */
