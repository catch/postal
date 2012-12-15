/* mongo-bson.c
 *
 * Copyright (C) 2011 Christian Hergert <christian@catch.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#ifdef HAVE_UNISTR_H
#include <unistr.h>
#endif

#include "mongo-bson.h"
#include "mongo-debug.h"

/**
 * SECTION:mongo-bson
 * @title: MongoBson
 * @short_description: BSON document structure.
 *
 * #MongoBson reprsents a BSON document that can be created, traversed,
 * and used throughout the Mongo-GLib library. They are referenced counted
 * structures that are referenced with mongo_bson_ref() and unreferenced
 * with mongo_bson_unref(). Memory allocated by the #MongoBson structure
 * is automatically released when the reference count reaches zero.
 *
 * You can iterate through the fields in a #MongoBson using the
 * mongo_bson_iter_* functions. #MongoBsonIter contains the state of
 * the iterator and is used to access the current value. You may jump
 * to the next field matching a given name using mongo_bson_iter_find().
 *
 * Keep in mind that BSON documents do not use modified UTF-8 like most
 * of GLib. Therefore, they may have %NULL characters within the target
 * string instead of the two-byte sequenced used in modified UTF-8. This
 * may change at some point to incur an extra cost for conversion to
 * modified UTF-8.
 */

#define ITER_IS_TYPE(iter, type) (GPOINTER_TO_INT(iter->user_data5) == type)

const gchar *
utf8_check (const gchar *str,
            gssize       len)
{
#ifdef HAVE_UNISTR_H
   return (const gchar *)u8_check((const guint8 *)str, len);
#else
   const gchar *end = NULL;
   if (!g_utf8_validate(str, len, &end)) {
      return NULL;
   }
   return end;
#endif
}

/**
 * mongo_bson_new_from_data:
 * @buffer: (array length=length): The buffer to create a #MongoBson.
 * @length: The length of @buffer.
 *
 * Creates a new #MongoBson instance using the buffer and the length.
 *
 * Returns: (transfer full): A new #MongoBson that should be freed
 *   with mongo_bson_unref().
 */
MongoBson *
mongo_bson_new_from_data (const guint8 *buffer,
                          gsize         length)
{
   GByteArray *bson;
   guint32 bson_len;

   g_return_val_if_fail(buffer != NULL, NULL);

   /*
    * The first 4 bytes of a BSON are the length, including the 4 bytes
    * containing said length.
    */
   memcpy(&bson_len, buffer, sizeof bson_len);
   bson_len = GUINT32_FROM_LE(bson_len);
   if (bson_len != length) {
      return NULL;
   }

   bson = g_byte_array_sized_new(length);
   g_byte_array_append(bson, buffer, length);

   return (MongoBson *)bson;
}

/**
 * mongo_bson_new_take_data:
 * @buffer: (in): The buffer to use when creating the #MongoBson.
 * @length: (in): The length of @buffer.
 *
 * Creates a new instance of #MongoBson using the buffers provided.
 * @buffer should be a buffer that can work with g_realloc() and
 * g_free().
 *
 * Returns: (transfer full): A newly allocated #MongoBson.
 */
MongoBson *
mongo_bson_new_take_data (guint8 *buffer,
                          gsize   length)
{
   guint32 bson_len;

   g_return_val_if_fail(buffer, NULL);
   g_return_val_if_fail(length >= 5, NULL);

   /*
    * The first 4 bytes of a BSON are the length, including the 4 bytes
    * containing said length.
    */
   memcpy(&bson_len, buffer, sizeof bson_len);
   bson_len = GUINT32_FROM_LE(bson_len);
   if (bson_len != length) {
      return NULL;
   }

   return (MongoBson *)g_byte_array_new_take(buffer, length);
}

/**
 * mongo_bson_new_empty:
 *
 * Creates a new instance of #MongoBson. This function is similar to
 * mongo_bson_new() except that it does not pre-populate the newly created
 * #MongoBson instance with an _id field.
 *
 * Returns: An empty #MongoBson that should be freed with mongo_bson_unref().
 */
MongoBson *
mongo_bson_new_empty (void)
{
   static const guint8 empty_bson[] = { 5, 0, 0, 0, 0 };
   GByteArray *ar;

   ar = g_byte_array_new();
   g_byte_array_append(ar, empty_bson, G_N_ELEMENTS(empty_bson));
   return (MongoBson *)ar;
}

/**
 * mongo_bson_new:
 *
 * Creates a new instance of #MongoBson. The #MongoBson instance is
 * pre-populated with an _id field and #MongoObjectId value. This value
 * is generated on the local machine and follows the guidelines suggested
 * by the BSON ObjectID specificiation.
 *
 * Returns: A #MongoBson that should be freed with mongo_bson_unref().
 */
MongoBson *
mongo_bson_new (void)
{
   MongoObjectId *oid;
   MongoBson *bson;

   bson = mongo_bson_new_empty();
   oid = mongo_object_id_new();
   mongo_bson_append_object_id(bson, "_id", oid);
   mongo_object_id_free(oid);

   return bson;
}

/**
 * mongo_bson_dup:
 * @bson: (in): A #MongoBson.
 *
 * Creates a new #MongoBson by copying @bson.
 *
 * Returns: (transfer full): A newly allocated #MongoBson.
 */
MongoBson *
mongo_bson_dup (const MongoBson *bson)
{
   GByteArray *copy;
   GByteArray *ar;

   if ((ar = (GByteArray *)bson)) {
      copy = g_byte_array_sized_new(ar->len);
      g_byte_array_append(copy, ar->data, ar->len);
      return (MongoBson *)copy;
   }

   return NULL;
}

