/* mongo-message-query.c
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
#include "mongo-message-query.h"

G_DEFINE_TYPE(MongoMessageQuery, mongo_message_query, MONGO_TYPE_MESSAGE)

struct _MongoMessageQueryPrivate
{
   gchar           *collection;
   MongoQueryFlags  flags;
   guint32          limit;
   MongoBson       *query;
   MongoBson       *fields;
   guint32          skip;
};

enum
{
   PROP_0,
   PROP_COLLECTION,
   PROP_FIELDS,
   PROP_FLAGS,
   PROP_LIMIT,
   PROP_MESSAGE_QUERY,
   PROP_SKIP,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

const gchar *
mongo_message_query_get_collection (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), NULL);
   return query->priv->collection;
}

MongoQueryFlags
mongo_message_query_get_flags (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), 0);
   return query->priv->flags;
}

guint
mongo_message_query_get_limit (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), 0);
   return query->priv->limit;
}

const MongoBson *
mongo_message_query_get_query (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), NULL);
   return query->priv->query;
}

const MongoBson *
mongo_message_query_get_fields (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), NULL);
   return query->priv->fields;
}

guint
mongo_message_query_get_skip (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), 0);
   return query->priv->skip;
}

gboolean
mongo_message_query_is_command (MongoMessageQuery *query)
{
   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), FALSE);

   if (query->priv->collection) {
      return g_str_has_suffix(query->priv->collection, ".$cmd");
   }

   return FALSE;
}

const gchar *
mongo_message_query_get_command_name (MongoMessageQuery *query)
{
   MongoMessageQueryPrivate *priv;
   MongoBsonIter iter;

   g_return_val_if_fail(MONGO_IS_MESSAGE_QUERY(query), NULL);

   priv = query->priv;

   if (mongo_message_query_is_command(query)) {
      if (priv->query) {
         mongo_bson_iter_init(&iter, priv->query);
         if (mongo_bson_iter_next(&iter)) {
            return mongo_bson_iter_get_key(&iter);
         }
      }
   }

   return NULL;
}

void
mongo_message_query_set_collection (MongoMessageQuery *query,
                                    const gchar       *collection)
{
   MongoMessageQueryPrivate *priv;

   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));

   priv = query->priv;

   g_free(priv->collection);
   priv->collection = g_strdup(collection);
   g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_COLLECTION]);
}

void
mongo_message_query_set_flags (MongoMessageQuery *query,
                               MongoQueryFlags    flags)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));
   query->priv->flags = flags;
   g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_FLAGS]);
}

void
mongo_message_query_set_limit (MongoMessageQuery *query,
                               guint              limit)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));
   query->priv->limit = limit;
   g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_LIMIT]);
}

void
mongo_message_query_set_query (MongoMessageQuery *query,
                               MongoBson         *bson)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));
   mongo_message_query_take_query(query, bson ? mongo_bson_ref(bson) : NULL);
}

void
mongo_message_query_set_fields (MongoMessageQuery *query,
                                MongoBson         *fields)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));
   mongo_message_query_take_fields(query,
                                   fields ? mongo_bson_ref(fields) : NULL);
}

void
mongo_message_query_set_skip (MongoMessageQuery *query,
                              guint              skip)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));
   query->priv->skip = skip;
   g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_SKIP]);
}

void
mongo_message_query_take_query (MongoMessageQuery *query,
                                MongoBson         *bson)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));

   mongo_clear_bson(&query->priv->query);
   query->priv->query = bson;
   g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_MESSAGE_QUERY]);
}

void
mongo_message_query_take_fields (MongoMessageQuery *query,
                                 MongoBson         *fields)
{
   g_return_if_fail(MONGO_IS_MESSAGE_QUERY(query));

   mongo_clear_bson(&query->priv->fields);
   query->priv->fields = fields;
   g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_FIELDS]);
}

static gboolean
mongo_message_query_load_from_data (MongoMessage *message,
                                    const guint8 *data,
                                    gsize         data_len)
{
   MongoMessageQueryPrivate *priv;
   MongoMessageQuery *query = (MongoMessageQuery *)message;
   const gchar *name;
   MongoBson *bson;
   guint32 vu32;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_QUERY(query));
   g_assert(data);
   g_assert(data_len);

   priv = query->priv;

   if (data_len > 4) {
      /* Query flags */
      memcpy(&vu32, data, sizeof vu32);
      priv->flags = GUINT32_FROM_LE(vu32);

      data_len -= 4;
      data += 4;

      /* Walk through collection name */
      for (name = (gchar *)data; data_len && *data; data_len--, data++) { }

      if (data_len) {
         mongo_message_query_set_collection(query, name);
         data_len--;
         data++;

         /* Skipped documents */
         if (data_len > 4) {
            memcpy(&vu32, data, sizeof vu32);
            mongo_message_query_set_skip(query, GUINT32_FROM_LE(vu32));
            data_len -= 4;
            data += 4;

            /* Maximum return documents */
            if (data_len > 4) {
               memcpy(&vu32, data, sizeof vu32);
               mongo_message_query_set_limit(query, GUINT32_FROM_LE(vu32));
               data_len -= 4;
               data += 4;

               /* Query BSON document */
               if (data_len > 4) {
                  memcpy(&vu32, data, sizeof vu32);
                  vu32 = GUINT32_FROM_LE(vu32);
                  if (data_len >= vu32) {
                     bson = mongo_bson_new_from_data(data, vu32);
                     mongo_message_query_take_query(query, bson);
                     data_len -= vu32;
                     data += vu32;

                     /* Fields bson document */
                     if (data_len > 4) {
                        memcpy(&vu32, data, sizeof vu32);
                        vu32 = GUINT32_FROM_LE(vu32);
                        if (data_len >= vu32) {
                           bson = mongo_bson_new_from_data(data, vu32);
                           mongo_message_query_take_fields(query, bson);
                           data_len -= vu32;
                           data += vu32;
                        }
                     }

                     RETURN(data_len == 0);
                  }
               }
            }
         }
      }
   }

   RETURN(FALSE);
}

