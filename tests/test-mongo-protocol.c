#include <mongo-glib/mongo-glib.h>

#ifdef G_DISABLE_ASSERT
#undef G_DISABLE_ASSERT
#endif

#define PUMP_MAIN_LOOP \
   G_STMT_START { \
      while (g_main_context_pending(g_main_context_default())) { \
         g_main_context_iteration(g_main_context_default(), FALSE); \
      } \
   } G_STMT_END

static GMainLoop *gMainLoop;

/*
 * TODO: Move this to shared private header.
 */
struct _MongoServerPrivate
{
   GHashTable *client_contexts;
};

static void
failed_cb (MongoProtocol *protocol,
           const GError  *error,
           gboolean      *failed)
{
   *failed = TRUE;
   g_printerr("%s\n", error->message);
}

static void
message_cb (MongoProtocol *protocol,
            MongoMessage  *message,
            guint         *count)
{
   (*count)++;
}

static void
test_MongoProtocol_replies (void)
{
   GSocketConnectable *connectable;
   GSocketConnection *connection;
   GHashTableIter iter;
   MongoProtocol *protocol;
   GSocketClient *client;
   MongoMessage *message;
   MongoServer *server;
   GIOStream *key;
   gboolean failed = FALSE;
   guint count = 0;
   guint port;
   guint i;

   port = g_random_int_range(30000, 31000);
   server = g_object_new(MONGO_TYPE_SERVER,
                         "listen-backlog", 10,
                         NULL);
   g_socket_listener_add_inet_port(G_SOCKET_LISTENER(server),
                                   port,
                                   NULL,
                                   NULL);
   g_socket_service_start(G_SOCKET_SERVICE(server));

   client = g_socket_client_new();
   connectable = g_network_address_new("localhost", port);
   connection = g_socket_client_connect(client, connectable, NULL, NULL);
   protocol = g_object_new(MONGO_TYPE_PROTOCOL,
                           "io-stream", connection,
                           NULL);
   g_signal_connect(protocol, "failed", G_CALLBACK(failed_cb), &failed);
   g_signal_connect(protocol, "message-read", G_CALLBACK(message_cb), &count);

   PUMP_MAIN_LOOP;

   g_hash_table_iter_init(&iter, server->priv->client_contexts);
   g_assert(g_hash_table_iter_next(&iter, (gpointer *)&key, NULL));
   g_assert(key);

   for (i = 0; i < 10; i++) {
      gboolean r;
      GError *error = NULL;
      guint8 *buf;
      gsize buflen;
      gsize written;

      message = g_object_new(MONGO_TYPE_MESSAGE_REPLY,
                             "cursor-id", 0UL,
                             "flags", MONGO_REPLY_NONE,
                             "offset", 0,
                             "request-id", 4321,
                             "response-to", 1234,
                             NULL);
      buf = mongo_message_save_to_data(message, &buflen);
      g_assert(buf);
      g_assert_cmpint(buflen, >, 16);

      r = g_output_stream_write_all(g_io_stream_get_output_stream(key),
                                    buf,
                                    buflen,
                                    &written,
                                    NULL,
                                    &error);
      g_assert_no_error(error);
      g_assert(r);
      g_assert_cmpint(written, ==, buflen);

      g_free(buf);
      g_clear_object(&message);
   }

   g_output_stream_flush(g_io_stream_get_output_stream(key), NULL, NULL);

   PUMP_MAIN_LOOP;

   for (i = 0; i < 10; i++) {
      gboolean r;
      GError *error = NULL;
      guint8 *buf;
      gsize buflen;
      gsize written;

      message = g_object_new(MONGO_TYPE_MESSAGE_MSG,
                             "message", "Hello, there!",
                             "request-id", 4321,
                             "response-to", 1234,
                             NULL);
      buf = mongo_message_save_to_data(message, &buflen);
      g_assert(buf);
      g_assert_cmpint(buflen, >, 16);

      r = g_output_stream_write_all(g_io_stream_get_output_stream(key),
                                    buf,
                                    buflen,
                                    &written,
                                    NULL,
                                    &error);
      g_assert_no_error(error);
      g_assert(r);
      g_assert_cmpint(written, ==, buflen);

      g_free(buf);
      g_clear_object(&message);
   }

   g_output_stream_flush(g_io_stream_get_output_stream(key), NULL, NULL);

   PUMP_MAIN_LOOP;

   /*
    * Try some short writes to simulate split up packets.
    */

   for (i = 0; i < 10; i++) {
      gboolean r;
      GError *error = NULL;
      guint8 *buf;
      guint8 *orig;
      gsize buflen;
      gsize written;

      message = g_object_new(MONGO_TYPE_MESSAGE_MSG,
                             "message", "Hello, there with split packets!",
                             "request-id", 4321,
                             "response-to", 1234,
                             NULL);
      buf = mongo_message_save_to_data(message, &buflen);
      g_assert(buf);
      g_assert_cmpint(buflen, >, 16);

      r = g_output_stream_write_all(g_io_stream_get_output_stream(key),
                                    buf,
                                    buflen / 2,
                                    &written,
                                    NULL,
                                    &error);
      g_assert_no_error(error);
      g_assert(r);

      g_output_stream_flush(g_io_stream_get_output_stream(key), NULL, NULL);

      PUMP_MAIN_LOOP;

      orig = buf;
      buf += written;
      buflen -= written;

      r = g_output_stream_write_all(g_io_stream_get_output_stream(key),
                                    buf,
                                    buflen,
                                    &written,
                                    NULL,
                                    &error);
      g_assert_no_error(error);
      g_assert(r);

      g_assert_cmpint(written, ==, buflen);

      g_free(orig);
      g_clear_object(&message);

      g_output_stream_flush(g_io_stream_get_output_stream(key), NULL, NULL);

      PUMP_MAIN_LOOP;
   }

   g_assert_cmpint(failed, !=, TRUE);

   while (count < 30) {
      PUMP_MAIN_LOOP;
   }

   g_assert_cmpint(count, ==, 30);

   g_socket_service_stop(G_SOCKET_SERVICE(server));

   g_object_add_weak_pointer(G_OBJECT(client), (gpointer *)&client);
   g_object_add_weak_pointer(G_OBJECT(connection), (gpointer *)&connection);
   g_object_add_weak_pointer(G_OBJECT(connectable), (gpointer *)&connectable);
   g_object_add_weak_pointer(G_OBJECT(protocol), (gpointer *)&protocol);
   g_object_add_weak_pointer(G_OBJECT(server), (gpointer *)&server);

   g_object_unref(client);
   g_object_unref(connection);
   g_object_unref(connectable);
   g_object_unref(protocol);
   g_object_unref(server);

   g_assert(!client);
   g_assert(!connectable);
   g_assert(!protocol);
   g_assert(!connection);

   PUMP_MAIN_LOOP;

   g_assert(!server);
}

gint
main (gint argc,
      gchar *argv[])
{
   g_type_init();
   g_test_init(&argc, &argv, NULL);
   gMainLoop = g_main_loop_new(NULL, FALSE);
   g_test_add_func("/MongoProtocol/replies", test_MongoProtocol_replies);
   return g_test_run();
}
