/* postal-remove-device.c
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <postal/postal-device.h>
#include <stdlib.h>

static gchar *gHost;
static guint  gPort;
static gchar *gUser;
static gchar *gDeviceToken;
static GOptionEntry gEntries[] = {
   { "device-token", 'd', 0, G_OPTION_ARG_STRING, &gDeviceToken,
     N_("Device identifier."),
     N_("STRING") },
   { "user", 'u', 0, G_OPTION_ARG_STRING, &gUser,
     N_("User identifier owning the device."),
     N_("STRING") },
   { "host", 'h', 0, G_OPTION_ARG_STRING, &gHost,
     N_("The hostname of the postal server."),
     N_("localhost") },
   { "port", 'p', 0, G_OPTION_ARG_INT, &gPort,
     N_("The port of the postal server."),
     N_("5300") },
   { 0 }
};

gint
main (gint   argc,
      gchar *argv[])
{
   GOptionContext *context;
   SoupSession *session;
   SoupMessage *message;
   GError *error = NULL;
   gchar *uri_string;

   context = g_option_context_new(_("- Remove device from Postal"));
   g_option_context_add_main_entries(context, gEntries, NULL);
   if (!g_option_context_parse(context, &argc, &argv, &error)) {
      g_printerr("%s\n", error->message);
      g_error_free(error);
      return EXIT_FAILURE;
   }

   g_type_init();

   if (!gUser || !*gUser) {
      g_printerr("Please provide -u user identifier.\n");
      return EXIT_FAILURE;
   }

   if (!gDeviceToken || !*gDeviceToken) {
      g_printerr("Please provide -d device token.\n");
      return EXIT_FAILURE;
   }

   uri_string = g_strdup_printf(
         "http://%s:%d/v1/users/%s/devices/%s",
         (gHost && *gHost) ? gHost : "localhost",
         gPort ? gPort : 5300,
         gUser,
         gDeviceToken);
   message = soup_message_new("DELETE", uri_string);
   g_free(uri_string);

   session = soup_session_sync_new();
   soup_session_send_message(session, message);

   g_object_unref(message);
   g_object_unref(session);

   g_option_context_free(context);
   g_free(gUser);
   g_free(gDeviceToken);

   return EXIT_SUCCESS;
}
