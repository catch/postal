#include <mongo-glib/mongo-glib.h>

static void
test1 (void)
{
   MongoMessageReply *reply;
   GList *list;
   GList *list2;

   reply = g_object_new(MONGO_TYPE_MESSAGE_REPLY,
                        "flags", MONGO_REPLY_QUERY_FAILURE,
                        NULL);

   list = g_list_append(NULL, mongo_bson_new());
   mongo_message_reply_set_documents(reply, list);
   list2 = mongo_message_reply_get_documents(reply);

   g_assert_cmpint(g_list_length(list), ==, g_list_length(list2));
   g_assert(list->data == list2->data);

   g_object_unref(reply);
   g_list_foreach(list, (GFunc)mongo_bson_unref, NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/MongoMessageReply/new", test1);
   return g_test_run();
}
