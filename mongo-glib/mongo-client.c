/* mongo-client.c
 *
 * Copyright (C) 2012 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "mongo-client.h"
#include "mongo-debug.h"
#include "mongo-input-stream.h"
#include "mongo-output-stream.h"
#include "mongo-source.h"

/*
 * TODO:
 *
 *   - I need a GCancellable for cancelling requests when the
 *     client needs to shut down.
 *   - Lots of testing on sync and async methods with connection
 *     state changes.
 *   - Queue requests before we have connected.
 *   - Automatically connect.
 */

/**
 * SECTION:mongo-client
 * @title: MongoClient
 * @short_description: Client for connecting with a MongoDB server.
 *
 * MongoClient is responsible for the communication to a MongoDB server.
 * It delivers and receives messages according to the MongoDB wire protocol.
 * Typically, you want to interact using #MongoDatabase or #MongoCollection
 * by retrieving them from the #MongoClient.
 *
 * mongo_client_send_async() can be used directly, but is typically used
 * by the #MongoDatabase, #MongoCollection, and #MongoCursor instances.
 */

G_DEFINE_TYPE(MongoClient, mongo_client, G_TYPE_OBJECT)

struct _MongoClientPrivate
{
   MongoInputStream  *input;
   MongoOutputStream *output;
   MongoWriteConcern *concern;
   GHashTable        *async_results;
   GMainContext      *async_context;
   MongoSource       *source;
   GCancellable      *input_shutdown;
   GCancellable      *output_shutdown;
};

enum
{
   PROP_0,
   PROP_ASYNC_CONTEXT,
   PROP_STREAM,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];
static GQuark      gQuarkCompleted;
static GQuark      gQuarkRequestId;
static GQuark      gQuarkReply;

#define IS_COMPLETED(o) \
   GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(o), gQuarkCompleted))
#define SET_COMPLETED(o) \
   g_object_set_qdata(G_OBJECT(o), gQuarkCompleted, GINT_TO_POINTER(TRUE))

static void
mongo_client_set_async_context (MongoClient  *client,
                                GMainContext *async_context)
{
   g_return_if_fail(MONGO_IS_CLIENT(client));
   g_return_if_fail(!client->priv->async_context);
   g_return_if_fail(!client->priv->source);

   if (!async_context)
      async_context = g_main_context_default();
   client->priv->async_context = g_main_context_ref(async_context);
   client->priv->source = mongo_source_new();
   g_source_attach((GSource *)client->priv->source, async_context);
   g_object_notify_by_pspec(G_OBJECT(client), gParamSpecs[PROP_ASYNC_CONTEXT]);
}

GMainContext *
mongo_client_get_async_context (MongoClient *client)
{
   g_return_val_if_fail(MONGO_IS_CLIENT(client), NULL);
   return client->priv->async_context;
}

MongoClient *
mongo_client_new_from_stream (GIOStream *stream)
{
   return g_object_new(MONGO_TYPE_CLIENT,
                       "stream", stream,
                       NULL);
}

static void
mongo_client_read_message_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   MongoClientPrivate *priv;
   GSimpleAsyncResult *simple;
   MongoInputStream *input = (MongoInputStream *)object;
   MongoMessage *message;
   MongoClient *client = user_data;
   gpointer response_to;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_INPUT_STREAM(input));
   g_assert(G_IS_ASYNC_RESULT(result));

   /*
    * Check for read failure before using client. As it could have been
    * cancelled by dispose of our client. If thathappened, it is also
    * possible that the client has been finalized.
    */
   if (!(message = mongo_input_stream_read_message_finish(input,
                                                          result,
                                                          &error))) {
      g_input_stream_close(G_INPUT_STREAM(input), NULL, NULL);
      g_error_free(error);
      EXIT;
   }

   g_assert(MONGO_IS_CLIENT(client));

   priv = client->priv;

   response_to = GINT_TO_POINTER(mongo_message_get_response_to(message));
   if ((simple = g_hash_table_lookup(priv->async_results, response_to))) {
      g_hash_table_steal(priv->async_results, response_to);
      if (!IS_COMPLETED(simple)) {
         SET_COMPLETED(simple);
         g_simple_async_result_set_op_res_gboolean(simple, TRUE);
         g_object_set_qdata_full(G_OBJECT(simple),
                                 gQuarkReply,
                                 g_object_ref(message),
                                 g_object_unref);
         mongo_source_complete_in_idle(priv->source, simple);
      }
   }

   mongo_input_stream_read_message_async(input,
                                         priv->input_shutdown,
                                         mongo_client_read_message_cb,
                                         client);

   g_object_unref(message);

   EXIT;
}

