/* postal-device.c
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

#include <glib/gi18n.h>

#include "postal-debug.h"
#include "postal-device.h"

G_DEFINE_TYPE(PostalDevice, postal_device, G_TYPE_OBJECT)

struct _PostalDevicePrivate
{
   gchar *user;
   gchar *device_token;
   gchar *device_type;
};

enum
{
   PROP_0,
   PROP_USER,
   PROP_DEVICE_TOKEN,
   PROP_DEVICE_TYPE,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * postal_device_new:
 *
 * Creates a new #PostalDevice.
 *
 * Returns: (transfer full): A #PostalDevice.
 */
PostalDevice *
postal_device_new (void)
{
   return g_object_new(POSTAL_TYPE_DEVICE, NULL);
}

/**
 * postal_device_get_user:
 * @device: (in): A #PostalDevice.
 *
 * Fetches the :user property of the #PostalDevice. This is a string
 * containing some uniquely identifying attribute of the user. Currently,
 * this should be their MongoDB ObjectId as a string.
 *
 * Returns: (transfer none): A string.
 */
const gchar *
postal_device_get_user (PostalDevice *device)
{
   g_return_val_if_fail(POSTAL_IS_DEVICE(device), NULL);
   return device->priv->user;
}

/**
 * postal_device_set_user:
 * @device: (in): A #PostalDevice.
 *
 * Sets the :user property of the #PostalDevice.
 * See postal_device_get_user().
 */
void
postal_device_set_user (PostalDevice *device,
                        const gchar  *user)
{
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   g_free(device->priv->user);
   device->priv->user = g_strdup(user);
   g_object_notify_by_pspec(G_OBJECT(device), gParamSpecs[PROP_USER]);
}

/**
 * postal_device_get_device_token:
 * @device: (in): A #PostalDevice.
 *
 * Fetches the :device-token property of the #PostalDevice. This is the token
 * identifying @device to external push notification systems.
 *
 * Returns: (transfer none): A string.
 */
const gchar *
postal_device_get_device_token (PostalDevice *device)
{
   g_return_val_if_fail(POSTAL_IS_DEVICE(device), NULL);
   return device->priv->device_token;
}

/**
 * postal_device_set_device_token:
 * @device: (in): A #PostalDevice.
 *
 * Sets the :device-token property of the #PostalDevice.
 * See postal_device_get_device_token().
 */
void
postal_device_set_device_token (PostalDevice *device,
                                const gchar  *device_token)
{
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   g_free(device->priv->device_token);
   device->priv->device_token = g_strdup(device_token);
   g_object_notify_by_pspec(G_OBJECT(device), gParamSpecs[PROP_DEVICE_TOKEN]);
}

/**
 * postal_device_get_device_type:
 * @device: (in): A #PostalDevice.
 *
 * Fetches the :device-type property of the #PostalDevice. This is the backend
 * that should be used to communicate with the device.
 *
 * Returns: (transfer none): A string.
 */
const gchar *
postal_device_get_device_type (PostalDevice *device)
{
   g_return_val_if_fail(POSTAL_IS_DEVICE(device), NULL);
   return device->priv->device_type;
}

/**
 * postal_device_set_device_type:
 * @device: (in): A #PostalDevice.
 *
 * Sets the :device-type property of the #PostalDevice.
 * See postal_device_get_device_type().
 */
void
postal_device_set_device_type (PostalDevice *device,
                               const gchar  *device_type)
{
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   g_free(device->priv->device_type);
   device->priv->device_type = g_strdup(device_type);
   g_object_notify_by_pspec(G_OBJECT(device), gParamSpecs[PROP_DEVICE_TYPE]);
}

/**
 * postal_device_save_to_bson:
 * @device: (in): A #PostalDevice.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Saves the #PostalDevice to a #MongoBson structure that is suitable for
 * storing in MongoDB. If an error occurred, %NULL is returned and @error
 * is set.
 *
 * Returns: (transfer full): A #MongoBson if successful; otherwise %NULL.
 */
MongoBson *
postal_device_save_to_bson (PostalDevice  *device,
                            GError       **error)
{
   PostalDevicePrivate *priv;
   MongoObjectId *id;
   MongoBson *ret;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_DEVICE(device), NULL);

   priv = device->priv;

   /*
    * We can't continue unless we have a creator for the device.
    */
   if (!priv->user) {
      g_set_error(error, POSTAL_DEVICE_ERROR,
                  POSTAL_DEVICE_ERROR_MISSING_USER,
                  _("You must supply user."));
      return NULL;
   }

   ret = mongo_bson_new_empty();
   mongo_bson_append_string(ret, "device_token", priv->device_token);
   mongo_bson_append_string(ret, "device_type", priv->device_type);

   /*
    * Special case user to opportunistically use ObjectId.
    */
   if (priv->user &&
       (id = mongo_object_id_new_from_string(priv->user))) {
      mongo_bson_append_object_id(ret, "user", id);
      mongo_object_id_free(id);
   } else {
      mongo_bson_append_string(ret, "user", priv->user);
   }

   RETURN(ret);
}

