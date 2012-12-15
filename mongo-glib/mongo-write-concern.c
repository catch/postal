/* mongo-write-concern.c
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

#include <string.h>

#include "mongo-debug.h"
#include "mongo-write-concern.h"
#include "mongo-message-query.h"

struct _MongoWriteConcern
{
   gboolean   _fsync;
   gboolean   journal;
   gint32     w;
   gboolean   w_majority;
   MongoBson *w_tags;
   guint32    wtimeoutms;
};

MongoWriteConcern *
mongo_write_concern_new_unsafe (void)
{
   MongoWriteConcern *concern;

   concern = g_slice_new0(MongoWriteConcern);
   return concern;
}

MongoWriteConcern *
mongo_write_concern_new (void)
{
   MongoWriteConcern *concern;

   concern = mongo_write_concern_new_unsafe();
   concern->w = 1;

   return concern;
}

MongoWriteConcern *
mongo_write_concern_copy (MongoWriteConcern *concern)
{
   MongoWriteConcern *copy;

   g_return_val_if_fail(concern, NULL);

   copy = mongo_write_concern_new_unsafe();
   memcpy(copy, concern, sizeof *copy);
   if (copy->w_tags) {
      copy->w_tags = mongo_bson_ref(copy->w_tags);
   }

   return copy;
}

GType
mongo_write_concern_get_type (void)
{
   static volatile GType type_id;

   if (g_once_init_enter(&type_id)) {
      GType gtype = g_boxed_type_register_static(
            "MongoWriteConcern",
            (GBoxedCopyFunc)mongo_write_concern_copy,
            (GBoxedFreeFunc)mongo_write_concern_free);
      g_once_init_leave(&type_id, gtype);
   }

   return type_id;
}

void
mongo_write_concern_free (MongoWriteConcern *concern)
{
   if (concern) {
      mongo_clear_bson(&concern->w_tags);
      g_slice_free(MongoWriteConcern, concern);
   }
}

void
mongo_write_concern_set_fsync (MongoWriteConcern *concern,
                               gboolean           _fsync)
{
   g_return_if_fail(concern);
   concern->_fsync = _fsync;
}

void
mongo_write_concern_set_journal (MongoWriteConcern *concern,
                                 gboolean           journal)
{
   g_return_if_fail(concern);
   concern->journal = journal;
}

gint
mongo_write_concern_get_w (MongoWriteConcern *concern)
{
   g_return_val_if_fail(concern, 0);
   return concern->w;
}

void
mongo_write_concern_set_w (MongoWriteConcern *concern,
                           gint               w)
{
   g_return_if_fail(concern);

   concern->w_majority = 0;
   mongo_clear_bson(&concern->w_tags);
   concern->w = w;
}

void
mongo_write_concern_set_w_majority (MongoWriteConcern *concern)
{
   g_return_if_fail(concern);

   mongo_clear_bson(&concern->w_tags);
   concern->w = 0;
   concern->w_majority = TRUE;
}

void
mongo_write_concern_set_w_tags (MongoWriteConcern *concern,
                                const MongoBson   *tags)
{
   g_return_if_fail(concern);
   g_return_if_fail(tags);

   mongo_clear_bson(&concern->w_tags);
   concern->w = 0;
   concern->w_majority = FALSE;
   concern->w_tags = mongo_bson_dup(tags);
}

void
mongo_write_concern_set_wtimeoutms (MongoWriteConcern *concern,
                                    guint              wtimeoutms)
{
   g_return_if_fail(concern);
   concern->wtimeoutms = wtimeoutms;
}

/**
 * mongo_write_concern_build_getlasterror:
 * @concern: A #MongoWriteConcern.
 *
 * Creates a #MongoMessageQuery that can be sent to a MongoDB server
 * to check the write status of the previous command. This uses the write
 * concern settings of the structure to build the getlasterror command.
 *
 * This is in a self-contained structure so that callers can provide
 * their own write conern to override the connections when performing
 * certain commands.
 *
 * Returns: (transfer full): A #MongoMessage.
 */
MongoMessage *
mongo_write_concern_build_getlasterror (MongoWriteConcern *concern,
                                        const gchar       *db)
{
   MongoMessage *ret;
   MongoBson *q;
   gchar *db_and_collection;

   ENTRY;

   g_return_val_if_fail(concern, NULL);

   if (concern->w == -1) {
      RETURN(NULL);
   }

   q = mongo_bson_new_empty();
   mongo_bson_append_int(q, "getlasterror", 1);

   if (concern->w > 0) {
      mongo_bson_append_int(q, "w", concern->w);
   } else if (concern->w_majority) {
      mongo_bson_append_string(q, "w", "majority");
   } else if (concern->w_tags) {
      mongo_bson_append_bson(q, "w", concern->w_tags);
   }

   if (concern->journal) {
      mongo_bson_append_boolean(q, "journal", TRUE);
   }

   if (concern->_fsync) {
      mongo_bson_append_boolean(q, "fsync", TRUE);
   }

   if (!db) {
      db = "admin";
   }

   db_and_collection = g_strdup_printf("%s.$cmd", db);
   ret = g_object_new(MONGO_TYPE_MESSAGE_QUERY,
                      "collection", db_and_collection,
                      "query", q,
                      NULL);
   g_free(db_and_collection);

   RETURN(ret);
}
