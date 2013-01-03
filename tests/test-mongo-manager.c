#include <mongo-glib/mongo-glib.h>

static void
test1 (void)
{
   MongoManager *mgr;
   gchar **items;

   mgr = mongo_manager_new();
   mongo_manager_add_seed(mgr, "localhost:27017");
   mongo_manager_add_host(mgr, "127.0.0.1:27017");

   items = mongo_manager_get_seeds(mgr);
   g_assert_cmpint(1, ==, g_strv_length(items));
   g_assert_cmpstr(items[0], ==, "localhost:27017");
   g_strfreev(items);

   items = mongo_manager_get_hosts(mgr);
   g_assert_cmpint(1, ==, g_strv_length(items));
   g_assert_cmpstr(items[0], ==, "127.0.0.1:27017");
   g_strfreev(items);

   mongo_manager_remove_seed(mgr, "localhost:27017");
   items = mongo_manager_get_seeds(mgr);
   g_assert_cmpint(0, ==, g_strv_length(items));
   g_strfreev(items);

   mongo_manager_remove_host(mgr, "127.0.0.1:27017");
   items = mongo_manager_get_hosts(mgr);
   g_assert_cmpint(0, ==, g_strv_length(items));
   g_strfreev(items);

   mongo_manager_unref(mgr);
}

static void
test2 (void)
{
   MongoManager *mgr;
   const gchar *host;
   guint delay;
   guint i;

   mgr = mongo_manager_new();

   mongo_manager_add_seed(mgr, "a:27017");
   mongo_manager_add_seed(mgr, "b:27017");
   mongo_manager_add_seed(mgr, "c:27017");
   mongo_manager_add_host(mgr, "d:27017");
   mongo_manager_add_host(mgr, "e:27017");
   mongo_manager_add_host(mgr, "f:27017");

   for (i = 0; i < 5; i++) {
      host = mongo_manager_next(mgr, &delay);
      g_assert_cmpstr(host, ==, "a:27017");
      g_assert_cmpint(delay, ==, 0);

      host = mongo_manager_next(mgr, &delay);
      g_assert_cmpstr(host, ==, "b:27017");
      g_assert_cmpint(delay, ==, 0);

      host = mongo_manager_next(mgr, &delay);
      g_assert_cmpstr(host, ==, "c:27017");
      g_assert_cmpint(delay, ==, 0);

      host = mongo_manager_next(mgr, &delay);
      g_assert_cmpstr(host, ==, "d:27017");
      g_assert_cmpint(delay, ==, 0);

      host = mongo_manager_next(mgr, &delay);
      g_assert_cmpstr(host, ==, "e:27017");
      g_assert_cmpint(delay, ==, 0);

      host = mongo_manager_next(mgr, &delay);
      g_assert_cmpstr(host, ==, "f:27017");
      g_assert_cmpint(delay, ==, 0);

      host = mongo_manager_next(mgr, &delay);
      g_assert(!host);
      g_assert_cmpint(delay, >=, 0);
      g_assert_cmpint(delay, <=, (1000 << i));
   }

   mongo_manager_unref(mgr);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_type_init();
   g_test_add_func("/MongoManager/basic", test1);
   g_test_add_func("/MongoManager/next", test2);
   return g_test_run();
}
