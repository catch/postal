/* mongo-input-stream.c
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
#include "mongo-input-stream.h"
#include "mongo-operation.h"
#include "mongo-source.h"

G_DEFINE_TYPE(MongoInputStream, mongo_input_stream, G_TYPE_FILTER_INPUT_STREAM)

struct _MongoInputStreamPrivate
{
   GCancellable *shutdown;
   MongoSource *source;
   gint32 msg_len;
   gint32 to_read;
   guint8 *buffer;
};

enum
{
   PROP_0,
   PROP_ASYNC_CONTEXT,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

MongoInputStream *
mongo_input_stream_new (GInputStream *base_stream)
{
   return g_object_new(MONGO_TYPE_INPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}

GMainContext *
mongo_input_stream_get_async_context (MongoInputStream *stream)
{
   g_return_val_if_fail(MONGO_IS_INPUT_STREAM(stream), NULL);
   return g_source_get_context((GSource *)stream->priv->source);
}

static void
mongo_input_stream_set_async_context (MongoInputStream *stream,
                                      GMainContext     *async_context)
{
   MongoInputStreamPrivate *priv;

   ENTRY;

   g_return_if_fail(MONGO_IS_INPUT_STREAM(stream));
   g_return_if_fail(!stream->priv->source);

   priv = stream->priv;

   if (!async_context) {
      async_context = g_main_context_default();
   }

   priv->source = mongo_source_new();
   g_source_set_name((GSource *)priv->source, "MongoInputStream");
   g_source_attach((GSource *)priv->source, async_context);

   g_object_notify_by_pspec(G_OBJECT(stream), gParamSpecs[PROP_ASYNC_CONTEXT]);

   EXIT;
}

static void
mongo_input_stream_read_message_body_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
   MongoInputStreamPrivate *priv;
   GSimpleAsyncResult *simple = user_data;
   MongoInputStream *input = (MongoInputStream *)object;
   MongoMessage *message = NULL;
   GError *error = NULL;
   gssize ret;
   GType type_id;
#pragma pack(push, 1)
   struct {
      gint32 msg_len;
      gint32 request_id;
      gint32 response_to;
      gint32 op_code;
   } header;
#pragma pack(pop)

   ENTRY;

   g_assert(MONGO_IS_INPUT_STREAM(input));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   priv = input->priv;

   ret = g_input_stream_read_finish(G_INPUT_STREAM(input), result, &error);
   if (ret == -1) {
      if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
         g_simple_async_result_take_error(simple, error);
      }
      GOTO(failure);
   } else if (ret == 0) {
      /* End of stream */
      GOTO(failure);
   } else if (ret != priv->to_read) {
      /* Short read! */
      priv->to_read -= ret;
      g_assert_cmpint(priv->msg_len + priv->to_read, >, 0);
      g_assert_cmpint(priv->msg_len, >, 0);
      g_assert_cmpint(priv->to_read, >, 0);
      g_input_stream_read_async(G_INPUT_STREAM(input),
                                priv->buffer + (priv->msg_len - priv->to_read),
                                priv->to_read,
                                G_PRIORITY_DEFAULT,
                                priv->shutdown,
                                mongo_input_stream_read_message_body_cb,
                                simple);
      EXIT;
   }

   memcpy(&header, priv->buffer, sizeof header);
   if (!(type_id = mongo_operation_get_message_type(header.op_code))) {
      GOTO(failure);
   }

   header.msg_len = GINT32_FROM_LE(header.msg_len);
   header.request_id = GINT32_FROM_LE(header.request_id);
   header.response_to = GINT32_FROM_LE(header.response_to);
   header.op_code = GINT32_FROM_LE(header.op_code);

   DUMP_BYTES(buffer, priv->buffer, priv->msg_len);

   message = g_object_new(type_id,
                          "request-id", header.request_id,
                          "response-to", header.response_to,
                          NULL);
   if (!mongo_message_load_from_data(message,
                                     priv->buffer + sizeof header, // FIXME: load all data.
                                     priv->msg_len - sizeof header)) {
      GOTO(failure);
   }

   g_simple_async_result_set_op_res_gpointer(simple, message, g_object_unref);
   mongo_source_complete_in_idle(priv->source, simple);
   g_object_unref(simple);
   g_free(priv->buffer);
   priv->buffer = NULL;
   priv->to_read = 0;
   priv->msg_len = 0;

   EXIT;

