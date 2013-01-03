/* neo-service.h
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

#ifndef NEO_SERVICE_H
#define NEO_SERVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NEO_TYPE_SERVICE             (neo_service_get_type())
#define NEO_SERVICE(o)               (G_TYPE_CHECK_INSTANCE_CAST((o),    NEO_TYPE_SERVICE, NeoService))
#define NEO_IS_SERVICE(o)            (G_TYPE_CHECK_INSTANCE_TYPE((o),    NEO_TYPE_SERVICE))
#define NEO_SERVICE_GET_INTERFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE((o), NEO_TYPE_SERVICE, NeoServiceIface))

typedef struct _NeoService      NeoService;
typedef struct _NeoServiceIface NeoServiceIface;

struct _NeoServiceIface
{
   GTypeInterface parent;

   void         (*add_child)      (NeoService  *service,
                                   NeoService  *child);
   NeoService  *(*get_child)      (NeoService  *service,
                                   const gchar *name);
   gboolean     (*get_is_running) (NeoService *service);
   const gchar *(*get_name)       (NeoService  *service);
   NeoService  *(*get_parent)     (NeoService  *service);
   void         (*set_parent)     (NeoService  *service,
                                   NeoService  *parent);
   void         (*start)          (NeoService  *service,
                                   GKeyFile    *config);
   void         (*stop)           (NeoService  *service);
};

void         neo_service_add_child      (NeoService  *service,
                                         NeoService  *child);
NeoService  *neo_service_get_child      (NeoService  *service,
                                         const gchar *name);
gboolean     neo_service_get_is_running (NeoService  *service);
const gchar *neo_service_get_name       (NeoService  *service);
GType        neo_service_get_type       (void) G_GNUC_CONST;
void         neo_service_set_parent     (NeoService  *service,
                                         NeoService  *parent);
void         neo_service_start          (NeoService  *service,
                                         GKeyFile    *config);
void         neo_service_stop           (NeoService  *service);
NeoService  *neo_service_get_peer       (NeoService  *service,
                                         const gchar *peer);

G_END_DECLS

#endif /* NEO_SERVICE_H */
