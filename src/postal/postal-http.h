/* postal-http.h
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

#ifndef POSTAL_HTTP_H
#define POSTAL_HTTP_H

#include <neo.h>

G_BEGIN_DECLS

#define POSTAL_TYPE_HTTP            (postal_http_get_type())
#define POSTAL_HTTP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_HTTP, PostalHttp))
#define POSTAL_HTTP_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_HTTP, PostalHttp const))
#define POSTAL_HTTP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_HTTP, PostalHttpClass))
#define POSTAL_IS_HTTP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_HTTP))
#define POSTAL_IS_HTTP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_HTTP))
#define POSTAL_HTTP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_HTTP, PostalHttpClass))

typedef struct _PostalHttp        PostalHttp;
typedef struct _PostalHttpClass   PostalHttpClass;
typedef struct _PostalHttpPrivate PostalHttpPrivate;

struct _PostalHttp
{
   NeoServiceBase parent;

   /*< private >*/
   PostalHttpPrivate *priv;
};

struct _PostalHttpClass
{
   NeoServiceBaseClass parent_class;
};

GType       postal_http_get_type (void) G_GNUC_CONST;
PostalHttp *postal_http_new      (void);

G_END_DECLS

#endif /* POSTAL_HTTP_H */
