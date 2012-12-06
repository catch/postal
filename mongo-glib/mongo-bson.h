/* mongo-bson.h
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

#if !defined (MONGO_INSIDE) && !defined (MONGO_COMPILATION)
#error "Only <mongo-glib/mongo-glib.h> can be included directly."
#endif

#ifndef MONGO_BSON_H
#define MONGO_BSON_H

#include <glib-object.h>

#include "mongo-object-id.h"

G_BEGIN_DECLS

#define MONGO_TYPE_BSON      (mongo_bson_get_type())
#define MONGO_TYPE_BSON_TYPE (mongo_bson_type_get_type())

/**
 * MONGO_BSON_ITER_HOLDS:
 * @b: A #MongoBsonIter.
 * @t: A #MongoBsonType.
 *
 * Checks to see if @b is pointing at a field of type @t. This is
 * syntacticaly the same as checking the fields value type with
 * mongo_bson_iter_get_value_type().
 *
 * Returns: %TRUE if @b holds the requested type.
 */
#define MONGO_BSON_ITER_HOLDS(b,t) \
   (mongo_bson_iter_get_value_type((b)) == t)

/**
 * MONGO_BSON_ITER_HOLDS_DOUBLE:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_DOUBLE.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_DOUBLE(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_DOUBLE))

/**
 * MONGO_BSON_ITER_HOLDS_UTF8:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_UTF8.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_UTF8(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_UTF8))

/**
 * MONGO_BSON_ITER_HOLDS_DOCUMENT:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_DOCUMENT.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_DOCUMENT(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_DOCUMENT))

/**
 * MONGO_BSON_ITER_HOLDS_ARRAY:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_ARRAY.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_ARRAY(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_ARRAY))

/**
 * MONGO_BSON_ITER_HOLDS_UNDEFINED:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_UNDEFINED.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_UNDEFINED(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_UNDEFINED))

/**
 * MONGO_BSON_ITER_HOLDS_OBJECT_ID:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_OBJECT_ID.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_OBJECT_ID(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_OBJECT_ID))

/**
 * MONGO_BSON_ITER_HOLDS_BOOLEAN:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_BOOLEAN.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_BOOLEAN(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_BOOLEAN))

/**
 * MONGO_BSON_ITER_HOLDS_DATE_TIME:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_DATE_TIME.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_DATE_TIME(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_DATE_TIME))

/**
 * MONGO_BSON_ITER_HOLDS_NULL:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_NULL.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_NULL(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_NULL))

/**
 * MONGO_BSON_ITER_HOLDS_REGEX:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_REGEX.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_REGEX(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_REGEX))

/**
 * MONGO_BSON_ITER_HOLDS_INT32:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_INT32.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_INT32(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_INT32))

/**
 * MONGO_BSON_ITER_HOLDS_INT64:
 * @b: A #MongoBsonIter.
 *
 * Checks to see if @b is pointing at a field of type %MONGO_BSON_INT64.
 *
 * Returns: %TRUE if the type matches.
 */
#define MONGO_BSON_ITER_HOLDS_INT64(b) \
   (MONGO_BSON_ITER_HOLDS(b, MONGO_BSON_INT64))

/**
 * MongoBson:
 * @data (array length=len): The raw bson buffer.
 * @len: The length of @data.
 *
 * #MongoBson represents a document serialized using the BSON format. More
 * information on BSON can be found at http://bsonspec.org .
 *
 * You can iterate through the fields in a #MongoBson using #MongoBsonIter.
 */
typedef struct _MongoBson MongoBson;

/**
 * MongoBsonIter:
 * @user_data1: A pointer to the raw data.
 * @user_data2: The length of @user_data1.
 * @user_data3: Current offset in the buffer.
 * @user_data4: Pointer to current key.
 * @user_data5: Pointer to current type.
 * @user_data6: Pointer to first value for mutli-value fields.
 * @user_data7: Pointer to second value for multi-value fields.
 * @flags: flags used while parsing data.
 * @reserved1: reserved for future use.
 *
 * #MongoBsonIter is used to iterate through the contents of a #MongoBson.
 * It is meant to be used on the stack and can allow for reading data
 * directly out of the #MongoBson without having to malloc a copy. This
 * can be handy when dealing with lots of medium to large sized documents.
 */
typedef struct _MongoBsonIter MongoBsonIter;

/**
 * MongoBsonType:
 * @MONGO_BSON_DOUBLE: Field contains a #gdouble.
 * @MONGO_BSON_UTF8: Field contains a UTF-8 string.
 * @MONGO_BSON_DOCUMENT: Field contains a BSON document.
 * @MONGO_BSON_ARRAY: Field contains a BSON array.
 * @MONGO_BSON_UNDEFINED: Field is JavaScript undefined.
 * @MONGO_BSON_OBJECT_ID: Field contains a #MongoObjectId.
 * @MONGO_BSON_BOOLEAN: Field contains a #gboolean.
 * @MONGO_BSON_DATE_TIME: Field contains a #GDateTime.
 * @MONGO_BSON_NULL: Field contains %NULL.
 * @MONGO_BSON_REGEX: Field contains a #GRegex.
 * @MONGO_BSON_INT32: Field contains a #gint32.
 * @MONGO_BSON_INT64: Field contains a #gint64.
 *
 * These enumerations specify the field type within a #MongoBson.
 * The field type can be retrieved with mongo_bson_iter_get_value_type().
 */
