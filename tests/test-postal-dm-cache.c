#include <postal/postal-dm-cache.h>

typedef struct
{
   const gchar *key;
   gboolean expected;
} Test1Data;

static Test1Data gTest1Data[] = {
   { "abcdef", FALSE },
   { "abcdef", TRUE },
   { "abcdef", TRUE },
   { "abcdefg", FALSE },
   { "abcdefg", TRUE },
   { "abcdef", TRUE },
   { "abcdefg", TRUE },
   { "123456", FALSE },
   { "123456", TRUE },
   { "888:8888", FALSE },
   { "888:8888", TRUE },
};

static void
test1 (void)
{
   PostalDmCache *cache;
   Test1Data *data;
   guint i;

   cache = postal_dm_cache_new(2048,
                               g_str_hash,
                               g_str_equal,
                               g_free);

   for (i = 0; i < G_N_ELEMENTS(gTest1Data); i++) {
      data = &gTest1Data[i];
      g_assert(data->key);
      g_assert_cmpint(data->expected, ==, postal_dm_cache_contains(cache, data->key));
      g_assert_cmpint(data->expected, ==, postal_dm_cache_insert(cache, g_strdup(data->key)));
   }

   postal_dm_cache_unref(cache);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/PostalDmCache/basic", test1);
   return g_test_run();
}
