/* mongo-client.h
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

#ifndef MONGO_CLIENT_H
#define MONGO_CLIENT_H

#include <gio/gio.h>

#include "mongo-bson.h"
#include "mongo-message.h"
#include "mongo-write-concern.h"

G_BEGIN_DECLS

#define MONGO_TYPE_CLIENT            (mongo_client_get_type())
#define MONGO_CLIENT_ERROR           (mongo_client_error_quark())
#define MONGO_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_CLIENT, MongoClient))
#define MONGO_CLIENT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MONGO_TYPE_CLIENT, MongoClient const))
#define MONGO_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MONGO_TYPE_CLIENT, MongoClientClass))
#define MONGO_IS_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MONGO_TYPE_CLIENT))
#define MONGO_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MONGO_TYPE_CLIENT))
#define MONGO_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MONGO_TYPE_CLIENT, MongoClientClass))

typedef struct _MongoClient        MongoClient;
typedef struct _MongoClientClass   MongoClientClass;
typedef struct _MongoClientPrivate MongoClientPrivate;
typedef enum   _MongoClientError   MongoClientError;

enum _MongoClientError
{
   MONGO_CLIENT_ERROR_NOT_CONNECTED = 1,
};

struct _MongoClient
{
   GObject parent;

   /*< private >*/
   MongoClientPrivate *priv;
};

struct _MongoClientClass
{
   GObjectClass parent_class;
};

GQuark       mongo_client_error_quark      (void) G_GNUC_CONST;
GType        mongo_client_get_type         (void) G_GNUC_CONST;
MongoClient *mongo_client_new              (void);
MongoClient *mongo_client_new_from_uri     (const gchar          *uri);
MongoClient *mongo_client_new_from_stream  (GIOStream            *stream);
gboolean     mongo_client_send             (MongoClient          *client,
                                            MongoMessage         *message,
                                            MongoWriteConcern    *concern,
                                            MongoMessage        **reply,
                                            GCancellable         *cancellable,
                                            GError              **error);
void         mongo_client_send_async       (MongoClient          *client,
                                            MongoMessage         *message,
                                            MongoWriteConcern    *concern,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
gboolean     mongo_client_send_finish      (MongoClient          *client,
                                            GAsyncResult         *result,
                                            MongoMessage        **reply,
                                            GError              **error);

G_END_DECLS

#endif /* MONGO_CLIENT_H */
