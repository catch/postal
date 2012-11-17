/* mongo-message-reply.c
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
#include "mongo-operation.h"
#include "mongo-message-reply.h"

G_DEFINE_TYPE(MongoMessageReply, mongo_message_reply, MONGO_TYPE_MESSAGE)

struct _MongoMessageReplyPrivate
{
   guint64           cursor_id;
   GList            *documents;
   MongoReplyFlags   flags;
   guint32           offset;
};

enum
{
   PROP_0,
   PROP_COUNT,
   PROP_CURSOR_ID,
   PROP_FLAGS,
   PROP_OFFSET,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

gsize
mongo_message_reply_get_count (MongoMessageReply *reply)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_REPLY(reply), 0);
   return g_list_length(reply->priv->documents);
}

guint64
mongo_message_reply_get_cursor_id (MongoMessageReply *reply)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_REPLY(reply), 0);
   return reply->priv->cursor_id;
}

void
mongo_message_reply_set_cursor_id (MongoMessageReply *reply,
                                   guint64     cursor_id)
{
   g_return_if_fail(MONGO_IS_MESSAGE_REPLY(reply));
   reply->priv->cursor_id = cursor_id;
   g_object_notify_by_pspec(G_OBJECT(reply), gParamSpecs[PROP_CURSOR_ID]);
}

/**
 * mongo_message_reply_get_documents:
 * @reply: (in): A #MongoMessageReply.
 *
 * Returns an array of documents for the reply. @count is set to the
 * number of documents returned.
 *
 * Returns: (transfer none) (element-type MongoBson*): A #GList of #MongoBson.
 */
GList *
mongo_message_reply_get_documents (MongoMessageReply *reply)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_REPLY(reply), NULL);
   return reply->priv->documents;
}

/**
 * mongo_message_reply_set_documents:
 * @reply: (in): A #MongoMessageReply.
 * @documents: (in) (transfer none) (element-type MongoBson*): A #GList
 *   of #MongoBson documents.
 *
 * Sets the documents for the reply. @documents is copied and each #MongoBson
 * inside has its reference count increased by one.
 */
void
mongo_message_reply_set_documents (MongoMessageReply *reply,
                                   GList             *documents)
{
   MongoMessageReplyPrivate *priv;
   GList *list = NULL;
   GList *iter;

   g_return_if_fail(MONGO_IS_MESSAGE_REPLY(reply));

   priv = reply->priv;

   g_list_foreach(priv->documents, (GFunc)mongo_bson_unref, NULL);
   g_list_free(priv->documents);
   priv->documents = NULL;

   for (iter = documents; iter; iter = iter->next) {
      if (iter->data) {
         list = g_list_prepend(list, mongo_bson_ref(iter->data));
      }
   }
   priv->documents = g_list_reverse(list);

   g_object_notify_by_pspec(G_OBJECT(reply), gParamSpecs[PROP_COUNT]);
}

MongoReplyFlags
mongo_message_reply_get_flags (MongoMessageReply *reply)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_REPLY(reply), 0);
   return reply->priv->flags;
}

void
mongo_message_reply_set_flags (MongoMessageReply *reply,
                               MongoReplyFlags    flags)
{
   g_return_if_fail(MONGO_IS_MESSAGE_REPLY(reply));
   reply->priv->flags = flags;
   g_object_notify_by_pspec(G_OBJECT(reply), gParamSpecs[PROP_FLAGS]);
}

guint
mongo_message_reply_get_offset (MongoMessageReply *reply)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_REPLY(reply), 0);
   return reply->priv->offset;
}

void
mongo_message_reply_set_offset (MongoMessageReply *reply,
                                guint       offset)
{
   g_return_if_fail(MONGO_IS_MESSAGE_REPLY(reply));
   reply->priv->offset = offset;
   g_object_notify_by_pspec(G_OBJECT(reply), gParamSpecs[PROP_OFFSET]);
}