/**
 * mongo_bson_ref:
 * @bson: (in): A #MongoBson.
 *
 * Atomically increments the reference count of @bson by one.
 *
 * Returns: (transfer full): @bson.
 */
MongoBson *
mongo_bson_ref (MongoBson *bson)
{
   return (MongoBson *)g_byte_array_ref((GByteArray *)bson);
}

/**
 * mongo_bson_unref:
 * @bson: A MongoBson.
 *
 * Atomically decrements the reference count of @bson by one.  When the
 * reference count reaches zero, the structure will be destroyed and
 * freed.
 */
void
mongo_bson_unref (MongoBson *bson)
{
   g_byte_array_unref((GByteArray *)bson);
}

/**
 * mongo_bson_get_empty:
 * @bson: (in): A #MongoBson.
 *
 * Checks to see if the contents of MongoBson are empty.
 *
 * Returns: %TRUE if @bson is empty.
 */
gboolean
mongo_bson_get_empty (MongoBson *bson)
{
   GByteArray *ar = (GByteArray *)bson;
   g_return_val_if_fail(ar, FALSE);
   return (ar->len == 5);
}

/**
 * mongo_bson_get_type:
 *
 * Retrieve the #GType for the #MongoBson boxed type.
 *
 * Returns: A #GType.
 */
GType
mongo_bson_get_type (void)
{
   static GType type_id;
   static gsize initialized;

   if (g_once_init_enter(&initialized)) {
      type_id = g_boxed_type_register_static("MongoBson",
         (GBoxedCopyFunc)mongo_bson_ref,
         (GBoxedFreeFunc)mongo_bson_unref);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}

/**
 * mongo_bson_type_get_type:
 *
 * Fetches the #GType for a #MongoBsonType.
 *
 * Returns: A #GType.
 */
GType
mongo_bson_type_get_type (void)
{
   static GType type_id;
   static gsize initialized;
   static const GEnumValue values[] = {
      { MONGO_BSON_DOUBLE,    "MONGO_BSON_DOUBLE",    "DOUBLE" },
      { MONGO_BSON_UTF8,      "MONGO_BSON_UTF8",      "UTF8" },
      { MONGO_BSON_DOCUMENT,  "MONGO_BSON_DOCUMENT",  "DOCUMENT" },
      { MONGO_BSON_ARRAY,     "MONGO_BSON_ARRAY",     "ARRAY" },
      { MONGO_BSON_UNDEFINED, "MONGO_BSON_UNDEFINED", "UNDEFINED" },
      { MONGO_BSON_OBJECT_ID, "MONGO_BSON_OBJECT_ID", "OBJECT_ID" },
      { MONGO_BSON_BOOLEAN,   "MONGO_BSON_BOOLEAN",   "BOOLEAN" },
      { MONGO_BSON_DATE_TIME, "MONGO_BSON_DATE_TIME", "DATE_TIME" },
      { MONGO_BSON_NULL,      "MONGO_BSON_NULL",      "NULL" },
      { MONGO_BSON_REGEX,     "MONGO_BSON_REGEX",     "REGEX" },
      { MONGO_BSON_INT32,     "MONGO_BSON_INT32",     "INT32" },
      { MONGO_BSON_INT64,     "MONGO_BSON_INT64",     "INT64" },
      { 0 }
   };

   if (g_once_init_enter(&initialized)) {
      type_id = g_enum_register_static("MongoBsonType", values);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}

/**
 * mongo_bson_append:
 * @bson: (in): A #MongoBson.
 * @type: (in) (type MongoBsonType): A #MongoBsonType.
 * @key: (in): The key for the field to append.
 * @data1: (in): The data for the first chunk of the data.
 * @len1: (in): The length of @data1.
 * @data2: (in): The data for the second chunk of the data.
 * @len2: (in): The length of @data2.
 *
 * This utility function helps us build a buffer for a #MongoBson given
 * the various #MongoBsonType<!-- -->'s and two-part data sections of
 * some fields.
 *
 * If @data2 is set, @data1 must also be set.
 */
static void
mongo_bson_append (MongoBson    *bson,
                   guint8        type,
                   const gchar  *key,
                   const guint8 *data1,
                   gsize         len1,
                   const guint8 *data2,
                   gsize         len2)
{
   const guint8 trailing = 0;
   GByteArray *buf = (GByteArray *)bson;
   gint32 doc_len;

   g_return_if_fail(bson);
   g_return_if_fail(type);
   g_return_if_fail(key);
   g_return_if_fail(g_utf8_validate(key, -1, NULL));
   g_return_if_fail(data1 || !len1);
   g_return_if_fail(data2 || !len2);
   g_return_if_fail(!data2 || data1);

   /*
    * Overwrite our trailing byte with the type for this key.
    */
   buf->data[buf->len - 1] = type;

   /*
    * Append the field name as a BSON cstring.
    */
   g_byte_array_append(buf, (guint8 *)key, strlen(key) + 1);

   /*
    * Append the data sections if needed.
    */
   if (data1) {
      g_byte_array_append(buf, data1, len1);
      if (data2) {
         g_byte_array_append(buf, data2, len2);
      }
   }

   /*
    * Append our trailing byte.
    */
   g_byte_array_append(buf, &trailing, 1);

   /*
    * Update the document length of the buffer.
    */
   doc_len = GUINT32_TO_LE(buf->len);
   memcpy(buf->data, &doc_len, sizeof doc_len);
}

/**
 * mongo_bson_append_array:
 * @bson: (in): A #MongoBson.
 * @key: (in): The field name.
 * @value: (in): The #MongoBson array to append.
 *
 * Appends a #MongoBson containing an array. A #MongoBson array is a document
 * that contains fields with string keys containing integers monotonically.
 *
 * <informalexample>
 *   <programlisting>
 * {"0": "First Value", "1": "Second Value"}
 *   </programlisting>
 * </informalexample>
 */
void
mongo_bson_append_array (MongoBson       *bson,
                         const gchar     *key,
                         const MongoBson *value)
{
   g_return_if_fail(bson);
   g_return_if_fail(bson != value);
   g_return_if_fail(key);
   g_return_if_fail(value);

   mongo_bson_append(bson, MONGO_BSON_ARRAY, key,
                     value->data, value->len,
                     NULL, 0);
}

/**
 * mongo_bson_append_boolean:
 * @bson: (in): A #MongoBson.
 * @key: (in): The string containing key.
 * @value: (in): A value to store in the document.
 *
 * Stores the value specified by @value as a boolean in the document
 * under @key.
 */
void
mongo_bson_append_boolean (MongoBson   *bson,
                           const gchar *key,
                           gboolean     value)
{
   guint8 b = !!value;

   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   mongo_bson_append(bson, MONGO_BSON_BOOLEAN, key, &b, 1, NULL, 0);
}

/**
 * mongo_bson_append_bson:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in): A #MongoBson to store.
 *
 * Stores the #MongoBson in the document under @key.
 */
void
mongo_bson_append_bson (MongoBson       *bson,
                        const gchar     *key,
                        const MongoBson *value)
{
   g_return_if_fail(bson);
   g_return_if_fail(bson != value);
   g_return_if_fail(key);
   g_return_if_fail(value);

   mongo_bson_append(bson, MONGO_BSON_DOCUMENT, key,
                     value->data, value->len, NULL, 0);
}

/**
 * mongo_bson_append_date_time:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in): A #GDateTime to store.
 *
 * Appends the #GDateTime to the #MongoBson under @key.
 */
void
mongo_bson_append_date_time (MongoBson   *bson,
                             const gchar *key,
                             GDateTime   *value)
{
   GTimeVal tv;

   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);
   g_return_if_fail(value != NULL);

   if (!g_date_time_to_timeval(value, &tv)) {
      g_warning("GDateTime is outside of storable range, ignoring!");
      return;
   }

   mongo_bson_append_timeval(bson, key, &tv);
}

/**
 * mongo_bson_append_double:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in): A #gdouble.
 *
 * Appends the #gdouble @value to the document under @key.
 */
void
mongo_bson_append_double (MongoBson   *bson,
                          const gchar *key,
                          gdouble      value)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   mongo_bson_append(bson, MONGO_BSON_DOUBLE, key,
                     (const guint8 *)&value, sizeof value,
                     NULL, 0);
}

