/* mongo-operation.c
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

#include "mongo-operation.h"
#include "mongo-message-reply.h"
#include "mongo-message-msg.h"
#include "mongo-message-update.h"
#include "mongo-message-insert.h"
#include "mongo-message-query.h"
#include "mongo-message-getmore.h"
#include "mongo-message-delete.h"
#include "mongo-message-kill-cursors.h"

GType
mongo_operation_get_type (void)
{
   static GType type_id;
   static gsize initialized;
   static const GEnumValue values[] = {
      { MONGO_OPERATION_REPLY, "MONGO_OPERATION_REPLY", "REPLY" },
      { MONGO_OPERATION_MSG, "MONGO_OPERATION_MSG", "MSG" },
      { MONGO_OPERATION_UPDATE, "MONGO_OPERATION_UPDATE", "UPDATE" },
      { MONGO_OPERATION_INSERT, "MONGO_OPERATION_INSERT", "INSERT" },
      { MONGO_OPERATION_QUERY, "MONGO_OPERATION_QUERY", "QUERY" },
      { MONGO_OPERATION_GETMORE, "MONGO_OPERATION_GETMORE", "GETMORE" },
      { MONGO_OPERATION_DELETE, "MONGO_OPERATION_DELETE", "DELETE" },
      { MONGO_OPERATION_KILL_CURSORS, "MONGO_OPERATION_KILL_CURSORS", "KILL_CURSORS" },
      { 0 }
   };

   if (g_once_init_enter(&initialized)) {
      type_id = g_enum_register_static("MongoOperation", values);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}

gboolean
mongo_operation_is_known (MongoOperation operation)
{
   switch (operation) {
   case MONGO_OPERATION_REPLY:
   case MONGO_OPERATION_MSG:
   case MONGO_OPERATION_UPDATE:
   case MONGO_OPERATION_INSERT:
   case MONGO_OPERATION_QUERY:
   case MONGO_OPERATION_GETMORE:
   case MONGO_OPERATION_DELETE:
   case MONGO_OPERATION_KILL_CURSORS:
      return TRUE;
   default:
      return FALSE;
   }
}

GType
mongo_operation_get_message_type (MongoOperation operation)
{
   GType type_id = G_TYPE_NONE;

   switch (operation) {
   case MONGO_OPERATION_REPLY:
      type_id = MONGO_TYPE_MESSAGE_REPLY;
      break;
   case MONGO_OPERATION_MSG:
      type_id = MONGO_TYPE_MESSAGE_MSG;
      break;
   case MONGO_OPERATION_UPDATE:
      type_id = MONGO_TYPE_MESSAGE_UPDATE;
      break;
   case MONGO_OPERATION_INSERT:
      type_id = MONGO_TYPE_MESSAGE_INSERT;
      break;
   case MONGO_OPERATION_QUERY:
      type_id = MONGO_TYPE_MESSAGE_QUERY;
      break;
   case MONGO_OPERATION_GETMORE:
      type_id = MONGO_TYPE_MESSAGE_GETMORE;
      break;
   case MONGO_OPERATION_DELETE:
      type_id = MONGO_TYPE_MESSAGE_DELETE;
      break;
   case MONGO_OPERATION_KILL_CURSORS:
      type_id = MONGO_TYPE_MESSAGE_KILL_CURSORS;
      break;
   default:
      break;
   }

   return type_id;
}
