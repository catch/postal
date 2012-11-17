#include <cut-n-paste/url-router.h>

static GHashTable *matches;

static void
handler (UrlRouter         *router,
         SoupServer        *server,
         SoupMessage       *message,
         const gchar       *path,
         GHashTable        *params,
         GHashTable        *query,
         SoupClientContext *client,
         gpointer           user_data)
{
   g_hash_table_insert(matches, user_data, GINT_TO_POINTER(1));
}

static void
test1 (void)
{
   SoupClientContext *client;
   SoupMessage *message;
   SoupServer *server;
   UrlRouter *router;

   router = url_router_new();
   matches = g_hash_table_new(g_str_hash, g_str_equal);
   server = g_object_new(SOUP_TYPE_SERVER, NULL);
   message = g_object_new(SOUP_TYPE_MESSAGE, NULL);
   client = GINT_TO_POINTER(1);

#define ADD_HANDLER(key) \
   url_router_add_handler(router, key, handler, (gpointer)key)

#define TEST_HANDLER(key, check) \
   G_STMT_START { \
      gboolean r = url_router_route(router, server, message, key, NULL, client); \
      g_assert(g_hash_table_contains(matches, check)); \
      g_assert_cmpint(g_hash_table_size(matches), ==, 1); \
      g_assert(r); \
      g_hash_table_remove_all(matches); \
   } G_STMT_END

#define TEST_FAILURE(key) \
   G_STMT_START { \
      gboolean r = url_router_route(router, server, message, key, NULL, client); \
      g_assert_cmpint(g_hash_table_size(matches), ==, 0); \
      g_assert(!r); \
      g_hash_table_remove_all(matches); \
   } G_STMT_END

   ADD_HANDLER("/v1");
   ADD_HANDLER("/v1/users");
   ADD_HANDLER("/v1/users/:user");
   ADD_HANDLER("/v1/users/:user/devices");
   ADD_HANDLER("/v1/users/:user/devices/:device");

   TEST_HANDLER("/v1", "/v1");
   TEST_HANDLER("/v1/", "/v1");
   TEST_HANDLER("/v1/users", "/v1/users");
   TEST_HANDLER("/v1/users/", "/v1/users");
   TEST_HANDLER("/v1/users/000011110000111100001111", "/v1/users/:user");
   TEST_HANDLER("/v1/users/000011110000111100001111/", "/v1/users/:user");
   TEST_HANDLER("/v1/users/000011110000111100001111/devices", "/v1/users/:user/devices");
   TEST_HANDLER("/v1/users/000011110000111100001111/devices/", "/v1/users/:user/devices");
   TEST_HANDLER("/v1/users/000011110000111100001111/devices/111100001111000011110000", "/v1/users/:user/devices/:device");
   TEST_HANDLER("/v1/users/000011110000111100001111/devices/111100001111000011110000/", "/v1/users/:user/devices/:device");

   TEST_FAILURE("/");
   TEST_FAILURE("//");
   TEST_FAILURE("////");
   TEST_FAILURE("/v1////");
   TEST_FAILURE("/v1/users/1234/devices/1234/blah");

#undef TEST_HANDLER
#undef ADD_HANDLER

   g_clear_object(&server);
   g_clear_object(&message);
   url_router_free(router);
   g_hash_table_unref(matches);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/UrlRouter/route", test1);
   return g_test_run();
}
