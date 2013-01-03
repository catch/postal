/* mongo-message-msg.c
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
#include "mongo-message-msg.h"

G_DEFINE_TYPE(MongoMessageMsg, mongo_message_msg, MONGO_TYPE_MESSAGE)

struct _MongoMessageMsgPrivate
{
   gchar *message;
};

enum
{
   PROP_0,
   PROP_MESSAGE,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

const gchar *
mongo_message_msg_get_message (MongoMessageMsg *msg)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_MSG(msg), NULL);
   return msg->priv->message;
}

void
mongo_message_msg_set_message (MongoMessageMsg *msg,
                               const gchar     *message)
{
   g_return_if_fail(MONGO_IS_MESSAGE_MSG(msg));

   g_free(msg->priv->message);
   msg->priv->message = g_strdup(message);
   g_object_notify_by_pspec(G_OBJECT(msg), gParamSpecs[PROP_MESSAGE]);
}

static gboolean
mongo_message_msg_load_from_data (MongoMessage *message,
                                  const guint8 *data,
                                  gsize         length)
{
   MongoMessageMsg *msg = (MongoMessageMsg *)message;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_MSG(msg));

   if (g_utf8_validate((gchar *)data, length - 1, NULL) &&
       (data[length-1] == '\0')) {
      mongo_message_msg_set_message(msg, (gchar *)data);
      RETURN(TRUE);
   }

   RETURN(FALSE);
}

static guint8 *
mongo_message_msg_save_to_data (MongoMessage *message,
                                gsize        *length)
{
   MongoMessageMsgPrivate *priv;
   MongoMessageMsg *msg = (MongoMessageMsg *)message;
   GByteArray *bytes;
   gint32 v32;
   guint8 *ret;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_MSG(msg));
   g_assert(length);

   priv = msg->priv;

   bytes = g_byte_array_sized_new(64);

   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_request_id(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_response_to(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GUINT32_TO_LE(MONGO_OPERATION_MSG);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   if (priv->message) {
      g_byte_array_append(bytes, (guint8 *)priv->message,
                          strlen(priv->message) + 1);
   } else {
      guint8 b = '\0';
      g_byte_array_append(bytes, &b, 1);
   }

   v32 = GUINT32_TO_LE(bytes->len);
   memcpy(bytes->data, &v32, sizeof v32);

   *length = bytes->len;

   DUMP_BYTES(buf, bytes->data, bytes->len);

   ret = g_byte_array_free(bytes, FALSE);
   RETURN(ret);
}

static void
mongo_message_msg_finalize (GObject *object)
{
   MongoMessageMsgPrivate *priv;

   ENTRY;

   priv = MONGO_MESSAGE_MSG(object)->priv;

   g_free(priv->message);

   G_OBJECT_CLASS(mongo_message_msg_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_message_msg_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
   MongoMessageMsg *msg = MONGO_MESSAGE_MSG(object);

   switch (prop_id) {
   case PROP_MESSAGE:
      g_value_set_string(value, mongo_message_msg_get_message(msg));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_msg_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
   MongoMessageMsg *msg = MONGO_MESSAGE_MSG(object);

   switch (prop_id) {
   case PROP_MESSAGE:
      mongo_message_msg_set_message(msg, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_msg_class_init (MongoMessageMsgClass *klass)
{
   GObjectClass *object_class;
   MongoMessageClass *message_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_message_msg_finalize;
   object_class->get_property = mongo_message_msg_get_property;
   object_class->set_property = mongo_message_msg_set_property;
   g_type_class_add_private(object_class, sizeof(MongoMessageMsgPrivate));

   message_class = MONGO_MESSAGE_CLASS(klass);
   message_class->operation = MONGO_OPERATION_MSG;
   message_class->load_from_data = mongo_message_msg_load_from_data;
   message_class->save_to_data = mongo_message_msg_save_to_data;

   /**
    * MongoMessageMsg:message:
    *
    * The message field of a MongoDB OP_MSG message.
    */
   gParamSpecs[PROP_MESSAGE] =
      g_param_spec_string("message",
                          _("Message"),
                          _("The message string."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_MESSAGE,
                                   gParamSpecs[PROP_MESSAGE]);
}

static void
mongo_message_msg_init (MongoMessageMsg *msg)
{
   msg->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(msg,
                                  MONGO_TYPE_MESSAGE_MSG,
                                  MongoMessageMsgPrivate);
}
