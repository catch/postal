/* mongo-message-getmore.c
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
#include "mongo-message-getmore.h"

G_DEFINE_TYPE(MongoMessageGetmore, mongo_message_getmore, MONGO_TYPE_MESSAGE)

struct _MongoMessageGetmorePrivate
{
   gchar   *collection;
   guint64  cursor_id;
   guint32  limit;
};

enum
{
   PROP_0,
   PROP_COLLECTION,
   PROP_CURSOR_ID,
   PROP_LIMIT,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

const gchar *
mongo_message_getmore_get_collection (MongoMessageGetmore *getmore)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_GETMORE(getmore), NULL);
   return getmore->priv->collection;
}

void
mongo_message_getmore_set_collection (MongoMessageGetmore *getmore,
                                      const gchar         *collection)
{
   g_return_if_fail(MONGO_IS_MESSAGE_GETMORE(getmore));
   g_free(getmore->priv->collection);
   getmore->priv->collection = g_strdup(collection);
   g_object_notify_by_pspec(G_OBJECT(getmore), gParamSpecs[PROP_COLLECTION]);
}

guint64
mongo_message_getmore_get_cursor_id (MongoMessageGetmore *getmore)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_GETMORE(getmore), 0UL);
   return getmore->priv->cursor_id;
}

void
mongo_message_getmore_set_cursor_id (MongoMessageGetmore *getmore,
                                     guint64              cursor_id)
{
   g_return_if_fail(MONGO_IS_MESSAGE_GETMORE(getmore));
   getmore->priv->cursor_id = cursor_id;
   g_object_notify_by_pspec(G_OBJECT(getmore), gParamSpecs[PROP_CURSOR_ID]);
}

guint
mongo_message_getmore_get_limit (MongoMessageGetmore *getmore)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_GETMORE(getmore), 0);
   return getmore->priv->limit;
}

void
mongo_message_getmore_set_limit (MongoMessageGetmore *getmore,
                                 guint                limit)
{
   g_return_if_fail(MONGO_IS_MESSAGE_GETMORE(getmore));
   getmore->priv->limit = limit;
   g_object_notify_by_pspec(G_OBJECT(getmore), gParamSpecs[PROP_LIMIT]);
}

static gboolean
mongo_message_getmore_load_from_data (MongoMessage *message,
                                      const guint8 *data,
                                      gsize         length)
{
   MongoMessageGetmore *getmore = (MongoMessageGetmore *)message;
   const gchar *name;
   guint32 v32;
   guint64 v64;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_GETMORE(getmore));

   if (length >= 4) {
      /* First 4 bytes are ZERO, reserved. */
      length -= 4;
      data += 4;

      for (name = (gchar *)data; length && *data; length--, data++) { }

      if (length) {
         mongo_message_getmore_set_collection(getmore, name);
         length--;
         data++;

         if (length >= sizeof v32) {
            memcpy(&v32, data, sizeof v32);
            mongo_message_getmore_set_limit(getmore,
                                            GUINT32_FROM_LE(v32));
            data += 4;
            length -= 4;

            if (length == 8) {
               memcpy(&v64, data, sizeof v64);
               mongo_message_getmore_set_cursor_id(getmore,
                                                   GUINT64_FROM_LE(v64));
               RETURN(TRUE);
            }
         }
      }
   }

   RETURN(FALSE);
}

