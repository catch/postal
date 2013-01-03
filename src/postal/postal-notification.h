/* postal-notification.h
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

#ifndef POSTAL_NOTIFICATION_H
#define POSTAL_NOTIFICATION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define POSTAL_TYPE_NOTIFICATION            (postal_notification_get_type())
#define POSTAL_NOTIFICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_NOTIFICATION, PostalNotification))
#define POSTAL_NOTIFICATION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_NOTIFICATION, PostalNotification const))
#define POSTAL_NOTIFICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_NOTIFICATION, PostalNotificationClass))
#define POSTAL_IS_NOTIFICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_NOTIFICATION))
#define POSTAL_IS_NOTIFICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_NOTIFICATION))
#define POSTAL_NOTIFICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_NOTIFICATION, PostalNotificationClass))

typedef struct _PostalNotification        PostalNotification;
typedef struct _PostalNotificationClass   PostalNotificationClass;
typedef struct _PostalNotificationPrivate PostalNotificationPrivate;

struct _PostalNotification
{
   GObject parent;

   /*< private >*/
   PostalNotificationPrivate *priv;
};

struct _PostalNotificationClass
{
   GObjectClass parent_class;
};

JsonObject         *postal_notification_get_aps          (PostalNotification *notification);
JsonObject         *postal_notification_get_c2dm         (PostalNotification *notification);
const gchar        *postal_notification_get_collapse_key (PostalNotification *notification);
JsonObject         *postal_notification_get_gcm          (PostalNotification *notification);
GType               postal_notification_get_type         (void) G_GNUC_CONST;
PostalNotification *postal_notification_new              (void);
void                postal_notification_set_aps          (PostalNotification *notification,
                                                          JsonObject         *aps);
void                postal_notification_set_c2dm         (PostalNotification *notification,
                                                          JsonObject         *aps);
void                postal_notification_set_gcm          (PostalNotification *notification,
                                                          JsonObject         *aps);
void                postal_notification_set_collapse_key (PostalNotification *notification,
                                                          const gchar        *collapse_key);

G_END_DECLS

#endif /* POSTAL_NOTIFICATION_H */
