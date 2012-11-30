/* mongo-protocol.c
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
#include "mongo-protocol.h"
#include "mongo-source.h"

/**
 * SECTION:mongo-protocol
 * @title: MongoProtocol
 * @short_description: Wire protocol communication for Mongo.
 *
 * #MongoProtocol encapsulates the wire protocol for Mongo DB.
 * It uses a #GIOStream for communication. Typically, this
 * is used by #MongoConnection but can be used directly if necessary.
 */

G_DEFINE_TYPE(MongoProtocol, mongo_protocol, G_TYPE_OBJECT)

static void
mongo_protocol_fill_header_cb (GBufferedInputStream *input_stream,
                               GAsyncResult         *result,
                               MongoProtocol        *protocol);

struct _MongoProtocolPrivate
{
   GIOStream *io_stream;
   GInputStream *input_stream;
   GOutputStream *output_stream;
   guint32 last_request_id;
   GCancellable *shutdown;
   GHashTable *requests;
   gboolean getlasterror_fsync;
   gint getlasterror_w;
   gint getlasterror_wtimeoutms;
   gboolean getlasterror_j;
   gboolean safe;
};

enum
{
   PROP_0,
   PROP_FSYNC,
   PROP_IO_STREAM,
   PROP_JOURNAL,
   PROP_SAFE,
   PROP_WRITE_QUORUM,
   PROP_WRITE_TIMEOUT,
   LAST_PROP
};

enum
{
   FAILED,
   LAST_SIGNAL
};

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];

static void
mongo_protocol_append_bson (GByteArray      *array,
                            const MongoBson *bson)
{
   ENTRY;
   g_byte_array_append(array, bson->data, bson->len);
   EXIT;
}

static void
mongo_protocol_append_cstring (GByteArray  *array,
                               const gchar *value)
{
   ENTRY;
   g_byte_array_append(array, (guint8 *)value, strlen(value) + 1);
   EXIT;
}

static void
mongo_protocol_append_int32 (GByteArray *array,
                             gint32      value)
{
   ENTRY;
   g_byte_array_append(array, (guint8 *)&value, sizeof value);
   EXIT;
}

static void
mongo_protocol_append_int64 (GByteArray *array,
                            gint64      value)
{
   ENTRY;
   g_byte_array_append(array, (guint8 *)&value, sizeof value);
   EXIT;
}

static void
mongo_protocol_overwrite_int32 (GByteArray *array,
                                guint       offset,
                                gint32      value)
{
   ENTRY;
   g_assert_cmpint(offset, <, array->len);
   memcpy(array->data + offset, &value, sizeof value);
   EXIT;
}

void
mongo_protocol_fail (MongoProtocol *protocol,
                     const GError  *error)
{
   MongoProtocolPrivate *priv;
   GHashTableIter iter;
   gpointer key;
   gpointer value;
   GError *local_error;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));

   priv = protocol->priv;

   if (error) {
      local_error = g_error_copy(error);
   } else {
      local_error = g_error_new(MONGO_PROTOCOL_ERROR,
                                MONGO_PROTOCOL_ERROR_UNEXPECTED,
                                _("An unexpected failure occurred."));
   }

   g_hash_table_iter_init(&iter, priv->requests);
   while (g_hash_table_iter_next(&iter, &key, &value)) {
      g_simple_async_result_set_from_error(value, local_error);
      mongo_simple_async_result_complete_in_idle(value);
   }

   g_hash_table_remove_all(priv->requests);

   g_signal_emit(protocol, gSignals[FAILED], 0, local_error);

   g_error_free(local_error);

   if (priv->io_stream && !g_io_stream_is_closed(priv->io_stream)) {
      g_io_stream_close(priv->io_stream, NULL, NULL);
   }

   EXIT;
}

void
mongo_protocol_flush_sync (MongoProtocol *protocol)
{
   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));

   if (!g_output_stream_has_pending(protocol->priv->output_stream)) {
      g_output_stream_flush(protocol->priv->output_stream, NULL, NULL);
   }

   EXIT;
}

