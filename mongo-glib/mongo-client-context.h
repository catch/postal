/* mongo-client-context.h
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

#ifndef MONGO_CLIENT_CONTEXT_H
#define MONGO_CLIENT_CONTEXT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MONGO_TYPE_CLIENT_CONTEXT (mongo_client_context_get_type())

typedef struct _MongoClientContext MongoClientContext;

GType               mongo_client_context_get_type (void) G_GNUC_CONST;
gchar              *mongo_client_context_get_uri  (MongoClientContext *context);
MongoClientContext *mongo_client_context_ref      (MongoClientContext *context);
void                mongo_client_context_unref    (MongoClientContext *context);

G_END_DECLS

#endif /* MONGO_CLIENT_CONTEXT_H */

