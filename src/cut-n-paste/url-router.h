/* url-router.h
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
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
 */

#ifndef URL_ROUTER_H
#define URL_ROUTER_H

#include <glib.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

typedef GNode UrlRouter;

typedef void (*UrlRouterHandler) (UrlRouter         *router,
                                  SoupServer        *server,
                                  SoupMessage       *message,
                                  const gchar       *path,
                                  GHashTable        *path_params,
                                  GHashTable        *query,
                                  SoupClientContext *client,
                                  gpointer           user_data);

UrlRouter *url_router_new         (void);
void       url_router_add_handler (UrlRouter         *router,
                                   const gchar       *signature,
                                   UrlRouterHandler   handler,
                                   gpointer           user_data);
gboolean   url_router_route       (UrlRouter         *router,
                                   SoupServer        *server,
                                   SoupMessage       *message,
                                   const gchar       *path,
                                   GHashTable        *query,
                                   SoupClientContext *client);
void       url_router_free        (UrlRouter         *router);

G_END_DECLS

#endif /* URL_ROUTER_H */
