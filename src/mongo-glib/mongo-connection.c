/* mongo-connection.c
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
#include <stdlib.h>

#include "guri.h"

#include "mongo-connection.h"
#include "mongo-debug.h"
#include "mongo-manager.h"
#include "mongo-protocol.h"
#include "mongo-source.h"

/**
 * SECTION:mongo-connection
 * @title: MongoConnection
 * @short_description: Connection to Mongo DB.
 */

/*
 * TODO: Query replica set information occasionally.
 *       Support the rest of the options currently parsed.
 *       Resiliency to network partitions.
 *       Memory leak detection.
 *
 */

G_DEFINE_TYPE(MongoConnection, mongo_connection, G_TYPE_OBJECT)

#ifndef MONGO_PORT_DEFAULT
#define MONGO_PORT_DEFAULT 27017
#endif

struct _MongoConnectionPrivate
{
   /*
    * Hashtable of databases opened.
    */
   GHashTable *databases;

   /*
    * Connection and protocol.
    */
   GSocketClient *socket_client;
   MongoProtocol *protocol;

   /*
    * Cancellable emitted when shutting down.
    */
   GCancellable *dispose_cancel;

   /*
    * Current connection state.
    */
   guint state;

   /*
    * Queued requests.
    */
   GQueue *queue;

   /*
    * Connection options.
    */
   guint connecttimeoutms;
   gboolean fsync;
   gboolean fsync_set;
   guint w;
   gboolean journal;
   gboolean journal_set;
   gchar *replica_set;
   gboolean safe;
   gboolean slave_okay;
   guint sockettimeoutms;
   GUri *uri;
   gchar *uri_string;
   guint wtimeoutms;

   /*
    * Node reconnection manager.
    */
   MongoManager *manager;
};

typedef struct
{
   MongoOperation oper;
   GSimpleAsyncResult *simple;
   GCancellable *cancellable;
   union {
      struct {
         gchar *db_and_collection;
         MongoUpdateFlags flags;
         MongoBson *selector;
         MongoBson *update;
      } update;
      struct {
         gchar *db_and_collection;
         MongoInsertFlags flags;
         GPtrArray *documents;
      } insert;
      struct {
         gchar *db_and_collection;
         MongoQueryFlags flags;
         guint32 skip;
         guint32 limit;
         MongoBson *query;
         MongoBson *field_selector;
      } query;
      struct {
         gchar *db_and_collection;
         guint32 limit;
         guint64 cursor_id;
      } getmore;
      struct {
         gchar *db_and_collection;
         MongoDeleteFlags flags;
         MongoBson *selector;
      } delete;
      struct {
         GArray *cursors;
      } kill_cursors;
   } u;
} Request;

enum
{
   PROP_0,
   PROP_REPLICA_SET,
   PROP_SLAVE_OKAY,
   PROP_URI,
   LAST_PROP
};

enum
{
   CONNECTED,
   LAST_SIGNAL
};

enum
{
   STATE_0,
   STATE_CONNECTING,
   STATE_CONNECTED,
   STATE_DISPOSED
};

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];

static void mongo_connection_start_connecting (MongoConnection *connection);

static void
mongo_connection_update_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   MongoBson *bson;
   gboolean ret;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(ret = mongo_protocol_update_finish(protocol,
                                            result,
                                            &bson,
                                            &error))) {
      g_simple_async_result_take_error(simple, error);
   } else {
      g_object_set_data_full(G_OBJECT(simple),
                             "document",
                             bson,
                             (GDestroyNotify)mongo_bson_unref);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   mongo_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static void
mongo_connection_insert_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   gboolean ret;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(ret = mongo_protocol_insert_finish(protocol, result, &error))) {
      g_simple_async_result_take_error(simple, error);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   mongo_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static void
mongo_connection_query_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   MongoMessageReply *reply;
   GError *error = NULL;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_protocol_query_finish(protocol, result, &error))) {
      g_simple_async_result_take_error(simple, error);
   } else {
      g_simple_async_result_set_op_res_gpointer(simple, reply, g_object_unref);
   }

   mongo_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static void
mongo_connection_getmore_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   MongoMessageReply *reply;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_protocol_getmore_finish(protocol, result, &error))) {
      g_simple_async_result_take_error(simple, error);
   } else {
      g_simple_async_result_set_op_res_gpointer(simple, reply, g_object_unref);
   }

   mongo_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static void
mongo_connection_delete_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   gboolean ret;
   GError *error = NULL;

   ENTRY;

   g_return_if_fail(MONGO_IS_PROTOCOL(protocol));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(ret = mongo_protocol_delete_finish(protocol, result, &error))) {
      g_simple_async_result_take_error(simple, error);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   mongo_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static void
mongo_connection_kill_cursors_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   gboolean ret;
   GError *error = NULL;

   ENTRY;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(ret = mongo_protocol_kill_cursors_finish(protocol, result, &error))) {
      g_simple_async_result_take_error(simple, error);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   mongo_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   EXIT;
}

static void
request_fail (Request      *request,
              const GError *error)
{
   g_simple_async_result_take_error(request->simple, g_error_copy(error));
   g_simple_async_result_complete_in_idle(request->simple);
}

