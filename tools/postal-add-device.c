/* postal-add-device.c
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
static gchar *gDeviceType;
static GOptionEntry gEntries[] = {
   { "device-token", 'd', 0, G_OPTION_ARG_STRING, &gDeviceToken,
     N_("Device identifier."),
     N_("STRING") },
   { "device-type", 't', 0, G_OPTION_ARG_STRING, &gDeviceType,
     N_("Device type."),
     N_("aps|c2dm|gcm") },
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
   PostalDeviceType device_type;
   GOptionContext *context;
   JsonGenerator *g;
   PostalDevice *d;
   SoupSession *session;
   SoupMessage *message;
   JsonNode *node;
   GError *error = NULL;
   gchar *str;
   gchar *uri_string;
   gsize length;

   context = g_option_context_new(_("- Add device to Postal"));
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

   if (!gDeviceType ||
       !(g_str_equal(gDeviceType, "aps") ||
         g_str_equal(gDeviceType, "c2dm") ||
         g_str_equal(gDeviceType, "gcm"))) {
      g_printerr("Please provide -t device type.\n");
      return EXIT_FAILURE;
   }

   if (g_str_equal(gDeviceType, "aps")) {
      device_type = POSTAL_DEVICE_APS;
   } else if (g_str_equal(gDeviceType, "c2dm")) {
      device_type = POSTAL_DEVICE_C2DM;
   } else if (g_str_equal(gDeviceType, "gcm")) {
      device_type = POSTAL_DEVICE_GCM;
   } else {
      g_assert_not_reached();
   }

   d = g_object_new(POSTAL_TYPE_DEVICE,
                    "device-type", device_type,
                    "device-token", gDeviceToken,
                    "user", gUser,
                    NULL);
   if (!(node = postal_device_save_to_json(d, &error))) {
      g_printerr("%s\n", error->message);
      g_error_free(error);
      g_object_unref(d);
      return EXIT_FAILURE;
   }

   g_object_unref(d);

   g = json_generator_new();
   json_generator_set_root(g, node);
   str = json_generator_to_data(g, &length);
   json_node_free(node);
   g_object_unref(g);

   uri_string = g_strdup_printf(
         "http://%s:%d/v1/users/%s/devices/%s",
         (gHost && *gHost) ? gHost : "localhost",
         gPort ? gPort : 5300,
         gUser,
         gDeviceToken);
   message = soup_message_new("PUT", uri_string);
   soup_message_body_append_take(message->request_body, (guint8 *)str, length);
   g_free(uri_string);

   session = soup_session_sync_new();
   soup_session_send_message(session, message);

   g_object_unref(message);
   g_object_unref(session);

   g_option_context_free(context);
   g_free(gUser);
   g_free(gDeviceToken);
   g_free(gDeviceType);

   return EXIT_SUCCESS;
}
