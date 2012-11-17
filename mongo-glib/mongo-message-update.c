/* mongo-message-update.c
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
#include "mongo-message-update.h"

G_DEFINE_TYPE(MongoMessageUpdate, mongo_message_update, MONGO_TYPE_MESSAGE)

struct _MongoMessageUpdatePrivate
{
   gchar            *collection;
   MongoUpdateFlags  flags;
   MongoBson        *query;
   MongoBson        *update;
};

enum
{
   PROP_0,
   PROP_COLLECTION,
   PROP_FLAGS,
   PROP_QUERY,
   PROP_UPDATE,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

const gchar *
mongo_message_update_get_collection (MongoMessageUpdate *update)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_UPDATE(update), NULL);
   return update->priv->collection;
}

void
mongo_message_update_set_collection (MongoMessageUpdate *update,
                                     const gchar        *collection)
{
   g_return_if_fail(MONGO_IS_MESSAGE_UPDATE(update));
   g_free(update->priv->collection);
   update->priv->collection = g_strdup(collection);
   g_object_notify_by_pspec(G_OBJECT(update), gParamSpecs[PROP_COLLECTION]);
}

MongoUpdateFlags
mongo_message_update_get_flags (MongoMessageUpdate *update)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_UPDATE(update), 0);
   return update->priv->flags;
}

void
mongo_message_update_set_flags (MongoMessageUpdate *update,
                                MongoUpdateFlags    flags)
{
   g_return_if_fail(MONGO_IS_MESSAGE_UPDATE(update));
   update->priv->flags = flags;
   g_object_notify_by_pspec(G_OBJECT(update), gParamSpecs[PROP_FLAGS]);
}

MongoBson *
mongo_message_update_get_query (MongoMessageUpdate *update)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_UPDATE(update), NULL);
   return update->priv->query;
}

void
mongo_message_update_set_query (MongoMessageUpdate *update,
                                   MongoBson          *query)
{
   g_return_if_fail(MONGO_IS_MESSAGE_UPDATE(update));
   mongo_clear_bson(&update->priv->query);
   if (query) {
      update->priv->query = mongo_bson_ref(query);
   }
   g_object_notify_by_pspec(G_OBJECT(update), gParamSpecs[PROP_QUERY]);
}

MongoBson *
mongo_message_update_get_update (MongoMessageUpdate *update)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_UPDATE(update), NULL);
   return update->priv->update;
}

void
mongo_message_update_set_update (MongoMessageUpdate *update,
                                 MongoBson          *update_bson)
{
   g_return_if_fail(MONGO_IS_MESSAGE_UPDATE(update));
   mongo_clear_bson(&update->priv->update);
   if (update_bson) {
      update->priv->update = mongo_bson_ref(update_bson);
   }
   g_object_notify_by_pspec(G_OBJECT(update), gParamSpecs[PROP_UPDATE]);
}

static gboolean
mongo_message_update_load_from_data (MongoMessage *message,
                                     const guint8 *data,
                                     gsize         length)
{
   MongoMessageUpdate *update = (MongoMessageUpdate *)message;
   const gchar *name;
   MongoBson *bson;
   guint32 v32;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_UPDATE(update));
   g_assert(data);
   g_assert(length);

   if (length >= 4) {
      /* First 4 bytes are ZERO, reserved for future. */
      length -= 4;
      data += 4;

      if (length >= 1) {
         /* Walk through collection name */
         for (name = (gchar *)data; length && *data; length--, data++) { }

         if (length) {
            mongo_message_update_set_collection(update, name);
            length--;
            data++;

            /* Update flags */
            if (length >= 4) {
               memcpy(&v32, data, sizeof v32);
               mongo_message_update_set_flags(update, GUINT32_FROM_LE(v32));
               length -= 4;
               data += 4;

               /* Query BSON */
               if (length >= 4) {
                  memcpy(&v32, data, sizeof v32);
                  v32 = GUINT32_FROM_LE(v32);
                  if (v32 <= length) {
                     if ((bson = mongo_bson_new_from_data(data, v32))) {
                        mongo_message_update_set_query(update, bson);
                        mongo_bson_unref(bson);
                        length -= v32;
                        data += v32;

                        /* Update BSON */
                        if (length >= 4) {
                           memcpy(&v32, data, sizeof v32);
                           v32 = GUINT32_FROM_LE(v32);
                           if (v32 <= length) {
                              if ((bson = mongo_bson_new_from_data(data, v32))) {
                                 mongo_message_update_set_update(update, bson);
                                 mongo_bson_unref(bson);
                                 length -= v32;
                                 data += v32;
                                 RETURN(length == 0);
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   RETURN(FALSE);
}

static guint8 *
mongo_message_update_save_to_data (MongoMessage *message,
                                   gsize        *length)
{
   static const guint8 empty_bson[] = { 5, 0, 0, 0, 0 };
   MongoMessageUpdatePrivate *priv;
   MongoMessageUpdate *update = (MongoMessageUpdate *)message;
   GByteArray *bytes;
   guint32 v32;
   guint8 *ret;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_UPDATE(update));
   g_assert(length);

   priv = update->priv;

   bytes = g_byte_array_sized_new(64);

   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_request_id(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_response_to(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GUINT32_TO_LE(MONGO_OPERATION_UPDATE);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Zero, reserved. */
   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Collection name */
   g_byte_array_append(bytes, (guint8 *)(priv->collection ?: ""),
                       strlen(priv->collection ?: "") + 1);

   /* Update flags. */
   v32 = GUINT32_TO_LE(priv->flags);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Query */
   if (priv->query) {
      g_byte_array_append(bytes, priv->query->data, priv->query->len);
   } else {
      g_byte_array_append(bytes, empty_bson, G_N_ELEMENTS(empty_bson));
   }

   /* Update */
   if (priv->update) {
      g_byte_array_append(bytes, priv->update->data, priv->update->len);
   } else {
      g_byte_array_append(bytes, empty_bson, G_N_ELEMENTS(empty_bson));
   }

   /* Update the message length */
   v32 = GUINT32_TO_LE(bytes->len);
   memcpy(bytes->data, &v32, sizeof v32);

   *length = bytes->len;

   DUMP_BYTES(buf, bytes->data, bytes->len);

   ret = g_byte_array_free(bytes, FALSE);
   RETURN(ret);
}

static void
mongo_message_update_finalize (GObject *object)
{
   MongoMessageUpdatePrivate *priv;

   ENTRY;
   priv = MONGO_MESSAGE_UPDATE(object)->priv;
   g_free(priv->collection);
   mongo_clear_bson(&priv->query);
   mongo_clear_bson(&priv->update);
   G_OBJECT_CLASS(mongo_message_update_parent_class)->finalize(object);
   EXIT;
}

static void
mongo_message_update_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
   MongoMessageUpdate *update = MONGO_MESSAGE_UPDATE(object);

   switch (prop_id) {
   case PROP_COLLECTION:
      g_value_set_string(value, mongo_message_update_get_collection(update));
      break;
   case PROP_FLAGS:
      g_value_set_flags(value, mongo_message_update_get_flags(update));
      break;
   case PROP_QUERY:
      g_value_set_boxed(value, mongo_message_update_get_query(update));
      break;
   case PROP_UPDATE:
      g_value_set_boxed(value, mongo_message_update_get_update(update));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_update_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
   MongoMessageUpdate *update = MONGO_MESSAGE_UPDATE(object);

   switch (prop_id) {
   case PROP_COLLECTION:
      mongo_message_update_set_collection(update, g_value_get_string(value));
      break;
   case PROP_FLAGS:
      mongo_message_update_set_flags(update, g_value_get_flags(value));
      break;
   case PROP_QUERY:
      mongo_message_update_set_query(update, g_value_get_boxed(value));
      break;
   case PROP_UPDATE:
      mongo_message_update_set_update(update, g_value_get_boxed(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_update_class_init (MongoMessageUpdateClass *klass)
{
   GObjectClass *object_class;
   MongoMessageClass *message_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_message_update_finalize;
   object_class->get_property = mongo_message_update_get_property;
   object_class->set_property = mongo_message_update_set_property;
   g_type_class_add_private(object_class, sizeof(MongoMessageUpdatePrivate));

   message_class = MONGO_MESSAGE_CLASS(klass);
   message_class->operation = MONGO_OPERATION_MSG;
   message_class->load_from_data = mongo_message_update_load_from_data;
   message_class->save_to_data = mongo_message_update_save_to_data;

   gParamSpecs[PROP_COLLECTION] =
      g_param_spec_string("collection",
                          _("Collection"),
                          _("The db.collection name."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_COLLECTION,
                                   gParamSpecs[PROP_COLLECTION]);

   gParamSpecs[PROP_FLAGS] =
      g_param_spec_flags("flags",
                         _("Flags"),
                         _("The update flags."),
                         MONGO_TYPE_UPDATE_FLAGS,
                         MONGO_UPDATE_NONE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_FLAGS,
                                   gParamSpecs[PROP_FLAGS]);

   gParamSpecs[PROP_QUERY] =
      g_param_spec_boxed("query",
                         _("Query"),
                         _("The query to select the document."),
                         MONGO_TYPE_BSON,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_QUERY,
                                   gParamSpecs[PROP_QUERY]);

   gParamSpecs[PROP_UPDATE] =
      g_param_spec_boxed("update",
                         _("Update"),
                         _("The specification of the update to perform."),
                         MONGO_TYPE_BSON,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_UPDATE,
                                   gParamSpecs[PROP_UPDATE]);
}

static void
mongo_message_update_init (MongoMessageUpdate *update)
{
   update->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(update,
                                  MONGO_TYPE_MESSAGE_UPDATE,
                                  MongoMessageUpdatePrivate);
}