typedef enum
{
   MONGO_BSON_DOUBLE    = 0x01,
   MONGO_BSON_UTF8      = 0x02,
   MONGO_BSON_DOCUMENT  = 0x03,
   MONGO_BSON_ARRAY     = 0x04,
   MONGO_BSON_UNDEFINED = 0x06,
   MONGO_BSON_OBJECT_ID = 0x07,
   MONGO_BSON_BOOLEAN   = 0x08,
   MONGO_BSON_DATE_TIME = 0x09,
   MONGO_BSON_NULL      = 0x0A,
   MONGO_BSON_REGEX     = 0x0B,
   MONGO_BSON_INT32     = 0x10,
   MONGO_BSON_INT64     = 0x12,
} MongoBsonType;

struct _MongoBson
{
   guint8 *data;
   guint   len;
};

struct _MongoBsonIter
{
   /*< private >*/
   gpointer user_data1; /* Raw data buffer */
   gpointer user_data2; /* Raw buffer length */
   gpointer user_data3; /* Offset */
   gpointer user_data4; /* Key */
   gpointer user_data5; /* Type */
   gpointer user_data6; /* Value1 */
   gpointer user_data7; /* Value2 */
   gint32   flags;
   gint32   reserved1;
};

GType          mongo_bson_get_type                 (void) G_GNUC_CONST;
GType          mongo_bson_type_get_type            (void) G_GNUC_CONST;
MongoBson     *mongo_bson_new                      (void);
MongoBson     *mongo_bson_new_empty                (void);
MongoBson     *mongo_bson_new_from_data            (const guint8    *buffer,
                                                    gsize            length);
MongoBson     *mongo_bson_new_take_data            (guint8          *buffer,
                                                    gsize            length);
MongoBson     *mongo_bson_dup                      (const MongoBson *bson);
MongoBson     *mongo_bson_ref                      (MongoBson       *bson);
void           mongo_bson_unref                    (MongoBson       *bson);
void           mongo_bson_append_array             (MongoBson       *bson,
                                                    const gchar     *key,
                                                    const MongoBson *value);
void           mongo_bson_append_boolean           (MongoBson       *bson,
                                                    const gchar     *key,
                                                    gboolean        value);
void           mongo_bson_append_bson              (MongoBson       *bson,
                                                    const gchar     *key,
                                                    const MongoBson *value);
void           mongo_bson_append_date_time         (MongoBson       *bson,
                                                    const gchar     *key,
                                                    GDateTime       *value);
void           mongo_bson_append_double            (MongoBson       *bson,
                                                    const gchar     *key,
                                                    gdouble          value);
void           mongo_bson_append_int               (MongoBson       *bson,
                                                    const gchar     *key,
                                                    gint32           value);
void           mongo_bson_append_int64             (MongoBson       *bson,
                                                    const gchar     *key,
                                                    gint64           value);
void           mongo_bson_append_null              (MongoBson       *bson,
                                                    const gchar     *key);
void           mongo_bson_append_object_id         (MongoBson           *bson,
                                                    const gchar         *key,
                                                    const MongoObjectId *object_id);
void           mongo_bson_append_regex             (MongoBson       *bson,
                                                    const gchar     *key,
                                                    const gchar     *regex,
                                                    const gchar     *options);
void           mongo_bson_append_string            (MongoBson       *bson,
                                                    const gchar     *key,
                                                    const gchar     *value);
void           mongo_bson_append_timeval           (MongoBson       *bson,
                                                    const gchar     *key,
                                                    GTimeVal        *value);
void           mongo_bson_append_undefined         (MongoBson       *bson,
                                                    const gchar     *key);
gboolean       mongo_bson_get_empty                (MongoBson       *bson);
void           mongo_bson_join                     (MongoBson       *bson,
                                                    const MongoBson *other);
void           mongo_bson_iter_init                (MongoBsonIter   *iter,
                                                    const MongoBson *bson);
gboolean       mongo_bson_iter_init_find           (MongoBsonIter   *iter,
                                                    const MongoBson *bson,
                                                    const gchar     *key);
gboolean       mongo_bson_iter_find                (MongoBsonIter   *iter,
                                                    const gchar     *key);
const gchar   *mongo_bson_iter_get_key             (MongoBsonIter   *iter);
MongoBson     *mongo_bson_iter_get_value_array     (MongoBsonIter   *iter);
gboolean       mongo_bson_iter_get_value_boolean   (MongoBsonIter   *iter);
MongoBson     *mongo_bson_iter_get_value_bson      (MongoBsonIter   *iter);
GDateTime     *mongo_bson_iter_get_value_date_time (MongoBsonIter   *iter);
gdouble        mongo_bson_iter_get_value_double    (MongoBsonIter   *iter);
MongoObjectId *mongo_bson_iter_get_value_object_id (MongoBsonIter   *iter);
gint32         mongo_bson_iter_get_value_int       (MongoBsonIter   *iter);
gint64         mongo_bson_iter_get_value_int64     (MongoBsonIter   *iter);
void           mongo_bson_iter_get_value_regex     (MongoBsonIter   *iter,
                                                    const gchar    **regex,
                                                    const gchar    **options);
const gchar   *mongo_bson_iter_get_value_string    (MongoBsonIter   *iter,
                                                    gsize           *length);
void           mongo_bson_iter_get_value_timeval   (MongoBsonIter   *iter,
                                                    GTimeVal        *value);
MongoBsonType  mongo_bson_iter_get_value_type      (MongoBsonIter   *iter);
gboolean       mongo_bson_iter_is_key              (MongoBsonIter   *iter,
                                                    const gchar     *key);
gboolean       mongo_bson_iter_next                (MongoBsonIter   *iter);
gboolean       mongo_bson_iter_recurse             (MongoBsonIter   *iter,
                                                    MongoBsonIter   *child);
gchar         *mongo_bson_to_string                (const MongoBson *bson,
                                                    gboolean         is_array);
void           mongo_clear_bson                    (MongoBson      **bson);


G_END_DECLS

#endif /* MONGO_BSON_H */