JsonNode *
postal_device_save_to_json (PostalDevice  *device,
                            GError       **error)
{
   PostalDevicePrivate *priv;
   JsonObject *obj;
   JsonNode *node;

   g_return_val_if_fail(POSTAL_IS_DEVICE(device), NULL);

   priv = device->priv;

   obj = json_object_new();

   if (priv->device_token) {
      json_object_set_string_member(obj, "device_token", priv->device_token);
   } else {
      json_object_set_null_member(obj, "device_token");
   }

   if (priv->device_type) {
      json_object_set_string_member(obj, "device_type", priv->device_type);
   } else {
      json_object_set_null_member(obj, "device_type");
   }

   if (priv->user) {
      json_object_set_string_member(obj, "user", priv->user);
   } else {
      json_object_set_null_member(obj, "user");
   }

   /*
    * TODO: removed_at, created_at, etc.
    */

   node = json_node_new(JSON_NODE_OBJECT);
   json_node_set_object(node, obj);
   json_object_unref(obj);

   return node;
}

gboolean
postal_device_load_from_json (PostalDevice  *device,
                              JsonNode      *node,
                              GError       **error)
{
   const gchar *str;
   JsonObject *obj;

   ENTRY;

   g_return_val_if_fail(POSTAL_IS_DEVICE(device), FALSE);
   g_return_val_if_fail(node, FALSE);

   /*
    * TODO: We need a generic strategy to handle extra fields.
    */

   if (!JSON_NODE_HOLDS_OBJECT(node) ||
       !(obj = json_node_get_object(node)) ||
       !json_object_has_member(obj, "device_type") ||
       !(node = json_object_get_member(obj, "device_type")) ||
       !JSON_NODE_HOLDS_VALUE(node) ||
       json_node_get_value_type(node) != G_TYPE_STRING ||
       !json_object_has_member(obj, "device_token") ||
       !(node = json_object_get_member(obj, "device_token")) ||
       !JSON_NODE_HOLDS_VALUE(node) ||
       json_node_get_value_type(node) != G_TYPE_STRING) {
      g_set_error(error,
                  POSTAL_DEVICE_ERROR,
                  POSTAL_DEVICE_ERROR_INVALID_JSON,
                  _("the json structure provided is invalid."));
      RETURN(FALSE);
   }

   str = json_object_get_string_member(obj, "device_type");
   if (!!g_strcmp0(str, "aps") &&
       !!g_strcmp0(str, "c2dm") &&
       !!g_strcmp0(str, "gcm")) {
      g_set_error(error,
                  POSTAL_DEVICE_ERROR,
                  POSTAL_DEVICE_ERROR_UNSUPPORTED_TYPE,
                  _("The device_type %s is not supported."),
                  str);
      RETURN(FALSE);
   }
   postal_device_set_device_type(device, str);

   str = json_object_get_string_member(obj, "device_token");
   postal_device_set_device_token(device, str);

   RETURN(TRUE);
}

static void
postal_device_finalize (GObject *object)
{
   PostalDevicePrivate *priv;

   ENTRY;

   priv = POSTAL_DEVICE(object)->priv;

   g_free(priv->device_token);
   g_free(priv->device_type);
   g_free(priv->user);

   G_OBJECT_CLASS(postal_device_parent_class)->finalize(object);

   EXIT;
}

static void
postal_device_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
   PostalDevice *device = POSTAL_DEVICE(object);

   switch (prop_id) {
   case PROP_DEVICE_TOKEN:
      g_value_set_string(value, postal_device_get_device_token(device));
      break;
   case PROP_DEVICE_TYPE:
      g_value_set_string(value, postal_device_get_device_type(device));
      break;
   case PROP_USER:
      g_value_set_string(value, postal_device_get_user(device));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_device_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
   PostalDevice *device = POSTAL_DEVICE(object);

   switch (prop_id) {
   case PROP_DEVICE_TOKEN:
      postal_device_set_device_token(device, g_value_get_string(value));
      break;
   case PROP_DEVICE_TYPE:
      postal_device_set_device_type(device, g_value_get_string(value));
      break;
   case PROP_USER:
      postal_device_set_user(device, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_device_class_init (PostalDeviceClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_device_finalize;
   object_class->get_property = postal_device_get_property;
   object_class->set_property = postal_device_set_property;
   g_type_class_add_private(object_class, sizeof(PostalDevicePrivate));

   gParamSpecs[PROP_DEVICE_TOKEN] =
      g_param_spec_string("device-token",
                          _("Device Token"),
                          _("The unique token identifying this device."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DEVICE_TOKEN,
                                   gParamSpecs[PROP_DEVICE_TOKEN]);

   gParamSpecs[PROP_DEVICE_TYPE] =
      g_param_spec_string("device-type",
                          _("Device Type"),
                          _("The backend for communicating with this device."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DEVICE_TYPE,
                                   gParamSpecs[PROP_DEVICE_TYPE]);

   gParamSpecs[PROP_USER] =
      g_param_spec_string("user",
                          _("User"),
                          _("The owner of the device."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_USER,
                                   gParamSpecs[PROP_USER]);

   EXIT;
}

static void
postal_device_init (PostalDevice *device)
{
   ENTRY;

   device->priv = G_TYPE_INSTANCE_GET_PRIVATE(device,
                                              POSTAL_TYPE_DEVICE,
                                              PostalDevicePrivate);

   EXIT;
}

GQuark
postal_device_error_quark (void)
{
   return g_quark_from_static_string("PostalDeviceError");
}
