/* mongo-output-stream.c
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

#include "mongo-debug.h"
#include "mongo-message-query.h"
#include "mongo-output-stream.h"
#include "mongo-source.h"
#include "mongo-write-concern.h"

/*
 * TODO:
 *
 *   - Make sure that multiple operations cannot happen concurrently.
 *     Only one async or sync command at a time.
 */

G_DEFINE_TYPE(MongoOutputStream, mongo_output_stream, G_TYPE_FILTER_OUTPUT_STREAM)

struct _MongoOutputStreamPrivate
{
   MongoSource  *source;
   GCancellable *shutdown;
   GHashTable   *async_results;
   GQueue       *queue;
   gint32        next_request_id;
   gboolean      flushing;
};

typedef struct
{
   GSimpleAsyncResult *simple;
   GBytes             *bytes;
   gboolean            ignore_error;
} Request;

enum
{
   PROP_0,
   PROP_ASYNC_CONTEXT,
   PROP_NEXT_REQUEST_ID,
   LAST_PROP
};

enum
{
   COMPLETE_0,
   COMPLETE_ON_WRITE,
   COMPLETE_ON_REPLY,
   COMPLETE_ON_GETLASTERROR,
   LAST_COMPLETE
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
request_free (Request *request)
{
   if (request) {
      g_clear_object(&request->simple);
      if (request->bytes) {
         g_bytes_unref(request->bytes);
         request->bytes = NULL;
      }
      request->ignore_error = FALSE;
      g_slice_free(Request, request);
   }
}

MongoOutputStream *
mongo_output_stream_new (GOutputStream *base_stream)
{
   return g_object_new(MONGO_TYPE_OUTPUT_STREAM,
                       "base-stream", base_stream,
                       "next-request-id", g_random_int_range(1, G_MAXINT32),
                       NULL);
}

GMainContext *
mongo_output_stream_get_async_context (MongoOutputStream *stream)
{
   g_return_val_if_fail(MONGO_IS_OUTPUT_STREAM(stream), NULL);
   if (stream->priv->source)
      return g_source_get_context((GSource *)stream->priv->source);
   return NULL;
}

static void
mongo_output_stream_set_async_context (MongoOutputStream *stream,
                                       GMainContext      *async_context)
{
   MongoOutputStreamPrivate *priv;

   g_return_if_fail(MONGO_IS_OUTPUT_STREAM(stream));
   g_return_if_fail(!stream->priv->source);

   priv = stream->priv;

   if (!async_context) {
      async_context = g_main_context_default();
   }

   priv->source = mongo_source_new();
   g_source_set_name((GSource *)priv->source, "MongoOutputStream");
   g_source_attach((GSource *)priv->source, async_context);

   g_object_notify_by_pspec(G_OBJECT(stream), gParamSpecs[PROP_ASYNC_CONTEXT]);
}

static gint32
mongo_output_stream_get_next_request_id (MongoOutputStream *stream)
{
   g_return_val_if_fail(MONGO_IS_OUTPUT_STREAM(stream), -1);
   if (stream->priv->next_request_id == G_MAXINT32)
      stream->priv->next_request_id = 1;
   return stream->priv->next_request_id++;
}

static void
mongo_output_stream_add_async_result (MongoOutputStream  *stream,
                                      gint32              request_id,
                                      GSimpleAsyncResult *simple)
{
   g_return_if_fail(MONGO_IS_OUTPUT_STREAM(stream));
   g_return_if_fail(request_id >= 0);
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_hash_table_insert(stream->priv->async_results,
                       GINT_TO_POINTER(request_id),
                       g_object_ref(simple));
}

static void
mongo_output_stream_queue (MongoOutputStream  *stream,
                           GSimpleAsyncResult *simple,
                           GBytes             *bytes,
                           gboolean            ignore_error)
{
   Request *request;

   ENTRY;

   g_return_if_fail(MONGO_IS_OUTPUT_STREAM(stream));
   g_return_if_fail(!simple || G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_return_if_fail(bytes);

   request = g_slice_new(Request);
   request->simple = simple ? g_object_ref(simple) : NULL;
   request->bytes = g_bytes_ref(bytes);
   request->ignore_error = ignore_error;

   g_queue_push_head(stream->priv->queue, request);

   EXIT;
}

static void
mongo_output_stream_flush_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   MongoOutputStreamPrivate *priv;
   MongoOutputStream *stream = (MongoOutputStream *)object;
   GOutputStream *output = (GOutputStream *)object;
   const guint8 *buf;
   Request *request = user_data;
   GError *error = NULL;
   gssize bytes_written;
   gsize expected;
   gsize buflen;

   ENTRY;

   g_assert(G_IS_OUTPUT_STREAM(output));
   g_assert(MONGO_IS_OUTPUT_STREAM(stream));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(request);

   priv = stream->priv;

   expected = g_bytes_get_size(request->bytes);
   bytes_written = g_output_stream_write_finish(output, result, &error);

   if (bytes_written != expected) {
      g_output_stream_close(output, NULL, NULL);
      if (request->simple && !request->ignore_error) {
         if (!error) {
            error = g_error_new(MONGO_OUTPUT_STREAM_ERROR,
                                MONGO_OUTPUT_STREAM_ERROR_SHORT_WRITE,
                                _("Failed to write all data to stream."));
         }
         g_simple_async_result_take_error(request->simple, error);
         mongo_source_complete_in_idle(priv->source, request->simple);
      }
      request_free(request);
      priv->flushing = FALSE;
      EXIT;
   }

   if (request->simple) {
      g_simple_async_result_set_op_res_gboolean(request->simple, TRUE);
      mongo_source_complete_in_idle(priv->source, request->simple);
   }

   request_free(request);

   if (!(request = g_queue_pop_tail(priv->queue))) {
      priv->flushing = FALSE;
      EXIT;
   }

   buf = g_bytes_get_data(request->bytes, &buflen);

   g_output_stream_write_async(G_OUTPUT_STREAM(stream),
                               buf,
                               buflen,
                               G_PRIORITY_DEFAULT,
                               priv->shutdown,
                               mongo_output_stream_flush_cb,
                               request);

   EXIT;
}

static void
mongo_output_stream_flush (MongoOutputStream *stream)
{
   MongoOutputStreamPrivate *priv;
   const guint8 *buf;
   Request *request;
   gsize buflen;

   ENTRY;

   g_return_if_fail(MONGO_IS_OUTPUT_STREAM(stream));

   priv = stream->priv;

   if (priv->flushing) {
      EXIT;
   }

   if (!(request = g_queue_pop_tail(priv->queue))) {
      EXIT;
   }

   priv->flushing = TRUE;

   buf = g_bytes_get_data(request->bytes, &buflen);

   g_output_stream_write_async(G_OUTPUT_STREAM(stream),
                               buf,
                               buflen,
                               G_PRIORITY_DEFAULT,
                               priv->shutdown,
                               mongo_output_stream_flush_cb,
                               request);

   EXIT;
}

/**
 * mongo_output_stream_write_message_async:
 * @stream: A #MongoOutputStream.
 * @message: A #MongoMessage.
 * @concern: A #MongoWriteConcern or %NULL.
 * @cancellable : (allow-none): A #GCancellable or %NULL.
 * @callback: (allow-none): A #GAsyncReadyCallback or %NULL.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously writes a message to the underlying stream. If a write concern
 * is provided, then a supplimental getlasterror command will be written if
 * necessary.
 *
 * Returns: 0 if no reply is expected, otherwise of the request-id of the
 *   request to expect a reply for. In some cases this may be for a
 *   supplimental getlasterror command.
 */
gint32
mongo_output_stream_write_message_async (MongoOutputStream   *stream,
                                         MongoMessage        *message,
                                         MongoWriteConcern   *concern,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
   MongoOutputStreamPrivate *priv;
   GSimpleAsyncResult *simple;
   MongoMessage *gle;
   gboolean ignore_error;
   GError *error = NULL;
   GBytes *bytes;
   gint32 request_id;
   guint complete;

   g_return_val_if_fail(MONGO_IS_OUTPUT_STREAM(stream), 0);
   g_return_val_if_fail(MONGO_IS_MESSAGE(message), 0);
   g_return_val_if_fail(concern, 0);
   g_return_val_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable), 0);

   priv = stream->priv;

   /*
    * Build our GAsyncResult to be used when completing this async request.
    */
   simple = g_simple_async_result_new(G_OBJECT(stream), callback, user_data,
                                      mongo_output_stream_write_message_async);
   g_simple_async_result_set_check_cancellable(simple, cancellable);

   /*
    * Get the next request-id for this message.
    */
   request_id = mongo_output_stream_get_next_request_id(stream);
   mongo_message_set_request_id(message, request_id);

   /*
    * Determine how this should be completed. Some require completion at
    * different stages.
    */
   switch (MONGO_MESSAGE_GET_CLASS(message)->operation) {
   case MONGO_OPERATION_QUERY:
   case MONGO_OPERATION_GETMORE:
      complete = COMPLETE_ON_REPLY;
      break;
   case MONGO_OPERATION_KILL_CURSORS:
   case MONGO_OPERATION_MSG:
   case MONGO_OPERATION_REPLY:
      complete = COMPLETE_ON_WRITE;
      request_id = 0;
      break;
   case MONGO_OPERATION_UPDATE:
   case MONGO_OPERATION_INSERT:
   case MONGO_OPERATION_DELETE:
      complete = COMPLETE_ON_GETLASTERROR;
      request_id = mongo_output_stream_get_next_request_id(stream);
      break;
   default:
      g_assert_not_reached();
      RETURN(0);
   }

   /*
    * Save the message to GBytes to be delivered to the MongoDB server.
    */
   if (!(bytes = mongo_message_save_to_bytes(message, &error))) {
      g_simple_async_result_take_error(simple, error);
      mongo_source_complete_in_idle(priv->source, simple);
      g_object_unref(simple);
      RETURN(0);
   }

   /*
    * Add GAsyncResult to be fired after the request has been written to
    * the underlying stream.
    */
   mongo_output_stream_add_async_result(stream, request_id, simple);

   /*
    * Queue the bytes to be written to the underlying stream.
    */
   ignore_error = (mongo_write_concern_get_w(concern) == -1);
   mongo_output_stream_queue(stream, simple, bytes, ignore_error);
   g_bytes_unref(bytes);

   /*
    * If this message requires a getlasterror to retrieve the completion,
    * then send that now.
    */
   if (complete == COMPLETE_ON_GETLASTERROR) {
      gle = mongo_write_concern_build_getlasterror(concern, "admin"); // FIXME
      g_assert(MONGO_IS_MESSAGE_QUERY(gle));

      bytes = mongo_message_save_to_bytes(gle, NULL);
      g_assert(bytes);

      mongo_output_stream_queue(stream, NULL, bytes, FALSE);

      g_bytes_unref(bytes);
      g_object_unref(gle);
   }

   /*
    * If we do not currently have a write loop, then start the asynchronous
    * write loop now.
    */
   mongo_output_stream_flush(stream);

   RETURN(request_id);
}

