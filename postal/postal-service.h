/* postal-service.h
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

#ifndef POSTAL_SERVICE_H
#define POSTAL_SERVICE_H

#include <glib-object.h>
#include <mongo-glib.h>
#include <neo.h>

#include "postal-device.h"
#include "postal-notification.h"

G_BEGIN_DECLS

#define POSTAL_TYPE_SERVICE            (postal_service_get_type())
#define POSTAL_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_SERVICE, PostalService))
#define POSTAL_SERVICE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_SERVICE, PostalService const))
#define POSTAL_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_SERVICE, PostalServiceClass))
#define POSTAL_IS_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_SERVICE))
#define POSTAL_IS_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_SERVICE))
#define POSTAL_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_SERVICE, PostalServiceClass))

typedef struct _PostalService        PostalService;
typedef struct _PostalServiceClass   PostalServiceClass;
typedef struct _PostalServicePrivate PostalServicePrivate;

struct _PostalService
{
   NeoServiceBase parent;

   /*< private >*/
   PostalServicePrivate *priv;
};

struct _PostalServiceClass
{
   NeoServiceBaseClass parent_class;
};

GKeyFile      *postal_service_get_config           (PostalService        *service);
GType          postal_service_get_type             (void) G_GNUC_CONST;
void           postal_service_add_device           (PostalService        *service,
                                                    PostalDevice         *device,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
gboolean       postal_service_add_device_finish    (PostalService        *service,
                                                    GAsyncResult         *result,
                                                    gboolean             *updated_existing,
                                                    GError              **error);
void           postal_service_find_device          (PostalService        *service,
                                                    const gchar          *user,
                                                    const gchar          *device,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
PostalDevice  *postal_service_find_device_finish   (PostalService        *service,
                                                    GAsyncResult         *result,
                                                    GError              **error);
void           postal_service_find_devices         (PostalService        *service,
                                                    const gchar          *user,
                                                    gsize                 offset,
                                                    gsize                 limit,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
GPtrArray     *postal_service_find_devices_finish  (PostalService        *service,
                                                    GAsyncResult         *result,
                                                    GError              **error);
PostalService *postal_service_new                  (void);
void           postal_service_remove_device        (PostalService        *service,
                                                    PostalDevice         *device,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
gboolean       postal_service_remove_device_finish (PostalService        *service,
                                                    GAsyncResult         *result,
                                                    GError              **error);
void           postal_service_set_config           (PostalService        *service,
                                                    GKeyFile             *config);
void           postal_service_notify               (PostalService        *service,
                                                    PostalNotification   *notification,
                                                    gchar               **users,
                                                    gchar               **device_tokens,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
gboolean       postal_service_notify_finish        (PostalService        *service,
                                                    GAsyncResult         *result,
                                                    GError              **error);

G_END_DECLS

#endif /* POSTAL_SERVICE_H */