static void
request_run (Request       *request,
             MongoProtocol *protocol)
{
   switch (request->oper) {
   case MONGO_OPERATION_UPDATE:
      mongo_protocol_update_async(
            protocol,
            request->u.update.db_and_collection,
            request->u.update.flags,
            request->u.update.selector,
            request->u.update.update,
            request->cancellable,
            mongo_connection_update_cb,
            g_object_ref(request->simple));
      break;
   case MONGO_OPERATION_INSERT:
      mongo_protocol_insert_async(
            protocol,
            request->u.insert.db_and_collection,
            request->u.insert.flags,
            (MongoBson **)request->u.insert.documents->pdata,
            request->u.insert.documents->len,
            request->cancellable,
            mongo_connection_insert_cb,
            g_object_ref(request->simple));
      break;
   case MONGO_OPERATION_QUERY:
      mongo_protocol_query_async(
            protocol,
            request->u.query.db_and_collection,
            request->u.query.flags,
            request->u.query.skip,
            request->u.query.limit,
            request->u.query.query,
            request->u.query.field_selector,
            request->cancellable,
            mongo_connection_query_cb,
            g_object_ref(request->simple));
      break;
   case MONGO_OPERATION_GETMORE:
      mongo_protocol_getmore_async(
            protocol,
            request->u.getmore.db_and_collection,
            request->u.getmore.limit,
            request->u.getmore.cursor_id,
            request->cancellable,
            mongo_connection_getmore_cb,
            g_object_ref(request->simple));
      break;
   case MONGO_OPERATION_DELETE:
      mongo_protocol_delete_async(
            protocol,
            request->u.delete.db_and_collection,
            request->u.delete.flags,
            request->u.delete.selector,
            request->cancellable,
            mongo_connection_delete_cb,
            g_object_ref(request->simple));
      break;
   case MONGO_OPERATION_KILL_CURSORS:
      mongo_protocol_kill_cursors_async(
            protocol,
            (guint64 *)(gpointer)request->u.kill_cursors.cursors->data,
            request->u.kill_cursors.cursors->len,
            request->cancellable,
            mongo_connection_kill_cursors_cb,
            g_object_ref(request->simple));
      break;
   case MONGO_OPERATION_REPLY:
   case MONGO_OPERATION_MSG:
   default:
      g_assert_not_reached();
      break;
   }
}

static Request *
request_new (gpointer             source,
             GCancellable        *cancellable,
             GAsyncReadyCallback  callback,
             gpointer             user_data,
             gpointer             tag)
{
   Request *request;

   g_assert(G_IS_OBJECT(source));
   g_assert(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_assert(callback);
   g_assert(tag);

   request = g_slice_new0(Request);
   request->cancellable = cancellable ? g_object_ref(cancellable) : NULL;
   request->simple = g_simple_async_result_new(source,
                                               callback,
                                               user_data,
                                               tag);
   g_simple_async_result_set_check_cancellable(request->simple,
                                               cancellable);

   return request;
}

static void
request_free (Request *request)
{
   if (request) {
      g_clear_object(&request->simple);
      g_clear_object(&request->cancellable);
      switch (request->oper) {
      case MONGO_OPERATION_UPDATE:
         g_free(request->u.update.db_and_collection);
         if (request->u.update.selector) {
            mongo_bson_unref(request->u.update.selector);
         }
         if (request->u.update.update) {
            mongo_bson_unref(request->u.update.update);
         }
         break;
      case MONGO_OPERATION_INSERT:
         g_free(request->u.insert.db_and_collection);
         g_ptr_array_free(request->u.insert.documents, TRUE);
         break;
      case MONGO_OPERATION_QUERY:
         g_free(request->u.insert.db_and_collection);
         if (request->u.query.query) {
            mongo_bson_unref(request->u.query.query);
         }
         if (request->u.query.field_selector) {
            mongo_bson_unref(request->u.query.field_selector);
         }
         break;
      case MONGO_OPERATION_GETMORE:
         g_free(request->u.insert.db_and_collection);
         break;
      case MONGO_OPERATION_DELETE:
         g_free(request->u.insert.db_and_collection);
         if (request->u.delete.selector) {
            mongo_bson_unref(request->u.delete.selector);
         }
         break;
      case MONGO_OPERATION_KILL_CURSORS:
         g_array_free(request->u.kill_cursors.cursors, TRUE);
         break;
      case MONGO_OPERATION_REPLY:
      case MONGO_OPERATION_MSG:
      default:
         g_assert_not_reached();
         break;
      }
      memset(&request->u, 0, sizeof request->u);
      g_slice_free(Request, request);
   }
}

static void
mongo_connection_protocol_failed (MongoProtocol   *protocol,
                                  const GError    *error,
                                  MongoConnection *connection)
{
   ENTRY;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(MONGO_IS_CONNECTION(connection));

   g_warning("Mongo protocol failure: %s.",
             error ? error->message : "Unknown error");

   /*
    * Clear the protocol so we can connect to the next host.
    */
   connection->priv->state = STATE_0;
   g_clear_object(&connection->priv->protocol);

   /*
    * Start connecting to the next configured host.
    */
   if (!g_cancellable_is_cancelled(connection->priv->dispose_cancel)) {
      mongo_connection_start_connecting(connection);
   }

   EXIT;
}

static void
mongo_connection_ismaster_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
   MongoConnectionPrivate *priv;
   MongoConnection *connection = user_data;
   MongoProtocol *protocol = (MongoProtocol *)object;
   MongoBsonIter iter;
   MongoBsonIter iter2;
   const gchar *host;
   const gchar *primary;
   const gchar *replica_set;
   MongoMessageReply *reply = NULL;
   gboolean ismaster = FALSE;
   Request *request;
   GError *error = NULL;
   GList *list = NULL;

   ENTRY;

   g_assert(MONGO_IS_PROTOCOL(protocol));
   g_assert(MONGO_IS_CONNECTION(connection));

   priv = connection->priv;

   /*
    * Complete asynchronous protocol query.
    */
   if (!(reply = mongo_protocol_query_finish(protocol, result, &error))) {
      g_message("%s", error->message);
      g_error_free(error);
      GOTO(failure);
   }

   /*
    * Make sure we got a valid document back.
    */
   if (!(list = mongo_message_reply_get_documents(reply))) {
      GOTO(failure);
   }

   /*
    * Make sure we got a valid response back.
    */
   mongo_bson_iter_init(&iter, list->data);
   if (mongo_bson_iter_find(&iter, "ok")) {
      if (!mongo_bson_iter_get_value_boolean(&iter)) {
         GOTO(failure);
      }
   }

   /*
    * Check to see if this host contains the repl set name.
    * If so, verify that it is the one we should be talking to.
    */
   if (priv->replica_set) {
      mongo_bson_iter_init(&iter, list->data);
      if (mongo_bson_iter_find(&iter, "setName") &&
          (mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_UTF8)) {
         replica_set = mongo_bson_iter_get_value_string(&iter, NULL);
         if (!!g_strcmp0(replica_set, priv->replica_set)) {
            g_message("Peer replicaSet does not match: %s", replica_set);
            GOTO(failure);
         }
      }
   }

   /*
    * Update who we think the primary is now so that we can reconnect
    * if this isn't the primary.
    */
   mongo_bson_iter_init(&iter, list->data);
   if (mongo_bson_iter_find(&iter, "primary") &&
       (mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_UTF8)) {
      primary = mongo_bson_iter_get_value_string(&iter, NULL);
      mongo_manager_add_host(priv->manager, primary);
   }

   /*
    * Update who we think is in the list of nodes for this replica set
    * so that we can iterate through them upon inability to find master.
    */
   mongo_bson_iter_init(&iter, list->data);
   if (mongo_bson_iter_find(&iter, "hosts") &&
       (mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_ARRAY)) {
      if (mongo_bson_iter_recurse(&iter, &iter2)) {
         while (mongo_bson_iter_next(&iter2)) {
            if (mongo_bson_iter_get_value_type(&iter2) == MONGO_BSON_UTF8) {
               host = mongo_bson_iter_get_value_string(&iter2, NULL);
               mongo_manager_add_host(priv->manager, host);
            }
         }
      }
   }

   /*
    * Check to see if this host is PRIMARY.
    */
   mongo_bson_iter_init(&iter, list->data);
   if (mongo_bson_iter_find(&iter, "ismaster") &&
       (mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_BOOLEAN)) {
      if (!(ismaster = mongo_bson_iter_get_value_boolean(&iter))) {
         GOTO(failure);
      }
   }

   /*
    * We can reset the connection delay since we were successful.
    */
   mongo_manager_reset_delay(priv->manager);

   /*
    * This is the master and we are connected, so lets store the
    * protocol for use by the connection and change the state to
    * connected.
    */
   priv->protocol = g_object_ref(protocol);
   priv->state = STATE_CONNECTED;

   /*
    * Wire up failure of the protocol so that we can connect to
    * the next host.
    */
   g_signal_connect(protocol, "failed",
                    G_CALLBACK(mongo_connection_protocol_failed),
                    connection);

   /*
    * Emit the ::connected signal.
    *
    * This is emitted before flushing pending requests so that it fires
    * whether or not those pending requests cause it to disconnect. The
    * queuing mechanism will check to see if the queue is empty so that
    * requests from this signal stay in proper FIFO order behind queued
    * requests.
    */
   g_signal_emit(connection, gSignals[CONNECTED], 0);

   /*
    * Flush any pending requests.
    */
   while ((priv->state == STATE_CONNECTED) &&
          (priv->protocol) &&
          (request = g_queue_pop_head(priv->queue))) {
      request_run(request, priv->protocol);
      request_free(request);
   }

   g_clear_object(&reply);
   EXIT;

failure:
   mongo_protocol_fail(protocol, NULL);
   priv->state = STATE_0;
   mongo_connection_start_connecting(connection);
   g_clear_object(&reply);

   EXIT;
}