/**
 * mongo_output_stream_write_message_finish:
 * @stream: A #MongoOutputStream.
 *
 * Completes an asynchronous request to write a message to the output stream.
 * If no #MongoWriteConcern was provided to
 * mongo_output_stream_write_message_async() or a write concern w=-1, then
 * this will never fail. However, if w=0 or more, then socket errors can
 * cause this to fail.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_output_stream_write_message_finish (MongoOutputStream  *stream,
                                          GAsyncResult       *result,
                                          GError            **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_OUTPUT_STREAM(stream), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   ret = !g_simple_async_result_propagate_error(simple, error);
   RETURN(ret);
}

static void
mongo_output_stream_write_message_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
   MongoOutputStream *stream = (MongoOutputStream *)object;
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GSimpleAsyncResult **waiter = user_data;

   ENTRY;

   g_assert(MONGO_IS_OUTPUT_STREAM(stream));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_assert(waiter);

   *waiter = g_object_ref(simple);

   EXIT;
}

gboolean
mongo_output_stream_write_message (MongoOutputStream  *stream,
                                   MongoMessage       *message,
                                   MongoWriteConcern  *concern,
                                   GCancellable       *cancellable,
                                   GError            **error)
{
   GSimpleAsyncResult *simple = NULL;
   GMainContext *main_context;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_OUTPUT_STREAM(stream), FALSE);
   g_return_val_if_fail(MONGO_IS_MESSAGE(message), FALSE);
   g_return_val_if_fail(concern, FALSE);
   g_return_val_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable), FALSE);

   mongo_output_stream_write_message_async(stream,
                                           message,
                                           concern,
                                           cancellable,
                                           mongo_output_stream_write_message_cb,
                                           &simple);

   main_context = mongo_output_stream_get_async_context(stream);

   while (!simple) {
      g_main_context_iteration(main_context, TRUE);
   }

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   g_object_unref(simple);

   RETURN(ret);
}

static void
mongo_output_stream_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
   MongoOutputStream *stream = MONGO_OUTPUT_STREAM(object);

   switch (prop_id) {
   case PROP_ASYNC_CONTEXT:
      g_value_set_boxed(value, mongo_output_stream_get_async_context(stream));
      break;
   case PROP_NEXT_REQUEST_ID:
      g_value_set_int(value, stream->priv->next_request_id);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_output_stream_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
   MongoOutputStream *stream = MONGO_OUTPUT_STREAM(object);

   switch (prop_id) {
   case PROP_ASYNC_CONTEXT:
      mongo_output_stream_set_async_context(stream, g_value_get_boxed(value));
      break;
   case PROP_NEXT_REQUEST_ID:
      stream->priv->next_request_id = g_value_get_int(value);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_output_stream_dispose (GObject *object)
{
   MongoOutputStreamPrivate *priv;
   GSimpleAsyncResult *simple;
   GHashTableIter iter;
   GHashTable *hashtable;
   Request *request;
   GQueue *queue;

   ENTRY;

   priv = MONGO_OUTPUT_STREAM(object)->priv;

   if (priv->shutdown) {
      g_cancellable_cancel(priv->shutdown);
   }

   hashtable = priv->async_results;
   priv->async_results = g_hash_table_new(g_int_hash, g_int_equal);

   g_hash_table_iter_init(&iter, hashtable);
   while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&simple)) {
      g_simple_async_result_set_error(simple,
                                      G_IO_ERROR,
                                      G_IO_ERROR_CANCELLED,
                                      _("The request was cancelled."));
      mongo_source_complete_in_idle(priv->source, simple);
      g_object_unref(simple);
   }
   g_hash_table_unref(hashtable);

   queue = priv->queue;
   priv->queue = g_queue_new();

   while ((request = g_queue_pop_tail(queue))) {
      g_simple_async_result_set_error(request->simple,
                                      G_IO_ERROR,
                                      G_IO_ERROR_CANCELLED,
                                      _("The request was cancelled."));
      request_free(request);
   }

   G_OBJECT_CLASS(mongo_output_stream_parent_class)->dispose(object);

   EXIT;
}

static void
mongo_output_stream_finalize (GObject *object)
{
   MongoOutputStreamPrivate *priv;

   ENTRY;

   priv = MONGO_OUTPUT_STREAM(object)->priv;

   if (g_hash_table_size(priv->async_results)) {
      g_warning("%s() called with in flight requests.", G_STRFUNC);
   }

   if (!g_queue_is_empty(priv->queue)) {
      g_warning("%s() called with queued requests.", G_STRFUNC);
   }

   g_hash_table_unref(priv->async_results);
   priv->async_results = NULL;

   g_source_destroy((GSource *)priv->source);
   priv->source = NULL;

   g_queue_free(priv->queue);
   priv->queue = NULL;

   g_clear_object(&priv->shutdown);

   G_OBJECT_CLASS(mongo_output_stream_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_output_stream_class_init (MongoOutputStreamClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->dispose = mongo_output_stream_dispose;
   object_class->finalize = mongo_output_stream_finalize;
   object_class->get_property = mongo_output_stream_get_property;
   object_class->set_property = mongo_output_stream_set_property;
   g_type_class_add_private(object_class, sizeof(MongoOutputStreamPrivate));

   gParamSpecs[PROP_ASYNC_CONTEXT] =
      g_param_spec_boxed("async-context",
                          _("Async Context"),
                          _("The GMainContext to perform callbacks within."),
                          G_TYPE_MAIN_CONTEXT,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_ASYNC_CONTEXT,
                                   gParamSpecs[PROP_ASYNC_CONTEXT]);

   gParamSpecs[PROP_NEXT_REQUEST_ID] =
      g_param_spec_int("next-request-id",
                       _("Next Request Id"),
                       _("The next request id to use."),
                       0,
                       G_MAXINT32,
                       0,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_NEXT_REQUEST_ID,
                                   gParamSpecs[PROP_NEXT_REQUEST_ID]);

   EXIT;
}

static void
mongo_output_stream_init (MongoOutputStream *stream)
{
   ENTRY;
   stream->priv = G_TYPE_INSTANCE_GET_PRIVATE(stream,
                                              MONGO_TYPE_OUTPUT_STREAM,
                                              MongoOutputStreamPrivate);
   stream->priv->async_results = g_hash_table_new(g_direct_hash,
                                                  g_direct_equal);
   stream->priv->queue = g_queue_new();
   stream->priv->shutdown = g_cancellable_new();
   EXIT;
}

GQuark
mongo_output_stream_error_quark (void)
{
   return g_quark_from_static_string("MongoOutputStream");
}