/**
 * mongo_bson_append_int:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in): A #gint32 containing the value.
 *
 * Appends @value to the document under @key.
 */
void
mongo_bson_append_int (MongoBson   *bson,
                       const gchar *key,
                       gint32       value)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   mongo_bson_append(bson, MONGO_BSON_INT32, key,
                     (const guint8 *)&value, sizeof value,
                     NULL, 0);
}

/**
 * mongo_bson_append_int64:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in): A #gint64 containing the value.
 *
 * Appends @value to the document under @key.
 */
void
mongo_bson_append_int64 (MongoBson   *bson,
                         const gchar *key,
                         gint64       value)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   mongo_bson_append(bson, MONGO_BSON_INT64, key,
                     (const guint8 *)&value, sizeof value,
                     NULL, 0);
}

/**
 * mongo_bson_append_null:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 *
 * Appends a %NULL value to the document under @key.
 */
void
mongo_bson_append_null (MongoBson   *bson,
                        const gchar *key)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   mongo_bson_append(bson, MONGO_BSON_NULL, key, NULL, 0, NULL, 0);
}

/**
 * mongo_bson_append_object_id:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @object_id: (in): A #MongoObjectId.
 *
 * Appends @object_id to the document under @key.
 */
void
mongo_bson_append_object_id (MongoBson           *bson,
                             const gchar         *key,
                             const MongoObjectId *object_id)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);
   g_return_if_fail(object_id != NULL);

   mongo_bson_append(bson, MONGO_BSON_OBJECT_ID, key,
                     (const guint8 *)object_id, 12,
                     NULL, 0);
}

/**
 * mongo_bson_append_regex:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @regex: (in): A string containing a regex.
 * @options: (in): Options for the regex.
 *
 * Appends a regex to the document under @key.
 */
void
mongo_bson_append_regex (MongoBson   *bson,
                         const gchar *key,
                         const gchar *regex,
                         const gchar *options)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);
   g_return_if_fail(regex != NULL);

   if (!options) {
      options = "";
   }

   mongo_bson_append(bson, MONGO_BSON_REGEX, key,
                     (const guint8 *)regex, strlen(regex) + 1,
                     (const guint8 *)options, strlen(options) + 1);
}

/**
 * mongo_bson_append_string:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in) (allow-none): A string containing the value.
 *
 * Stores the string @value in the document under @key. If @value is
 * %NULL, then a %MONGO_BSON_NULL field will be added instead of a string.
 */
void
mongo_bson_append_string (MongoBson   *bson,
                          const gchar *key,
                          const gchar *value)
{
   guint32 value_len;
   guint32 value_len_swab;

   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   if (!value) {
      mongo_bson_append_null(bson, key);
      return;
   }

   g_return_if_fail(g_utf8_validate(value, -1, NULL));

   value = value ? value : "";
   value_len = strlen(value) + 1;
   value_len_swab = GUINT32_TO_LE(value_len);

   mongo_bson_append(bson, MONGO_BSON_UTF8, key,
                     (const guint8 *)&value_len_swab, sizeof value_len_swab,
                     (const guint8 *)value, value_len);
}

