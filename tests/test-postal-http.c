#include <postal/postal-application.h>
#include <libsoup/soup.h>

static PostalApplication *gApplication;
static gchar             *gAccount;
static const gchar       *gConfig =
   "[mongo]\n"
   "db = test\n"
   "collection = devices\n"
   "uri = mongodb://127.0.0.1:27017\n"
   "[http]\n"
   "port = 6616\n"
   "nologging = true\n";

static void
get_devices_cb (SoupSession *session,
                SoupMessage *message,
                gpointer     user_data)
{
   g_assert(SOUP_IS_SESSION(session));
   g_assert(SOUP_IS_MESSAGE(message));

   g_assert_cmpint(message->status_code, ==, 200);
   g_assert_cmpstr(message->response_body->data, ==, "[\n]");

   g_application_quit(G_APPLICATION(gApplication));
}

static void
test1 (void)
{
   SoupSession *session;
   SoupMessage *message;
   gchar *url;

   session = soup_session_async_new();
   g_assert(SOUP_IS_SESSION(session));

   url = g_strdup_printf("http://localhost:6616/v1/users/%s/devices", gAccount);
   message = soup_message_new("GET", url);
   g_assert(SOUP_IS_MESSAGE(message));

   soup_session_queue_message(session, message, get_devices_cb, NULL);

   g_application_run(G_APPLICATION(gApplication), 0, NULL);
}

gint
main (gint argc,
      gchar *argv[])
{
   GKeyFile *key_file;

   g_type_init();
   g_test_init(&argc, &argv, NULL);
   key_file = g_key_file_new();
   g_key_file_load_from_data(key_file, gConfig, -1, 0, NULL);
   gApplication = POSTAL_APPLICATION_DEFAULT;
   gAccount = g_strdup_printf("%024d", g_random_int());
   g_object_set(gApplication,
                "config", key_file,
                NULL);
   g_key_file_unref(key_file);
   g_test_add_func("/PostalHttp/get_devices", test1);
   return g_test_run();
}
