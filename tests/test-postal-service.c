#include <postal/postal-service.h>

static GMainLoop   *gMainLoop;
static gboolean     gSuccess;
static const gchar  gKeyData[] =
   "[mongo]\n"
   "uri = mongodb://127.0.0.1:27017\n"
   "db = test\n"
   "collection = devices\n";

static void
test1_cb3 (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
   const MongoObjectId *o;
   MongoObjectId *oo;
   PostalService *service = (PostalService *)object;
   PostalDevice *device = user_data;
   MongoBsonIter i;
   MongoBson *b;
   GError *error = NULL;
   GList *list;
   gchar *s;

   list = postal_service_find_devices_finish(service, result, &error);
   g_assert_no_error(error);
   g_assert(list);

   b = g_list_last(list)->data;
   g_assert(mongo_bson_iter_init_find(&i, b, "_id"));
   o = mongo_bson_iter_get_value_object_id(&i);
   g_assert(o);
   g_assert(mongo_object_id_equal(o, postal_device_get_id(device)));
   g_assert(mongo_bson_iter_init_find(&i, b, "user"));
   g_assert(mongo_bson_iter_get_value_type(&i) == MONGO_BSON_OBJECT_ID);
   oo = mongo_bson_iter_get_value_object_id(&i);
   s = mongo_object_id_to_string(oo);
   g_assert_cmpstr(s, ==, postal_device_get_user(device));
   mongo_object_id_free(oo);
   g_free(s);

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

   postal_device_set_device_token(device, "my_device_token");
   postal_service_update_device(service, device, NULL, test1_cb1, device);
}

static void
test1 (void)
{
   PostalService *service;
   PostalDevice *device;
   GKeyFile *key_file;

   gSuccess = FALSE;
   key_file = g_key_file_new();
   g_key_file_load_from_data(key_file, gKeyData, -1, 0, NULL);
   service = POSTAL_SERVICE_DEFAULT;
   postal_service_set_config(service, key_file);
   postal_service_start(service);
   device = postal_device_new();
   postal_device_set_user(device, "000011110000111100001111");
   postal_device_set_device_token(device, "some_token");
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
