#include "test-helper.h"

#include <mongo-glib/mongo-glib.h>

static GMainLoop *gMainLoop;
static MongoConnection *gConnection;

static void
test1_count_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
   MongoCursor *cursor = (MongoCursor *)object;
   gboolean *success = user_data;
   GError *error = NULL;
   guint64 count = 0;

   *success = mongo_cursor_count_finish(cursor, result, &count, &error);
   g_assert_no_error(error);
   g_assert(*success);

   g_assert_cmpint(count, >, 0);

   g_main_loop_quit(gMainLoop);
}

static void
test1 (void)
{
   gboolean success = FALSE;
   MongoCollection *col;
   MongoDatabase *db;
   MongoCursor *cursor;
   MongoBson *query = NULL;
   MongoBson *fields = NULL;

   gConnection = mongo_connection_new();

   db = mongo_connection_get_database(gConnection, "dbtest1");
   g_assert(db);

   col = mongo_database_get_collection(db, "dbcollection1");
   g_assert(col);

   cursor = mongo_collection_find(col, query, fields, 0, 100, MONGO_QUERY_NONE);
   g_assert(cursor);

   mongo_cursor_count_async(cursor, NULL, test1_count_cb, &success);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static gboolean
test2_foreach_func (MongoCursor *cursor,
                    MongoBson   *bson,
                    gpointer     user_data)
{
   guint *count = user_data;

#if 0
   gchar *str;
   str = mongo_bson_to_string(bson, FALSE);
   g_print("%s\n", str);
   g_free(str);
#endif

   (*count)++;

   return TRUE;
}

static void
test2_foreach_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
   gboolean ret;
   GError *error = NULL;

   ret = mongo_cursor_foreach_finish(MONGO_CURSOR(object), result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   g_main_loop_quit(gMainLoop);
}

static void
test2 (void)
{
   MongoCollection *col;
   MongoDatabase *db;
   MongoCursor *cursor;
   MongoBson *query = NULL;
   MongoBson *fields = NULL;
   guint count = 0;

   gConnection = mongo_connection_new();

   db = mongo_connection_get_database(gConnection, "dbtest1");
   g_assert(db);

   col = mongo_database_get_collection(db, "dbcollection1");
   g_assert(col);

   cursor = mongo_collection_find(col, query, fields, 0, 100, MONGO_QUERY_NONE);
   g_assert(cursor);

   mongo_cursor_foreach_async(cursor,
                              test2_foreach_func,
                              &count,
                              NULL,
                              NULL,
                              test2_foreach_cb,
                              &count);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(count, ==, 1);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_type_init();

   gMainLoop = g_main_loop_new(NULL, FALSE);

   g_test_add_func("/MongoCursor/count", test1);
   g_test_add_func("/MongoCursor/foreach", test2);

   return g_test_run();
}
