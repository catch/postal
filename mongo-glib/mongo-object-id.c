/* mongo-object-id.c
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mongo-object-id.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

/**
 * SECTION:mongo-object-id
 * @title: MongoObjectId
 * @short_description: ObjectId structure used by BSON and Mongo.
 *
 * #MongoObjectId represents an ObjectId as used by the BSON serialization
 * format and Mongo. It is a 12-byte structure designed in a way to allow
 * client side generation of unique object identifiers. The first 4 bytes
 * contain the UNIX timestamp since January 1, 1970. The following 3 bytes
 * contain the first 3 bytes of the hostname MD5 hashed. Following that are
 * 2 bytes containing the Process ID or Thread ID. The last 3 bytes contain
 * a continually incrementing counter.
 *
 * When creating a new #MongoObjectId using mongo_object_id_new(), all of
 * this data is created for you.
 *
 * To create a #MongoObjectId from an existing 12 bytes, use
 * mongo_object_id_new_from_data().
 *
 * To free an allocated #MongoObjectId, use mongo_object_id_free().
 */

struct _MongoObjectId
{
   guint8 data[12];
};

static guint8  gMachineId[3];
static gushort gPid;
static gint32  gIncrement;
static const guint16 gHexTable[] = {
#if G_BYTE_ORDER == G_BIG_ENDIAN
   12336, 12337, 12338, 12339, 12340, 12341, 12342, 12343, 12344, 12345,
   12385, 12386, 12387, 12388, 12389, 12390, 12592, 12593, 12594, 12595,
   12596, 12597, 12598, 12599, 12600, 12601, 12641, 12642, 12643, 12644,
   12645, 12646, 12848, 12849, 12850, 12851, 12852, 12853, 12854, 12855,
   12856, 12857, 12897, 12898, 12899, 12900, 12901, 12902, 13104, 13105,
   13106, 13107, 13108, 13109, 13110, 13111, 13112, 13113, 13153, 13154,
   13155, 13156, 13157, 13158, 13360, 13361, 13362, 13363, 13364, 13365,
   13366, 13367, 13368, 13369, 13409, 13410, 13411, 13412, 13413, 13414,
   13616, 13617, 13618, 13619, 13620, 13621, 13622, 13623, 13624, 13625,
   13665, 13666, 13667, 13668, 13669, 13670, 13872, 13873, 13874, 13875,
   13876, 13877, 13878, 13879, 13880, 13881, 13921, 13922, 13923, 13924,
   13925, 13926, 14128, 14129, 14130, 14131, 14132, 14133, 14134, 14135,
   14136, 14137, 14177, 14178, 14179, 14180, 14181, 14182, 14384, 14385,
   14386, 14387, 14388, 14389, 14390, 14391, 14392, 14393, 14433, 14434,
   14435, 14436, 14437, 14438, 14640, 14641, 14642, 14643, 14644, 14645,
   14646, 14647, 14648, 14649, 14689, 14690, 14691, 14692, 14693, 14694,
   24880, 24881, 24882, 24883, 24884, 24885, 24886, 24887, 24888, 24889,
   24929, 24930, 24931, 24932, 24933, 24934, 25136, 25137, 25138, 25139,
   25140, 25141, 25142, 25143, 25144, 25145, 25185, 25186, 25187, 25188,
   25189, 25190, 25392, 25393, 25394, 25395, 25396, 25397, 25398, 25399,
   25400, 25401, 25441, 25442, 25443, 25444, 25445, 25446, 25648, 25649,
   25650, 25651, 25652, 25653, 25654, 25655, 25656, 25657, 25697, 25698,
   25699, 25700, 25701, 25702, 25904, 25905, 25906, 25907, 25908, 25909,
   25910, 25911, 25912, 25913, 25953, 25954, 25955, 25956, 25957, 25958,
   26160, 26161, 26162, 26163, 26164, 26165, 26166, 26167, 26168, 26169,
   26209, 26210, 26211, 26212, 26213, 26214
#else
   12336, 12592, 12848, 13104, 13360, 13616, 13872, 14128, 14384, 14640,
   24880, 25136, 25392, 25648, 25904, 26160, 12337, 12593, 12849, 13105,
   13361, 13617, 13873, 14129, 14385, 14641, 24881, 25137, 25393, 25649,
   25905, 26161, 12338, 12594, 12850, 13106, 13362, 13618, 13874, 14130,
   14386, 14642, 24882, 25138, 25394, 25650, 25906, 26162, 12339, 12595,
   12851, 13107, 13363, 13619, 13875, 14131, 14387, 14643, 24883, 25139,
   25395, 25651, 25907, 26163, 12340, 12596, 12852, 13108, 13364, 13620,
   13876, 14132, 14388, 14644, 24884, 25140, 25396, 25652, 25908, 26164,
   12341, 12597, 12853, 13109, 13365, 13621, 13877, 14133, 14389, 14645,
   24885, 25141, 25397, 25653, 25909, 26165, 12342, 12598, 12854, 13110,
   13366, 13622, 13878, 14134, 14390, 14646, 24886, 25142, 25398, 25654,
   25910, 26166, 12343, 12599, 12855, 13111, 13367, 13623, 13879, 14135,
   14391, 14647, 24887, 25143, 25399, 25655, 25911, 26167, 12344, 12600,
   12856, 13112, 13368, 13624, 13880, 14136, 14392, 14648, 24888, 25144,
   25400, 25656, 25912, 26168, 12345, 12601, 12857, 13113, 13369, 13625,
   13881, 14137, 14393, 14649, 24889, 25145, 25401, 25657, 25913, 26169,
   12385, 12641, 12897, 13153, 13409, 13665, 13921, 14177, 14433, 14689,
   24929, 25185, 25441, 25697, 25953, 26209, 12386, 12642, 12898, 13154,
   13410, 13666, 13922, 14178, 14434, 14690, 24930, 25186, 25442, 25698,
   25954, 26210, 12387, 12643, 12899, 13155, 13411, 13667, 13923, 14179,
   14435, 14691, 24931, 25187, 25443, 25699, 25955, 26211, 12388, 12644,
   12900, 13156, 13412, 13668, 13924, 14180, 14436, 14692, 24932, 25188,
   25444, 25700, 25956, 26212, 12389, 12645, 12901, 13157, 13413, 13669,
   13925, 14181, 14437, 14693, 24933, 25189, 25445, 25701, 25957, 26213,
   12390, 12646, 12902, 13158, 13414, 13670, 13926, 14182, 14438, 14694,
   24934, 25190, 25446, 25702, 25958, 26214
#endif
};