static void
mongo_protocol_write (MongoProtocol      *protocol,
                      guint32             request_id,
                      GSimpleAsyncResult *simple,
                      const guint8       *buffer,
                      gsize               buffer_len)
{
   MongoProtocolPrivate *priv;
   GError *error = NULL;
   gsize n_written = 0;

   ENTRY;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(request_id);
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));
   g_assert(buffer);
   g_assert(buffer_len);

   priv = protocol->priv;

   DUMP_BYTES(buffer, buffer, buffer_len);

   if (!g_output_stream_write_all(priv->output_stream,
                                  buffer,
                                  buffer_len,
                                  &n_written,
                                  NULL, &error)) {
      mongo_protocol_fail(protocol, error);
      g_simple_async_result_take_error(simple, error);
      mongo_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      EXIT;
   }

   mongo_protocol_flush_sync(protocol);

   EXIT;
}

static void
mongo_protocol_append_getlasterror (MongoProtocol *protocol,
                                    GByteArray    *array,
                                    const gchar   *db_and_collection)
{
   MongoProtocolPrivate *priv;
   MongoBson *bson;
   guint32 request_id;
   gchar **split;
   gchar *db_cmd;
   guint offset;

   ENTRY;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(array);
   g_assert(db_and_collection);

   /*
    * TODO: If the user did not request safe=True, we should either make us not
    *       call this and return success immediately or handle it here for
    *       everything and synthesize the result here.
    */

   priv = protocol->priv;

   offset = array->len;
   request_id = ++protocol->priv->last_request_id;

   /*
    * Build getlasterror command spec.
    */
   bson = mongo_bson_new_empty();
   mongo_bson_append_int(bson, "getlasterror", 1);
   mongo_bson_append_boolean(bson, "j", priv->getlasterror_j);
   if (priv->getlasterror_w < 0) {
      mongo_bson_append_string(bson, "w", "majority");
   } else if (priv->getlasterror_w > 0) {
      mongo_bson_append_int(bson, "w", priv->getlasterror_w);
   }
   if (priv->getlasterror_wtimeoutms) {
      mongo_bson_append_int(bson, "wtimeout", priv->getlasterror_wtimeoutms);
   }
   if (priv->getlasterror_fsync) {
      mongo_bson_append_boolean(bson, "fsync", priv->getlasterror_fsync);
   }

   split = g_strsplit(db_and_collection, ".", 2);
   db_cmd = g_strdup_printf("%s.$cmd", split[0]);
   g_strfreev(split);

   /*
    * Build the MONGO_OPERATION_QUERY message.
    */
   mongo_protocol_append_int32(array, 0);
   mongo_protocol_append_int32(array, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(array, 0);
   mongo_protocol_append_int32(array, GINT32_TO_LE(MONGO_OPERATION_QUERY));
   mongo_protocol_append_int32(array, GINT32_TO_LE(MONGO_QUERY_NONE));
   mongo_protocol_append_cstring(array, db_cmd);
   mongo_protocol_append_int32(array, 0);
   mongo_protocol_append_int32(array, 1);
   mongo_protocol_append_bson(array, bson);
   mongo_protocol_overwrite_int32(array, offset,
                                  GINT32_TO_LE(array->len - offset));

   g_free(db_cmd);
   mongo_bson_unref(bson);

   EXIT;
}

void
mongo_protocol_update_async (MongoProtocol       *protocol,
                             const gchar         *db_and_collection,
                             MongoUpdateFlags     flags,
                             const MongoBson     *selector,
                             const MongoBson     *update,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(selector);
   g_return_if_fail(update);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_update_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_UPDATE));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_cstring(buffer, db_and_collection);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(flags));
   mongo_protocol_append_bson(buffer, selector);
   mongo_protocol_append_bson(buffer, update);
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));
   mongo_protocol_append_getlasterror(protocol, buffer, db_and_collection);

   /*
    * We get our response from the getlasterror command, so use it's request
    * id as the key in the hashtable.
    */
   g_hash_table_insert(priv->requests,
                       GINT_TO_POINTER(request_id + 1),
                       simple);

   /*
    * Write the bytes to the buffered stream.
    */
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