static guint8 *
mongo_message_query_save_to_data (MongoMessage *message,
                                  gsize        *length)
{
   static const guint8 empty_bson[] = { 5, 0, 0, 0, 0 };
   MongoMessageQueryPrivate *priv;
   MongoMessageQuery *query = (MongoMessageQuery *)message;
   GByteArray *bytes;
   guint32 v32;
   guint8 *ret;

   ENTRY;

   g_assert(MONGO_IS_MESSAGE_QUERY(query));
   g_assert(length);

   priv = query->priv;

   bytes = g_byte_array_sized_new(64);

   v32 = 0;
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_request_id(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GINT32_TO_LE(mongo_message_get_response_to(message));
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   v32 = GUINT32_TO_LE(MONGO_OPERATION_QUERY);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Query flags. */
   v32 = GUINT32_TO_LE(priv->flags);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Collection name */
   g_byte_array_append(bytes, (guint8 *)(priv->collection ?: ""),
                       strlen(priv->collection ?: "") + 1);

   /* Number to skip */
   v32 = GUINT32_TO_LE(priv->skip);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Number to return */
   v32 = GUINT32_TO_LE(priv->limit);
   g_byte_array_append(bytes, (guint8 *)&v32, sizeof v32);

   /* Query */
   if (priv->query) {
      g_byte_array_append(bytes, priv->query->data, priv->query->len);
   } else {
      g_byte_array_append(bytes, empty_bson, G_N_ELEMENTS(empty_bson));
   }

   /* Fields */
   if (priv->fields) {
      g_byte_array_append(bytes, priv->fields->data, priv->fields->len);
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
mongo_message_query_finalize (GObject *object)
{
   MongoMessageQueryPrivate *priv;

   ENTRY;

   priv = MONGO_MESSAGE_QUERY(object)->priv;

   g_free(priv->collection);
   mongo_clear_bson(&priv->query);
   mongo_clear_bson(&priv->fields);

   G_OBJECT_CLASS(mongo_message_query_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_message_query_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
   MongoMessageQuery *query = MONGO_MESSAGE_QUERY(object);

   switch (prop_id) {
   case PROP_COLLECTION:
      g_value_set_string(value, mongo_message_query_get_collection(query));
      break;
   case PROP_FLAGS:
      g_value_set_flags(value, mongo_message_query_get_flags(query));
      break;
   case PROP_LIMIT:
      g_value_set_uint(value, mongo_message_query_get_limit(query));
      break;
   case PROP_MESSAGE_QUERY:
      g_value_set_boxed(value, mongo_message_query_get_query(query));
      break;
   case PROP_FIELDS:
      g_value_set_boxed(value, mongo_message_query_get_fields(query));
      break;
   case PROP_SKIP:
      g_value_set_uint(value, mongo_message_query_get_skip(query));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_query_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
   MongoMessageQuery *query = MONGO_MESSAGE_QUERY(object);

   switch (prop_id) {
   case PROP_COLLECTION:
      mongo_message_query_set_collection(query, g_value_get_string(value));
      break;
   case PROP_FLAGS:
      mongo_message_query_set_flags(query, g_value_get_flags(value));
      break;
   case PROP_LIMIT:
      mongo_message_query_set_limit(query, g_value_get_uint(value));
      break;
   case PROP_MESSAGE_QUERY:
      mongo_message_query_set_query(query, g_value_get_boxed(value));
      break;
   case PROP_FIELDS:
      mongo_message_query_set_fields(query, g_value_get_boxed(value));
      break;
   case PROP_SKIP:
      mongo_message_query_set_skip(query, g_value_get_uint(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_message_query_class_init (MongoMessageQueryClass *klass)
{
   GObjectClass *object_class;
   MongoMessageClass *message_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_message_query_finalize;
   object_class->get_property = mongo_message_query_get_property;
   object_class->set_property = mongo_message_query_set_property;
   g_type_class_add_private(object_class, sizeof(MongoMessageQueryPrivate));

   message_class = MONGO_MESSAGE_CLASS(klass);
   message_class->operation = MONGO_OPERATION_QUERY;
   message_class->load_from_data = mongo_message_query_load_from_data;
   message_class->save_to_data = mongo_message_query_save_to_data;

   gParamSpecs[PROP_COLLECTION] =
      g_param_spec_string("collection",
                          _("Collection"),
                          _("The db.collection name to query."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_COLLECTION,
                                   gParamSpecs[PROP_COLLECTION]);

   gParamSpecs[PROP_FLAGS] =
      g_param_spec_flags("flags",
                         _("Flags"),
                         _("The query flags."),
                         MONGO_TYPE_QUERY_FLAGS,
                         MONGO_QUERY_NONE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_FLAGS,
                                   gParamSpecs[PROP_FLAGS]);

   gParamSpecs[PROP_LIMIT] =
      g_param_spec_uint("limit",
                        _("Limit"),
                        _("The maximum number of returned documents."),
                        0,
                        G_MAXUINT32,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_LIMIT,
                                   gParamSpecs[PROP_LIMIT]);

   gParamSpecs[PROP_MESSAGE_QUERY] =
      g_param_spec_boxed("query",
                         _("MessageQuery"),
                         _("A bson containing the query to perform."),
                         MONGO_TYPE_BSON,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_MESSAGE_QUERY,
                                   gParamSpecs[PROP_MESSAGE_QUERY]);

   gParamSpecs[PROP_FIELDS] =
      g_param_spec_boxed("fields",
                         _("Fields"),
                         _("A bson containing the fields to return."),
                         MONGO_TYPE_BSON,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_FIELDS,
                                   gParamSpecs[PROP_FIELDS]);

   gParamSpecs[PROP_SKIP] =
      g_param_spec_uint("skip",
                        _("Skip"),
                        _("The number of matching documents to skip."),
                        0,
                        G_MAXUINT32,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_SKIP,
                                   gParamSpecs[PROP_SKIP]);
}

static void
mongo_message_query_init (MongoMessageQuery *query)
{
   query->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(query,
                                  MONGO_TYPE_MESSAGE_QUERY,
                                  MongoMessageQueryPrivate);
}