static void
mongo_object_id_init (void)
{
   gchar hostname[HOST_NAME_MAX] = { 0 };
   char *md5;
   int ret;

   if (0 != (ret = gethostname(hostname, sizeof hostname - 1))) {
      g_error("Failed to get hostname, cannot generate MongoObjectId");
   }

   md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, hostname,
                                       sizeof hostname);
   memcpy(gMachineId, md5, sizeof gMachineId);
   g_free(md5);

   gPid = (gushort)getpid();
}

/**
 * mongo_object_id_new:
 *
 * Create a new #MongoObjectId. The timestamp, PID, hostname, and counter
 * fields of the #MongoObjectId will be generated.
 *
 * Returns: (transfer full): A #MongoObjectId that should be freed with
 * mongo_object_id_free().
 */
MongoObjectId *
mongo_object_id_new (void)
{
   static gsize initialized = FALSE;
   GTimeVal val = { 0 };
   guint32 t;
   guint8 bytes[12];
   gint32 inc;

   if (g_once_init_enter(&initialized)) {
      mongo_object_id_init();
      g_once_init_leave(&initialized, TRUE);
   }

   g_get_current_time(&val);
   t = GUINT32_TO_BE(val.tv_sec);
   inc = GUINT32_TO_BE(g_atomic_int_add(&gIncrement, 1));

   memcpy(&bytes[0], &t, sizeof t);
   memcpy(&bytes[4], &gMachineId, sizeof gMachineId);
   memcpy(&bytes[7], &gPid, sizeof gPid);
   bytes[9] = ((guint8 *)&inc)[1];
   bytes[10] = ((guint8 *)&inc)[2];
   bytes[11] = ((guint8 *)&inc)[3];

   return mongo_object_id_new_from_data(bytes);
}