static void
mongo_client_set_stream (MongoClient *client,
                         GIOStream   *stream)
{
   MongoClientPrivate *priv;
   GInputStream *base_input;
   GOutputStream *base_output;

   ENTRY;

   g_return_if_fail(MONGO_IS_CLIENT(client));
   g_return_if_fail(G_IS_IO_STREAM(stream));

   priv = client->priv;

   g_clear_object(&priv->input);
   g_clear_object(&priv->output);

   base_input = g_io_stream_get_input_stream(stream);
   base_output = g_io_stream_get_output_stream(stream);

   priv->input = g_object_new(MONGO_TYPE_INPUT_STREAM,
                              "base-stream", base_input,
                              "async-context", priv->async_context,
                              NULL);
   priv->output = g_object_new(MONGO_TYPE_OUTPUT_STREAM,
                               "base-stream", base_output,
                               "async-context", priv->async_context,
                               NULL);

   mongo_input_stream_read_message_async(priv->input,
                                         priv->input_shutdown,
                                         mongo_client_read_message_cb,
                                         client);

   g_object_notify_by_pspec(G_OBJECT(client), gParamSpecs[PROP_STREAM]);

   EXIT;
}

static void
mongo_client_add_async_result (MongoClient        *client,
                               gint32              request_id,
                               GSimpleAsyncResult *simple)
{
   g_assert(MONGO_IS_CLIENT(client));
   g_assert(request_id);
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_hash_table_insert(client->priv->async_results,
                       GINT_TO_POINTER(request_id),
                       g_object_ref(simple));
}

static void
mongo_client_send_write_message_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
   MongoClientPrivate *priv;
   GSimpleAsyncResult *simple = user_data;
   MongoOutputStream *stream = (MongoOutputStream *)object;
   gpointer request_id;
   GObject *client;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_OUTPUT_STREAM(stream));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   client = g_async_result_get_source_object(G_ASYNC_RESULT(simple));
   priv = MONGO_CLIENT(client)->priv;
   request_id = g_object_get_qdata(G_OBJECT(simple), gQuarkRequestId);

   /*
    * This handles the completion of the write of the message to the
    * underlying socket. If that failed, we need to remove the async
    * result from the hashtable of results to be completed by replies.
    * Additionally, if the result does not have a request-id to be
    * responded to, then we are done now. This can happen in fire and
    * forget scenarios as well as OP_MSG, OP_REPLY, etc.
    */

   if (!mongo_output_stream_write_message_finish(stream, result, &error)) {
      if (!IS_COMPLETED(simple)) {
         SET_COMPLETED(simple);
         g_hash_table_remove(priv->async_results, request_id);
         g_simple_async_result_take_error(simple, error);
         mongo_source_complete_in_idle(priv->source, simple);
      } else {
         g_error_free(error);
      }
   } else if (!g_object_get_qdata(G_OBJECT(simple), gQuarkRequestId)) {
      SET_COMPLETED(simple);
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
      mongo_source_complete_in_idle(priv->source, simple);
   }

   g_object_unref(client);
   g_object_unref(simple);

   EXIT;
}

/**
 * mongo_client_send_async:
 * @client: A #MongoClient.
 * @message: A #MongoMessage.
 * @concern: (allow-none): A #MongoWriteConcern or %NULL.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: (allow-none): A callback to execute upon completion.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously sends a message to the MongoDB server.
 * @callback must call mongo_client_send_finish() to complete the request.
 */
void
mongo_client_send_async (MongoClient         *client,
                         MongoMessage        *message,
                         MongoWriteConcern   *concern,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
   MongoClientPrivate *priv;
   GSimpleAsyncResult *simple;
   guint32 request_id;

   g_return_if_fail(MONGO_IS_CLIENT(client));
   g_return_if_fail(MONGO_IS_MESSAGE(message));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));

   priv = client->priv;

   simple = g_simple_async_result_new(G_OBJECT(client), callback, user_data,
                                      mongo_client_send_async);
   g_simple_async_result_set_check_cancellable(simple, cancellable);

   /*
    * TODO: Implement queueing while requests process.
    */

   /*
    * Asynchronously write the message to the output stream. This can fail
    * asynchronously and races with the receiving of the response to complete
    * the GSimpleAsyncResult.
    */
   request_id = mongo_output_stream_write_message_async(priv->output,
                                                        message,
                                                        concern ?: priv->concern,
                                                        cancellable,
                                                        mongo_client_send_write_message_cb,
                                                        simple);
   if (request_id) {
      g_object_set_qdata(G_OBJECT(simple), gQuarkRequestId,
                        GINT_TO_POINTER(request_id));
      mongo_client_add_async_result(client, request_id, simple);
   }

   EXIT;
}

static void
mongo_client_send_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GSimpleAsyncResult **waiter = user_data;
   MongoClient *client = (MongoClient *)object;

   ENTRY;

   g_assert(MONGO_IS_CLIENT(client));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_assert(waiter);

   *waiter = g_object_ref(simple);

   EXIT;
}

