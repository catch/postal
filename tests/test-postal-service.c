#include <postal/postal-service.h>

static GMainLoop   *gMainLoop;
static gboolean     gSuccess;
static const gchar  gKeyData[] =
   "[mongo]\n"
   "uri = mongodb://127.0.0.1:27017/?w=1&fsync=true\n"
   "db = test\n"
   "collection = devices\n";

static void
test1_cb3 (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   PostalDevice *device = user_data;
   GPtrArray *devices;
   GError *error = NULL;
   guint i;

   devices = postal_service_find_devices_finish(service, result, &error);
   g_assert_no_error(error);
   g_assert(devices);

   for (i = 0; i < devices->len; i++) {
      if (!g_strcmp0(postal_device_get_device_token(device),
                     postal_device_get_device_token(g_ptr_array_index(devices, i)))) {
         break;
      }
   }

   g_assert_cmpint(i, <, devices->len);
   g_assert_cmpstr(postal_device_get_user(device), ==, postal_device_get_user(g_ptr_array_index(devices, i)));

   gSuccess = TRUE;
   g_main_loop_quit(gMainLoop);
}

static void
test1_cb2 (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   PostalDevice *device = user_data;
   gboolean ret;
   GError *error = NULL;

   ret = postal_service_remove_device_finish(service, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   postal_service_find_devices(service,
                               postal_device_get_user(device),
                               0,
                               0,
                               NULL,
                               test1_cb3,
                               device);
}

static void
test1_cb1 (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   PostalDevice *device = user_data;
   gboolean ret;
   GError *error = NULL;

   ret = postal_service_update_device_finish(service, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   postal_service_remove_device(service, device, NULL, test1_cb2, device);
}

static void
test1_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
   PostalService *service = (PostalService *)object;
   PostalDevice *device = user_data;
   gboolean ret;
   GError *error = NULL;

   ret = postal_service_add_device_finish(service, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   postal_service_update_device(service, device, NULL, test1_cb1, device);
}

static void
test1 (void)
{
   PostalService *service;
   PostalDevice *device;
   GKeyFile *key_file;
   gchar *rand_str;

   gSuccess = FALSE;
   key_file = g_key_file_new();
   g_key_file_load_from_data(key_file, gKeyData, -1, 0, NULL);
   service = POSTAL_SERVICE_DEFAULT;
   postal_service_set_config(service, key_file);
   postal_service_start(service);
   device = postal_device_new();
   rand_str = g_strdup_printf("%d", g_random_int());
   postal_device_set_device_token(device, rand_str);
   g_free(rand_str);
   postal_device_set_user(device, "000011110000111100001111");
   postal_device_set_device_type(device, "c2dm");
   postal_service_add_device(service, device, NULL, test1_cb, device);
   g_main_loop_run(gMainLoop);
   g_object_unref(device);
   g_assert(gSuccess);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_type_init();
   gMainLoop = g_main_loop_new(NULL, FALSE);
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/Postal/Service/add_update_remove_find", test1);
   return g_test_run();
}