/**
 * mongo_object_id_new_from_data:
 * @bytes: (array): The bytes containing the object id.
 *
 * Creates a new #MongoObjectId from an array of 12 bytes. @bytes
 * MUST contain 12-bytes.
 *
 * Returns: (transfer full): A newly allocated #MongoObjectId that should
 * be freed with mongo_object_id_free().
 */
MongoObjectId *
mongo_object_id_new_from_data (const guint8 *bytes)
{
   MongoObjectId *object_id;

   object_id = g_slice_new0(MongoObjectId);

   if (bytes) {
      memcpy(object_id, bytes, sizeof *object_id);
   }

   return object_id;
}

/**
 * mongo_object_id_new_from_string:
 * @string: A 24-character string containing the object id.
 *
 * Creates a new #MongoObjectId from a 24-character, hex-encoded, string.
 *
 * Returns: A newly created #MongoObjectId if successful; otherwise %NULL.
 */
MongoObjectId *
mongo_object_id_new_from_string (const gchar *string)
{
   guint32 v;
   guint8 data[12] = { 0 };
   guint i;

   g_return_val_if_fail(string, NULL);

   if (strlen(string) != 24) {
      return NULL;
   }

   for (i = 0; i < 12; i++) {
      sscanf(&string[i * 2], "%02x", &v);
      data[i] = v;
   }

   return mongo_object_id_new_from_data(data);
}

/**
 * mongo_object_id_to_string_r: (skip)
 * @object_id: (in): A #MongoObjectId.
 * @string: (out): A location for the resulting string bytes.
 *
 * Converts @object_id into a hex string and stores it in @string.
 */
void
mongo_object_id_to_string_r (const MongoObjectId *object_id,
                             gchar                string[25])
{
   guint16 *dst;
   guint8 *id = (guint8 *)object_id;

   g_return_if_fail(object_id);
   g_return_if_fail(string);

   dst = (guint16 *)(gpointer)string;

   dst[0] = gHexTable[id[0]];
   dst[1] = gHexTable[id[1]];
   dst[2] = gHexTable[id[2]];
   dst[3] = gHexTable[id[3]];
   dst[4] = gHexTable[id[4]];
   dst[5] = gHexTable[id[5]];
   dst[6] = gHexTable[id[6]];
   dst[7] = gHexTable[id[7]];
   dst[8] = gHexTable[id[8]];
   dst[9] = gHexTable[id[9]];
   dst[10] = gHexTable[id[10]];
   dst[11] = gHexTable[id[11]];
   string[24] = '\0';
}

/**
 * mongo_object_id_to_string:
 * @object_id: (in): A #MongoObjectId.
 *
 * Converts @object_id into a hex string.
 *
 * Returns: (transfer full): The ObjectId as a string.
 */
gchar *
mongo_object_id_to_string (const MongoObjectId *object_id)
{
   guint16 *dst;
   guint8 *id = (guint8 *)object_id;
   gchar *str;

   g_return_val_if_fail(object_id, NULL);

   str = g_new(gchar, 25);
   dst = (guint16 *)(gpointer)str;

   dst[0] = gHexTable[id[0]];
   dst[1] = gHexTable[id[1]];
   dst[2] = gHexTable[id[2]];
   dst[3] = gHexTable[id[3]];
   dst[4] = gHexTable[id[4]];
   dst[5] = gHexTable[id[5]];
   dst[6] = gHexTable[id[6]];
   dst[7] = gHexTable[id[7]];
   dst[8] = gHexTable[id[8]];
   dst[9] = gHexTable[id[9]];
   dst[10] = gHexTable[id[10]];
   dst[11] = gHexTable[id[11]];
   str[24] = '\0';

   return str;
}

/**
 * mongo_object_id_copy:
 * @object_id: (in): A #MongoObjectId.
 *
 * Creates a new #MongoObjectId that is a copy of @object_id.
 *
 * Returns: (transfer full): A #MongoObjectId that should be freed with
 * mongo_object_id_free().
 */
MongoObjectId *
mongo_object_id_copy (const MongoObjectId *object_id)
{
   MongoObjectId *copy = NULL;

   if (object_id) {
      copy = g_slice_new(MongoObjectId);
      memcpy(copy, object_id, sizeof *object_id);
   }

   return copy;
}

