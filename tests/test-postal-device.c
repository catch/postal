#include <postal/postal-device.h>

static void
test1 (void)
{
   MongoBsonIter iter;
   PostalDevice *d;
   MongoBson *b;
   gboolean r;

   d = postal_device_new();
   g_assert(!postal_device_get_id(d));
   b = postal_device_save_to_bson(d, NULL);
   g_assert(!b);
   postal_device_set_user(d, "000011110000111100001111");
   b = postal_device_save_to_bson(d, NULL);
   g_assert(b);
   g_assert(postal_device_get_id(d));
   r = mongo_bson_iter_init_find(&iter, b, "_id");
   g_assert(r);
   mongo_bson_unref(b);
   g_object_unref(d);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/Postal/Device/save_to_bson", test1);
   return g_test_run();
}
