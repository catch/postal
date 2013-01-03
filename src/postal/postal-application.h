/* postal-application.h
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

#ifndef POSTAL_APPLICATION_H
#define POSTAL_APPLICATION_H

#include <gio/gio.h>
#include <neo.h>

G_BEGIN_DECLS

#define POSTAL_TYPE_APPLICATION            (postal_application_get_type())
#define POSTAL_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_APPLICATION, PostalApplication))
#define POSTAL_APPLICATION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_APPLICATION, PostalApplication const))
#define POSTAL_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_APPLICATION, PostalApplicationClass))
#define POSTAL_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_APPLICATION))
#define POSTAL_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_APPLICATION))
#define POSTAL_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_APPLICATION, PostalApplicationClass))

typedef struct _PostalApplication        PostalApplication;
typedef struct _PostalApplicationClass   PostalApplicationClass;
typedef struct _PostalApplicationPrivate PostalApplicationPrivate;

struct _PostalApplication
{
   NeoApplication parent;

   /*< private >*/
   PostalApplicationPrivate *priv;
};

struct _PostalApplicationClass
{
   NeoApplicationClass parent_class;
};

GType postal_application_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* POSTAL_APPLICATION_H */
