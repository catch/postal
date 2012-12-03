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

#include "neo-logger-unix.h"
#include "postal-application.h"
#include "postal-debug.h"
#include "postal-http.h"
#ifdef ENABLE_REDIS
#include "postal-redis.h"
#endif
#include "postal-service.h"

G_DEFINE_TYPE(PostalApplication, postal_application, NEO_TYPE_APPLICATION)

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

   ENTRY;

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
      GOTO(cleanup);
   }

#ifdef SYSCONFDIR
   if (!config) {
      if (g_file_test(SYSCONFDIR"/postald.conf", G_FILE_TEST_EXISTS)) {
         config = g_strdup(SYSCONFDIR"/postald.conf");
      }
   }
#endif


   if (config) {
      key_file = g_key_file_new();
      if (!g_key_file_load_from_file(key_file, config, 0, &error)) {
         g_application_command_line_printerr(command_line,
                                             _("Failed to parse config (%s): %s\n"),
                                             config,
                                             error->message);
         g_key_file_unref(key_file);
         GOTO(cleanup);
      }
      neo_application_set_config(NEO_APPLICATION(application), key_file);
      g_key_file_unref(key_file);
   }

   neo_service_start(NEO_SERVICE(application), NULL);

cleanup:
   g_option_context_free(context);
   g_clear_error(&error);
   g_strfreev(argv);
   g_free(config);

   RETURN(ret);
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
   NeoService *service;
   NeoLogger *logger;

   logger = neo_logger_unix_new(STDOUT_FILENO, FALSE);
   neo_application_add_logger(NEO_APPLICATION(application), logger);
   g_object_unref(logger);

   service = NEO_SERVICE(postal_service_new());
   neo_service_add_child(NEO_SERVICE(application), service);
   g_object_unref(service);

   service = NEO_SERVICE(postal_http_new());
   neo_service_add_child(NEO_SERVICE(application), service);
   g_object_unref(service);

#ifdef ENABLE_REDIS
   service = NEO_SERVICE(postal_redis_new());
   neo_service_add_child(NEO_SERVICE(application), service);
   g_object_unref(service);
#endif
}