/**
 * mongo_client_send:
 * @client: A #MongoClient.
 * @message: A #MongoMessage.
 * @concern: (allow-none): A #MongoWriteConcern or %NULL.
 * @reply: (out): A location for a #MongoMessage or %NULL.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: (out): A location for a #GError or %NULL.
 *
 * Synchronously sends a message to the MongoDB server. If there was a reply
 * from the MongoDB server, it will be placed in @reply. The caller is
 * responsible for freeing it with g_object_unref().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_client_send (MongoClient        *client,
                   MongoMessage       *message,
                   MongoWriteConcern  *concern,
                   MongoMessage      **reply,
                   GCancellable       *cancellable,
                   GError            **error)
{
   GSimpleAsyncResult *simple = NULL;
   MongoMessage *r;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CLIENT(client), FALSE);
   g_return_val_if_fail(MONGO_IS_MESSAGE(message), FALSE);
   g_return_val_if_fail(!reply || !*reply, FALSE);
   g_return_val_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable), FALSE);
   g_return_val_if_fail(!error || !*error, FALSE);

   mongo_client_send_async(client,
                           message,
                           concern,
                           cancellable,
                           mongo_client_send_cb,
                           &simple);

   while (!simple) {
      g_main_context_iteration(client->priv->async_context, TRUE);
   }

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else if (reply) {
      if ((r = g_object_get_qdata(G_OBJECT(simple), gQuarkReply))) {
         *reply = g_object_ref(r);
      } else {
         *reply = NULL;
      }
   }

   RETURN(ret);
}

static void
mongo_client_dispose (GObject *object)
{
   MongoClientPrivate *priv;

   ENTRY;

   priv = MONGO_CLIENT(object)->priv;

   g_cancellable_cancel(priv->input_shutdown);
   g_cancellable_cancel(priv->output_shutdown);

   G_OBJECT_CLASS(mongo_client_parent_class)->dispose(object);

   EXIT;
}

static void
mongo_client_finalize (GObject *object)
{
   MongoClientPrivate *priv;

   ENTRY;

   priv = MONGO_CLIENT(object)->priv;

   g_clear_object(&priv->input);
   g_clear_object(&priv->output);

   if (priv->concern) {
      mongo_write_concern_free(priv->concern);
      priv->concern = NULL;
   }

   if (priv->async_context) {
      g_main_context_unref(priv->async_context);
      priv->async_context = NULL;
   }

   g_clear_object(&priv->input_shutdown);
   g_clear_object(&priv->output_shutdown);

   G_OBJECT_CLASS(mongo_client_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_client_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
   MongoClient *client = MONGO_CLIENT(object);

   switch (prop_id) {
   case PROP_ASYNC_CONTEXT:
      g_value_set_boxed(value, mongo_client_get_async_context(client));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_client_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
   MongoClient *client = MONGO_CLIENT(object);

   switch (prop_id) {
   case PROP_ASYNC_CONTEXT:
      mongo_client_set_async_context(client, g_value_get_boxed(value));
      break;
   case PROP_STREAM:
      mongo_client_set_stream(client, g_value_get_object(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_client_class_init (MongoClientClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_client_finalize;
   object_class->dispose = mongo_client_dispose;
   object_class->get_property = mongo_client_get_property;
   object_class->set_property = mongo_client_set_property;
   g_type_class_add_private(object_class, sizeof(MongoClientPrivate));

   gParamSpecs[PROP_ASYNC_CONTEXT] =
      g_param_spec_boxed("async-context",
                          _("Async Context"),
                          _("The GMainContext to perform callbacks within."),
                          G_TYPE_MAIN_CONTEXT,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_ASYNC_CONTEXT,
                                   gParamSpecs[PROP_ASYNC_CONTEXT]);

   gParamSpecs[PROP_STREAM] =
      g_param_spec_object("stream",
                          _("Stream"),
                          _("The underlying stream."),
                          G_TYPE_IO_STREAM,
                          G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_STREAM,
                                   gParamSpecs[PROP_STREAM]);

   gQuarkCompleted = g_quark_from_static_string("completed");
   gQuarkRequestId = g_quark_from_static_string("request-id");
   gQuarkReply = g_quark_from_static_string("reply");

   EXIT;
}

static void
mongo_client_init (MongoClient *client)
{
   ENTRY;

   client->priv = G_TYPE_INSTANCE_GET_PRIVATE(client,
                                              MONGO_TYPE_CLIENT,
                                              MongoClientPrivate);
   client->priv->concern = mongo_write_concern_new();
   client->priv->async_results = g_hash_table_new(g_direct_hash,
                                                  g_direct_equal);
   client->priv->input_shutdown = g_cancellable_new();
   client->priv->output_shutdown = g_cancellable_new();

   EXIT;
}

GQuark
mongo_client_error_quark (void)
{
   return g_quark_from_static_string("MongoClientError");
}
