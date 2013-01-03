/* neo-application.c
 *
 * Copyright (C) 2012 Catch.com
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
 *
 * Authors:
 *    Christian Hergert <christian@catch.com>
 */

#include <glib/gi18n.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif /* __linux__ */

#include "neo-application.h"
#include "neo-debug.h"
#include "neo-service.h"

struct _NeoApplicationPrivate
{
   GHashTable *children;
   GKeyFile   *config;
   gboolean    is_running;
   GMutex      mutex;
   GPtrArray  *loggers;
   gboolean    logging_enabled;
};

enum
{
   PROP_0,
   PROP_LOGGING_ENABLED,
   LAST_PROP
};

static void neo_service_init (NeoServiceIface *iface);

G_DEFINE_TYPE_EXTENDED(NeoApplication,
                       neo_application,
                       G_TYPE_APPLICATION,
                       0,
                       G_IMPLEMENT_INTERFACE(NEO_TYPE_SERVICE,
                                             neo_service_init))

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * neo_application_set_config:
 * @application: A #NeoApplication.
 * @config: A #GKeyFile.
 *
 * Sets the configuration to use for the application. This should be set
 * before calling neo_application_start().
 */
void
neo_application_set_config (NeoApplication *application,
                            GKeyFile       *config)
{
   NeoApplicationPrivate *priv;

   g_return_if_fail(NEO_IS_APPLICATION(application));

   priv = application->priv;

   if (priv->config) {
      g_key_file_unref(priv->config);
      priv->config = NULL;
   }

   if (config) {
      priv->config = g_key_file_ref(config);
   }
}

static inline guint
postal_application_get_thread (void)
{
#if __linux__
   return (gint)syscall(SYS_gettid);
#else
   return getpid();
#endif /* __linux__ */
}

static void
neo_application_log_handler (const gchar    *log_domain,
                             GLogLevelFlags  log_level,
                             const gchar    *message,
                             gpointer        user_data)
{
   NeoApplicationPrivate *priv;
   const gchar *log_level_str = "UNKNOWN";
   NeoApplication *application = user_data;
   const gchar *host;
   NeoLogger *logger;
   GTimeVal event_time;
   struct tm tt;
   time_t t;
   gchar *formatted = NULL;
   gchar ftime[32];
   guint tid;
   guint i;

   g_assert(NEO_IS_APPLICATION(application));

   priv = application->priv;

   tid = postal_application_get_thread();

   g_get_current_time(&event_time);
   t = (time_t)event_time.tv_sec;
   localtime_r(&t, &tt);
   strftime(ftime, sizeof ftime, "%Y/%m/%d %H:%M:%S", &tt);
   ftime[31] = '\0';

   #define CASE_LEVEL_STR(_l) case G_LOG_LEVEL_##_l: log_level_str = #_l; break
   switch ((log_level & G_LOG_LEVEL_MASK)) {
   CASE_LEVEL_STR(ERROR);
   CASE_LEVEL_STR(CRITICAL);
   CASE_LEVEL_STR(WARNING);
   CASE_LEVEL_STR(MESSAGE);
   CASE_LEVEL_STR(INFO);
   CASE_LEVEL_STR(DEBUG);
   case NEO_LOG_LEVEL_TRACE:
      log_level_str = "TRACE";
   default: break;
   }
   #undef CASE_LEVEL_STR

   host = g_get_host_name();
   formatted = g_strdup_printf("%s.%04ld  %s: %14s[%d]: %8s: %s\n",
                               ftime,
                               event_time.tv_usec / 100,
                               host,
                               log_domain,
                               tid,
                               log_level_str,
                               message);

   g_mutex_lock(&priv->mutex);

   for (i = 0; i < priv->loggers->len; i++) {
      logger = g_ptr_array_index(priv->loggers, i);
      neo_logger_log(logger,
                     &event_time,
                     log_domain,
                     host,
                     getpid(),
                     tid,
                     log_level,
                     message,
                     formatted);
   }

   g_mutex_unlock(&priv->mutex);

   g_free(formatted);
}

/**
 * neo_application_get_logging_enabled:
 * @application: A #NeoApplication.
 *
 * Gets if @application should manage logging for the process.
 *
 * Returns: %TRUE if #NeoApplication is managing process logging.
 */
gboolean
neo_application_get_logging_enabled (NeoApplication *application)
{
   g_return_val_if_fail(NEO_IS_APPLICATION(application), FALSE);
   return application->priv->logging_enabled;
}

/**
 * neo_application_set_logging_enabled:
 * @application: A #NeoApplication.
 * @logging_enabled: A #gboolean.
 *
 * Sets if @application should manage logging for the process. If
 * @logging_enabled is %FALSE, then the default logger will be enabled.
 * Otherwise, the @application instance will configure it's own logger
 * and loggers added with neo_application_add_logger() will be notified
 * on new log messages.
 */
void
neo_application_set_logging_enabled (NeoApplication *application,
                                     gboolean        logging_enabled)
{
   g_return_if_fail(NEO_IS_APPLICATION(application));

   if ((application->priv->logging_enabled = logging_enabled)) {
      g_log_set_default_handler(neo_application_log_handler, application);
   } else {
      g_log_set_default_handler(g_log_default_handler, NULL);
   }

   g_object_notify_by_pspec(G_OBJECT(application),
                            gParamSpecs[PROP_LOGGING_ENABLED]);
}

/**
 * neo_application_add_logger:
 * @application: A #NeoApplication.
 * @logger: A #NeoLogger.
 *
 * Adds @logger to the list of loggers notified on incoming log messages.
 * :logging-enabled must be %TRUE for @logger to be notified.
 */
