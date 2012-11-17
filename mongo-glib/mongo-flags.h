/* mongo-flags.h
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

#ifndef MONGO_FLAGS_H
#define MONGO_FLAGS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MONGO_TYPE_DELETE_FLAGS (mongo_delete_flags_get_type())
#define MONGO_TYPE_INSERT_FLAGS (mongo_insert_flags_get_type())
#define MONGO_TYPE_QUERY_FLAGS  (mongo_query_flags_get_type())
#define MONGO_TYPE_REPLY_FLAGS  (mongo_reply_flags_get_type())
#define MONGO_TYPE_UPDATE_FLAGS (mongo_update_flags_get_type())

/**
 * MongoDeleteFlags:
 * @MONGO_DELETE_NONE: Specify no delete flags.
 * @MONGO_DELETE_SINGLE_REMOVE: Only remove the first document matching the
 *    document selector.
 *
 * #MongoDeleteFlags are used when performing a delete operation.
 */
typedef enum
{
   MONGO_DELETE_NONE          = 0,
   MONGO_DELETE_SINGLE_REMOVE = 1 << 0,
} MongoDeleteFlags;

/**
 * MongoInsertFlags:
 * @MONGO_INSERT_NONE: Specify no insert flags.
 * @MONGO_INSERT_CONTINUE_ON_ERROR: Continue inserting documents from
 *    the insertion set even if one fails.
 *
 * #MongoInsertFlags are used when performing an insert operation.
 */
typedef enum
{
   MONGO_INSERT_NONE              = 0,
   MONGO_INSERT_CONTINUE_ON_ERROR = 1 << 0,
} MongoInsertFlags;

/**
 * MongoQueryFlags:
 * @MONGO_QUERY_NONE: No query flags supplied.
 * @MONGO_QUERY_TAILABLE_CURSOR: Cursor will not be closed when the last
 *    data is retrieved. You can resume this cursor later.
 * @MONGO_QUERY_SLAVE_OK: Allow query of replica slave.
 * @MONGO_QUERY_OPLOG_REPLAY: Used internally by Mongo.
 * @MONGO_QUERY_NO_CURSOR_TIMEOUT: The server normally times out idle
 *    cursors after an inactivity period (10 minutes). This prevents that.
 * @MONGO_QUERY_AWAIT_DATA: Use with %MONGO_QUERY_TAILABLE_CURSOR. Block
 *    rather than returning no data. After a period, time out.
 * @MONGO_QUERY_EXHAUST: Stream the data down full blast in multiple
 *    "more" packages. Faster when you are pulling a lot of data and
 *    know you want to pull it all down.
 * @MONGO_QUERY_PARTIAL: Get partial results from mongos if some shards
 *    are down (instead of throwing an error).
 *
 * #MongoQueryFlags is used for querying a Mongo instance.
 */
typedef enum
{
   MONGO_QUERY_NONE              = 0,
   MONGO_QUERY_TAILABLE_CURSOR   = 1 << 1,
   MONGO_QUERY_SLAVE_OK          = 1 << 2,
   MONGO_QUERY_OPLOG_REPLAY      = 1 << 3,
   MONGO_QUERY_NO_CURSOR_TIMEOUT = 1 << 4,
   MONGO_QUERY_AWAIT_DATA        = 1 << 5,
   MONGO_QUERY_EXHAUST           = 1 << 6,
   MONGO_QUERY_PARTIAL           = 1 << 7,
} MongoQueryFlags;

/**
 * MongoReplyFlags:
 * @MONGO_REPLY_NONE: No flags set.
 * @MONGO_REPLY_CURSOR_NOT_FOUND: Cursor was not found.
 * @MONGO_REPLY_QUERY_FAILURE: Query failed, error document provided.
 * @MONGO_REPLY_SHARD_CONFIG_STALE: Shard configuration is stale.
 * @MONGO_REPLY_AWAIT_CAPABLE: Wait for data to be returned until timeout
 *    has passed. Used with %MONGO_QUERY_TAILABLE_CURSOR.
 *
 * #MongoReplyFlags contains flags supplied by the Mongo server in reply
 * to a request.
 */
typedef enum
{
   MONGO_REPLY_NONE               = 0,
   MONGO_REPLY_CURSOR_NOT_FOUND   = 1 << 0,
   MONGO_REPLY_QUERY_FAILURE      = 1 << 1,
   MONGO_REPLY_SHARD_CONFIG_STALE = 1 << 2,
   MONGO_REPLY_AWAIT_CAPABLE      = 1 << 3,
} MongoReplyFlags;

/**
 * MongoUpdateFlags:
 * @MONGO_UPDATE_NONE: No update flags specified.
 * @MONGO_UPDATE_UPSERT: Perform an upsert.
 * @MONGO_UPDATE_MULTI_UPDATE: Continue updating after first match.
 *
 * #MongoUpdateFlags is used when updating documents found in Mongo.
 */
typedef enum
{
   MONGO_UPDATE_NONE         = 0,
   MONGO_UPDATE_UPSERT       = 1 << 0,
   MONGO_UPDATE_MULTI_UPDATE = 1 << 1,
} MongoUpdateFlags;

GType mongo_delete_flags_get_type (void) G_GNUC_CONST;
GType mongo_insert_flags_get_type (void) G_GNUC_CONST;
GType mongo_query_flags_get_type  (void) G_GNUC_CONST;
GType mongo_reply_flags_get_type  (void) G_GNUC_CONST;
GType mongo_update_flags_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MONGO_FLAGS_H */
