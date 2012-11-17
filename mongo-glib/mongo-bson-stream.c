/* mongo-bson-stream.c
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

#include "mongo-bson-stream.h"

G_DEFINE_TYPE(MongoBsonStream, mongo_bson_stream, G_TYPE_OBJECT)

/**
 * SECTION:mongo-bson-stream
 * @title: MongoBsonStream
 * @short_description: Sequential BSON document streams.
 *
 * #MongoBsonStream allows you to read from a file or stream that
 * contains #MongoBson documents sequentially one after another.
 * This is the case for backups of Mongo performed with the
 * mongodump command.
 */

struct _MongoBsonStreamPrivate
{
   GInputStream *stream;
   GIOChannel *channel;
};

/**
 * mongo_bson_stream_new:
 *
 * Creates a new instance of #MongoBsonStream. This should be freed
 * with g_object_unref() when you are finished with it.
 *
 * Returns: A newly created #MongoBsonStream.
 */
MongoBsonStream *
mongo_bson_stream_new (void)
{
   return g_object_new(MONGO_TYPE_BSON_STREAM, NULL);
}

/**
 * mongo_bson_stream_load_from_channel:
 * @stream: (in): A #MongoBsonStream.
 * @channel: (in): A #GIOChannel.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Enables #MongoBsonStream to use the content of @channel for the
 * underlying BSON stream.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_bson_stream_load_from_channel (MongoBsonStream  *stream,
                                     GIOChannel       *channel,
                                     GError          **error)
{
   MongoBsonStreamPrivate *priv;

   g_return_val_if_fail(MONGO_IS_BSON_STREAM(stream), FALSE);
   g_return_val_if_fail(channel != NULL, FALSE);

   priv = stream->priv;

   if (priv->stream || priv->channel) {
      g_set_error(error, MONGO_BSON_STREAM_ERROR,
                  MONGO_BSON_STREAM_ERROR_ALREADY_LOADED,
                  _("Cannot load stream, one is already loaded."));
      return FALSE;
   }

   priv->channel = g_io_channel_ref(channel);

   return TRUE;
}

/**
 * mongo_bson_stream_load_from_file:
 * @stream: (in): A #MongoBsonStream.
 * @file: (in): A #GFile containing the BSON stream.
 * @cancellable: (in) (allow-none): A #GCancellable or %NULL.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Load a #GFile containing a BSON stream to be used for iterating BSON
 * documents.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_bson_stream_load_from_file (MongoBsonStream  *stream,
                                  GFile            *file,
                                  GCancellable     *cancellable,
                                  GError          **error)
{
   MongoBsonStreamPrivate *priv;
   GFileInputStream *file_stream;

   g_return_val_if_fail(MONGO_IS_BSON_STREAM(stream), FALSE);
   g_return_val_if_fail(G_IS_FILE(file), FALSE);
   g_return_val_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable), FALSE);

   priv = stream->priv;

   if (priv->stream || priv->channel) {
      g_set_error(error, MONGO_BSON_STREAM_ERROR,
                  MONGO_BSON_STREAM_ERROR_ALREADY_LOADED,
                  _("Cannot load stream, one is already loaded."));
      return FALSE;
   }

   if (!(file_stream = g_file_read(file, cancellable, error))) {
      return FALSE;
   }

   priv->stream = G_INPUT_STREAM(file_stream);

   return TRUE;
}

static gboolean
mongo_bson_stream_read_channel (MongoBsonStream *stream,
                                guint8          *buffer,
                                gsize            buffer_len,
                                gsize            n_bytes)
{
   MongoBsonStreamPrivate *priv;
   GIOStatus status;
   guint32 offset = 0;
   gsize bytes_written;

   g_return_val_if_fail(MONGO_IS_BSON_STREAM(stream), FALSE);
   g_return_val_if_fail(buffer != NULL, FALSE);

   priv = stream->priv;

   if (!priv->channel) {
      return FALSE;
   }

   if (n_bytes > buffer_len) {
      return FALSE;
   }

again:
   status = g_io_channel_read_chars(priv->channel,
                                    (gchar *)(buffer + offset),
                                    n_bytes - offset,
                                    &bytes_written,
                                    NULL);

   switch (status) {
   case G_IO_STATUS_AGAIN:
      goto again;
   case G_IO_STATUS_NORMAL:
      offset += bytes_written;
      if (offset == n_bytes) {
         return TRUE;
      } else if (offset < n_bytes) {
         goto again;
      } else {
         g_assert_not_reached();
      }
   case G_IO_STATUS_ERROR:
   case G_IO_STATUS_EOF:
   default:
      break;
   }

   return FALSE;
}

static gboolean
mongo_bson_stream_read_stream (MongoBsonStream *stream,
                               guint8          *buffer,
                               gsize            buffer_len,
                               gsize            n_bytes)
{
   MongoBsonStreamPrivate *priv;
   guint32 offset = 0;
   gssize ret;

   g_return_val_if_fail(MONGO_IS_BSON_STREAM(stream), FALSE);
   g_return_val_if_fail(buffer != NULL, FALSE);

   priv = stream->priv;

   if (!priv->stream) {
      return FALSE;
   }

   if (n_bytes > buffer_len) {
      return FALSE;
   }

again:
   ret = g_input_stream_read(priv->stream,
                             buffer + offset,
                             n_bytes - offset,
                             NULL,
                             NULL);

   switch (ret) {
   case -1:
      return FALSE;
   case 0:
      return FALSE;
   default:
      offset += ret;
      if (offset == n_bytes) {
         return TRUE;
      } else if (offset < n_bytes) {
         goto again;
      } else {
         g_assert_not_reached();
      }
   }

   return FALSE;
}

static gboolean
mongo_bson_stream_read (MongoBsonStream *stream,
                        guint8          *buffer,
                        gsize            buffer_len,
                        gsize            n_bytes)
{
   g_return_val_if_fail(MONGO_IS_BSON_STREAM(stream), FALSE);
   g_return_val_if_fail(buffer, FALSE);
   g_return_val_if_fail(buffer_len, FALSE);
   g_return_val_if_fail(n_bytes, FALSE);

   if (stream->priv->stream) {
      return mongo_bson_stream_read_stream(stream,
                                           buffer,
                                           buffer_len,
                                           n_bytes);
   } else {
      return mongo_bson_stream_read_channel(stream,
                                            buffer,
                                            buffer_len,
                                            n_bytes);
   }
}

/**
 * mongo_bson_stream_next:
 * @stream: (in): A #MongoBsonStream.
 *
 * Gets the next #MongoBson document found in the stream.
 *
 * Returns: (transfer full): A #MongoBson if successful; otherwise %NULL.
 */