static guint8 *
mongo_message_reply_save_to_data (MongoMessage *message,
                                  gsize        *length)
{
   MongoMessageReplyPrivate *priv;
   MongoMessageReply *reply = (MongoMessageReply *)message;
   GByteArray *bytes;
   MongoBson *bson;
   gint32 v32;
   gint64 v64;
   guint8 *ret;
   GList *iter;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_REPLY(reply));
   g_assert(length);

   priv = reply->priv;

   bytes = g_byte_array_sized_new(64);

   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_request_id(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_response_to(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GUINT32_TO_LE(MONGO_OPERATION_REPLY);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Reply flags */
   v32 = GUINT32_TO_LE(priv->flags);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Server side cursor id */
   v64 = GUINT64_TO_LE(priv->cursor_id);
   g_byte_array_append(bytes, (guint8 *)&v64, sizeof v64);

   /* Offset in result set. */
   v32 = GINT32_TO_LE(priv->offset);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Number of documents returned */
   v32 = GUINT32_TO_LE(g_list_length(priv->documents));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* encode BSON documents */
   for (iter = priv->documents; iter; iter = iter->next) {
      bson = iter->data;
      g_byte_array_append(bytes, bson->data, bson->len);
   }

   /* Update message length */
   v32 = GUINT32_TO_LE(bytes->len);
   memcpy(bytes->data, &v32, sizeof v32);

   *length = bytes->len;

   DUMP_BYTES(buf, bytes->data, bytes->len);

   ret = g_byte_array_free(bytes, FALSE);
   RETURN(ret);
}

static gboolean
mongo_message_reply_load_from_data (MongoMessage *message,
                                    const guint8 *data,
                                    gsize         length)
{
   MongoMessageReplyPrivate *priv;
   MongoMessageReply *reply = (MongoMessageReply *)message;
   MongoBson *bson;
   guint64 cursor;
   guint32 flags;
   guint32 offset;
   guint32 count;
   guint32 msg_len;
   gssize len = length;
   GList *list = NULL;
   guint i;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_REPLY(reply));
   g_assert(data);
   g_assert(len);

   priv = reply->priv;

   memcpy(&flags, data, sizeof flags);
   flags = GUINT32_FROM_LE(flags);
   data += 4;
   len -= 4;

   memcpy(&cursor, data, sizeof cursor);
   cursor = GUINT64_FROM_LE(cursor);
   data += 8;
   len -= 8;

   memcpy(&offset, data, sizeof offset);
   offset = GUINT32_FROM_LE(offset);
   data += 4;
   len -= 4;

   memcpy(&count, data, sizeof count);
   count = GUINT32_FROM_LE(count);
   data += 4;
   len -= 4;

   for (i = 0; i < count; i++) {
      if (len <= 0) {
         GOTO(failure);
      }

      memcpy(&msg_len, data, sizeof msg_len);
      msg_len = GUINT32_FROM_LE(msg_len);

      if (msg_len <= len) {
         if ((bson = mongo_bson_new_from_data(data, msg_len))) {
            list = g_list_append(list, bson);
         } else {
            GOTO(failure);
         }
      }

      data += msg_len;
   }

   if (g_list_length(list) != count) {
      GOTO(failure);
   }

   priv->cursor_id = cursor;
   priv->flags = flags;
   priv->offset = offset;
   priv->documents = list;
   RETURN(TRUE);

failure:
   g_list_foreach(list, (GFunc)mongo_bson_unref, NULL);
   g_list_free(list);
   RETURN(FALSE);
}

static void
mongo_message_reply_finalize (GObject *object)
{
   MongoMessageReplyPrivate *priv;

   ENTRY;

   priv = MONGO_MESSAGE_REPLY(object)->priv;

   g_list_foreach(priv->documents, (GFunc)mongo_bson_unref, NULL);
   g_list_free(priv->documents);

   G_OBJECT_CLASS(mongo_message_reply_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_message_reply_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
   MongoMessageReply *reply = MONGO_MESSAGE_REPLY(object);

   switch (prop_id) {
   case PROP_COUNT:
      g_value_set_uint(value, mongo_message_reply_get_count(reply));
      break;
   case PROP_CURSOR_ID:
      g_value_set_uint64(value, mongo_message_reply_get_cursor_id(reply));
      break;
   case PROP_FLAGS:
      g_value_set_flags(value, mongo_message_reply_get_flags(reply));
      break;
   case PROP_OFFSET:
      g_value_set_uint(value, mongo_message_reply_get_offset(reply));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_reply_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
   MongoMessageReply *reply = MONGO_MESSAGE_REPLY(object);

   switch (prop_id) {
   case PROP_CURSOR_ID:
      mongo_message_reply_set_cursor_id(reply, g_value_get_uint64(value));
      break;
   case PROP_FLAGS:
      mongo_message_reply_set_flags(reply, g_value_get_flags(value));
      break;
   case PROP_OFFSET:
      mongo_message_reply_set_offset(reply, g_value_get_uint(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_reply_class_init (MongoMessageReplyClass *klass)
{
   GObjectClass *object_class;
   MongoMessageClass *message_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_message_reply_finalize;
   object_class->get_property = mongo_message_reply_get_property;
   object_class->set_property = mongo_message_reply_set_property;
   g_type_class_add_private(object_class, sizeof(MongoMessageReplyPrivate));

   message_class = MONGO_MESSAGE_CLASS(klass);
   message_class->load_from_data = mongo_message_reply_load_from_data;
   message_class->operation = MONGO_OPERATION_REPLY;
   message_class->save_to_data = mongo_message_reply_save_to_data;

   gParamSpecs[PROP_COUNT] =
      g_param_spec_uint("count",
                        _("Count"),
                        _("The number of documents."),
                        0,
                        G_MAXUINT32,
                        0,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_COUNT,
                                   gParamSpecs[PROP_COUNT]);

   gParamSpecs[PROP_CURSOR_ID] =
      g_param_spec_uint64("cursor-id",
                          _("Cursor Id"),
                          _("The cursor identifier."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_CURSOR_ID,
                                   gParamSpecs[PROP_CURSOR_ID]);

   gParamSpecs[PROP_FLAGS] =
      g_param_spec_flags("flags",
                         _("Flags"),
                         _("The reply flags."),
                         MONGO_TYPE_REPLY_FLAGS,
                         MONGO_REPLY_NONE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_FLAGS,
                                   gParamSpecs[PROP_FLAGS]);

   gParamSpecs[PROP_OFFSET] =
      g_param_spec_uint("offset",
                        _("Offset"),
                        _("The offset of the documents in the result set."),
                        0,
                        G_MAXUINT32,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_OFFSET,
                                   gParamSpecs[PROP_OFFSET]);
}

static void
mongo_message_reply_init (MongoMessageReply *reply)
{
   reply->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(reply,
                                  MONGO_TYPE_MESSAGE_REPLY,
                                  MongoMessageReplyPrivate);
}
