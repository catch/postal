/* postal-bson.c
 *
 * Copyright (C) 2012 Catch.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "postal-bson.h"

static JsonNode *postal_bson_array_to_json    (MongoBsonIter *iter);
static JsonNode *postal_bson_document_to_json (MongoBsonIter *iter);

static JsonNode *
postal_bson_iter_to_json (MongoBsonIter *iter)
{
   MongoObjectId *oid;
   MongoBsonType type;
   MongoBsonIter child;
   JsonNode *ret = NULL;
   GTimeVal tv;
   gchar *str;

   type = mongo_bson_iter_get_value_type(iter);

   switch (type) {
   case MONGO_BSON_ARRAY:
      if (mongo_bson_iter_recurse(iter, &child)) {
         ret = postal_bson_array_to_json(&child);
      }
      break;
   case MONGO_BSON_BOOLEAN:
      ret = json_node_new(JSON_NODE_VALUE);
      json_node_set_boolean(ret, mongo_bson_iter_get_value_boolean(iter));
      break;
   case MONGO_BSON_DATE_TIME:
      ret = json_node_new(JSON_NODE_VALUE);
      mongo_bson_iter_get_value_timeval(iter, &tv);
      str = g_time_val_to_iso8601(&tv);
      json_node_set_string(ret, str);
      g_free(str);
      break;
   case MONGO_BSON_DOCUMENT:
      if (mongo_bson_iter_recurse(iter, &child)) {
         ret = postal_bson_document_to_json(&child);
      }
      break;
   case MONGO_BSON_DOUBLE:
      json_node_set_double(ret, mongo_bson_iter_get_value_double(iter));
      break;
   case MONGO_BSON_INT32:
      ret = json_node_new(JSON_NODE_VALUE);
      json_node_set_int(ret, mongo_bson_iter_get_value_int(iter));
      break;
   case MONGO_BSON_INT64:
      ret = json_node_new(JSON_NODE_VALUE);
      json_node_set_double(ret, mongo_bson_iter_get_value_int64(iter));
      break;
   case MONGO_BSON_NULL:
      ret = json_node_new(JSON_NODE_NULL);
      break;
   case MONGO_BSON_OBJECT_ID:
      ret = json_node_new(JSON_NODE_VALUE);
      oid = mongo_bson_iter_get_value_object_id(iter);
      str = mongo_object_id_to_string(oid);
      json_node_set_string(ret, str);
      mongo_object_id_free(oid);
      g_free(str);
      break;
   case MONGO_BSON_REGEX:
      /* TODO: */
      break;
   case MONGO_BSON_UNDEFINED:
      ret = json_node_new(JSON_NODE_NULL);
      break;
   case MONGO_BSON_UTF8:
      ret = json_node_new(JSON_NODE_VALUE);
      json_node_set_string(ret, mongo_bson_iter_get_value_string(iter, NULL));
      break;
   default:
      g_warning("Unknown BSON type: %d", type);
      return NULL;
   }

   return ret;
}

static JsonNode *
postal_bson_array_to_json (MongoBsonIter *iter)
{
   JsonArray *ar;
   JsonNode *node;

   ar = json_array_new();
   while (mongo_bson_iter_next(iter)) {
      if ((node = postal_bson_iter_to_json(iter))) {
         json_array_add_element(ar, node);
      }
   }

   node = json_node_new(JSON_NODE_ARRAY);
   json_node_set_array(node, ar);
   json_array_unref(ar);

   return node;
}

static JsonNode *
postal_bson_document_to_json (MongoBsonIter *iter)
{
   const gchar *key;
   JsonObject *obj;
   JsonNode *node;

   obj = json_object_new();
   while (mongo_bson_iter_next(iter)) {
      key = mongo_bson_iter_get_key(iter);
      if ((node = postal_bson_iter_to_json(iter))) {
         json_object_set_member(obj, key, node);
      }
   }

   node = json_node_new(JSON_NODE_OBJECT);
   json_node_set_object(node, obj);
   json_object_unref(obj);

   return node;
}

JsonNode *
postal_bson_to_json (const MongoBson *bson)
{
   MongoBsonIter iter;
   g_return_val_if_fail(bson, NULL);
   mongo_bson_iter_init(&iter, bson);
   return postal_bson_document_to_json(&iter);
}
