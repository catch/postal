#include <postal/postal-application.h>
#include <libsoup/soup.h>
#include <string.h>

static PostalApplication *gApplication;
static GKeyFile          *gKeyFile;
static gchar             *gAccount;
static gchar             *gC2dmDevice;
static gint               gC2dmDeviceId;
static const gchar       *gConfig =
   "[mongo]\n"
   "db = test\n"
   "collection = devices\n"
   "uri = mongodb://127.0.0.1:27017\n"
   "[http]\n"
   "port = 6616\n"
   "nologging = true\n";

static PostalApplication *
application_new (const gchar *name)
{
   PostalApplication *app;
   gchar *app_name;

   app_name = g_strdup_printf("com.catch.postald.http.%s", name);
   app = g_object_new(POSTAL_TYPE_APPLICATION,
                      "application-id", app_name,
                      "config", gKeyFile,
                      "flags", G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_NON_UNIQUE,
                      NULL);
   g_free(app_name);

   return app;
}

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

   gApplication = application_new(G_STRFUNC);

   session = soup_session_async_new();
   g_assert(SOUP_IS_SESSION(session));

   url = g_strdup_printf("http://localhost:6616/v1/users/%s/devices", gAccount);
   message = soup_message_new("GET", url);
   g_assert(SOUP_IS_MESSAGE(message));
   g_free(url);

   soup_session_queue_message(session, message, get_devices_cb, NULL);

   g_application_run(G_APPLICATION(gApplication), 0, NULL);

   g_clear_object(&gApplication);
}

static void
add_device_cb (SoupSession *session,
               SoupMessage *message,
               gpointer     user_data)
{
   gchar *str;

   g_assert(SOUP_IS_SESSION(session));
   g_assert(SOUP_IS_MESSAGE(message));

   g_assert_cmpint(message->status_code, ==, 201);

   str = g_strdup_printf("{\n"
                         "  \"device_token\" : \"%064d\",\n"
                         "  \"device_type\" : \"c2dm\",\n"
                         "  \"user\" : \"%s\"\n"
                         "}",
                         gC2dmDeviceId,
                         gAccount);
   g_assert_cmpstr(message->response_body->data, ==, str);
   g_free(str);

   g_application_quit(G_APPLICATION(gApplication));
}

static void
test2 (void)
{
   SoupSession *session;
   SoupMessage *message;
   gchar *url;

   gApplication = application_new(G_STRFUNC);

   session = soup_session_async_new();
   g_assert(SOUP_IS_SESSION(session));

   url = g_strdup_printf("http://localhost:6616/v1/users/%s/devices", gAccount);
   message = soup_message_new("POST", url);
   g_assert(SOUP_IS_MESSAGE(message));
   g_free(url);

   soup_message_headers_set_content_type(message->request_headers,
                                         "application/json",
                                         NULL);
   soup_message_body_append(message->request_body,
                            SOUP_MEMORY_STATIC,
                            gC2dmDevice,
                            strlen(gC2dmDevice));

   soup_session_queue_message(session, message, add_device_cb, NULL);

   g_application_run(G_APPLICATION(gApplication), 0, NULL);

   g_clear_object(&gApplication);
}

static void
get_devices2_cb (SoupSession *session,
                 SoupMessage *message,
                 gpointer     user_data)
{
   gchar *str;

   g_assert(SOUP_IS_SESSION(session));
   g_assert(SOUP_IS_MESSAGE(message));

   g_assert_cmpint(message->status_code, ==, 200);

   str = g_strdup_printf("[\n"
                         "  {\n"
                         "    \"device_token\" : \"%064d\",\n"
                         "    \"device_type\" : \"c2dm\",\n"
                         "    \"user\" : \"%s\"\n"
                         "  }\n"
                         "]",
                         gC2dmDeviceId,
                         gAccount);
   g_assert_cmpstr(message->response_body->data, ==, str);
   g_free(str);

   g_application_quit(G_APPLICATION(gApplication));
}

static void
test3 (void)
{
   SoupSession *session;
   SoupMessage *message;
   gchar *url;

   gApplication = application_new(G_STRFUNC);

   session = soup_session_async_new();
   g_assert(SOUP_IS_SESSION(session));

   url = g_strdup_printf("http://localhost:6616/v1/users/%s/devices", gAccount);
   message = soup_message_new("GET", url);
   g_assert(SOUP_IS_MESSAGE(message));
   g_free(url);

   soup_session_queue_message(session, message, get_devices2_cb, NULL);

   g_application_run(G_APPLICATION(gApplication), 0, NULL);

   g_clear_object(&gApplication);
}

gint
main (gint argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);

   gKeyFile = g_key_file_new();
   g_key_file_load_from_data(gKeyFile, gConfig, -1, 0, NULL);

   gAccount = g_strdup_printf("%024d", g_random_int());

   gC2dmDeviceId = g_random_int();
   gC2dmDevice = g_strdup_printf("{\n"
                                 "  \"device_token\": \"%064d\",\n"
                                 "  \"device_type\": \"c2dm\"\n"
                                 "}", gC2dmDeviceId);

   g_test_add_func("/PostalHttp/get_devices", test1);
   g_test_add_func("/PostalHttp/add_device", test2);
   g_test_add_func("/PostalHttp/get_devices2", test3);

   return g_test_run();
}