/**
 * mongo_protocol_update_finish:
 * @protocol: (in): A #MongoProtocol.
 * @result: (in): A #GAsyncResult.
 * @updated_existing: (out) (allow-none): A location for a #gboolean.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to update the MongoDB database.
 * If @updated_existing is not %NULL, then it will be set to %TRUE if a
 * document was updated. Otherwise %FALSE.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_protocol_update_finish (MongoProtocol  *protocol,
                              GAsyncResult   *result,
                              gboolean       *updated_existing,
                              GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *reply;
   MongoBsonIter iter;
   GList *list;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(reply = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   if (updated_existing) {
      *updated_existing = FALSE;
      if ((list = mongo_message_reply_get_documents(reply))) {
         if (mongo_bson_iter_init_find(&iter, list->data, "updatedExisting")) {
            if (MONGO_BSON_ITER_HOLDS_BOOLEAN(&iter)) {
               *updated_existing = mongo_bson_iter_get_value_boolean(&iter);
            }
         }
      }
   }

   RETURN(!!reply);
}

void
mongo_protocol_insert_async (MongoProtocol        *protocol,
                             const gchar          *db_and_collection,
                             MongoInsertFlags      flags,
                             MongoBson           **documents,
                             gsize                 n_documents,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;
   guint i;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(documents);
   g_return_if_fail(n_documents >= 1);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_insert_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_INSERT));
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(flags));
   mongo_protocol_append_cstring(buffer, db_and_collection);
   for (i = 0; i < n_documents; i++) {
      mongo_protocol_append_bson(buffer, documents[i]);
   }
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));
   mongo_protocol_append_getlasterror(protocol, buffer, db_and_collection);

   /*
    * We get our response from the getlasterror command, so use it's request
    * id as the key in the hashtable.
    */
   g_hash_table_insert(priv->requests,
                       GINT_TO_POINTER(request_id + 1),
                       simple);

   /*
    * Write the bytes to the buffered stream.
    */
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

gboolean
mongo_protocol_insert_finish (MongoProtocol  *protocol,
                              GAsyncResult   *result,
                              GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

void
mongo_protocol_query_async (MongoProtocol       *protocol,
                            const gchar         *db_and_collection,
                            MongoQueryFlags      flags,
                            guint32              skip,
                            guint32              limit,
                            const MongoBson     *query,
                            const MongoBson     *field_selector,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(query);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_query_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_QUERY));
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(flags));
   mongo_protocol_append_cstring(buffer, db_and_collection);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(skip));
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(limit));
   mongo_protocol_append_bson(buffer, query);
   if (field_selector) {
      mongo_protocol_append_bson(buffer, field_selector);
   }
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));

   g_hash_table_insert(priv->requests, GINT_TO_POINTER(request_id), simple);
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

/**
 * mongo_protocol_query_finish:
 * @protocol: (in): A #MongoProtocol.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completed an asynchronous request to query.
 *
 * Returns: (transfer full): A #MongoMessageReply.
 */
MongoMessageReply *
mongo_protocol_query_finish (MongoProtocol    *protocol,
                             GAsyncResult     *result,
                             GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *reply;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(reply = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   reply = reply ? g_object_ref(reply) : NULL;

   RETURN(reply);
}

void
mongo_protocol_getmore_async (MongoProtocol       *protocol,
                              const gchar         *db_and_collection,
                              guint32              limit,
                              guint64              cursor_id,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_getmore_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_GETMORE));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_cstring(buffer, db_and_collection);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(limit));
   mongo_protocol_append_int64(buffer, GINT64_TO_LE(cursor_id));
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));

   g_hash_table_insert(priv->requests, GINT_TO_POINTER(request_id), simple);
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

/**
 * mongo_protocol_getmore_finish:
 * @protocol: (in): A #MongoProtocol.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to fetch more documents from a cursor.
 *
 * Returns: (transfer full): A #MongoMessageReply if successful; otherwise or %NULL.
 */