static void
mongo_connection_connect_to_host_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
   MongoConnectionPrivate *priv;
   GSocketConnection *conn;
   MongoConnection *connection = user_data;
   GSocketClient *socket_client = (GSocketClient *)object;
   MongoProtocol *protocol;
   MongoBson *command;
   GError *error = NULL;

   ENTRY;

   g_assert(G_IS_SOCKET_CLIENT(socket_client));
   g_assert(MONGO_IS_CONNECTION(connection));

   priv = connection->priv;

   /*
    * Complete the asynchronous connection attempt.
    */
   if (!(conn = g_socket_client_connect_to_host_finish(socket_client,
                                                       result,
                                                       &error))) {
      g_message("Failed to connect to host: %s", error->message);
      priv->state = STATE_0;
      mongo_connection_start_connecting(connection);
      g_error_free(error);
      EXIT;
   }

   /*
    * Build a protocol using our connection.
    */
   protocol = g_object_new(MONGO_TYPE_PROTOCOL,
                           "fsync", priv->fsync,
                           "io-stream", conn,
                           "journal", priv->journal,
                           "safe", priv->safe,
                           "write-timeout", priv->wtimeoutms,
                           "write-quorum", priv->w,
                           NULL);

   /*
    * We then need to check that the server is PRIMARY and matches our
    * requested replica set.
    */
   command = mongo_bson_new_empty();
   mongo_bson_append_int(command, "ismaster", 1);
   mongo_protocol_query_async(protocol,
                              "admin.$cmd",
                              MONGO_QUERY_EXHAUST,
                              0,
                              1,
                              command,
                              NULL,
                              priv->dispose_cancel,
                              mongo_connection_ismaster_cb,
                              connection);
   mongo_bson_unref(command);
   g_object_unref(protocol);
   g_object_unref(conn);

   EXIT;
}

static gboolean
mongo_connection_start_connecting_timeout (gpointer data)
{
   mongo_connection_start_connecting(data);
   g_object_unref(data);
   return FALSE;
}