/**
 * mongo_bson_append_timeval:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 * @value: (in): A #GTimeVal containing the date and time.
 *
 * Appends the date and time represented by @value up to milliseconds.
 * The value is stored in the document under @key.
 *
 * See also: mongo_bson_append_date_time().
 */
void
mongo_bson_append_timeval (MongoBson   *bson,
                           const gchar *key,
                           GTimeVal    *value)
{
   guint64 msec;

   g_return_if_fail(bson);
   g_return_if_fail(key);
   g_return_if_fail(value);

   msec = ((guint64)value->tv_sec * 1000UL) +
          ((guint64)value->tv_usec / 1000UL);
   mongo_bson_append(bson, MONGO_BSON_DATE_TIME, key,
                     (const guint8 *)&msec, sizeof msec,
                     NULL, 0);
}

/**
 * mongo_bson_append_undefined:
 * @bson: (in): A #MongoBson.
 * @key: (in): A string containing the key.
 *
 * Appends a javascript "undefined" value in the document under @key.
 */
void
mongo_bson_append_undefined (MongoBson   *bson,
                             const gchar *key)
{
   g_return_if_fail(bson != NULL);
   g_return_if_fail(key != NULL);

   mongo_bson_append(bson, MONGO_BSON_UNDEFINED, key,
                     NULL, 0, NULL, 0);
}

/**
 * mongo_bson_iter_init:
 * @iter: an uninitialized #MongoBsonIter.
 * @bson: a #MongoBson.
 *
 * Initializes a #MongoBsonIter for iterating through a #MongoBson document.
 */
void
mongo_bson_iter_init (MongoBsonIter   *iter,
                      const MongoBson *bson)
{
   g_return_if_fail(iter != NULL);
   g_return_if_fail(bson != NULL);

   memset(iter, 0, sizeof *iter);
   iter->user_data1 = bson->data;
   iter->user_data2 = GINT_TO_POINTER(bson->len);
   iter->user_data3 = GINT_TO_POINTER(3); /* End of size buffer */
}

/**
 * mongo_bson_iter_init_find:
 * @iter: an uninitialized #MongoBsonIter.
 * @bson: (in): A #MongoBson.
 * @key: (in): The key to find.
 *
 * Convenience function to initialize a #MongoBsonIter and find the key
 * matching @key. If @key was found, %TRUE is returned, otherwise %FALSE.
 *
 * Returns: %TRUE if @key was found.
 */
gboolean
mongo_bson_iter_init_find (MongoBsonIter   *iter,
                           const MongoBson *bson,
                           const gchar     *key)
{
   mongo_bson_iter_init(iter, bson);
   return mongo_bson_iter_find(iter, key);
}

/**
 * mongo_bson_iter_find:
 * @iter: (in): A #MongoBsonIter.
 * @key: (in): A key to find in the BSON document.
 *
 * Iterates through all upcoming keys in a #MongoBsonIter until @key is
 * found or the end of the document has been reached.
 *
 * Returns: %TRUE if @key was found, otherwise %FALSE.
 */
gboolean
mongo_bson_iter_find (MongoBsonIter *iter,
                      const gchar   *key)
{
   g_return_val_if_fail(iter != NULL, FALSE);
   g_return_val_if_fail(key != NULL, FALSE);

   while (mongo_bson_iter_next(iter)){
      if (!g_strcmp0(key, mongo_bson_iter_get_key(iter))) {
         return TRUE;
      }
   }

   return FALSE;
}

/**
 * mongo_bson_iter_get_key:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the key currently pointed to by @iter.
 *
 * Returns: A string containing the key.
 */
const gchar *
mongo_bson_iter_get_key (MongoBsonIter *iter)
{
   g_return_val_if_fail(iter != NULL, NULL);
   return (const gchar *)iter->user_data4;
}

static MongoBson *
mongo_bson_iter_get_value_document (MongoBsonIter *iter,
                                    MongoBsonType  type)
{
   const guint8 *buffer;
   MongoBson *ret;
   gpointer endbuf;
   guint32 array_len;

   ENTRY;

   g_return_val_if_fail(iter != NULL, NULL);
   g_return_val_if_fail(iter->user_data1 != NULL, NULL);
   g_return_val_if_fail(iter->user_data2 != NULL, NULL);
   g_return_val_if_fail(iter->user_data6 != NULL, NULL);
   g_return_val_if_fail((type == MONGO_BSON_ARRAY) ||
                        (type == MONGO_BSON_DOCUMENT), NULL);

   if (G_LIKELY(ITER_IS_TYPE(iter, type))) {
      endbuf = GSIZE_TO_POINTER(GPOINTER_TO_SIZE(iter->user_data1) +
                                GPOINTER_TO_SIZE(iter->user_data2));
      if ((iter->user_data6 + 5) > endbuf) {
         return NULL;
      }
      memcpy(&array_len, iter->user_data6, sizeof array_len);
      array_len = GINT_FROM_LE(array_len);
      if ((iter->user_data6 + array_len) > endbuf) {
         return NULL;
      }
      buffer = iter->user_data6;
      ret = mongo_bson_new_from_data(buffer, array_len);
      RETURN(ret);
   }

   if (type == MONGO_BSON_ARRAY) {
      g_warning("Current key is not an array.");
   } else if (type == MONGO_BSON_DOCUMENT) {
      g_warning("Current key is not a document.");
   } else {
      g_assert_not_reached();
   }

   RETURN(NULL);
}

/**
 * mongo_bson_iter_get_value_array:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the array document current pointed to by @iter. This makes a
 * copy of the document and returns a new #MongoBson instance. For more
 * optimized cases, you may want to use mongo_bson_iter_recurse() to avoid
 * copying the memory if only iteration is needed.
 *
 * Returns: (transfer full): A #MongoBson.
 */