/**
 * mongo_object_id_compare:
 * @object_id: (in): The first #MongoObjectId.
 * @other: (in): The second #MongoObjectId.
 *
 * A qsort() style compare function that will return less than zero
 * if @object_id is less than @other, zero if they are the same, and
 * greater than one if @other is greater than @object_id.
 *
 * Returns: A qsort() style compare integer.
 */
gint
mongo_object_id_compare (const MongoObjectId *object_id,
                         const MongoObjectId *other)
{
   return memcmp(object_id, other, sizeof object_id->data);
}

/**
 * mongo_object_id_equal:
 * @v1: (in) (type MongoObjectId*): A #MongoObjectId.
 * @v2: (in) (type MongoObjectId*): A #MongoObjectId.
 *
 * Checks if @v1 and @v2 contain the same object id.
 *
 * Returns: %TRUE if @v1 and @v2 are equal.
 */
gboolean
mongo_object_id_equal (gconstpointer v1,
                       gconstpointer v2)
{
   return !mongo_object_id_compare(v1, v2);
}

/**
 * mongo_object_id_hash:
 * @v: (in) (type MongoObjectId*): A #MongoObjectId.
 *
 * Hashes the bytes of the provided #MongoObjectId using DJB hash.
 * This is suitable for using as a hash function for #GHashTable.
 *
 * Returns: A hash value corresponding to the key.
 */
guint
mongo_object_id_hash (gconstpointer v)
{
   const MongoObjectId *object_id = v;
   guint hash = 5381;
   guint i;

   g_return_val_if_fail(object_id, 5381);

   for (i = 0; i < G_N_ELEMENTS(object_id->data); i++) {
      hash = ((hash << 5) + hash) + object_id->data[i];
   }

   return hash;
}

/**
 * mongo_object_id_get_data:
 * @object_id: (in): A #MongoObjectId.
 * @length: (out) (allow-none): Then number of bytes returned.
 *
 * Gets the raw bytes for the object id. The length of the bytes is
 * returned in the out paramter @length for language bindings and
 * is always 12.
 *
 * Returns: (transfer none) (array length=length): The object id bytes.
 */
const guint8 *
mongo_object_id_get_data (const MongoObjectId *object_id,
                           gsize              *length)
{
   g_return_val_if_fail(object_id, NULL);
   if (length)
      *length = sizeof object_id->data;
   return object_id->data;
}

/**
 * mongo_object_id_get_timeval:
 * @object_id: (in): A #MongoObjectId.
 * @tv: (out): A location for a #GTimeVal.
 *
 * Gets the timestamp portion of @object_id and stores it in @tv.
 */
void
mongo_object_id_get_timeval (const MongoObjectId *object_id,
                             GTimeVal            *tv)
{
   guint32 t;

   g_return_if_fail(object_id);
   g_return_if_fail(tv);

   memcpy(&t, object_id, sizeof t);
   tv->tv_sec = GUINT32_FROM_BE(t);
   tv->tv_usec = 0;
}

/**
 * mongo_object_id_free:
 * @object_id: (in): A #MongoObjectId.
 *
 * Frees a #MongoObjectId.
 */
void
mongo_object_id_free (MongoObjectId *object_id)
{
   if (object_id) {
      g_slice_free(MongoObjectId, object_id);
   }
}

/**
 * mongo_clear_object_id:
 * @object_id: (out): A pointer to a #MongoObjectId.
 *
 * Clears the pointer to a #MongoObjectId by freeing the #MongoObjectId
 * and then setting the pointer to %NULL. If no #MongoObjectId was found,
 * then no operation is performed.
 */
void
mongo_clear_object_id (MongoObjectId **object_id)
{
   if (object_id && *object_id) {
      mongo_object_id_free(*object_id);
      *object_id = NULL;
   }
}

/**
 * mongo_object_id_get_type:
 *
 * Fetches the boxed #GType for #MongoObjectId.
 *
 * Returns: A #GType.
 */
GType
mongo_object_id_get_type (void)
{
   static GType type_id = 0;
   static gsize initialized = FALSE;

   if (g_once_init_enter(&initialized)) {
      type_id = g_boxed_type_register_static(
         "MongoObjectId",
         (GBoxedCopyFunc)mongo_object_id_copy,
         (GBoxedFreeFunc)mongo_object_id_free);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}