void
neo_application_add_logger (NeoApplication *application,
                            NeoLogger      *logger)
{
   NeoApplicationPrivate *priv;

   g_return_if_fail(NEO_IS_APPLICATION(application));
   g_return_if_fail(NEO_IS_LOGGER(logger));

   priv = application->priv;

   g_mutex_lock(&priv->mutex);
   g_ptr_array_add(priv->loggers, g_object_ref(logger));
   g_mutex_unlock(&priv->mutex);
}

static void
neo_application_add_child (NeoService *service,
                           NeoService *child)
{
   NeoApplicationPrivate *priv;
   NeoApplication *application = (NeoApplication *)service;

   g_return_if_fail(NEO_IS_APPLICATION(application));
   g_return_if_fail(NEO_IS_SERVICE(child));

   priv = application->priv;

   g_hash_table_insert(priv->children,
                       g_strdup(neo_service_get_name(child)),
                       g_object_ref(child));
   neo_service_set_parent(child, service);
}

static NeoService *
neo_application_get_child (NeoService  *service,
                           const gchar *name)
{
   NeoApplication *application = (NeoApplication *)service;
   NeoService *child;

   NEO_ENTRY;
   g_return_val_if_fail(NEO_IS_APPLICATION(application), NULL);
   child = g_hash_table_lookup(application->priv->children, name);
   NEO_RETURN(child);
}

static gboolean
neo_application_get_is_running (NeoService *service)
{
   NeoApplication *application = (NeoApplication *)service;
   g_return_val_if_fail(NEO_IS_APPLICATION(application), FALSE);
   return application->priv->is_running;
}

static const gchar *
neo_application_get_name (NeoService *service)
{
   g_return_val_if_fail(NEO_IS_APPLICATION(service), NULL);
   return g_application_get_application_id(G_APPLICATION(service));
}

static void
neo_application_start (NeoService *service,
                       GKeyFile   *config)
{
   NeoApplicationPrivate *priv;
   NeoApplication *application = (NeoApplication *)service;
   GHashTableIter iter;
   gpointer value;

   g_return_if_fail(NEO_IS_APPLICATION(application));

   priv = application->priv;

   priv->is_running = TRUE;

   if (!config) {
      config = priv->config;
   }

   g_hash_table_iter_init(&iter, priv->children);
   while (g_hash_table_iter_next(&iter, NULL, &value)) {
      neo_service_start(value, config);
   }

   g_application_hold(G_APPLICATION(service));
}

static void
neo_application_stop (NeoService *service)
{
   NeoApplicationPrivate *priv;
   NeoApplication *application = (NeoApplication *)service;
   GHashTableIter iter;
   gpointer value;

   g_return_if_fail(NEO_IS_APPLICATION(application));

   priv = application->priv;

   priv->is_running = FALSE;

   g_hash_table_iter_init(&iter, priv->children);
   while (g_hash_table_iter_next(&iter, NULL, &value)) {
      neo_service_stop(value);
   }

   g_application_release(G_APPLICATION(service));
}

static void
neo_application_finalize (GObject *object)
{
   NeoApplicationPrivate *priv;

   priv = NEO_APPLICATION(object)->priv;

   if (priv->is_running) {
      neo_service_stop(NEO_SERVICE(object));
   }

   if (priv->logging_enabled) {
      g_log_set_default_handler(g_log_default_handler, NULL);
   }

   g_hash_table_unref(priv->children);
   priv->children = NULL;

   g_mutex_clear(&priv->mutex);

   g_ptr_array_unref(priv->loggers);
   priv->loggers = NULL;

   G_OBJECT_CLASS(neo_application_parent_class)->finalize(object);
}

static void
neo_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
   NeoApplication *application = NEO_APPLICATION(object);

   switch (prop_id) {
   case PROP_LOGGING_ENABLED:
      g_value_set_boolean(value,
                          neo_application_get_logging_enabled(application));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
   NeoApplication *application = NEO_APPLICATION(object);

   switch (prop_id) {
   case PROP_LOGGING_ENABLED:
      neo_application_set_logging_enabled(application,
                                          g_value_get_boolean(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_application_class_init (NeoApplicationClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = neo_application_finalize;
   object_class->get_property = neo_application_get_property;
   object_class->set_property = neo_application_set_property;
   g_type_class_add_private(object_class, sizeof(NeoApplicationPrivate));

   gParamSpecs[PROP_LOGGING_ENABLED] =
      g_param_spec_boolean("logging-enabled",
                           _("Logging Enabled"),
                           _("If the application should manage logging."),
                           TRUE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_LOGGING_ENABLED,
                                   gParamSpecs[PROP_LOGGING_ENABLED]);
}

static void
neo_application_init (NeoApplication *application)
{
   application->priv = G_TYPE_INSTANCE_GET_PRIVATE(application,
                                                   NEO_TYPE_APPLICATION,
                                                   NeoApplicationPrivate);
   application->priv->children = g_hash_table_new_full(g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);
   application->priv->loggers = g_ptr_array_new_with_free_func(g_object_unref);
   neo_application_set_logging_enabled(application, TRUE);
}

static void
neo_service_init (NeoServiceIface *iface)
{
   iface->add_child = neo_application_add_child;
   iface->get_child = neo_application_get_child;
   iface->get_is_running = neo_application_get_is_running;
   iface->get_name = neo_application_get_name;
   iface->start = neo_application_start;
   iface->stop = neo_application_stop;
}