MongoMessageReply *
mongo_protocol_getmore_finish (MongoProtocol  *protocol,
                               GAsyncResult   *result,
                               GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *reply;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(reply = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   reply = reply ? g_object_ref(reply) : NULL;

   RETURN(reply);
}

void
mongo_protocol_delete_async (MongoProtocol       *protocol,
                             const gchar         *db_and_collection,
                             MongoDeleteFlags     flags,
                             const MongoBson     *selector,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(selector);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_delete_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_DELETE));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_cstring(buffer, db_and_collection);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(flags));
   mongo_protocol_append_bson(buffer, selector);
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));
   mongo_protocol_append_getlasterror(protocol, buffer, db_and_collection);

   /*
    * We get our response from the getlasterror command, so use it's request
    * id as the key in the hashtable.
    */
   g_hash_table_insert(priv->requests,
                       GINT_TO_POINTER(request_id + 1),
                       simple);

   /*
    * Write the bytes to the buffered stream.
    */
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

gboolean
mongo_protocol_delete_finish (MongoProtocol  *protocol,
                              GAsyncResult   *result,
                              GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

void
mongo_protocol_kill_cursors_async (MongoProtocol       *protocol,
                                   guint64             *cursors,
                                   gsize                n_cursors,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;
   guint i;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(cursors);
   g_return_if_fail(n_cursors);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_kill_cursors_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_KILL_CURSORS));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, n_cursors);
   for (i = 0; i < n_cursors; i++) {
      mongo_protocol_append_int64(buffer, cursors[i]);
   }
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));

   g_hash_table_insert(priv->requests, GINT_TO_POINTER(request_id), simple);
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

gboolean
mongo_protocol_kill_cursors_finish (MongoProtocol  *protocol,
                                    GAsyncResult   *result,
                                    GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

void
mongo_protocol_msg_async (MongoProtocol       *protocol,
                          const gchar         *message,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *simple;
   GByteArray *buffer;
   guint32 request_id;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(message);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = protocol->priv;

   simple = g_simple_async_result_new(G_OBJECT(protocol), callback, user_data,
                                      mongo_protocol_msg_async);

   request_id = ++priv->last_request_id;

   buffer = g_byte_array_new();
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(request_id));
   mongo_protocol_append_int32(buffer, 0);
   mongo_protocol_append_int32(buffer, GINT32_TO_LE(MONGO_OPERATION_MSG));
   mongo_protocol_append_cstring(buffer, message);
   mongo_protocol_overwrite_int32(buffer, 0, GINT32_TO_LE(buffer->len));

   g_hash_table_insert(priv->requests, GINT_TO_POINTER(request_id), simple);
   mongo_protocol_write(protocol, request_id, simple,
                        buffer->data, buffer->len);

   g_byte_array_free(buffer, TRUE);

   EXIT;
}

