/* main.c
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
#include <glib/gi18n.h>

#include "postal-application.h"

gint
main (gint   argc,
      gchar *argv[])
{
   PostalApplication *application;
   gint ret;

   g_type_init();
   g_set_prgname("Postal");
   g_set_application_name(_("PostalD"));

   application = g_object_new(POSTAL_TYPE_APPLICATION,
                              "application-id", "com.catch.postald",
                              "flags", G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_COMMAND_LINE,
                              NULL);
   g_application_set_default(G_APPLICATION(application));
   ret = g_application_run(G_APPLICATION(application), argc, argv);
   g_clear_object(&application);

   return ret;
}
