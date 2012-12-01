/* neo-application.h
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

#ifndef NEO_APPLICATION_H
#define NEO_APPLICATION_H

#include "neo-logger.h"

#include <gio/gio.h>

G_BEGIN_DECLS

#define NEO_TYPE_APPLICATION            (neo_application_get_type())
#define NEO_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_APPLICATION, NeoApplication))
#define NEO_APPLICATION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_APPLICATION, NeoApplication const))
#define NEO_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NEO_TYPE_APPLICATION, NeoApplicationClass))
#define NEO_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEO_TYPE_APPLICATION))
#define NEO_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NEO_TYPE_APPLICATION))
#define NEO_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NEO_TYPE_APPLICATION, NeoApplicationClass))

typedef struct _NeoApplication        NeoApplication;
typedef struct _NeoApplicationClass   NeoApplicationClass;
typedef struct _NeoApplicationPrivate NeoApplicationPrivate;

struct _NeoApplication
{
   GApplication parent;

   /*< private >*/
   NeoApplicationPrivate *priv;
};

struct _NeoApplicationClass
{
   GApplicationClass parent_class;
};

void     neo_application_add_logger          (NeoApplication *application,
                                              NeoLogger      *logger);
gboolean neo_application_get_logging_enabled (NeoApplication *application);
GType    neo_application_get_type            (void) G_GNUC_CONST;
void     neo_application_set_config          (NeoApplication *application,
                                              GKeyFile       *config);
void     neo_application_set_logging_enabled (NeoApplication *application,
                                              gboolean        logging_enabled);

G_END_DECLS

#endif /* NEO_APPLICATION_H */
