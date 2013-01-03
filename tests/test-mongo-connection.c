#include "test-helper.h"

#include <mongo-glib/mongo-glib.h>

static GMainLoop *gMainLoop;

static void
test1_insert_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
   MongoConnection *connection = (MongoConnection *)object;
   gboolean *success = user_data;
   GError *error = NULL;

   *success = mongo_connection_insert_finish(connection, result, &error);
   g_assert_no_error(error);
   g_assert(*success);

   g_main_loop_quit(gMainLoop);
}

static void
test1 (void)
{
   MongoConnection *connection;
   MongoBson *bson;
   gboolean success = FALSE;

   connection = mongo_connection_new();
   bson = mongo_bson_new();
   mongo_bson_append_int(bson, "key1", 1234);
   mongo_bson_append_string(bson, "key2", "Some test string");
   mongo_connection_insert_async(connection, "dbtest1.dbcollection1",
                                 MONGO_INSERT_NONE, &bson, 1, NULL,
                                 test1_insert_cb, &success);
   mongo_bson_unref(bson);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static void
test2_query_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
   MongoMessageReply *reply;
   MongoConnection *connection = (MongoConnection *)object;
   gboolean *success = user_data;
   GError *error = NULL;
   GList *list;
   GList *iter;

   reply = mongo_connection_query_finish(connection, result, &error);
   g_assert_no_error(error);
   g_assert(reply);

   list = mongo_message_reply_get_documents(reply);

   for (iter = list; iter; iter = iter->next) {
      g_assert(iter->data);
   }

   g_object_unref(reply);

   *success = TRUE;
   g_main_loop_quit(gMainLoop);
}

static void
test2 (void)
{
   MongoConnection *connection;
   MongoBson *bson;
   gboolean success = FALSE;

   connection = mongo_connection_new();
   bson = mongo_bson_new_empty();
   mongo_bson_append_int(bson, "key1", 1234);
   mongo_connection_query_async(connection, "dbtest1.dbcollection1", MONGO_QUERY_NONE,
                            0, 0, bson, NULL, NULL, test2_query_cb, &success);
   mongo_bson_unref(bson);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static void
test3_query_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
   MongoConnection *connection = (MongoConnection *)object;
   gboolean *success = user_data;
   GError *error = NULL;

   *success = mongo_connection_delete_finish(connection, result, &error);
   g_assert_no_error(error);
   g_assert(*success);

   g_main_loop_quit(gMainLoop);
}

static void
test3 (void)
{
   MongoConnection *connection;
   MongoBson *selector;
   gboolean success = FALSE;

   connection = mongo_connection_new();
   selector = mongo_bson_new_empty();
   mongo_connection_delete_async(connection,
                             "dbtest1.dbcollection1",
                             MONGO_DELETE_NONE,
                             selector,
                             NULL,
                             test3_query_cb,
                             &success);
   mongo_bson_unref(selector);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static void
test4_query_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
   MongoConnection *connection = (MongoConnection *)object;
   gboolean *success = user_data;
   MongoMessageReply *reply;
   GError *error = NULL;

   reply = mongo_connection_command_finish(connection, result, &error);
   g_assert_no_error(error);
   g_assert(reply);

   *success = TRUE;

   g_object_unref(reply);
   g_main_loop_quit(gMainLoop);
}

static void
test4 (void)
{
   MongoConnection *connection;
   MongoBson *command;
   gboolean success = FALSE;

   connection = mongo_connection_new();
   command = mongo_bson_new_empty();
   mongo_bson_append_int(command, "ismaster", 1);
   mongo_connection_command_async(connection,
                                  "admin",
                                  command,
                                  NULL,
                                  test4_query_cb,
                                  &success);
   mongo_bson_unref(command);

   g_main_loop_run(gMainLoop);

   g_assert_cmpint(success, ==, TRUE);
}

static void
test5 (void)
{
#define TEST_URI(str) \
   G_STMT_START { \
      MongoConnection *c = mongo_connection_new_from_uri(str); \
      g_assert(c); \
      g_object_unref(c); \
   } G_STMT_END

   TEST_URI("mongodb://127.0.0.1:27017");
   TEST_URI("mongodb://127.0.0.1:27017/");
   TEST_URI("mongodb://127.0.0.1:27017/?replicaSet=abc");
   TEST_URI("mongodb://127.0.0.1:27017/?replicaSet=abc"
                                      "&connectTimeoutMS=1000"
                                      "&fsync=false"
                                      "&journal=true"
                                      "&safe=true"
                                      "&socketTimeoutMS=5000"
                                      "&wTimeoutMS=1000");
   TEST_URI("mongodb://mongo/?replicaSet=abc");
   TEST_URI("mongodb://mongo:27017?replicaSet=abc");
   TEST_URI("mongodb://mongo:27017/?replicaSet=abc");
   TEST_URI("mongodb://mongo.example.com:27017?replicaSet=abc");
   TEST_URI("mongodb://mongo.example.com?replicaSet=abc");
   TEST_URI("mongodb://mongo.example.com/?replicaSet=abc");
   TEST_URI("mongodb://127.0.0.1,127.0.0.2:27017/?w=123");
   TEST_URI("mongodb://127.0.0.1,127.0.0.2:27017?w=123");

   /*
    * We do not yet support port per host like follows.
    */
#if 0
   TEST_URI("mongodb://user:pass@127.0.0.1:27017,127.0.0.2:27017/?w=123");
#endif

#undef TEST_URI
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_type_init();
   gMainLoop = g_main_loop_new(NULL, FALSE);
   g_test_add_func("/MongoConnection/insert_async", test1);
   g_test_add_func("/MongoConnection/query_async", test2);
   g_test_add_func("/MongoConnection/delete_async", test3);
   g_test_add_func("/MongoConnection/command_async", test4);
   g_test_add_func("/MongoConnection/uri", test5);
   return g_test_run();
}