static void
mongo_connection_start_connecting (MongoConnection *connection)
{
   MongoConnectionPrivate *priv;
   const gchar *host;
   Request *r;
   GError *error;
   guint delay = 0;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(connection->priv->state == STATE_0 ||
                    connection->priv->state == STATE_CONNECTING);

   priv = connection->priv;

   priv->state = STATE_CONNECTING;

   if (!(host = mongo_manager_next(priv->manager, &delay))) {
      /*
       * No more hosts to connect to this round. We need to therefore cancel
       * any pending requests immediately.
       */
      error = g_error_new(MONGO_CONNECTION_ERROR,
                          MONGO_CONNECTION_ERROR_CONNECT_FAILED,
                          _("Failed to connect to MongoDB."));
      while ((r = g_queue_pop_head(priv->queue))) {
         request_fail(r, error);
         request_free(r);
      }
      g_error_free(error);
      g_message("No more hosts, delaying for %u milliseconds.", delay);
      g_timeout_add(delay,
                    mongo_connection_start_connecting_timeout,
                    g_object_ref(connection));
      EXIT;
   }

   g_socket_client_connect_to_host_async(priv->socket_client,
                                         host,
                                         MONGO_PORT_DEFAULT,
                                         priv->dispose_cancel,
                                         mongo_connection_connect_to_host_cb,
                                         connection);

   EXIT;
}

static void
mongo_connection_queue (MongoConnection *connection,
                        Request         *request)
{
   MongoConnectionPrivate *priv;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(request);

   priv = connection->priv;

   switch (priv->state) {
   case STATE_0:
      g_queue_push_tail(priv->queue, request);
      mongo_connection_start_connecting(connection);
      break;
   case STATE_CONNECTING:
      g_queue_push_tail(priv->queue, request);
      break;
   case STATE_CONNECTED:
      /*
       * If we were called via ::connected callback then there could
       * be other requests to service first. Push to the back of the
       * queue in that case.
       */
      if (g_queue_is_empty(priv->queue)) {
         request_run(request, priv->protocol);
         request_free(request);
      } else {
         g_queue_push_tail(priv->queue, request);
      }
      break;
   case STATE_DISPOSED:
   default:
      g_assert_not_reached();
      break;
   }
}

/**
 * mongo_connection_new:
 *
 * Creates a new instance of #MongoConnection which will connect to
 * mongodb://127.0.0.1:27017.
 *
 * Returns: A newly allocated #MongoConnection.
 */
MongoConnection *
mongo_connection_new (void)
{
   MongoConnection *ret;

   ENTRY;
   ret = g_object_new(MONGO_TYPE_CONNECTION, NULL);
   RETURN(ret);
}

/**
 * mongo_connection_new_from_uri:
 * @uri: (in) (allow-none): A URI string.
 *
 * Creates a new #MongoConnection using the URI provided. The URI should be in
 * the mongodb://host:port form.
 *
 * Currently, mongodb:// style URIs that contain multiple hosts must all
 * connect on the same port number, specified in the last host due to an
 * incomplete implementation of URI parsing. This will be supported in
 * the future.
 *
 * Such a URI would look like:
 *
 *   mongodb://127.0.0.1:27017,127.0.0.2:27018
 *
 * And will result in %NULL being returned.
 *
 * Returns: (transfer full): A newly created #MongoConnection.
 */
MongoConnection *
mongo_connection_new_from_uri (const gchar *uri)
{
   MongoConnection *ret;

   ENTRY;
   ret = g_object_new(MONGO_TYPE_CONNECTION, "uri", uri, NULL);
   RETURN(ret);
}

/**
 * mongo_connection_get_database:
 * @connection: (in): A #MongoConnection.
 * @name: (in): The database name.
 *
 * Fetches a #MongoDatabase for the database available via @connection.
 *
 * Returns: (transfer none): #MongoDatabase.
 */
MongoDatabase *
mongo_connection_get_database (MongoConnection *connection,
                               const gchar     *name)
{
   MongoConnectionPrivate *priv;
   MongoDatabase *database;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), NULL);
   g_return_val_if_fail(name, NULL);

   priv = connection->priv;

   if (!(database = g_hash_table_lookup(priv->databases, name))) {
      database = g_object_new(MONGO_TYPE_DATABASE,
                              "connection", connection,
                              "name", name,
                              NULL);
      g_hash_table_insert(priv->databases, g_strdup(name), database);
   }

   RETURN(database);
}