gboolean
mongo_protocol_msg_finish (MongoProtocol  *protocol,
                           GAsyncResult   *result,
                           GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

/**
 * mongo_protocol_get_io_stream:
 * @protocol: (in): A #MongoProtocol.
 *
 * Fetch the #GIOStream used by @protocol.
 *
 * Returns: (transfer none): A #GIOStream.
 */
GIOStream *
mongo_protocol_get_io_stream (MongoProtocol *protocol)
{
   g_return_val_if_fail(MONGO_IS_PROTOCOL(protocol), NULL);
   return protocol->priv->io_stream;
}

static void
mongo_protocol_fill_message_cb (GBufferedInputStream *input_stream,
                                GAsyncResult         *result,
                                MongoProtocol        *protocol)
{
   MongoProtocolPrivate *priv;
   GSimpleAsyncResult *request;
   const guint8 *buffer;
   MongoMessageReply *reply;
   GError *error = NULL;
   guint8 *reply_buffer = NULL;
   gsize count;
#pragma pack(push, 1)
   struct {
      guint32 msg_len;
      guint32 request_id;
      guint32 response_to;
      guint32 op_code;
   } header;
#pragma pack(pop)

   ENTRY;

   g_assert(G_IS_BUFFERED_INPUT_STREAM(input_stream));
   g_assert(!result || G_IS_ASYNC_RESULT(result));
   g_assert(MONGO_IS_PROTOCOL(protocol));

   priv = protocol->priv;

   /*
    * Check if succeeded filling buffered input with Mongo reply message.
    */
   if (result) {
      if (!g_buffered_input_stream_fill_finish(input_stream, result, &error)) {
         /*
          * TODO: Check if this was a cancellation from our finalizer.
          */
         g_assert_not_reached();
         GOTO(cleanup);
      }
   }

   buffer = g_buffered_input_stream_peek_buffer(input_stream, &count);
   g_assert(buffer);
   g_assert_cmpint(count, >=, 36);

   /*
    * Make sure we got a reply.
    */
   memcpy(&header, buffer, sizeof header);
   header.msg_len = GUINT32_FROM_LE(header.msg_len);
   header.request_id = GUINT32_FROM_LE(header.request_id);
   header.response_to = GUINT32_FROM_LE(header.response_to);
   header.op_code = GUINT32_FROM_LE(header.op_code);
   if (header.op_code != MONGO_OPERATION_REPLY) {
      GOTO(failure);
   }

   g_input_stream_skip(G_INPUT_STREAM(input_stream),
                       sizeof header,
                       NULL,
                       NULL);

   /*
    * TODO: If the message size is decently small, we should use alloca()
    *       to allocate space on the stack instead of the heap. I suspect
    *       this could be a decent speedup when handling lots of packets.
    */

   count = 0;
   reply_buffer = g_malloc(header.msg_len);
   g_input_stream_read_all(G_INPUT_STREAM(input_stream),
                           reply_buffer,
                           header.msg_len - sizeof header,
                           &count,
                           NULL,
                           &error);

   DUMP_BYTES(reply_buffer, reply_buffer, count);

   if (count != (header.msg_len - sizeof header)) {
      GOTO(failure);
   }

   reply = g_object_new(MONGO_TYPE_MESSAGE_REPLY,
                        "request-id", header.request_id,
                        "response-to", header.response_to,
                        NULL);
   if (!mongo_message_load_from_data(MONGO_MESSAGE(reply),
                                     reply_buffer,
                                     count)) {
      g_clear_object(&reply);
      GOTO(failure);
   }

   /*
    * See if there was someone waiting for this request.
    */
   if ((request = g_hash_table_lookup(priv->requests,
                                      GINT_TO_POINTER(header.response_to)))) {
      g_simple_async_result_set_op_res_gpointer(request, reply, g_object_unref);
      mongo_simple_async_result_complete_in_idle(request);
      g_hash_table_remove(priv->requests, GINT_TO_POINTER(header.response_to));
   }

   /*
    * Wait for the next message to arrive.
    */
   if (g_buffered_input_stream_get_available(input_stream) < 16) {
      g_buffered_input_stream_fill_async(
            input_stream,
            16, /* sizeof MsgHeader */
            G_PRIORITY_DEFAULT,
            priv->shutdown,
            (GAsyncReadyCallback)mongo_protocol_fill_header_cb,
            g_object_ref(protocol));
   } else {
      mongo_protocol_fill_header_cb(input_stream,
                                    NULL,
                                    g_object_ref(protocol));
   }

cleanup:
   g_object_unref(protocol);
   g_free(reply_buffer);
   EXIT;

failure:
   mongo_protocol_fail(protocol, error);
   g_object_unref(protocol);
   g_free(reply_buffer);
   EXIT;
}

static void
mongo_protocol_fill_header_cb (GBufferedInputStream *input_stream,
                               GAsyncResult         *result,
                               MongoProtocol        *protocol)
{
   MongoProtocolPrivate *priv;
   const guint8 *buffer;
   guint32 msg_len;
   guint32 op_code;
   GError *error = NULL;
   gsize count;

   ENTRY;

   g_assert(G_IS_BUFFERED_INPUT_STREAM(input_stream));
   g_assert(!result || G_IS_ASYNC_RESULT(result));
   g_assert(MONGO_IS_PROTOCOL(protocol));

   priv = protocol->priv;

   /*
    * Check if we succeeded filling buffered input with Mongo reply header.
    * If result is NULL, then this was done synchronously and we don't need
    * to worry about finishing an async request.
    */
   if (result) {
      if (!g_buffered_input_stream_fill_finish(input_stream, result, &error)) {
         mongo_protocol_fail(protocol, NULL);
         GOTO(cleanup);
      }
   }

   /*
    * Read the message length so that we may wait until that many bytes have
    * been filled into the buffer.
    */
   buffer = g_buffered_input_stream_peek_buffer(input_stream, &count);

   /*
    * If we got no data back, its because we closed the connection from a
    * failure scenario.
    */
   if (!count) {
      GOTO(cleanup);
   }

   g_assert_cmpint(count, >=, 16);
   DUMP_BYTES(buffer, buffer, count);

   /*
    * Determine the size of incoming message and op_code.
    */
   msg_len = GUINT32_FROM_LE(*(const guint32 *)buffer);
   op_code = GUINT32_FROM_LE(*(const guint32 *)(buffer + 12));

   /*
    * We only know about MONGO_OPERATION_REPLY from the server. Everything
    * else is a protocol error.
    */
   if (op_code != MONGO_OPERATION_REPLY) {
      mongo_protocol_fail(protocol, NULL);
   }

   /*
    * Wait until the entire message has been received.
    */
   if (g_buffered_input_stream_get_available(input_stream) < msg_len) {
      g_buffered_input_stream_fill_async(
            input_stream,
            MAX(36, msg_len),
            G_PRIORITY_DEFAULT,
            priv->shutdown,
            (GAsyncReadyCallback)mongo_protocol_fill_message_cb,
            g_object_ref(protocol));
   } else {
      mongo_protocol_fill_message_cb(input_stream, NULL, protocol);
   }

cleanup:
   g_object_unref(protocol);
   EXIT;
}

static void
mongo_protocol_set_io_stream (MongoProtocol *protocol,
                              GIOStream     *io_stream)
{
   MongoProtocolPrivate *priv;
   GOutputStream *output_stream;
   GInputStream *input_stream;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(G_IS_IO_STREAM(io_stream));
   g_return_if_fail(!protocol->priv->io_stream);

   priv = protocol->priv;

   priv->io_stream = g_object_ref(io_stream);

   input_stream = g_io_stream_get_input_stream(io_stream);
   priv->input_stream = g_buffered_input_stream_new(input_stream);
#if 0
   g_buffered_input_stream_set_buffer_size(
         G_BUFFERED_INPUT_STREAM(input_stream),
         4096);
#endif

   output_stream = g_io_stream_get_output_stream(io_stream);
   priv->output_stream = g_buffered_output_stream_new(output_stream);
   g_buffered_output_stream_set_auto_grow(
         G_BUFFERED_OUTPUT_STREAM(priv->output_stream),
         TRUE);

   g_buffered_input_stream_fill_async(
         G_BUFFERED_INPUT_STREAM(priv->input_stream),
         16, /* sizeof MsgHeader */
         G_PRIORITY_DEFAULT,
         priv->shutdown,
         (GAsyncReadyCallback)mongo_protocol_fill_header_cb,
         g_object_ref(protocol));

   EXIT;
}

static void
mongo_protocol_finalize (GObject *object)
{
   MongoProtocolPrivate *priv;
   GHashTable *hash;

   ENTRY;

   priv = MONGO_PROTOCOL(object)->priv;

   g_cancellable_cancel(priv->shutdown);

   if ((hash = priv->requests)) {
      priv->requests = NULL;
      g_hash_table_unref(hash);
   }

   g_clear_object(&priv->shutdown);
   g_clear_object(&priv->input_stream);
   g_clear_object(&priv->output_stream);
   g_clear_object(&priv->io_stream);

   G_OBJECT_CLASS(mongo_protocol_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_protocol_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
   MongoProtocol *protocol = MONGO_PROTOCOL(object);

   switch (prop_id) {
   case PROP_IO_STREAM:
      g_value_set_object(value, mongo_protocol_get_io_stream(protocol));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_protocol_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
   MongoProtocol *protocol = MONGO_PROTOCOL(object);

   switch (prop_id) {
   case PROP_FSYNC:
      protocol->priv->getlasterror_fsync = g_value_get_boolean(value);
      break;
   case PROP_JOURNAL:
      protocol->priv->getlasterror_j = g_value_get_boolean(value);
      break;
   case PROP_SAFE:
      protocol->priv->safe = g_value_get_boolean(value);
      break;
   case PROP_WRITE_QUORUM:
      protocol->priv->getlasterror_w = g_value_get_int(value);
      break;
   case PROP_WRITE_TIMEOUT:
      protocol->priv->getlasterror_wtimeoutms = g_value_get_uint(value);
      break;
   case PROP_IO_STREAM:
      mongo_protocol_set_io_stream(protocol, g_value_get_object(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_protocol_class_init (MongoProtocolClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_protocol_finalize;
   object_class->get_property = mongo_protocol_get_property;
   object_class->set_property = mongo_protocol_set_property;
   g_type_class_add_private(object_class, sizeof(MongoProtocolPrivate));

   gParamSpecs[PROP_FSYNC] =
      g_param_spec_boolean("fsync",
                           _("Fsync"),
                           _("Wait for filesystem sync on getlasterror."),
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_FSYNC,
                                   gParamSpecs[PROP_FSYNC]);

   gParamSpecs[PROP_IO_STREAM] =
      g_param_spec_object("io-stream",
                          _("I/O Stream"),
                          _("I/O stream to communicate over."),
                          G_TYPE_IO_STREAM,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_IO_STREAM,
                                   gParamSpecs[PROP_IO_STREAM]);

   gParamSpecs[PROP_JOURNAL] =
      g_param_spec_boolean("journal",
                           _("Journal"),
                           _("Journal on getlasterror."),
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_JOURNAL,
                                   gParamSpecs[PROP_JOURNAL]);

   gParamSpecs[PROP_SAFE] =
      g_param_spec_boolean("safe",
                           _("Safe"),
                           _("Perform getlasterror after operation."),
                           TRUE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_SAFE,
                                   gParamSpecs[PROP_SAFE]);

   gParamSpecs[PROP_WRITE_QUORUM] =
      g_param_spec_int("write-quorum",
                       _("Write Quorum"),
                       _("Number of nodes that must safely write to disk."),
                       -1,
                       G_MAXINT,
                       0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_WRITE_QUORUM,
                                   gParamSpecs[PROP_WRITE_QUORUM]);

   gParamSpecs[PROP_WRITE_TIMEOUT] =
      g_param_spec_uint("write-timeout",
                        _("Write Timeout"),
                        _("Write timeout in milliseconds."),
                        0,
                        G_MAXUINT,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_WRITE_TIMEOUT,
                                   gParamSpecs[PROP_WRITE_TIMEOUT]);

   gSignals[FAILED] = g_signal_new("failed",
                                   MONGO_TYPE_PROTOCOL,
                                   G_SIGNAL_RUN_FIRST,
                                   0,
                                   NULL,
                                   NULL,
                                   g_cclosure_marshal_VOID__BOXED,
                                   G_TYPE_NONE,
                                   1,
                                   G_TYPE_ERROR);

   EXIT;
}

static void
mongo_protocol_init (MongoProtocol *protocol)
{
   ENTRY;

   protocol->priv = G_TYPE_INSTANCE_GET_PRIVATE(protocol,
                                                MONGO_TYPE_PROTOCOL,
                                                MongoProtocolPrivate);
   protocol->priv->last_request_id = g_random_int();
   protocol->priv->getlasterror_w = 0;
   protocol->priv->getlasterror_j = TRUE;
   protocol->priv->shutdown = g_cancellable_new();
   protocol->priv->requests = g_hash_table_new_full(g_direct_hash,
                                                    g_direct_equal,
                                                    NULL,
                                                    g_object_unref);

   EXIT;
}

GQuark
mongo_protocol_error_quark (void)
{
   return g_quark_from_static_string("mongo-protocol-error-quark");
}
