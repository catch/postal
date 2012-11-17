/* mongo-operation.h
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

#ifndef MONGO_OPERATION_H
#define MONGO_OPERATION_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * MongoOperation:
 * @MONGO_OPERATION_REPLY: OP_REPLY from Mongo.
 * @MONGO_OPERATION_MSG: Generic message operation.
 * @MONGO_OPERATION_UPDATE: Operation to update documents.
 * @MONGO_OPERATION_INSERT: Operation to insert documents.
 * @MONGO_OPERATION_QUERY: Operation to find documents.
 * @MONGO_OPERATION_GETMORE: Operation to getmore documents on a cursor.
 * @MONGO_OPERATION_DELETE: Operation to delete documents.
 * @MONGO_OPERATION_KILL_CURSORS: Operation to kill an array of cursors.
 *
 * #MongoOperation represents the operation command identifiers used by
 * the Mongo wire protocol. This is mainly provided for completeness sake
 * and is unlikely to be needed by most consumers of this library.
 */
typedef enum
{
   MONGO_OPERATION_REPLY        = 1,
   MONGO_OPERATION_MSG          = 1000,
   MONGO_OPERATION_UPDATE       = 2001,
   MONGO_OPERATION_INSERT       = 2002,
   MONGO_OPERATION_QUERY        = 2004,
   MONGO_OPERATION_GETMORE      = 2005,
   MONGO_OPERATION_DELETE       = 2006,
   MONGO_OPERATION_KILL_CURSORS = 2007,
} MongoOperation;

GType mongo_operation_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MONGO_OPERATION_H */
