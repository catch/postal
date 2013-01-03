#include <mongo-glib/mongo-glib.h>

static void
test1 (void)
{
   MongoMessageInsert *insert;
   GList *list;
   GList *list2;

   insert = g_object_new(MONGO_TYPE_MESSAGE_INSERT,
                         "collection", "db.collection",
                         "flags", MONGO_INSERT_CONTINUE_ON_ERROR,
                         NULL);
   g_assert_cmpstr(mongo_message_insert_get_collection(insert), ==, "db.collection");
   g_assert_cmpint(mongo_message_insert_get_flags(insert), ==, MONGO_INSERT_CONTINUE_ON_ERROR);

   list = g_list_append(NULL, mongo_bson_new());
   mongo_message_insert_set_documents(insert, list);
   list2 = mongo_message_insert_get_documents(insert);

   g_assert_cmpint(g_list_length(list), ==, g_list_length(list2));
   g_assert(list->data == list2->data);

   g_object_unref(insert);
   g_list_foreach(list, (GFunc)mongo_bson_unref, NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/MongoMessageInsert/new", test1);
   return g_test_run();
}
