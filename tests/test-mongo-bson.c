#include "test-helper.h"

#include <mongo-glib/mongo-glib.h>

static void
assert_bson (MongoBson   *bson,
             const gchar *name)
{
   gchar *filename;
   guint8 *buffer = NULL;
   GError *error = NULL;
   gsize length = 0;
   guint i;

   filename = g_build_filename(SRC_DIR, "tests", "bson", name, NULL);

   if (!g_file_get_contents(filename, (gchar **)&buffer, &length, &error)) {
      g_assert_no_error(error);
      g_assert(FALSE);
   }

   g_assert_cmpint(length, ==, bson->len);

   for (i = 0; i < length; i++) {
      if (bson->data[i] != buffer[i]) {
         g_error("Expected 0x%02x at offset %d got 0x%02x.", buffer[i], i, bson->data[i]);
      }
   }

   g_free(filename);
   g_free(buffer);
}

static void
append_tests (void)
{
   MongoObjectId *id;
   guint8 bytes[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01, 0x23, 0x45 };
   MongoBson *bson;
   MongoBson *array;
   MongoBson *subdoc;
   GDateTime *dt;
   GTimeZone *tz;
   GTimeVal tv;

   bson = mongo_bson_new_empty();
   mongo_bson_append_int(bson, "int", 1);
   assert_bson(bson, "test1.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   mongo_bson_append_int64(bson, "int64", 1);
   assert_bson(bson, "test2.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   mongo_bson_append_double(bson, "double", 1.123);
   assert_bson(bson, "test3.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   tz = g_time_zone_new("Z");
   dt = g_date_time_new(tz, 2011, 10, 22, 12, 13, 14.123);
   mongo_bson_append_date_time(bson, "utc", dt);
   assert_bson(bson, "test4.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   g_date_time_to_timeval(dt, &tv);
   mongo_bson_append_timeval(bson, "utc", &tv);
   assert_bson(bson, "test4.bson");
   mongo_bson_unref(bson);
   g_date_time_unref(dt);
   g_time_zone_unref(tz);

   bson = mongo_bson_new_empty();
   mongo_bson_append_string(bson, "string", "some string");
   assert_bson(bson, "test5.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   array = mongo_bson_new_empty();
   mongo_bson_append_int(array, "0", 1);
   mongo_bson_append_int(array, "1", 2);
   mongo_bson_append_int(array, "2", 3);
   mongo_bson_append_int(array, "3", 4);
   mongo_bson_append_int(array, "4", 5);
   mongo_bson_append_int(array, "5", 6);
   mongo_bson_append_array(bson, "array[int]", array);
   assert_bson(bson, "test6.bson");
   mongo_bson_unref(array);
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   array = mongo_bson_new_empty();
   mongo_bson_append_double(array, "0", 1.123);
   mongo_bson_append_double(array, "1", 2.123);
   mongo_bson_append_array(bson, "array[double]", array);
   assert_bson(bson, "test7.bson");
   mongo_bson_unref(array);
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   subdoc = mongo_bson_new_empty();
   mongo_bson_append_int(subdoc, "int", 1);
   mongo_bson_append_bson(bson, "document", subdoc);
   assert_bson(bson, "test8.bson");
   mongo_bson_unref(subdoc);
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   mongo_bson_append_null(bson, "null");
   assert_bson(bson, "test9.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   mongo_bson_append_regex(bson, "regex", "1234", "i");
   assert_bson(bson, "test10.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   mongo_bson_append_string(bson, "hello", "world");
   assert_bson(bson, "test11.bson");
   mongo_bson_unref(bson);

   bson = mongo_bson_new_empty();
   array = mongo_bson_new_empty();
   mongo_bson_append_string(array, "0", "awesome");
   mongo_bson_append_double(array, "1", 5.05);
   mongo_bson_append_int(array, "2", 1986);
   mongo_bson_append_array(bson, "BSON", array);
   assert_bson(bson, "test12.bson");
   mongo_bson_unref(bson);
   mongo_bson_unref(array);

   bson = mongo_bson_new_empty();
   id = mongo_object_id_new_from_data(bytes);
   mongo_bson_append_object_id(bson, "_id", id);
   subdoc = mongo_bson_new_empty();
   mongo_bson_append_object_id(subdoc, "_id", id);
   mongo_object_id_free(id);
   array = mongo_bson_new_empty();
   mongo_bson_append_string(array, "0", "1");
   mongo_bson_append_string(array, "1", "2");
   mongo_bson_append_string(array, "2", "3");
   mongo_bson_append_string(array, "3", "4");
   mongo_bson_append_array(subdoc, "tags", array);
   mongo_bson_unref(array);
   mongo_bson_append_string(subdoc, "text", "asdfanother");
   array = mongo_bson_new_empty();
   mongo_bson_append_string(array, "name", "blah");
   mongo_bson_append_bson(subdoc, "source", array);
   mongo_bson_unref(array);
   mongo_bson_append_bson(bson, "document", subdoc);
   mongo_bson_unref(subdoc);
   array = mongo_bson_new_empty();
   mongo_bson_append_string(array, "0", "source");
   mongo_bson_append_array(bson, "type", array);
   mongo_bson_unref(array);
   array = mongo_bson_new_empty();
   mongo_bson_append_string(array, "0", "server_created_at");
   mongo_bson_append_array(bson, "missing", array);
   mongo_bson_unref(array);
   assert_bson(bson, "test17.bson");
   mongo_bson_unref(bson);
}

static MongoBson *
get_bson (const gchar *name)
{
   const gchar *filename = g_build_filename(SRC_DIR, "tests", "bson", name, NULL);
   GError *error = NULL;
   gchar *buffer;
   gsize length;
   MongoBson *bson;

   if (!g_file_get_contents(filename, (gchar **)&buffer, &length, &error)) {
      g_assert_no_error(error);
      g_assert(FALSE);
   }

   bson = mongo_bson_new_from_data((const guint8 *)buffer, length);
   g_free(buffer);
   g_assert(bson);

   return bson;
}

static void
iter_tests (void)
{
   MongoBson *bson;
   MongoBsonIter iter;
   MongoBsonIter iter2;
   MongoBsonIter iter3;
   GDateTime *dt;
   GTimeVal tv;
   const gchar *regex = NULL;
   const gchar *options = NULL;

   bson = get_bson("test1.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_INT32(&iter));
   g_assert_cmpstr("int", ==, mongo_bson_iter_get_key(&iter));
   g_assert_cmpint(1, ==, mongo_bson_iter_get_value_int(&iter));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test2.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_INT64(&iter));
   g_assert_cmpstr("int64", ==, mongo_bson_iter_get_key(&iter));
   g_assert_cmpint(1L, ==, mongo_bson_iter_get_value_int64(&iter));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test3.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_DOUBLE(&iter));
   g_assert_cmpstr("double", ==, mongo_bson_iter_get_key(&iter));
   g_assert_cmpint(1.123, ==, mongo_bson_iter_get_value_double(&iter));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test4.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_DATE_TIME(&iter));
   g_assert_cmpstr("utc", ==, mongo_bson_iter_get_key(&iter));
   mongo_bson_iter_get_value_timeval(&iter, &tv);
   g_assert_cmpint(tv.tv_sec, ==, 1319285594);
   g_assert_cmpint(tv.tv_usec, ==, 123);
   dt = mongo_bson_iter_get_value_date_time(&iter);
   g_assert_cmpint(g_date_time_get_year(dt), ==, 2011);
   g_assert_cmpint(g_date_time_get_month(dt), ==, 10);
   g_assert_cmpint(g_date_time_get_day_of_month(dt), ==, 22);
   g_assert_cmpint(g_date_time_get_hour(dt), ==, 12);
   g_assert_cmpint(g_date_time_get_minute(dt), ==, 13);
   g_assert_cmpint(g_date_time_get_second(dt), ==, 14);
   g_assert_cmpint(g_date_time_get_microsecond(dt), ==, 123);
   g_date_time_unref(dt);
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test5.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_UTF8(&iter));
   g_assert_cmpstr("string", ==, mongo_bson_iter_get_key(&iter));
   g_assert_cmpstr("some string", ==, mongo_bson_iter_get_value_string(&iter, NULL));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test6.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_ARRAY(&iter));
   g_assert_cmpstr("array[int]", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(1, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(2, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("2", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(3, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("3", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(4, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("4", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(5, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("5", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(6, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test7.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_ARRAY(&iter));
   g_assert_cmpstr("array[double]", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(1.123 == mongo_bson_iter_get_value_double(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(2.123 == mongo_bson_iter_get_value_double(&iter2));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test8.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_DOCUMENT(&iter));
   g_assert_cmpstr("document", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("int", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(1, ==, mongo_bson_iter_get_value_int(&iter2));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test9.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_NULL(&iter));
   g_assert_cmpstr("null", ==, mongo_bson_iter_get_key(&iter));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test10.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_REGEX(&iter));
   mongo_bson_iter_get_value_regex(&iter, &regex, &options);
   g_assert_cmpstr(regex, ==, "1234");
   g_assert_cmpstr(options, ==, "i");
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test11.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_UTF8(&iter));
   g_assert_cmpstr("hello", ==, mongo_bson_iter_get_key(&iter));
   g_assert_cmpstr("world", ==, mongo_bson_iter_get_value_string(&iter, NULL));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test12.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_ARRAY(&iter));
   g_assert_cmpstr("BSON", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpstr("awesome", ==, mongo_bson_iter_get_value_string(&iter2, NULL));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(5.05 == mongo_bson_iter_get_value_double(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("2", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(1986 == mongo_bson_iter_get_value_int(&iter2));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test13.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_ARRAY(&iter));
   g_assert_cmpstr("array[bool]", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(TRUE, ==, mongo_bson_iter_get_value_boolean(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(FALSE, ==, mongo_bson_iter_get_value_boolean(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("2", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(TRUE, ==, mongo_bson_iter_get_value_boolean(&iter2));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test14.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_ARRAY(&iter));
   g_assert_cmpstr("array[string]", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpstr("hello", ==, mongo_bson_iter_get_value_string(&iter2, NULL));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpstr("world", ==, mongo_bson_iter_get_value_string(&iter2, NULL));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test15.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(MONGO_BSON_ARRAY, ==, mongo_bson_iter_get_value_type(&iter));
   g_assert_cmpstr("array[datetime]", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(MONGO_BSON_DATE_TIME, ==, mongo_bson_iter_get_value_type(&iter2));
   mongo_bson_iter_get_value_timeval(&iter2, &tv);
   g_assert_cmpint(tv.tv_sec, ==, 0);
   g_assert_cmpint(tv.tv_usec, ==, 0);
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpint(MONGO_BSON_DATE_TIME, ==, mongo_bson_iter_get_value_type(&iter2));
   mongo_bson_iter_get_value_timeval(&iter2, &tv);
   g_assert_cmpint(tv.tv_sec, ==, 1319285594);
   g_assert_cmpint(tv.tv_usec, ==, 123);
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test16.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(MONGO_BSON_ARRAY, ==, mongo_bson_iter_get_value_type(&iter));
   g_assert_cmpstr("array[null]", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_NULL, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_NULL, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_NULL, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("2", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);

   bson = get_bson("test17.bson");
   mongo_bson_iter_init(&iter, bson);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert(MONGO_BSON_ITER_HOLDS_OBJECT_ID(&iter));
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(MONGO_BSON_DOCUMENT, ==, mongo_bson_iter_get_value_type(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert(MONGO_BSON_ITER_HOLDS_OBJECT_ID(&iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_ARRAY, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("tags", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(mongo_bson_iter_recurse(&iter2, &iter3));
   g_assert(mongo_bson_iter_next(&iter3));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter3));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_value_string(&iter3, NULL));
   g_assert(mongo_bson_iter_next(&iter3));
   g_assert_cmpint(MONGO_BSON_UTF8, ==, mongo_bson_iter_get_value_type(&iter3));
   g_assert_cmpstr("1", ==, mongo_bson_iter_get_key(&iter3));
   g_assert_cmpstr("2", ==, mongo_bson_iter_get_value_string(&iter3, NULL));
   g_assert(mongo_bson_iter_next(&iter3));
   g_assert_cmpint(MONGO_BSON_UTF8, ==, mongo_bson_iter_get_value_type(&iter3));
   g_assert_cmpstr("2", ==, mongo_bson_iter_get_key(&iter3));
   g_assert_cmpstr("3", ==, mongo_bson_iter_get_value_string(&iter3, NULL));
   g_assert(mongo_bson_iter_next(&iter3));
   g_assert_cmpint(MONGO_BSON_UTF8, ==, mongo_bson_iter_get_value_type(&iter3));
   g_assert_cmpstr("3", ==, mongo_bson_iter_get_key(&iter3));
   g_assert_cmpstr("4", ==, mongo_bson_iter_get_value_string(&iter3, NULL));
   g_assert(!mongo_bson_iter_next(&iter3));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_UTF8, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("text", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpstr("asdfanother", ==, mongo_bson_iter_get_value_string(&iter2, NULL));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_DOCUMENT, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("source", ==, mongo_bson_iter_get_key(&iter2));
   g_assert(mongo_bson_iter_recurse(&iter2, &iter3));
   g_assert(mongo_bson_iter_next(&iter3));
   g_assert_cmpint(MONGO_BSON_UTF8, ==, mongo_bson_iter_get_value_type(&iter3));
   g_assert_cmpstr("name", ==, mongo_bson_iter_get_key(&iter3));
   g_assert_cmpstr("blah", ==, mongo_bson_iter_get_value_string(&iter3, NULL));
   g_assert(!mongo_bson_iter_next(&iter3));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(MONGO_BSON_ARRAY, ==, mongo_bson_iter_get_value_type(&iter));
   g_assert_cmpstr("type", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpint(MONGO_BSON_UTF8, ==, mongo_bson_iter_get_value_type(&iter2));
   g_assert_cmpstr("source", ==, mongo_bson_iter_get_value_string(&iter2, NULL));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(MONGO_BSON_ARRAY, ==, mongo_bson_iter_get_value_type(&iter));
   g_assert_cmpstr("missing", ==, mongo_bson_iter_get_key(&iter));
   g_assert(mongo_bson_iter_recurse(&iter, &iter2));
   g_assert(mongo_bson_iter_next(&iter2));
   g_assert_cmpstr("0", ==, mongo_bson_iter_get_key(&iter2));
   g_assert_cmpstr("server_created_at", ==, mongo_bson_iter_get_value_string(&iter2, NULL));
   g_assert(!mongo_bson_iter_next(&iter2));
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(bson);
}

static void
join (void)
{
   MongoBsonIter iter;
   MongoBson *b1;
   MongoBson *b2;
   MongoBson *b3;

   b1 = mongo_bson_new_empty();
   mongo_bson_append_int(b1, "key1", 1234);
   mongo_bson_append_int(b1, "key2", 4321);

   b2 = mongo_bson_new_empty();
   mongo_bson_append_int(b2, "key1", 1234);

   b3 = mongo_bson_new_empty();
   mongo_bson_append_int(b3, "key2", 4321);

   mongo_bson_join(b2, b3);

   mongo_bson_iter_init(&iter, b2);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(mongo_bson_iter_get_value_type(&iter), ==, MONGO_BSON_INT32);
   g_assert_cmpstr(mongo_bson_iter_get_key(&iter), ==, "key1");
   g_assert_cmpint(mongo_bson_iter_get_value_int(&iter), ==, 1234);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpint(mongo_bson_iter_get_value_type(&iter), ==, MONGO_BSON_INT32);
   g_assert_cmpstr(mongo_bson_iter_get_key(&iter), ==, "key2");
   g_assert_cmpint(mongo_bson_iter_get_value_int(&iter), ==, 4321);
   g_assert(!mongo_bson_iter_next(&iter));

   mongo_bson_unref(b1);
   mongo_bson_unref(b2);
   mongo_bson_unref(b3);
}

static void
invalid_tests (void)
{
   static const guint8 short_length[] = { 3, 0, 0, 0 };
   static const guint8 short_data[] = { 6, 0, 0, 0, 0 };
   static const guint8 bad_key[] = { 5, 0, 0, 0, 1 };
   static const guint8 empty_key[] = { 11, 0, 0, 0, 16, 0, 9, 0, 0, 0, 0 };
   MongoBsonIter iter;
   MongoBson *b;

   g_assert(!mongo_bson_new_from_data(short_length, G_N_ELEMENTS(short_length)));
   g_assert(!mongo_bson_new_from_data(short_data, G_N_ELEMENTS(short_data)));

   b = mongo_bson_new_from_data(bad_key, G_N_ELEMENTS(bad_key));
   g_assert(b);
   mongo_bson_iter_init(&iter, b);
   g_assert(!mongo_bson_iter_next(&iter));
   mongo_bson_unref(b);

   b = mongo_bson_new_from_data(empty_key, G_N_ELEMENTS(empty_key));
   g_assert(b);
   mongo_bson_iter_init(&iter, b);
   g_assert(mongo_bson_iter_next(&iter));
   g_assert_cmpstr(mongo_bson_iter_get_key(&iter), ==, "");
   g_assert_cmpint(mongo_bson_iter_get_value_int(&iter), ==, 9);
   mongo_bson_unref(b);

   /*
    * TODO: Work on fuzzing tests.
    */
}

static void
null_string (void)
{
   MongoBson *b;
   MongoBsonIter i;

   b = mongo_bson_new_empty();
   mongo_bson_append_string(b, "key", NULL);
   g_assert(mongo_bson_iter_init_find(&i, b, "key"));
   g_assert_cmpint(mongo_bson_iter_get_value_type(&i), ==, MONGO_BSON_NULL);

   mongo_bson_unref(b);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/MongoBson/append_tests", append_tests);
   g_test_add_func("/MongoBson/iter_tests", iter_tests);
   g_test_add_func("/MongoBson/join", join);
   g_test_add_func("/MongoBson/invalid", invalid_tests);
   g_test_add_func("/MongoBson/null_string", null_string);
   return g_test_run();
}
