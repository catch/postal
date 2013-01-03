#include "test-helper.h"

#include <mongo-glib/mongo-glib.h>

static GMainLoop *gMainLoop;
static MongoConnection *gConnection;

static void
test1_count_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
   MongoCollection *col = (MongoCollection *)object;
   gboolean *success = user_data;
   guint64 count = 0;
   GError *error = NULL;

   *success = mongo_collection_count_finish(col, result, &count, &error);
   g_assert_no_error(error);
   g_assert(*success);

   g_assert_cmpint(count, ==, 0);

   g_main_loop_quit(gMainLoop);
}

static void
test1_drop_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
   MongoCollection *col = (MongoCollection *)object;
   MongoBson *bson = NULL;
   gboolean *success = user_data;
   GError *error = NULL;

   *success = mongo_collection_drop_finish(col, result, &error);
   g_assert_no_error(error);
   g_assert(*success);
   *success = FALSE;

   mongo_collection_count_async(col, bson, NULL, test1_count_cb, success);
}

static void
test1_insert_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
   MongoCollection *col = (MongoCollection *)object;
   gboolean *success = user_data;
   GError *error = NULL;

   *success = mongo_collection_insert_finish(col, result, &error);
   g_assert_no_error(error);
   g_assert(*success);
   *success = FALSE;

   mongo_collection_drop_async(col, NULL, test1_drop_cb, success);
}

static void
test1 (void)
{
   MongoCollection *col;
   MongoDatabase *db;
   MongoBson *bson = NULL;
   gboolean success = FALSE;

   gConnection = mongo_connection_new();

   db = mongo_connection_get_database(gConnection, "dbtest1");
   g_assert(db);

   col = mongo_database_get_collection(db, "dbcollection1");
   g_assert(col);

   bson = mongo_bson_new();
   mongo_collection_insert_async(col, &bson, 1, MONGO_INSERT_NONE, NULL,
                                 test1_insert_cb, &success);
   mongo_bson_unref(bson);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static void
test2_insert_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
   MongoCollection *col = (MongoCollection *)object;
   gboolean *success = user_data;
   GError *error = NULL;

   *success = mongo_collection_insert_finish(col, result, &error);
   g_assert_no_error(error);
   g_assert(*success);

   g_main_loop_quit(gMainLoop);
}

static void
test2 (void)
{
   MongoCollection *col;
   MongoDatabase *db;
   MongoBson *doc;
   gboolean success = FALSE;

   gConnection = mongo_connection_new();

   db = mongo_connection_get_database(gConnection, "dbtest1");
   g_assert(db);

   col = mongo_database_get_collection(db, "dbcollection1");
   g_assert(col);

   doc = mongo_bson_new();
   mongo_bson_append_int(doc, "test-key", 54321);
   mongo_collection_insert_async(col, &doc, 1, MONGO_INSERT_NONE, NULL, test2_insert_cb, &success);
   mongo_bson_unref(doc);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static void
test3_find_one_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
   MongoCollection *collection = (MongoCollection *)object;
   MongoBson *bson;
   gboolean *success = user_data;
   GError *error = NULL;

   g_assert(MONGO_IS_COLLECTION(collection));

   bson = mongo_collection_find_one_finish(collection, result, &error);
   g_assert_no_error(error);
   g_assert(bson);

#if 0
   g_print("%s\n", mongo_bson_to_string(bson, FALSE));
#endif

   *success = !!bson;

   g_main_loop_quit(gMainLoop);
}

static void
test3 (void)
{
   MongoCollection *col;
   MongoDatabase *db;
   MongoBson *query = NULL;
   gboolean success = FALSE;

   gConnection = mongo_connection_new();

   db = mongo_connection_get_database(gConnection, "dbtest1");
   g_assert(db);

   col = mongo_database_get_collection(db, "dbcollection1");
   g_assert(col);

   mongo_collection_find_one_async(col, query, NULL, MONGO_QUERY_NONE,
                                   NULL, test3_find_one_cb, &success);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_type_init();

   gMainLoop = g_main_loop_new(NULL, FALSE);

   g_test_add_func("/MongoCollection/count", test1);
   g_test_add_func("/MongoCollection/insert", test2);
   g_test_add_func("/MongoCollection/find_one", test3);

   return g_test_run();
}