MongoBson *
mongo_bson_iter_get_value_array (MongoBsonIter *iter)
{
   MongoBson *ret;

   ENTRY;
   ret = mongo_bson_iter_get_value_document(iter, MONGO_BSON_ARRAY);
   RETURN(ret);
}

/**
 * mongo_bson_iter_get_value_boolean:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the current value pointed to by the iterator, expecting it to
 * be a boolean.
 *
 * Returns: The current value.
 */
gboolean
mongo_bson_iter_get_value_boolean (MongoBsonIter *iter)
{
   guint8 b;

   ENTRY;

   g_return_val_if_fail(iter, 0);
   g_return_val_if_fail(iter->user_data6, 0);

   if (ITER_IS_TYPE(iter, MONGO_BSON_BOOLEAN)) {
      memcpy(&b, iter->user_data6, sizeof b);
      RETURN(!!b);
   } else if (ITER_IS_TYPE(iter, MONGO_BSON_INT32)) {
      b = !!mongo_bson_iter_get_value_int(iter);
      RETURN(b);
   } else if (ITER_IS_TYPE(iter, MONGO_BSON_INT64)) {
      b = !!mongo_bson_iter_get_value_int64(iter);
      RETURN(b);
   } else if (ITER_IS_TYPE(iter, MONGO_BSON_DOUBLE)) {
      b = (mongo_bson_iter_get_value_double(iter) == 1.0);
      RETURN(b);
   }

   g_warning("Current key cannot be coerced to boolean.");

   RETURN(FALSE);
}

/**
 * mongo_bson_iter_get_value_bson:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the current value pointed to by @iter if it is a
 * %MONGO_BSON_DOCUMENT. The document is copied and returned as a new
 * #MongoBson instance. If you simply need to iterate the child document,
 * you may want to use mongo_bson_iter_recurse().
 *
 * Returns: (transfer full): A #MongoBson if successful; otherwise %NULL.
 */
MongoBson *
mongo_bson_iter_get_value_bson (MongoBsonIter *iter)
{
   MongoBson *ret;

   ENTRY;
   ret = mongo_bson_iter_get_value_document(iter, MONGO_BSON_DOCUMENT);
   RETURN(ret);
}

#if GLIB_CHECK_VERSION(2, 26, 0)
/**
 * mongo_bson_iter_get_value_date_time:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches a #GDateTime for the current value pointed to by @iter.
 *
 * Returns: A new #GDateTime which should be freed with g_date_time_unref().
 */
GDateTime *
mongo_bson_iter_get_value_date_time (MongoBsonIter *iter)
{
   GTimeVal tv;

   g_return_val_if_fail(iter != NULL, NULL);

   mongo_bson_iter_get_value_timeval(iter, &tv);
   return g_date_time_new_from_timeval_utc(&tv);
}
#endif

/**
 * mongo_bson_iter_get_value_double:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the current value pointed to by @iter if it is a
 * %MONGO_BSON_DOUBLE.
 *
 * Returns: A #gdouble.
 */
gdouble
mongo_bson_iter_get_value_double (MongoBsonIter *iter)
{
   gdouble value;

   g_return_val_if_fail(iter != NULL, 0);
   g_return_val_if_fail(iter->user_data6 != NULL, 0);

   if (ITER_IS_TYPE(iter, MONGO_BSON_DOUBLE)) {
      memcpy(&value, iter->user_data6, sizeof value);
      return value;
   }

   g_warning("Current value is not a double.");

   return 0.0;
}

/**
 * mongo_bson_iter_get_value_object_id:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the current value pointed to by @iter if it is a
 * %MONGO_BSON_OBJECT_ID. The resulting #MongoObjectId should be freed
 * with mongo_object_id_free().
 *
 * Returns: (transfer full): A #MongoObjectId.
 */
MongoObjectId *
mongo_bson_iter_get_value_object_id (MongoBsonIter *iter)
{
   g_return_val_if_fail(iter != NULL, NULL);
   g_return_val_if_fail(iter->user_data6 != NULL, NULL);

   if (ITER_IS_TYPE(iter, MONGO_BSON_OBJECT_ID)) {
      return mongo_object_id_new_from_data(iter->user_data6);
   }

   g_warning("Current value is not an ObjectId.");

   return NULL;
}

/**
 * mongo_bson_iter_get_value_int:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the current value pointed to by @iter if it is a
 * %MONGO_BSON_INT32.
 *
 * Returns: A #gint32 containing the value.
 */
gint32
mongo_bson_iter_get_value_int (MongoBsonIter *iter)
{
   gint32 value;

   g_return_val_if_fail(iter != NULL, 0);
   g_return_val_if_fail(iter->user_data6 != NULL, 0);

   if (ITER_IS_TYPE(iter, MONGO_BSON_INT32)) {
      memcpy(&value, iter->user_data6, sizeof value);
      return GINT_FROM_LE(value);
   }

   g_warning("Current value is not an int32.");

   return 0;
}

/**
 * mongo_bson_iter_get_value_int64:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the current value pointed to by @iter if it is a
 * %MONGO_BSON_INT64.
 *
 * Returns: A #gint64.
 */
gint64
mongo_bson_iter_get_value_int64 (MongoBsonIter *iter)
{
   gint64 value;

   g_return_val_if_fail(iter != NULL, 0L);
   g_return_val_if_fail(iter->user_data6 != NULL, 0L);

   if (ITER_IS_TYPE(iter, MONGO_BSON_INT64)) {
      memcpy(&value, iter->user_data6, sizeof value);
      return GINT64_FROM_LE(value);
   }

   g_warning("Current value is not an int64.");

   return 0L;
}