failure:
   g_input_stream_close(G_INPUT_STREAM(input), NULL, NULL);
   mongo_source_complete_in_idle(input->priv->source, simple);
   g_object_unref(simple);
   g_free(priv->buffer);
   priv->buffer = NULL;
   g_clear_object(&message);
   priv->to_read = 0;
   priv->msg_len = 0;

   EXIT;
}

static void
mongo_input_stream_read_message_header_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
   MongoInputStreamPrivate *priv;
   GSimpleAsyncResult *simple = user_data;
   MongoInputStream *input = (MongoInputStream *)object;
   gint32 msg_len_le;
   GError *error = NULL;
   gssize ret;

   ENTRY;

   g_assert(MONGO_IS_INPUT_STREAM(input));
   g_assert(G_IS_ASYNC_RESULT(result));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   priv = input->priv;

   ret = g_input_stream_read_finish(G_INPUT_STREAM(input), result, &error);
   if (ret != sizeof priv->msg_len) {
      g_input_stream_close(G_INPUT_STREAM(input), NULL, NULL);
      if (error) {
         g_simple_async_result_take_error(simple, error);
      } else {
         g_simple_async_result_set_error(simple,
                                         G_IO_ERROR,
                                         G_IO_ERROR_CLOSED,
                                         _("The stream is closed."));
      }
      mongo_source_complete_in_idle(input->priv->source, simple);
      g_object_unref(simple);
      EXIT;
   }

   msg_len_le = priv->msg_len;
   priv->msg_len = GINT32_FROM_LE(priv->msg_len);

   if (priv->msg_len <= 16) {
      g_simple_async_result_set_error(simple,
                                      MONGO_INPUT_STREAM_ERROR,
                                      MONGO_INPUT_STREAM_ERROR_INSUFFICIENT_DATA,
                                      _("Insufficient data for message."));
      mongo_source_complete_in_idle(input->priv->source, simple);
      g_object_unref(simple);
      EXIT;
   }

   priv->buffer = g_malloc(priv->msg_len);
   memcpy(priv->buffer, &msg_len_le, sizeof msg_len_le);
   priv->to_read = priv->msg_len - 4;

   g_input_stream_read_async(G_INPUT_STREAM(input),
                             priv->buffer + (priv->msg_len - priv->to_read),
                             priv->to_read,
                             G_PRIORITY_DEFAULT,
                             priv->shutdown,
                             mongo_input_stream_read_message_body_cb,
                             simple);

   EXIT;
}

/**
 * mongo_input_stream_read_message_async:
 * @stream: A #MongoInputStream.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: (allow-none): A #GAsyncReadyCallback or %NULL.
 * @user_data: user data for @callback.
 *
 * Asynchronously reads the next message from the #MongoInputStream.
 */
void
mongo_input_stream_read_message_async (MongoInputStream    *stream,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
   MongoInputStreamPrivate *priv;
   GSimpleAsyncResult *simple;

   ENTRY;

   g_return_if_fail(MONGO_IS_INPUT_STREAM(stream));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));

   priv = stream->priv;

   simple = g_simple_async_result_new(G_OBJECT(stream), callback, user_data,
                                      mongo_input_stream_read_message_async);
   g_simple_async_result_set_check_cancellable(simple, cancellable);

   g_input_stream_read_async(G_INPUT_STREAM(stream),
                             &priv->msg_len,
                             sizeof priv->msg_len,
                             G_PRIORITY_DEFAULT,
                             priv->shutdown,
                             mongo_input_stream_read_message_header_cb,
                             simple);

   EXIT;
}

/**
 * mongo_input_stream_read_message_finish:
 * @stream: A #MongoInputStream.
 *
 * Completes an asynchronous request to read the next message from the
 * underlying #GInputStream.
 *
 * Upon failure, %NULL is returned and @error is set.
 *
 * Returns: (transfer full): A #MongoMessage if successful; otherwise
 *    %NULL and @error is set.
 */
