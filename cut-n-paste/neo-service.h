/* neo-service.h
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

#ifndef NEO_SERVICE_H
#define NEO_SERVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NEO_TYPE_SERVICE            (neo_service_get_type())
#define NEO_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_SERVICE, NeoService))
#define NEO_SERVICE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_SERVICE, NeoService const))
#define NEO_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NEO_TYPE_SERVICE, NeoServiceClass))
#define NEO_IS_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEO_TYPE_SERVICE))
#define NEO_IS_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NEO_TYPE_SERVICE))
#define NEO_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NEO_TYPE_SERVICE, NeoServiceClass))

typedef struct _NeoService        NeoService;
typedef struct _NeoServiceClass   NeoServiceClass;
typedef struct _NeoServicePrivate NeoServicePrivate;

struct _NeoService
{
   GObject parent;

   /*< private >*/
   NeoServicePrivate *priv;
};

struct _NeoServiceClass
{
   GObjectClass parent_class;

   void (*start) (NeoService *service);
   void (*stop)  (NeoService *service);
};

GKeyFile    *neo_service_get_config     (NeoService  *service);
gboolean     neo_service_get_is_running (NeoService  *service);
const gchar *neo_service_get_name       (NeoService  *service);
GType        neo_service_get_type       (void) G_GNUC_CONST;
void         neo_service_set_config     (NeoService  *service,
                                         GKeyFile    *config);
void         neo_service_set_name       (NeoService  *service,
                                         const gchar *name);
void         neo_service_start          (NeoService  *service);
void         neo_service_stop           (NeoService  *service);

G_END_DECLS

#endif /* NEO_SERVICE_H */