/**
 * mongo_bson_iter_get_value_regex:
 * @iter: (in): A #MongoBsonIter.
 * @regex: (out) (allow-none) (transfer none): A location for a string containing the regex.
 * @options: (out) (allow-none) (transfer none): A location for a string containing the options.
 *
 * Fetches the current value pointed to by @iter if it is a regex. The values
 * MUST NOT be modified or freed.
 */
void
mongo_bson_iter_get_value_regex (MongoBsonIter  *iter,
                                 const gchar   **regex,
                                 const gchar   **options)
{
   g_return_if_fail(iter != NULL);

   if (ITER_IS_TYPE(iter, MONGO_BSON_REGEX)) {
      if (regex) {
         *regex = iter->user_data6;
      }
      if (options) {
         *options = iter->user_data7;
      }
      return;
   }

   g_warning("Current value is not a Regex.");
}

/**
 * mongo_bson_iter_get_value_string:
 * @iter: (in): A #MongoBsonIter.
 * @length: (out) (allow-none): The length of the resulting string.
 *
 * Fetches the current value pointed to by @iter if it is a
 * %MONGO_BSON_UTF8. If @length is not %NULL, then the length of the
 * string will be stored in the location pointed to by @length.
 *
 * Returns: A string which should not be modified or freed.
 */
const gchar *
mongo_bson_iter_get_value_string (MongoBsonIter *iter,
                                  gsize         *length)
{
   gint32 real_length;

   g_return_val_if_fail(iter != NULL, NULL);
   g_return_val_if_fail(iter->user_data6 != NULL, NULL);
   g_return_val_if_fail(iter->user_data7 != NULL, NULL);

   if (ITER_IS_TYPE(iter, MONGO_BSON_UTF8)) {
      memcpy(&real_length, iter->user_data6, sizeof real_length);
      real_length = GUINT32_FROM_LE(real_length);
      real_length = MIN(real_length, strlen(iter->user_data7));
      if (length) {
         *length = real_length;
      }
      return iter->user_data7;
   }

   g_warning("Current value is not a String");

   return NULL;
}

/**
 * mongo_bson_iter_get_value_timeval:
 * @iter: (in): A #MongoBsonIter.
 * @value: (out): A location for a #GTimeVal.
 *
 * Fetches the current value pointed to by @iter as a #GTimeVal if the type
 * is a %MONGO_BSON_DATE_TIME.
 */
void
mongo_bson_iter_get_value_timeval (MongoBsonIter *iter,
                                   GTimeVal      *value)
{
   gint64 v_int64;

   g_return_if_fail(iter != NULL);
   g_return_if_fail(value != NULL);

   if (ITER_IS_TYPE(iter, MONGO_BSON_DATE_TIME)) {
      memcpy(&v_int64, iter->user_data6, sizeof v_int64);
      v_int64 = GINT64_FROM_LE(v_int64);
      value->tv_sec = v_int64 / 1000;
      value->tv_usec = v_int64 % 1000;
      return;
   }

   g_warning("Current value is not a DateTime");
}

/**
 * mongo_bson_iter_get_value_type:
 * @iter: (in): A #MongoBsonIter.
 *
 * Fetches the #MongoBsonType currently pointed to by @iter.
 *
 * Returns: A #MongoBsonType.
 */
MongoBsonType
mongo_bson_iter_get_value_type (MongoBsonIter *iter)
{
   MongoBsonType type;

   g_return_val_if_fail(iter != NULL, 0);
   g_return_val_if_fail(iter->user_data5 != NULL, 0);

   type = GPOINTER_TO_INT(iter->user_data5);

   switch (type) {
   case MONGO_BSON_DOUBLE:
   case MONGO_BSON_UTF8:
   case MONGO_BSON_DOCUMENT:
   case MONGO_BSON_ARRAY:
   case MONGO_BSON_UNDEFINED:
   case MONGO_BSON_OBJECT_ID:
   case MONGO_BSON_BOOLEAN:
   case MONGO_BSON_DATE_TIME:
   case MONGO_BSON_NULL:
   case MONGO_BSON_REGEX:
   case MONGO_BSON_INT32:
   case MONGO_BSON_INT64:
      return type;
   default:
      g_warning("Unknown BSON type 0x%02x", type);
      return 0;
   }
}

/**
 * mongo_bson_iter_is_key:
 * @iter: (in): A #MongoBsonIter.
 * @key: (in): The key to check for.
 *
 * Checks to see if the iterator is on the given key.
 *
 * Returns: %TRUE if @iter is observing @key.
 */
gboolean
mongo_bson_iter_is_key (MongoBsonIter *iter,
                        const gchar   *key)
{
   const gchar *current_key;

   g_return_val_if_fail(iter, FALSE);
   g_return_val_if_fail(key, FALSE);

   current_key = mongo_bson_iter_get_key(iter);
   return !g_strcmp0(key, current_key);
}

/**
 * mongo_bson_iter_recurse:
 * @iter: (in): A #MongoBsonIter.
 * @child: (out): A #MongoBsonIter.
 *
 * Recurses into the child BSON document found at the key currently observed
 * by the #MongoBsonIter. The @child #MongoBsonIter is initialized.
 *
 * Returns: %TRUE if @child is initialized; otherwise %FALSE.
 */
gboolean
mongo_bson_iter_recurse (MongoBsonIter *iter,
                         MongoBsonIter *child)
{
   gint32 buflen;

   g_return_val_if_fail(iter != NULL, FALSE);
   g_return_val_if_fail(iter != NULL, FALSE);
   g_return_val_if_fail(child != NULL, FALSE);

   if (ITER_IS_TYPE(iter, MONGO_BSON_ARRAY) ||
       ITER_IS_TYPE(iter, MONGO_BSON_DOCUMENT)) {
      memset(child, 0, sizeof *child);
      memcpy(&buflen, iter->user_data6, sizeof buflen);
      child->user_data1 = iter->user_data6;
      child->user_data2 = GINT_TO_POINTER(GINT_FROM_LE(buflen));
      child->user_data3 = GINT_TO_POINTER(3); /* End of size buffer */
      return TRUE;
   }

   g_warning("Current value is not a BSON document or array.");

   return FALSE;
}

