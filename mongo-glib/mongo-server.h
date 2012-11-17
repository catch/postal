/* mongo-server.h
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

#ifndef MONGO_SERVER_H
#define MONGO_SERVER_H

#include <gio/gio.h>

#include "mongo-client-context.h"
#include "mongo-message.h"

G_BEGIN_DECLS

#define MONGO_TYPE_SERVER            (mongo_server_get_type())
#define MONGO_TYPE_CLIENT_CONTEXT    (mongo_client_context_get_type())
#define MONGO_SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_SERVER, MongoServer))
#define MONGO_SERVER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_SERVER, MongoServer const))
#define MONGO_SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_SERVER, MongoServerClass))
#define MONGO_IS_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_SERVER))
#define MONGO_IS_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_SERVER))
#define MONGO_SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_SERVER, MongoServerClass))

typedef struct _MongoServer        MongoServer;
typedef struct _MongoServerClass   MongoServerClass;
typedef struct _MongoServerPrivate MongoServerPrivate;

struct _MongoServer
{
   GSocketService parent;

   /*< private >*/
   MongoServerPrivate *priv;
};

struct _MongoServerClass
{
   GSocketServiceClass parent_class;

   void     (*request_started)      (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   void     (*request_finished)     (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_read)         (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_reply)        (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_msg)          (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_update)       (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_insert)       (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_query)        (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_getmore)      (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_delete)       (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gboolean (*request_kill_cursors) (MongoServer        *server,
                                     MongoClientContext *client,
                                     MongoMessage       *message);

   gpointer _reserved1;
   gpointer _reserved2;
   gpointer _reserved3;
   gpointer _reserved4;
   gpointer _reserved5;
   gpointer _reserved6;
   gpointer _reserved7;
   gpointer _reserved8;
};

GType        mongo_server_get_type         (void) G_GNUC_CONST;
MongoServer *mongo_server_new              (void);
void         mongo_server_pause_message    (MongoServer         *server,
                                            MongoMessage        *message);
void         mongo_server_unpause_message  (MongoServer         *server,
                                            MongoMessage        *message);

G_END_DECLS

#endif /* MONGO_SERVER_H */
