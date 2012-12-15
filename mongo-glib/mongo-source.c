/* mongo-source.c
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

#include "mongo-debug.h"
#include "mongo-source.h"

struct _MongoSource
{
   GSource source;
   GMutex mutex;
   GSList *head;
   GSList *tail;
};

/*
 * TODO: This currently only allows for completions within the default
 *       GMainContext. This might be useful to allow completing in other
 *       contexts (create a new source for each). That would require that
 *       we allow for configuring either protocol or connection with an
 *       async context.
 */

static gboolean mongo_source_prepare  (MongoSource *source,
                                       gint        *timeout_);
static gboolean mongo_source_check    (MongoSource *source);
static gboolean mongo_source_dispatch (MongoSource *source,
                                       GSourceFunc  callback,
                                       gpointer     user_data);
static void     mongo_source_finalize (MongoSource *source);

static MongoSource  *gMongoSource;
static GSourceFuncs  gMongoSourceFuncs = {
   (gpointer)mongo_source_prepare,
   (gpointer)mongo_source_check,
   (gpointer)mongo_source_dispatch,
   (gpointer)mongo_source_finalize,
};

MongoSource *
mongo_source_new (void)
{
   MongoSource *msource;
   GSource *source;

   source = g_source_new(&gMongoSourceFuncs, sizeof(MongoSource));
   msource = (MongoSource *)source;
   g_source_set_name(source, "MongoSource");
   g_source_set_priority(source, G_PRIORITY_DEFAULT);
   g_mutex_init(&msource->mutex);

   return msource;
}

MongoSource *
mongo_source_get_source (void)
{
   if (g_once_init_enter(&gMongoSource)) {
      MongoSource *tmp;
      tmp = (MongoSource *)g_source_new(&gMongoSourceFuncs,
                                        sizeof(MongoSource));
      g_mutex_init(&tmp->mutex);
      g_source_set_name((GSource *)tmp, "MongoSource");
      g_source_attach((GSource *)tmp, g_main_context_default());
      g_once_init_leave(&gMongoSource, tmp);
   }

   return gMongoSource;
}

static gboolean
mongo_source_prepare (MongoSource *source,
                      gint        *timeout_)
{
   return !!source->head;
}

static gboolean
mongo_source_check (MongoSource *source)
{
   return !!source->head;
}

static gboolean
mongo_source_dispatch (MongoSource *source,
                       GSourceFunc  callback,
                       gpointer     user_data)
{
   GSList *list;
   GSList *iter;

   ENTRY;

   if (callback) {
      (*callback) (user_data);
   }

   g_mutex_lock(&source->mutex);

   list = source->head;
   source->head = NULL;
   source->tail = NULL;

   g_mutex_unlock(&source->mutex);

   for (iter = list; iter; iter = iter->next) {
      g_simple_async_result_complete(iter->data);
      g_object_unref(iter->data);
   }

   g_slist_free(list);

   RETURN(TRUE);
}

static void
mongo_source_finalize (MongoSource *source)
{
   GSList *list;

   ENTRY;

   g_mutex_clear(&source->mutex);

   list = source->head;
   source->head = NULL;
   source->tail = NULL;

   g_slist_foreach(list, (GFunc)g_object_unref, NULL);
   g_slist_free(list);

   EXIT;
}

void
mongo_source_complete_in_idle (MongoSource        *source,
                               GSimpleAsyncResult *simple)
{
   GSList *link_;

   ENTRY;

   link_ = g_slist_append(NULL, g_object_ref(simple));

   g_mutex_lock(&source->mutex);

   if (source->tail) {
      source->tail->next = link_;
   }

   source->tail = link_;

   if (!source->head) {
      source->head = link_;
   }

   g_mutex_unlock(&source->mutex);

   g_main_context_wakeup(g_source_get_context((GSource *)source));

   EXIT;
}

/**
 * mongo_simple_async_result_complete_in_idle:
 * @simple: A #GSimpleAsyncResult.
 *
 * Requests the completion of a simple async result in the main loop.
 * This is preferred to g_simple_async_result_complete_in_idle() as
 * it doesn't require a new GSource to be allocated and attached like
 * g_simple_async_result_complete_in_idle() does.
 */
void
mongo_simple_async_result_complete_in_idle (GSimpleAsyncResult *simple)
{
   MongoSource *source;
   GSList *link_;

   ENTRY;

   source = mongo_source_get_source();
   link_ = g_slist_append(NULL, g_object_ref(simple));

   g_mutex_lock(&source->mutex);

   if (source->tail) {
      source->tail->next = link_;
   }

   source->tail = link_;

   if (!source->head) {
      source->head = link_;
   }

   g_mutex_unlock(&source->mutex);

   g_main_context_wakeup(g_source_get_context((GSource *)source));

   EXIT;
}