static inline guint
first_nul (const gchar *data,
           guint        max_bytes)
{
   guint i;

   for (i = 0; i < max_bytes; i++) {
      if (!data[i]) {
         return i;
      }
   }

   return 0;
}

/**
 * mongo_bson_iter_next:
 * @iter: (inout): A #MongoBsonIter.
 *
 * Moves @iter to the next field in the document. If no more fields exist,
 * then %FALSE is returned.
 *
 * Returns: %FALSE if there are no more fields or an error; otherwise %TRUE.
 */
gboolean
mongo_bson_iter_next (MongoBsonIter *iter)
{
   const guint8 *rawbuf;
   gsize rawbuf_len;
   gsize offset;
   const gchar *key;
   MongoBsonType type;
   const guint8 *value1;
   const guint8 *value2;
   const gchar *end = NULL;
   guint32 max_len;
   guint32 v32;

   ENTRY;

   g_return_val_if_fail(iter, FALSE);

   /*
    * Copy values onto stack from iter.
    */
   rawbuf = iter->user_data1;
   rawbuf_len = GPOINTER_TO_SIZE(iter->user_data2);
   offset = GPOINTER_TO_SIZE(iter->user_data3);
   key = (const gchar *)iter->user_data4;
   type = GPOINTER_TO_INT(iter->user_data5);
   value1 = (const guint8 *)iter->user_data6;
   value2 = (const guint8 *)iter->user_data7;

   /*
    * Check for end of buffer.
    */
   if ((offset + 1) >= rawbuf_len) {
      GOTO(failure);
   }

   /*
    * Get the type of the next field.
    */
   if (!(type = rawbuf[++offset])) {
      /*
       * This is the end of the iterator.
       */
      GOTO(failure);
   }

   /*
    * Get the key of the next field.
    */
   key = (const gchar *)&rawbuf[++offset];
   max_len = first_nul(key, rawbuf_len - offset - 1);
   if (!g_utf8_validate(key, max_len, &end)) {
      GOTO(failure);
   }
   offset += strlen(key) + 1;

   switch (type) {
   case MONGO_BSON_UTF8:
      if ((offset + 5) < rawbuf_len) {
         value1 = &rawbuf[offset];
         offset += 4;
         value2 = &rawbuf[offset];
         memcpy(&v32, value1, sizeof v32);
         max_len = GUINT32_FROM_LE(v32);
         if ((offset + max_len - 10) < rawbuf_len) {
            if ((end = utf8_check((const gchar *)value2, max_len - 1))) {
               /*
                * Well, we have quite the delima here. The UTF-8 string is
                * invalid, but there was definitely a key here. Consumers
                * might need to get at data after this too. So the best
                * we can do is probably set the value to as long of a valid
                * utf-8 string as we can. We will simply NULL the end of
                * the buffer at the given error offset.
                */
               *(gchar *)end = '\0';
               offset += max_len - 1;
               GOTO(success);
            }
            offset += max_len - 1;
            if (value2[max_len - 1] == '\0') {
               GOTO(success);
            }
         }
      }
      GOTO(failure);
   case MONGO_BSON_DOCUMENT:
   case MONGO_BSON_ARRAY:
      if ((offset + 5) < rawbuf_len) {
         value1 = &rawbuf[offset];
         value2 = NULL;
         memcpy(&max_len, value1, sizeof max_len);
         max_len = GUINT32_FROM_LE(max_len);
         if ((offset + max_len) <= rawbuf_len) {
            offset += max_len - 1;
            GOTO(success);
         }
      }
      GOTO(failure);
   case MONGO_BSON_NULL:
   case MONGO_BSON_UNDEFINED:
      value1 = NULL;
      value2 = NULL;
      offset--;
      GOTO(success);
   case MONGO_BSON_OBJECT_ID:
      if ((offset + 12) < rawbuf_len) {
         value1 = &rawbuf[offset];
         value2 = NULL;
         offset += 11;
         GOTO(success);
      }
      GOTO(failure);
   case MONGO_BSON_BOOLEAN:
      if ((offset + 1) < rawbuf_len) {
         value1 = &rawbuf[offset];
         value2 = NULL;
         GOTO(success);
      }
      GOTO(failure);
   case MONGO_BSON_DATE_TIME:
   case MONGO_BSON_DOUBLE:
   case MONGO_BSON_INT64:
      if ((offset + 8) < rawbuf_len) {
         value1 = &rawbuf[offset];
         value2 = NULL;
         offset += 7;
         GOTO(success);
      }
      GOTO(failure);
   case MONGO_BSON_REGEX:
      value1 = &rawbuf[offset];
      max_len = first_nul((gchar *)value1, rawbuf_len - offset - 1);
      if (!g_utf8_validate((gchar *)value1, max_len, &end)) {
         GOTO(failure);
      }
      offset += max_len + 1;
      if ((offset + 1) >= rawbuf_len) {
         GOTO(failure);
      }
      value2 = &rawbuf[offset];
      max_len = first_nul((gchar *)value2, rawbuf_len - offset - 1);
      if (!g_utf8_validate((gchar *)value2, max_len, &end)) {
         GOTO(failure);
      }
      offset += max_len + 1;
      GOTO(success);
   case MONGO_BSON_INT32:
      if ((offset + 4) < rawbuf_len) {
         value1 = &rawbuf[offset];
         value2 = NULL;
         offset += 3;
         GOTO(success);
      }
      GOTO(failure);
   default:
      g_warning("Unknown type: %d key: %s", type, key);
      GOTO(failure);
   }

success:
   iter->user_data3 = GSIZE_TO_POINTER(offset);
   iter->user_data4 = (gpointer)key;
   iter->user_data5 = GINT_TO_POINTER(type);
   iter->user_data6 = (gpointer)value1;
   iter->user_data7 = (gpointer)value2;
   RETURN(TRUE);

failure:
   memset(iter, 0, sizeof *iter);
   RETURN(FALSE);
}

