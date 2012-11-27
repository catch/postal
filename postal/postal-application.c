/* postal-application.c
 *
 * Copyright (C) 2012 Catch.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif /* __linux__ */

#include "neo-logger-unix.h"
#include "postal-application.h"
#include "postal-debug.h"
#include "postal-http.h"
#ifdef ENABLE_REDIS
#include "postal-redis.h"
#endif
#include "postal-service.h"

G_DEFINE_TYPE(PostalApplication, postal_application, G_TYPE_APPLICATION)

static NeoLogger *gLoggerStdout;
static guint      gPid;

/**
 * postal_application_command_line:
 * @application: A #GApplication.
 * @command_line: A #GApplicationCommandLine.
 *
 * This function handles the command-line for a local or remote process.
 * The process could be remote if there is already an instance. That allows
 * us to do things like remotely control this instance and shutdown, reload
 * or other commands.
 */
static gint
postal_application_command_line (GApplication            *application,
                                 GApplicationCommandLine *command_line)
{
   static gchar *config;
   static const GOptionEntry entries[] = {
      { "config", 'c', 0, G_OPTION_ARG_FILENAME, &config, N_("Configuration filename.") },
      { 0 }
   };
   GOptionContext *context;
   GKeyFile *key_file = NULL;
   GError *error = NULL;
   gchar **argv;
   gint argc;
   gint ret = EXIT_SUCCESS;

   g_assert(G_IS_APPLICATION(application));
   g_assert(G_IS_APPLICATION_COMMAND_LINE(command_line));

   /*
    * Setup our command line parser to parse arguments to the starting
    * application. This could be for our own process or for another instance
    * of it trying to communicate with us (transfering its command line
    * arguments to use to take proper action).
    */
   context = g_option_context_new(_("- Push Notification Daemon"));
   g_option_context_add_main_entries(context, entries, NULL);
   argv = g_application_command_line_get_arguments(command_line, &argc);
   if (!g_option_context_parse(context, &argc, &argv, &error)) {
      g_application_command_line_printerr(command_line, "%s\n", error->message);
      ret = EXIT_FAILURE;
      goto cleanup;
   }

   /*
    * If we are parsing command line from this application starting, we can
    * go ahead and load the configuration file and start the service.
    */
   if (!g_application_command_line_get_is_remote(command_line)) {
#ifdef SYSCONFDIR
      if (!config) {
         if (g_file_test(SYSCONFDIR"/postal.conf", G_FILE_TEST_EXISTS)) {
            config = g_strdup(SYSCONFDIR"/postal.conf");
         }
      }
#endif

      if (config) {
         key_file = g_key_file_new();
         if (!g_key_file_load_from_file(key_file,
                                        config,
                                        G_KEY_FILE_NONE,
                                        &error)) {
            g_application_command_line_printerr(
                  command_line,
                  _("Failed to parse config (%s): %s\n"),
                  config, error->message);
            g_key_file_unref(key_file);
            goto cleanup;
         }
         g_object_set(POSTAL_SERVICE_DEFAULT,
                      "config", key_file,
                      NULL);
      }

      /*
       * Initialize subsystems.
       */
      postal_service_start(POSTAL_SERVICE_DEFAULT);
      postal_http_init(key_file);
#ifdef ENABLE_REDIS
      postal_redis_init(key_file);
#endif

      /*
       * Take reference to application so
       * that it doesn't return immediately.
       */
      g_application_hold(application);
   }

cleanup:
   if (key_file) {
      g_key_file_unref(key_file);
   }
   g_option_context_free(context);
   g_clear_error(&error);
   g_strfreev(argv);
   g_free(config);
   config = NULL;

   return ret;
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
postal_application_log_handler (const gchar    *log_domain,
                                GLogLevelFlags  log_level,
                                const gchar    *message,
                                gpointer        user_data)
{
   const gchar *log_level_str = "UNKNOWN";
   const gchar *host;
   GTimeVal event_time;
   struct tm tt;
   time_t t;
   gchar *formatted = NULL;
   gchar ftime[32];
   guint tid;

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
   case POSTAL_LOG_LEVEL_TRACE:
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
   neo_logger_log(gLoggerStdout,
                  &event_time,
                  log_domain,
                  host,
                  gPid,
                  tid,
                  log_level,
                  message,
                  formatted);
   g_free(formatted);
}

static void
postal_application_class_init (PostalApplicationClass *klass)
{
   GApplicationClass *application_class;

   application_class = G_APPLICATION_CLASS(klass);
   application_class->command_line = postal_application_command_line;
}

static void
postal_application_init (PostalApplication *application)
{
   gPid = getpid();
   gLoggerStdout = neo_logger_unix_new(STDOUT_FILENO, FALSE);
   g_log_set_default_handler(postal_application_log_handler, NULL);
}