static void
mongo_connection_command_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoMessageReply *reply = NULL;
   MongoConnection *connection = (MongoConnection *)object;
   MongoBsonIter iter;
   const gchar *errmsg;
   GError *error = NULL;
   GList *list;

   ENTRY;

   g_assert(MONGO_IS_CONNECTION(connection));
   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   /*
    * Get the query reply, which may contain an error document.
    */
   if (!(reply = mongo_connection_command_finish(connection, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      GOTO(finish);
   }

   /*
    * Check to see if the command provided a failure document.
    */
   if (!(list = mongo_message_reply_get_documents(reply))) {
      mongo_bson_iter_init(&iter, list->data);
      if (mongo_bson_iter_find(&iter, "ok")) {
         if (!mongo_bson_iter_get_value_boolean(&iter)) {
            mongo_bson_iter_init(&iter, list->data);
            if (mongo_bson_iter_find(&iter, "errmsg") &&
                mongo_bson_iter_get_value_type(&iter) == MONGO_BSON_UTF8) {
               errmsg = mongo_bson_iter_get_value_string(&iter, NULL);
               g_simple_async_result_set_error(
                     simple,
                     MONGO_CONNECTION_ERROR,
                     MONGO_CONNECTION_ERROR_COMMAND_FAILED,
                     _("Command failed: %s"),
                     errmsg);
            } else {
               g_simple_async_result_set_error(
                     simple,
                     MONGO_CONNECTION_ERROR,
                     MONGO_CONNECTION_ERROR_COMMAND_FAILED,
                     _("Command failed."));
            }
            GOTO(finish);
         }
      }
   }

   g_simple_async_result_set_op_res_gpointer(simple,
                                             g_object_ref(reply),
                                             g_object_unref);

finish:
   mongo_simple_async_result_complete_in_idle(simple);
   g_clear_object(&reply);
   g_object_unref(simple);

   EXIT;
}

/**
 * mongo_connection_command_async:
 * @connection: A #MongoConnection.
 * @db: The database execute the command within.
 * @command: (transfer none): A #MongoBson containing the command.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously requests that a command is executed on the remote Mongo
 * server.
 *
 * @callback MUST execute mongo_connection_command_finish().
 */
void
mongo_connection_command_async (MongoConnection     *connection,
                                const gchar         *db,
                                const MongoBson     *command,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
   GSimpleAsyncResult *simple;
   gchar *db_and_cmd;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(db);
   g_return_if_fail(command);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   simple = g_simple_async_result_new(G_OBJECT(connection), callback, user_data,
                                      mongo_connection_command_async);
   g_simple_async_result_set_check_cancellable(simple, cancellable);
   db_and_cmd = g_strdup_printf("%s.$cmd", db);
   mongo_connection_query_async(connection,
                            db_and_cmd,
                            MONGO_QUERY_EXHAUST,
                            0,
                            1,
                            command,
                            NULL,
                            cancellable,
                            mongo_connection_command_cb,
                            simple);
   g_free(db_and_cmd);

   EXIT;
}

/**
 * mongo_connection_command_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to execute a command on a remote
 * Mongo server. Upon failure, %NULL is returned and @error is set.
 *
 * Returns: (transfer full): A #MongoMessageReply if successful; otherwise %NULL.
 */
MongoMessageReply *
mongo_connection_command_finish (MongoConnection  *connection,
                                 GAsyncResult     *result,
                                 GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   ret = ret ? g_object_ref(ret) : NULL;

   RETURN(ret);
}

/**
 * mongo_connection_delete_async:
 * @connection: (in): A #MongoConnection.
 * @db_and_collection: A string containing the "db.collection".
 * @flags: A bitwise-or of #MongoDeleteFlags.
 * @selector: A #MongoBson of fields to select for deletion.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to execute upon completion.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously requests the removal of one or more documents in a Mongo
 * collection. If you only want to remove a single document from the Mongo
 * collection, then you MUST specify the %MONGO_DELETE_SINGLE_REMOVE flag
 * in @flags.
 *
 * Selector should be a #MongoBson containing the fields to match.
 *
 * @callback MUST call mongo_connection_delete_finish().
 */
void
mongo_connection_delete_async (MongoConnection     *connection,
                               const gchar         *db_and_collection,
                               MongoDeleteFlags     flags,
                               const MongoBson     *selector,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
   Request *request;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(strstr(db_and_collection, "."));
   g_return_if_fail(selector);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   request = request_new(connection, cancellable, callback, user_data,
                         mongo_connection_delete_async);
   request->oper = MONGO_OPERATION_DELETE;
   request->u.delete.db_and_collection = g_strdup(db_and_collection);
   request->u.delete.flags = flags;
   request->u.delete.selector = mongo_bson_dup(selector);
   mongo_connection_queue(connection, request);

   EXIT;
}

/**
 * mongo_connection_delete_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to remove one or more documents from a
 * Mongo collection.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_connection_delete_finish (MongoConnection  *connection,
                                GAsyncResult     *result,
                                GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   ENTRY;

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

/**
 * mongo_connection_update_async:
 * @connection: A #MongoConnection.
 * @db_and_collection: A string containing the "db.collection".
 * @flags: A bitwise-or of #MongoUpdateFlags.
 * @selector: (allow-none): A #MongoBson or %NULL.
 * @update: A #MongoBson to apply as an update to documents matching @selector.
 * @cancellable: (allow-none): A #GCancellable, or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously requests an update to all documents matching @selector.
 *
 * @callback MUST call mongo_connection_update_finish().
 */
void
mongo_connection_update_async (MongoConnection     *connection,
                               const gchar         *db_and_collection,
                               MongoUpdateFlags     flags,
                               const MongoBson     *selector,
                               const MongoBson     *update,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
   Request *request;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(strstr(db_and_collection, "."));
   g_return_if_fail(selector);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   request = request_new(connection, cancellable, callback, user_data,
                         mongo_connection_update_async);
   request->oper = MONGO_OPERATION_UPDATE;
   request->u.update.db_and_collection = g_strdup(db_and_collection);
   request->u.update.flags = flags;
   request->u.update.selector = mongo_bson_dup(selector);
   request->u.update.update = mongo_bson_dup(update);
   mongo_connection_queue(connection, request);

   EXIT;
}

/**
 * mongo_connection_update_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @document: (out) (allow-none) (transfer full): A location for the result
 *   document if provided by the server.
 * @error: (allow-none) (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to mongo_connection_update_async().
 *
 * If @document is not %NULL, then it will store the resulting document
 * provided by the server in that location. The caller is responsible for
 * freeing that data. This can be useful if you need to know if
 * "updatedExisting" was set when performing a %MONGO_UPDATE_UPSERT
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_connection_update_finish (MongoConnection  *connection,
                                GAsyncResult     *result,
                                MongoBson       **document,
                                GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoBson *bson;
   gboolean ret;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   ENTRY;

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   if (document) {
      bson = g_object_get_data(G_OBJECT(result), "document");
      *document = bson ? mongo_bson_ref(bson) : NULL;
   }

   RETURN(ret);
}

/**
 * mongo_connection_insert_async:
 * @connection: A #MongoConnection.
 * @db_and_collection: A string containing the "db.collection".
 * @flags: A bitwise-or of #MongoInsertFlags.
 * @documents: (array length=n_documents) (element-type MongoBson*): Array  of
 *    #MongoBson documents to insert.
 * @n_documents: (in): The number of elements in @documents.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously requests the insertion of a document into the Mongo server.
 *
 * @callback MUST call mongo_connection_insert_finish().
 */
void
mongo_connection_insert_async (MongoConnection      *connection,
                               const gchar          *db_and_collection,
                               MongoInsertFlags      flags,
                               MongoBson           **documents,
                               gsize                 n_documents,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
   Request *request;
   guint i;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(strstr(db_and_collection, "."));
   g_return_if_fail(documents);
   g_return_if_fail(n_documents);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   request = request_new(connection, cancellable, callback, user_data,
                         mongo_connection_insert_async);
   request->oper = MONGO_OPERATION_INSERT;
   request->u.insert.db_and_collection = g_strdup(db_and_collection);
   request->u.insert.flags = flags;
   request->u.insert.documents = g_ptr_array_sized_new(n_documents);
   g_ptr_array_set_free_func(request->u.insert.documents,
                             (GDestroyNotify)mongo_bson_unref);
   for (i = 0; i < n_documents; i++) {
      g_ptr_array_add(request->u.insert.documents,
                      mongo_bson_dup(documents[i]));
   }
   mongo_connection_queue(connection, request);

   EXIT;
}

/**
 * mongo_connection_insert_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asychronous request to insert a document.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_connection_insert_finish (MongoConnection  *connection,
                                GAsyncResult     *result,
                                GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   ENTRY;

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

/**
 * mongo_connection_query_async:
 * @connection: A #MongoConnection.
 * @db_and_collection: A string containing "db.collection".
 * @flags: A bitwise-or of #MongoQueryFlags.
 * @skip: The number of documents to skip in the result set.
 * @limit: The maximum number of documents to retrieve.
 * @query: (allow-none): A #MongoBson containing the query.
 * @field_selector: (allow-none): A #MongoBson describing requested fields.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously queries Mongo for the documents that match. This retrieves
 * the first reply from the server side cursor. Further replies can be
 * retrieved with mongo_connection_getmore_async().
 *
 * @callback MUST call mongo_connection_query_finish().
 */
void
mongo_connection_query_async (MongoConnection     *connection,
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
   MongoConnectionPrivate *priv;
   Request *request;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(db_and_collection);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = connection->priv;

   if (priv->slave_okay) {
      flags |= MONGO_QUERY_SLAVE_OK;
   }

   request = request_new(connection, cancellable, callback, user_data,
                         mongo_connection_query_async);
   request->oper = MONGO_OPERATION_QUERY;
   request->u.query.db_and_collection = g_strdup(db_and_collection);
   request->u.query.flags = flags;
   request->u.query.skip = skip;
   request->u.query.limit = limit;
   request->u.query.query =
      query ? mongo_bson_dup(query) : mongo_bson_new_empty();
   request->u.query.field_selector = mongo_bson_dup(field_selector);
   mongo_connection_queue(connection, request);

   EXIT;
}

/**
 * mongo_connection_query_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to mongo_connection_query_async().
 *
 * Returns: (transfer full): A #MongoMessageReply.
 */
MongoMessageReply *
mongo_connection_query_finish (MongoConnection  *connection,
                               GAsyncResult     *result,
                               GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *reply;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(reply = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   reply = reply ? g_object_ref(reply) : NULL;

   RETURN(reply);
}

/**
 * mongo_connection_getmore_async:
 * @connection: A #MongoConnection.
 * @db_and_collection: A string containing the 'db.collection".
 * @limit: The maximum number of documents to return in the cursor.
 * @cursor_id: The cursor_id provided by the server.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously requests more results from a cursor on the Mongo server.
 *
 * @callback MUST call mongo_connection_getmore_finish().
 */
void
mongo_connection_getmore_async (MongoConnection     *connection,
                                const gchar         *db_and_collection,
                                guint32              limit,
                                guint64              cursor_id,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
   Request *request;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   request = request_new(connection, cancellable, callback, user_data,
                         mongo_connection_getmore_async);
   request->oper = MONGO_OPERATION_GETMORE;
   request->u.getmore.db_and_collection = g_strdup(db_and_collection);
   request->u.getmore.limit = limit;
   request->u.getmore.cursor_id = cursor_id;
   mongo_connection_queue(connection, request);

   EXIT;
}

/**
 * mongo_connection_getmore_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @error: (allow-none) (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to mongo_connection_getmore_finish().
 *
 * Returns: (transfer full): A #MongoMessageReply.
 */
MongoMessageReply *
mongo_connection_getmore_finish (MongoConnection  *connection,
                                 GAsyncResult     *result,
                                 GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   MongoMessageReply *reply;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(reply = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   reply = reply ? g_object_ref(reply) : NULL;

   RETURN(reply);
}

/**
 * mongo_connection_kill_cursors_async:
 * @connection: A #MongoConnection.
 * @cursors: (array length=n_cursors) (element-type guint64): Array of cursors.
 * @n_cursors: Number of elements in @cursors.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback.
 * @user_data: (allow-none): User data for @callback.
 *
 * Asynchronously requests that a series of cursors are killed on the Mongo
 * server.
 *
 * @callback MUST call mongo_connection_kill_cursors_finish().
 */
void
mongo_connection_kill_cursors_async (MongoConnection     *connection,
                                     guint64             *cursors,
                                     gsize                n_cursors,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
   Request *request;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_return_if_fail(cursors);
   g_return_if_fail(n_cursors);
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   request = request_new(connection, cancellable, callback, user_data,
                         mongo_connection_kill_cursors_async);
   request->oper = MONGO_OPERATION_KILL_CURSORS;
   request->u.kill_cursors.cursors = g_array_new(FALSE, FALSE, sizeof(guint64));
   g_array_append_vals(request->u.kill_cursors.cursors, cursors, n_cursors);
   mongo_connection_queue(connection, request);

   EXIT;
}

/**
 * mongo_connection_kill_cursors_finish:
 * @connection: A #MongoConnection.
 * @result: A #GAsyncResult.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to mongo_connection_kill_cursors_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mongo_connection_kill_cursors_finish (MongoConnection  *connection,
                                      GAsyncResult     *result,
                                      GError          **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   RETURN(ret);
}

const gchar *
mongo_connection_get_replica_set (MongoConnection *connection)
{
   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), NULL);
   return connection->priv->replica_set;
}

void
mongo_connection_set_replica_set (MongoConnection *connection,
                                  const gchar     *replica_set)
{
   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   g_free(connection->priv->replica_set);
   connection->priv->replica_set = g_strdup(replica_set);
   g_object_notify_by_pspec(G_OBJECT(connection), gParamSpecs[PROP_REPLICA_SET]);
}

/**
 * mongo_connection_get_uri:
 * @connection: (in): A #MongoConnection.
 *
 * Fetches the uri for the connection.
 *
 * Returns: The uri as a string.
 */
const gchar *
mongo_connection_get_uri (MongoConnection *connection)
{
   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), NULL);
   return connection->priv->uri_string;
}

static void
mongo_connection_set_uri (MongoConnection *connection,
                          const gchar     *uri)
{
   MongoConnectionPrivate *priv;
   const gchar *value;
   GHashTable *params;
   GError *error = NULL;
   gchar **hosts;
   gchar *host_port;
   gchar *lower;
   guint i;
   GUri *guri;

   ENTRY;

   g_return_if_fail(MONGO_IS_CONNECTION(connection));

   priv = connection->priv;

   if (!uri) {
      uri = "mongodb://127.0.0.1:27017";
   }

   if (!g_str_has_prefix(uri, "mongodb://")) {
      g_warning("\"uri\" must start with mongodb://");
      EXIT;
   }

   if (!(guri = g_uri_new(uri, G_URI_PARSE_NON_DNS, &error))) {
      g_warning("Invalid uri: %s", error->message);
      g_error_free(error);
      EXIT;
   }

   if (priv->uri) {
      g_uri_free(priv->uri);
      priv->uri = NULL;
   }

   /*
    * Save the uri for later.
    */
   priv->uri = guri;
   g_free(priv->uri_string);
   priv->uri_string = g_strdup(uri);

   /*
    * Clear seeds/hosts to be used for connecting.
    */
   mongo_manager_clear_seeds(priv->manager);
   mongo_manager_clear_hosts(priv->manager);

   /*
    * Add the seeds to the manager.
    */
   hosts = g_strsplit(priv->uri->host, ",", -1);
   for (i = 0; hosts[i]; i++) {
      /*
       * TODO: This should be fixed after our URI fixes to support
       *       , in the hosts properly.
       */
      if (!strstr(hosts[i], ":") && guri->port) {
         host_port = g_strdup_printf("%s:%u", hosts[i], guri->port);
         mongo_manager_add_seed(priv->manager, host_port);
         g_free(host_port);
      } else {
         mongo_manager_add_seed(priv->manager, hosts[i]);
      }
   }
   g_strfreev(hosts);

   /*
    * Clear existing parameters.
    */
   priv->connecttimeoutms = 0;
   priv->fsync = FALSE;
   priv->fsync_set = FALSE;
   priv->w = 0;
   priv->journal = FALSE;
   priv->journal_set = FALSE;
   g_free(priv->replica_set);
   priv->replica_set = NULL;
   priv->safe = TRUE;
   priv->slave_okay = FALSE;
   priv->sockettimeoutms = 0;
   priv->wtimeoutms = 0;

   /*
    * Parse URI parameters containing connection options.
    */
   lower = g_utf8_strdown(priv->uri->query ?: "", -1);
   if ((params = g_uri_parse_params(lower, -1, '&', TRUE))) {
      if ((value = g_hash_table_lookup(params, "replicaset"))) {
         priv->replica_set = g_strdup(value);
      }
      if ((value = g_hash_table_lookup(params, "slaveok"))) {
         priv->slave_okay = !!g_strcmp0(value, "false");
      }
      if ((value = g_hash_table_lookup(params, "safe"))) {
         priv->safe = !!g_strcmp0(value, "false");
      }
      if ((value = g_hash_table_lookup(params, "w"))) {
         priv->w = MAX(0, strtol(value, NULL, 10));
      }
      if ((value = g_hash_table_lookup(params, "wtimeoutms"))) {
         priv->wtimeoutms = MAX(0, strtol(value, NULL, 10));
      }
      if ((value = g_hash_table_lookup(params, "fsync"))) {
         priv->fsync = !!g_strcmp0(value, "false");
         priv->fsync_set = TRUE;
      }
      if ((value = g_hash_table_lookup(params, "journal"))) {
         priv->journal = !!g_strcmp0(value, "false");
         priv->journal_set = TRUE;
      }
      if ((value = g_hash_table_lookup(params, "connecttimeoutms"))) {
         priv->connecttimeoutms = MAX(0, strtol(value, NULL, 10));
      }
      if ((value = g_hash_table_lookup(params, "sockettimeoutms"))) {
         priv->sockettimeoutms = MAX(0, strtol(value, NULL, 10));
      }
      g_hash_table_unref(params);
   }
   g_free(lower);

   EXIT;
}

/**
 * mongo_connection_get_slave_okay:
 * @connection: A #MongoConnection.
 *
 * Retrieves the "slave-okay" property. If "slave-okay" is %TRUE, then
 * %MONGO_QUERY_SLAVE_OK will be set on all outgoing queries.
 *
 * Returns: %TRUE if "slave-okay" is set.
 */
gboolean
mongo_connection_get_slave_okay (MongoConnection *connection)
{
   g_return_val_if_fail(MONGO_IS_CONNECTION(connection), FALSE);
   return connection->priv->slave_okay;
}

/**
 * mongo_connection_set_slave_okay:
 * @connection: A #MongoConnection.
 * @slave_okay: A #gboolean.
 *
 * Sets the "slave-okay" property. If @slave_okay is %TRUE, then all queries
 * will have the %MONGO_QUERY_SLAVE_OK flag set, allowing them to be executed
 * on slave servers.
 */
void
mongo_connection_set_slave_okay (MongoConnection *connection,
                                 gboolean         slave_okay)
{
   g_return_if_fail(MONGO_IS_CONNECTION(connection));
   connection->priv->slave_okay = slave_okay;
   g_object_notify_by_pspec(G_OBJECT(connection),
                            gParamSpecs[PROP_SLAVE_OKAY]);
}

static void
mongo_connection_finalize (GObject *object)
{
   MongoConnectionPrivate *priv;
   GHashTable *hash;
   Request *request;

   ENTRY;

   priv = MONGO_CONNECTION(object)->priv;

   if ((hash = priv->databases)) {
      priv->databases = NULL;
      g_hash_table_unref(hash);
   }

   /*
    * TODO: Move a lot of this to dispose.
    */

   while ((request = g_queue_pop_head(priv->queue))) {
      request_fail(request, NULL);
      request_free(request);
   }

   mongo_manager_unref(priv->manager);
   priv->manager = NULL;

   g_queue_free(priv->queue);
   priv->queue = NULL;

   g_clear_object(&priv->socket_client);
   g_clear_object(&priv->protocol);

   if (priv->uri_string) {
      g_free(priv->uri_string);
      priv->uri_string = NULL;
   }

   g_uri_free(priv->uri);
   priv->uri = NULL;

   G_OBJECT_CLASS(mongo_connection_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_connection_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
   MongoConnection *connection = MONGO_CONNECTION(object);

   switch (prop_id) {
   case PROP_REPLICA_SET:
      g_value_set_string(value, mongo_connection_get_replica_set(connection));
      break;
   case PROP_SLAVE_OKAY:
      g_value_set_boolean(value, mongo_connection_get_slave_okay(connection));
      break;
   case PROP_URI:
      g_value_set_string(value, mongo_connection_get_uri(connection));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_connection_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
   MongoConnection *connection = MONGO_CONNECTION(object);

   switch (prop_id) {
   case PROP_REPLICA_SET:
      mongo_connection_set_replica_set(connection, g_value_get_string(value));
      break;
   case PROP_SLAVE_OKAY:
      mongo_connection_set_slave_okay(connection, g_value_get_boolean(value));
      break;
   case PROP_URI:
      mongo_connection_set_uri(connection, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_connection_class_init (MongoConnectionClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_connection_finalize;
   object_class->get_property = mongo_connection_get_property;
   object_class->set_property = mongo_connection_set_property;
   g_type_class_add_private(object_class, sizeof(MongoConnectionPrivate));

   gParamSpecs[PROP_REPLICA_SET] =
      g_param_spec_string("replica-set",
                          _("Replica Set"),
                          _("The replica set to enforce connecting to."),
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_REPLICA_SET,
                                   gParamSpecs[PROP_REPLICA_SET]);

   gParamSpecs[PROP_SLAVE_OKAY] =
      g_param_spec_boolean("slave-okay",
                          _("Slave Okay"),
                          _("If it is okay to query a Mongo slave."),
                          FALSE,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_SLAVE_OKAY,
                                   gParamSpecs[PROP_SLAVE_OKAY]);

   gParamSpecs[PROP_URI] =
      g_param_spec_string("uri",
                          _("URI"),
                          _("The connection URI."),
                          "mongodb://127.0.0.1:27017/",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_URI,
                                   gParamSpecs[PROP_URI]);

   gSignals[CONNECTED] = g_signal_new("connected",
                                      MONGO_TYPE_CONNECTION,
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL,
                                      NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);

   EXIT;
}

static void
mongo_connection_init (MongoConnection *connection)
{
   ENTRY;

   connection->priv = G_TYPE_INSTANCE_GET_PRIVATE(connection,
                                              MONGO_TYPE_CONNECTION,
                                              MongoConnectionPrivate);
   connection->priv->databases = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                   g_free, g_object_unref);
   connection->priv->manager = mongo_manager_new();
   mongo_manager_add_seed(connection->priv->manager, "127.0.0.1:27017");
   connection->priv->queue = g_queue_new();
   connection->priv->safe = TRUE;
   connection->priv->socket_client =
         g_object_new(G_TYPE_SOCKET_CLIENT,
                      "timeout", 0,
                      "family", G_SOCKET_FAMILY_IPV4,
                      "protocol", G_SOCKET_PROTOCOL_TCP,
                      "type", G_SOCKET_TYPE_STREAM,
                      NULL);

   EXIT;
}

GQuark
mongo_connection_error_quark (void)
{
   return g_quark_from_static_string("MongoConnectionError");
}
