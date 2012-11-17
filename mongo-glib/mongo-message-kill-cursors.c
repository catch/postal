/* mongo-message-kill-cursors.c
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

#include <glib/gi18n.h>

#include "mongo-debug.h"
#include "mongo-message-kill-cursors.h"

G_DEFINE_TYPE(MongoMessageKillCursors, mongo_message_kill_cursors, MONGO_TYPE_MESSAGE)

struct _MongoMessageKillCursorsPrivate
{
   guint64 *cursors;
   gsize    n_cursors;
};

static gboolean
mongo_message_kill_cursors_load_from_data (MongoMessage *message,
                                           const guint8 *data,
                                           gsize         length)
{
   MongoMessageKillCursorsPrivate *priv;
   guint32 n_cursors;
   guint64 cursor;
   GArray *cursors;
   guint i;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_KILL_CURSORS(message));
   g_assert(data);
   g_assert(length);

   priv = MONGO_MESSAGE_KILL_CURSORS(message)->priv;

   if (length >= 4) {
      /*
       * First 32-bits are ZERO, reserved for future use.
       */
      data += 4;
      length -= 4;

      if (length >= 4) {
         /*
          * Next 4 bytes are the number of trailing cursors.
          */
         memcpy(&n_cursors, data, sizeof n_cursors);
         data += 4;
         length -= 4;

         /*
          * Read each of the cursors, special case for none.
          */
         if (n_cursors == 0 && !length) {
            g_free(priv->cursors);
            priv->cursors = NULL;
            priv->n_cursors = 0;
            RETURN(TRUE);
         } else if ((n_cursors * sizeof cursor) == length) {
            cursors = g_array_new(FALSE, FALSE, sizeof cursor);
            for (i = 0; i < n_cursors; i++) {
               memcpy(&cursor, data, sizeof cursor);
               g_array_append_val(cursors, cursor);
               data += sizeof cursor;
               length -= sizeof cursor;
            }
            g_free(priv->cursors);
            priv->cursors = (guint64 *)g_array_free(cursors, FALSE);
            priv->n_cursors = n_cursors;
            RETURN(TRUE);
         }
      }
   }

   RETURN(FALSE);
}

static guint8 *
mongo_message_kill_cursors_save_to_data (MongoMessage *message,
                                         gsize        *length)
{
   MongoMessageKillCursorsPrivate *priv;
   MongoMessageKillCursors *cursors = (MongoMessageKillCursors *)message;
   GByteArray *bytes;
   guint64 v64;
   guint32 v32;
   guint8 *ret;
   guint i;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_KILL_CURSORS(cursors));
   g_assert(length);

   priv = cursors->priv;

   bytes = g_byte_array_new();

   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_request_id(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_response_to(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GUINT32_TO_LE(MONGO_OPERATION_KILL_CURSORS);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* ZERO */
   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Number of cursors */
   v32 = priv->n_cursors;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Array of cursor ids */
   for (i = 0; i < priv->n_cursors; i++) {
      v64 = GUINT64_TO_LE(priv->cursors[i]);
      g_byte_array_append(bytes, (guint8 *)&v64, sizeof v64);
   }

   /* Update message size */
   v32 = GUINT32_TO_LE(bytes->len);
   memcpy(bytes->data, &v32, sizeof v32);

   *length = bytes->len;

   DUMP_BYTES(buf, bytes->data, bytes->len);

   ret = g_byte_array_free(bytes, FALSE);
   RETURN(ret);
}

static void
mongo_message_kill_cursors_finalize (GObject *object)
{
   MongoMessageKillCursorsPrivate *priv;

   ENTRY;

   priv = MONGO_MESSAGE_KILL_CURSORS(object)->priv;

   g_free(priv->cursors);
   priv->cursors = NULL;

   G_OBJECT_CLASS(mongo_message_kill_cursors_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_message_kill_cursors_class_init (MongoMessageKillCursorsClass *klass)
{
   GObjectClass *object_class;
   MongoMessageClass *message_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_message_kill_cursors_finalize;
   g_type_class_add_private(object_class, sizeof(MongoMessageKillCursorsPrivate));

   message_class = MONGO_MESSAGE_CLASS(klass);
   message_class->operation = MONGO_OPERATION_KILL_CURSORS;
   message_class->load_from_data = mongo_message_kill_cursors_load_from_data;
   message_class->save_to_data = mongo_message_kill_cursors_save_to_data;
}

static void
mongo_message_kill_cursors_init (MongoMessageKillCursors *cursors)
{
   cursors->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(cursors,
                                  MONGO_TYPE_MESSAGE_KILL_CURSORS,
                                  MongoMessageKillCursorsPrivate);
}