/**
 * mongo_clear_bson:
 * @bson: (inout) (allow-none): A pointer to a #MongoBson or %NULL.
 *
 * If @bson is a pointer to a #MongoBson, it will be freed and zeroed.
 */
void
mongo_clear_bson (MongoBson **bson)
{
   if (bson && *bson) {
      mongo_bson_unref(*bson);
      *bson = NULL;
   }
}

/**
 * mongo_bson_to_string:
 * @bson: A #MongoBson.
 * @is_array: If the document should be generated as an array.
 *
 * Build a string representing the BSON document. Since BSON documents are
 * used for both documents and arrays, you can change the format using
 * @is_array.
 *
 * Returns: (transfer full): A string representing the BSON document.
 */
gchar *
mongo_bson_to_string (const MongoBson *bson,
                      gboolean         is_array)
{
   MongoBsonIter iter;
   MongoBsonType type;
   GString *str;
   gchar *esc;


   g_return_val_if_fail(bson, NULL);

   str = g_string_new(is_array ? "[ " : "{ ");

   mongo_bson_iter_init(&iter, bson);
   if (mongo_bson_iter_next(&iter)) {
again:
      if (!is_array) {
         esc = g_strescape(mongo_bson_iter_get_key(&iter), NULL);
         g_string_append_printf(str, "\"%s\": ", esc);
         g_free(esc);

      }
      type = mongo_bson_iter_get_value_type(&iter);
      switch (type) {
      case MONGO_BSON_DOUBLE:
         g_string_append_printf(str, "%f",
                                mongo_bson_iter_get_value_double(&iter));
         break;
      case MONGO_BSON_DATE_TIME:
         {
            GTimeVal tv = { 0 };
            gchar *dstr;

            mongo_bson_iter_get_value_timeval(&iter, &tv);
            dstr = g_time_val_to_iso8601(&tv);
            g_string_append_printf(str, "ISODate(\"%s\")", dstr);
            g_free(dstr);
         }
         break;
      case MONGO_BSON_INT32:
         g_string_append_printf(str, "NumberLong(%d)",
                                mongo_bson_iter_get_value_int(&iter));
         break;
      case MONGO_BSON_INT64:
         g_string_append_printf(str, "NumberLong(%"G_GINT64_FORMAT")",
                                mongo_bson_iter_get_value_int64(&iter));
         break;
      case MONGO_BSON_UTF8:
         esc = g_strescape(mongo_bson_iter_get_value_string(&iter, NULL), NULL);
         g_string_append_printf(str, "\"%s\"", esc);
         g_free(esc);
         break;
      case MONGO_BSON_ARRAY:
         {
            MongoBson *child;
            gchar *childstr;

            if ((child = mongo_bson_iter_get_value_array(&iter))) {
               childstr = mongo_bson_to_string(child, TRUE);
               g_string_append(str, childstr);
               mongo_bson_unref(child);
               g_free(childstr);
            }
         }
         break;
      case MONGO_BSON_DOCUMENT:
         {
            MongoBson *child;
            gchar *childstr;

            if ((child = mongo_bson_iter_get_value_bson(&iter))) {
               childstr = mongo_bson_to_string(child, FALSE);
               g_string_append(str, childstr);
               mongo_bson_unref(child);
               g_free(childstr);
            }
         }
         break;
      case MONGO_BSON_BOOLEAN:
         g_string_append_printf(str,
            mongo_bson_iter_get_value_boolean(&iter) ? "true" : "false");
         break;
      case MONGO_BSON_OBJECT_ID:
         {
            MongoObjectId *id;
            gchar *idstr;

            id = mongo_bson_iter_get_value_object_id(&iter);
            idstr = mongo_object_id_to_string(id);
            g_string_append_printf(str, "ObjectId(\"%s\")", idstr);
            mongo_object_id_free(id);
            g_free(idstr);
         }
         break;
      case MONGO_BSON_NULL:
         g_string_append(str, "null");
         break;
      case MONGO_BSON_REGEX:
         /* TODO: */
         g_assert_not_reached();
         break;
      case MONGO_BSON_UNDEFINED:
         g_string_append(str, "undefined");
         break;
      default:
         g_assert_not_reached();
      }

      if (mongo_bson_iter_next(&iter)) {
         g_string_append(str, ", ");
         goto again;
      }
   }

   g_string_append(str, is_array ? " ]" : " }");

   return g_string_free(str, FALSE);
}

/**
 * mongo_bson_join:
 * @bson: (in): A #MongoBson.
 * @other: (in): A #MongoBson.
 *
 * Appends the contents of @other to the end of @bson.
 */
void
mongo_bson_join (MongoBson       *bson,
                 const MongoBson *other)
{
   GByteArray *ar = (GByteArray *)bson;
   guint32 new_size;

   g_return_if_fail(bson);
   g_return_if_fail(other);

   if (other->len > 5) {
      g_byte_array_remove_index(ar, ar->len - 1);
      g_byte_array_append(ar, other->data + 4, other->len - 4);
   }

   new_size = GUINT32_TO_LE(ar->len);
   memcpy(bson->data, &new_size, sizeof new_size);
}
