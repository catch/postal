/* neo-service.c
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

#include "neo-debug.h"
#include "neo-service.h"

void
neo_service_add_child (NeoService *service,
                       NeoService *child)
{
   NeoServiceIface *iface;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));
   g_return_if_fail(NEO_IS_SERVICE(child));

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->add_child) {
      iface->add_child(service, child);
   }

   NEO_EXIT;
}

/**
 * neo_service_get_child:
 * @service: A #NeoService.
 * @name: The name of the child service.
 *
 * Fetches a child service of the #NeoService named @name.
 *
 * Returns: (transfer none): A #NeoService if successful; or %NULL.
 */
NeoService *
neo_service_get_child (NeoService  *service,
                       const gchar *name)
{
   NeoServiceIface *iface;
   NeoService *ret = NULL;

   NEO_ENTRY;

   g_return_val_if_fail(NEO_IS_SERVICE(service), NULL);

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->get_child) {
      ret = iface->get_child(service, name);
   }

   NEO_RETURN(ret);
}

gboolean
neo_service_get_is_running (NeoService *service)
{
   NeoServiceIface *iface;
   gboolean ret = FALSE;

   NEO_ENTRY;

   g_return_val_if_fail(NEO_IS_SERVICE(service), FALSE);

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->get_is_running) {
      ret = iface->get_is_running(service);
   }

   NEO_RETURN(ret);
}

const gchar *
neo_service_get_name (NeoService *service)
{
   NeoServiceIface *iface;
   const gchar *ret = FALSE;

   NEO_ENTRY;

   g_return_val_if_fail(NEO_IS_SERVICE(service), NULL);

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->get_name) {
      ret = iface->get_name(service);
   }

   NEO_RETURN(ret);
}

/**
 * neo_service_get_parent:
 * @service: A #NeoService.
 *
 * Fetches the parent service of the service.
 *
 * Returns: (transfer none): A #NeoService if successful; otherwise %NULL.
 */
NeoService *
neo_service_get_parent (NeoService *service)
{
   NeoServiceIface *iface;
   NeoService *ret = NULL;

   NEO_ENTRY;

   g_return_val_if_fail(NEO_IS_SERVICE(service), NULL);

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->get_parent) {
      ret = iface->get_parent(service);
   }

   NEO_RETURN(ret);
}

void
neo_service_set_parent (NeoService *service,
                        NeoService *parent)
{
   NeoServiceIface *iface;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->set_parent) {
      iface->set_parent(service, parent);
   }

   NEO_EXIT;
}

void
neo_service_start (NeoService *service,
                   GKeyFile   *config)
{
   NeoServiceIface *iface;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->start) {
      iface->start(service, config);
   }

   NEO_EXIT;
}

void
neo_service_stop (NeoService *service)
{
   NeoServiceIface *iface;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));

   if ((iface = NEO_SERVICE_GET_INTERFACE(service))->stop) {
      iface->stop(service);
   }

   NEO_EXIT;
}

/**
 * neo_service_get_peer:
 * @service: A #NeoService.
 * @name: A string containing the service name.
 *
 * Fetches a service that is a peer to this one. A peer service is a service
 * that shares the same parent.
 *
 * Returns: (transfer none): A #NeoService if successful; otherwise %NULL.
 */
NeoService *
neo_service_get_peer (NeoService  *service,
                      const gchar *name)
{
   NeoService *parent;
   NeoService *ret = NULL;

   NEO_ENTRY;

   g_return_val_if_fail(NEO_IS_SERVICE(service), NULL);
   g_return_val_if_fail(name, NULL);

   if ((parent = neo_service_get_parent(service))) {
      ret = neo_service_get_child(parent, name);
   }

   NEO_RETURN(ret);
}

GType
neo_service_get_type (void)
{
   static volatile GType type_id = 0;

   if (g_once_init_enter((gsize *)&type_id)) {
      GType _type_id;
      const GTypeInfo g_type_info = {
         sizeof(NeoServiceIface),
         NULL, /* base_init */
         NULL, /* base_finalize */
         NULL, /* class_init */
         NULL, /* class_finalize */
         NULL, /* class_data */
         0,    /* instance_size */
         0,    /* n_preallocs */
         NULL, /* instance_init */
         NULL  /* value_vtable */
      };

      _type_id = g_type_register_static(G_TYPE_INTERFACE,
                                        "NeoService",
                                        &g_type_info,
                                        0);
      g_type_interface_add_prerequisite(_type_id, G_TYPE_OBJECT);
      g_once_init_leave((gsize *)&type_id, _type_id);
   }

   return type_id;
}