MongoMessage *
mongo_input_stream_read_message_finish (MongoInputStream  *stream,
                                        GAsyncResult      *result,
                                        GError           **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessage *ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_INPUT_STREAM(stream), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      g_object_ref(ret);
   }

   RETURN(ret);
}

static void
mongo_input_stream_read_message_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
   MongoInputStream *stream = (MongoInputStream *)object;
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GSimpleAsyncResult **waiter = user_data;

   ENTRY;

   g_assert(MONGO_IS_INPUT_STREAM(stream));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_assert(waiter);

   *waiter = g_object_ref(simple);

   EXIT;
}

/**
 * mongo_input_stream_read_message:
 * @stream: A #MongoInputStream.
 * @cancellable: (allow-none): A #GCancellable, or %NULL.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Read the next incoming message from the underlying #GInputStream. This
 * will block while reading from the stream.
 *
 * Upon error, %NULL is returned and @error is set.
 *
 * Returns: (transfer full): A #MongoInputStream if successful. Otherwise
 *   %NULL and @error is set.
 */
MongoMessage *
mongo_input_stream_read_message (MongoInputStream  *stream,
                                 GCancellable      *cancellable,
                                 GError           **error)
{
   GSimpleAsyncResult *simple = NULL;
   GMainContext *main_context;
   MongoMessage *ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_INPUT_STREAM(stream), NULL);
   g_return_val_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable), NULL);

   mongo_input_stream_read_message_async(stream,
                                         cancellable,
                                         mongo_input_stream_read_message_cb,
                                         &simple);

   main_context = mongo_input_stream_get_async_context(stream);

   while (!simple) {
      g_main_context_iteration(main_context, TRUE);
   }

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      g_object_ref(ret);
   }

   g_object_unref(simple);

   RETURN(ret);
}

static void
mongo_input_stream_dispose (GObject *object)
{
   MongoInputStreamPrivate *priv;

   ENTRY;

   priv = MONGO_INPUT_STREAM(object)->priv;

   g_cancellable_cancel(priv->shutdown);

   G_OBJECT_CLASS(mongo_input_stream_parent_class)->dispose(object);

   EXIT;
}

static void
mongo_input_stream_finalize (GObject *object)
{
   MongoInputStreamPrivate *priv;

   ENTRY;

   priv = MONGO_INPUT_STREAM(object)->priv;

   g_source_destroy((GSource *)priv->source);
   priv->source = NULL;

   G_OBJECT_CLASS(mongo_input_stream_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_input_stream_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
   MongoInputStream *stream = MONGO_INPUT_STREAM(object);

   switch (prop_id) {
   case PROP_ASYNC_CONTEXT:
      g_value_set_boxed(value, mongo_input_stream_get_async_context(stream));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_input_stream_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
   MongoInputStream *stream = MONGO_INPUT_STREAM(object);

   switch (prop_id) {
   case PROP_ASYNC_CONTEXT:
      mongo_input_stream_set_async_context(stream, g_value_get_boxed(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_input_stream_class_init (MongoInputStreamClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_input_stream_finalize;
   object_class->dispose = mongo_input_stream_dispose;
   object_class->get_property = mongo_input_stream_get_property;
   object_class->set_property = mongo_input_stream_set_property;
   g_type_class_add_private(object_class, sizeof(MongoInputStreamPrivate));

   gParamSpecs[PROP_ASYNC_CONTEXT] =
      g_param_spec_boxed("async-context",
                          _("Async Context"),
                          _("The main context to perform callbacks within."),
                          G_TYPE_MAIN_CONTEXT,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_ASYNC_CONTEXT,
                                   gParamSpecs[PROP_ASYNC_CONTEXT]);

   EXIT;
}

static void
mongo_input_stream_init (MongoInputStream *stream)
{
   ENTRY;
   stream->priv = G_TYPE_INSTANCE_GET_PRIVATE(stream,
                                              MONGO_TYPE_INPUT_STREAM,
                                              MongoInputStreamPrivate);
   stream->priv->shutdown = g_cancellable_new();
   EXIT;
}

GQuark
mongo_input_stream_error_quark (void)
{
   return g_quark_from_static_string("MongoInputStreamError");
}