MongoBson *
mongo_bson_stream_next (MongoBsonStream *stream)
{
   MongoBson *bson;
   guint32 doc_len_le;
   guint32 doc_len;
   guint8 *buffer;

   g_return_val_if_fail(MONGO_IS_BSON_STREAM(stream), NULL);
   g_return_val_if_fail(stream->priv->stream ||
                        stream->priv->channel,
                        NULL);

   if (!mongo_bson_stream_read(stream,
                               (guint8 *)&doc_len_le,
                               sizeof doc_len_le,
                               sizeof doc_len_le)) {
      return NULL;
   }

   doc_len = GUINT32_FROM_LE(doc_len_le);

   /*
    * Sanity check to make sure it is less than 16 MB and
    * greater than 5 bytes (minimum required).
    */
   if (doc_len > (1024 * 1024 * 16) || doc_len <= 5) {
      return NULL;
   }

   buffer = g_malloc(doc_len);
   memcpy(buffer, &doc_len_le, sizeof doc_len_le);

   /*
    * Read the rest of the BSON document into our buffer.
    */
   if (!mongo_bson_stream_read(stream,
                               buffer + sizeof doc_len_le,
                               doc_len,
                               doc_len - sizeof doc_len_le)) {
      g_free(buffer);
      return NULL;
   }

   if (!(bson = mongo_bson_new_take_data(buffer, doc_len))) {
      g_free(buffer);
      return NULL;
   }

   return bson;
}

static void
mongo_bson_stream_finalize (GObject *object)
{
   MongoBsonStream *stream = (MongoBsonStream *)object;

   g_clear_object(&stream->priv->stream);

   if (stream->priv->channel) {
      g_io_channel_unref(stream->priv->channel);
      stream->priv->channel = NULL;
   }

   G_OBJECT_CLASS(mongo_bson_stream_parent_class)->finalize(object);
}

static void
mongo_bson_stream_class_init (MongoBsonStreamClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_bson_stream_finalize;
   g_type_class_add_private(object_class, sizeof(MongoBsonStreamPrivate));
}

static void
mongo_bson_stream_init (MongoBsonStream *stream)
{
   stream->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(stream,
                                  MONGO_TYPE_BSON_STREAM,
                                  MongoBsonStreamPrivate);
}

GQuark
mongo_bson_stream_error_quark (void)
{
   return g_quark_from_string("mongo-bson-stream-error-quark");
}
