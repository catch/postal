/* neo-service-base.h
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

#ifndef NEO_SERVICE_BASE_H
#define NEO_SERVICE_BASE_H

#include "neo-service.h"

G_BEGIN_DECLS

#define NEO_TYPE_SERVICE_BASE            (neo_service_base_get_type())
#define NEO_SERVICE_BASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_SERVICE_BASE, NeoServiceBase))
#define NEO_SERVICE_BASE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_SERVICE_BASE, NeoServiceBase const))
#define NEO_SERVICE_BASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NEO_TYPE_SERVICE_BASE, NeoServiceBaseClass))
#define NEO_IS_SERVICE_BASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEO_TYPE_SERVICE_BASE))
#define NEO_IS_SERVICE_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NEO_TYPE_SERVICE_BASE))
#define NEO_SERVICE_BASE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NEO_TYPE_SERVICE_BASE, NeoServiceBaseClass))

typedef struct _NeoServiceBase        NeoServiceBase;
typedef struct _NeoServiceBaseClass   NeoServiceBaseClass;
typedef struct _NeoServiceBasePrivate NeoServiceBasePrivate;

struct _NeoServiceBase
{
   GObject parent;

   /*< private >*/
   NeoServiceBasePrivate *priv;
};

struct _NeoServiceBaseClass
{
   GObjectClass parent_class;

   void (*start) (NeoServiceBase *base,
                  GKeyFile       *config);
   void (*stop)  (NeoServiceBase *base);

   gpointer reserved1;
   gpointer reserved2;
   gpointer reserved3;
   gpointer reserved4;
   gpointer reserved5;
   gpointer reserved6;
};

GType neo_service_base_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* NEO_SERVICE_BASE_H */
