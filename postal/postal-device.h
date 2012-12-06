/* postal-device.h
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

#ifndef POSTAL_DEVICE_H
#define POSTAL_DEVICE_H

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <mongo-glib.h>

G_BEGIN_DECLS

#define POSTAL_TYPE_DEVICE            (postal_device_get_type())
#define POSTAL_TYPE_DEVICE_TYPE       (postal_device_type_get_type())
#define POSTAL_DEVICE_ERROR           (postal_device_error_quark())
#define POSTAL_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_DEVICE, PostalDevice))
#define POSTAL_DEVICE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_DEVICE, PostalDevice const))
#define POSTAL_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_DEVICE, PostalDeviceClass))
#define POSTAL_IS_DEVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_DEVICE))
#define POSTAL_IS_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_DEVICE))
#define POSTAL_DEVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_DEVICE, PostalDeviceClass))

typedef struct _PostalDevice        PostalDevice;
typedef struct _PostalDeviceClass   PostalDeviceClass;
typedef struct _PostalDevicePrivate PostalDevicePrivate;
typedef enum   _PostalDeviceType    PostalDeviceType;

typedef enum
{
   POSTAL_DEVICE_ERROR_MISSING_USER = 1,
   POSTAL_DEVICE_ERROR_MISSING_ID,
   POSTAL_DEVICE_ERROR_INVALID_ID,
   POSTAL_DEVICE_ERROR_INVALID_JSON,
   POSTAL_DEVICE_ERROR_NOT_FOUND,
   POSTAL_DEVICE_ERROR_UNSUPPORTED_TYPE,
} PostalDeviceError;

enum _PostalDeviceType
{
   POSTAL_DEVICE_APS  = 1,
   POSTAL_DEVICE_C2DM = 2,
   POSTAL_DEVICE_GCM  = 3,
};

struct _PostalDevice
{
   GObject parent;

   /*< private >*/
   PostalDevicePrivate *priv;
};

struct _PostalDeviceClass
{
   GObjectClass parent_class;
};

GQuark            postal_device_error_quark      (void) G_GNUC_CONST;
GTimeVal         *postal_device_get_created_at   (PostalDevice      *device);
const gchar      *postal_device_get_device_token (PostalDevice      *device);
PostalDeviceType  postal_device_get_device_type  (PostalDevice      *device);
GTimeVal         *postal_device_get_removed_at   (PostalDevice      *device);
const gchar      *postal_device_get_user         (PostalDevice      *device);
GType             postal_device_get_type         (void) G_GNUC_CONST;
gboolean          postal_device_load_from_bson   (PostalDevice      *device,
                                                  MongoBson         *bson,
                                                  GError           **error);
gboolean          postal_device_load_from_json   (PostalDevice      *device,
                                                  JsonNode          *node,
                                                  GError           **error);
PostalDevice     *postal_device_new              (void);
MongoBson        *postal_device_save_to_bson     (PostalDevice      *device,
                                                  GError           **error);
JsonNode         *postal_device_save_to_json     (PostalDevice      *device,
                                                  GError           **error);
void              postal_device_set_device_token (PostalDevice      *device,
                                                  const gchar       *device_token);
void              postal_device_set_device_type  (PostalDevice      *device,
                                                  PostalDeviceType   device_type);
void              postal_device_set_removed_at   (PostalDevice      *device,
                                                  GTimeVal          *removed_at);
void              postal_device_set_user         (PostalDevice      *device,
                                                  const gchar       *user);
GType             postal_device_type_get_type    (void) G_GNUC_CONST;

G_END_DECLS

#endif /* POSTAL_DEVICE_H */