static guint8 *
mongo_message_getmore_save_to_data (MongoMessage *message,
                                    gsize        *length)
{
   MongoMessageGetmorePrivate *priv;
   MongoMessageGetmore *getmore = (MongoMessageGetmore *)message;
   GByteArray *bytes;
   guint32 v32;
   guint64 v64;
   guint8 *ret;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_GETMORE(getmore));
   g_assert(length);

   priv = getmore->priv;

   bytes = g_byte_array_sized_new(64);

   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_request_id(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_response_to(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GUINT32_TO_LE(MONGO_OPERATION_GETMORE);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* ZERO, reserved for future */
   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Collection name */
   g_byte_array_append(bytes,
                       (guint8 *)(priv->collection ?: ""),
                       strlen(priv->collection ?: "") + 1);

   /* Limit */
   v32 = GUINT32_TO_LE(priv->limit);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Cursor Id */
   v64 = GUINT32_TO_LE(priv->cursor_id);
   g_byte_array_append(bytes, (guint8 *)&v64, sizeof v64);

   /* Update message length */
   v32 = GUINT32_TO_LE(bytes->len);
   memcpy(bytes->data, &v32, sizeof v32);

   *length = bytes->len;

   DUMP_BYTES(buf, bytes->data, bytes->len);

   ret = g_byte_array_free(bytes, FALSE);
   RETURN(ret);
}

static void
mongo_message_getmore_finalize (GObject *object)
{
   MongoMessageGetmorePrivate *priv;

   ENTRY;
   priv = MONGO_MESSAGE_GETMORE(object)->priv;
   g_free(priv->collection);
   G_OBJECT_CLASS(mongo_message_getmore_parent_class)->finalize(object);
   EXIT;
}

static void
mongo_message_getmore_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
   MongoMessageGetmore *getmore = MONGO_MESSAGE_GETMORE(object);

   switch (prop_id) {
   case PROP_COLLECTION:
      g_value_set_string(value, mongo_message_getmore_get_collection(getmore));
      break;
   case PROP_CURSOR_ID:
      g_value_set_uint64(value, mongo_message_getmore_get_cursor_id(getmore));
      break;
   case PROP_LIMIT:
      g_value_set_uint(value, mongo_message_getmore_get_limit(getmore));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_getmore_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
   MongoMessageGetmore *getmore = MONGO_MESSAGE_GETMORE(object);

   switch (prop_id) {
   case PROP_COLLECTION:
      mongo_message_getmore_set_collection(getmore, g_value_get_string(value));
      break;
   case PROP_CURSOR_ID:
      mongo_message_getmore_set_cursor_id(getmore, g_value_get_uint64(value));
      break;
   case PROP_LIMIT:
      mongo_message_getmore_set_limit(getmore, g_value_get_uint(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_getmore_class_init (MongoMessageGetmoreClass *klass)
{
   GObjectClass *object_class;
   MongoMessageClass *message_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_message_getmore_finalize;
   object_class->get_property = mongo_message_getmore_get_property;
   object_class->set_property = mongo_message_getmore_set_property;
   g_type_class_add_private(object_class, sizeof(MongoMessageGetmorePrivate));

   message_class = MONGO_MESSAGE_CLASS(klass);
   message_class->operation = MONGO_OPERATION_GETMORE;
   message_class->load_from_data = mongo_message_getmore_load_from_data;
   message_class->save_to_data = mongo_message_getmore_save_to_data;

   gParamSpecs[PROP_COLLECTION] =
      g_param_spec_string("collection",
                          _("Collection"),
                          _("The db.collection name."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_COLLECTION,
                                   gParamSpecs[PROP_COLLECTION]);

   gParamSpecs[PROP_CURSOR_ID] =
      g_param_spec_uint64("cursor-id",
                          _("Cursor Id"),
                          _("The server side cursor identifier."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_CURSOR_ID,
                                   gParamSpecs[PROP_CURSOR_ID]);

   gParamSpecs[PROP_LIMIT] =
      g_param_spec_uint("limit",
                        _("Limit"),
                        _("The maximum number of documents to return."),
                        0,
                        G_MAXUINT32,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_LIMIT,
                                   gParamSpecs[PROP_LIMIT]);
}

static void
mongo_message_getmore_init (MongoMessageGetmore *getmore)
{
   getmore->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(getmore,
                                  MONGO_TYPE_MESSAGE_GETMORE,
                                  MongoMessageGetmorePrivate);
}
