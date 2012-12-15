/* mongo-glib.h
 *
 * Copyright (C) 2011 Christian Hergert <christian@catch.com>
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

#ifndef MONGO_GLIB_H
#define MONGO_GLIB_H

#include <glib.h>

G_BEGIN_DECLS

#define MONGO_INSIDE

#include "mongo-bson.h"
#include "mongo-bson-stream.h"
#include "mongo-client.h"
#include "mongo-client-context.h"
#include "mongo-collection.h"
#include "mongo-connection.h"
#include "mongo-cursor.h"
#include "mongo-database.h"
#include "mongo-flags.h"
#include "mongo-input-stream.h"
#include "mongo-manager.h"
#include "mongo-message.h"
#include "mongo-message-delete.h"
#include "mongo-message-getmore.h"
#include "mongo-message-insert.h"
#include "mongo-message-kill-cursors.h"
#include "mongo-message-msg.h"
#include "mongo-message-query.h"
#include "mongo-message-reply.h"
#include "mongo-message-update.h"
#include "mongo-object-id.h"
#include "mongo-operation.h"
#include "mongo-output-stream.h"
#include "mongo-protocol.h"
#include "mongo-server.h"
#include "mongo-version.h"

#undef MONGO_INSIDE

G_END_DECLS

#endif /* MONGO_GLIB_H */
