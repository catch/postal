/* push-aps-identity.c
 *
 * Copyright (C) 2012 Christian Hergert <chris@dronelabs.com>
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

#include <glib/gi18n.h>

#include "push-aps-identity.h"
#include "push-debug.h"

/**
 * SECTION:push-aps-identity
 * @title: PushApsIdentity
 * @short_description: An APS device that can be notified.
 *
 * #PushApsIdentity represents a device that can receive notifications
 * via the Apple push gateway.
 */

G_DEFINE_TYPE(PushApsIdentity, push_aps_identity, G_TYPE_OBJECT)

struct _PushApsIdentityPrivate
{
   gchar *device_token;
};

enum
{
   PROP_0,
   PROP_DEVICE_TOKEN,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * push_aps_identity_new:
 * @device_token: (allow-none): The APS device token as base64.
 *
 * Creates a new #PushApsIdentity for the device token.
 *
 * Returns: A newly allocated #PushApsIdentity.
 */
PushApsIdentity *
push_aps_identity_new (const gchar *device_token)
{
   return g_object_new(PUSH_TYPE_APS_IDENTITY,
                       "device-token", device_token,
                       NULL);
}

/**
 * push_aps_identity_get_device_token:
 * @identity: (in): A #PushApsIdentity.
 *
 * Fetches the base64 encoded device token.
 *
 * Returns: A string which should not be modified or freed.
 */
const gchar *
push_aps_identity_get_device_token (PushApsIdentity *identity)
{
   g_return_val_if_fail(PUSH_IS_APS_IDENTITY(identity), NULL);
   return identity->priv->device_token;
}

/**
 * push_aps_identity_set_device_token:
 * @identity: (in): A #PushApsIdentity.
 * @device_token: The "device-token".
 *
 * Sets the "device-token" property.
 */
void
push_aps_identity_set_device_token (PushApsIdentity *identity,
                                    const gchar     *device_token)
{
   g_return_if_fail(PUSH_IS_APS_IDENTITY(identity));
   g_free(identity->priv->device_token);
   identity->priv->device_token = g_strdup(device_token);
   g_object_notify_by_pspec(G_OBJECT(identity), gParamSpecs[PROP_DEVICE_TOKEN]);
}

static void
push_aps_identity_finalize (GObject *object)
{
   PushApsIdentityPrivate *priv;

   ENTRY;

   priv = PUSH_APS_IDENTITY(object)->priv;

   g_free(priv->device_token);
   priv->device_token = NULL;

   G_OBJECT_CLASS(push_aps_identity_parent_class)->finalize(object);

   EXIT;
}

static void
push_aps_identity_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
   PushApsIdentity *identity = PUSH_APS_IDENTITY(object);

   switch (prop_id) {
   case PROP_DEVICE_TOKEN:
      g_value_set_string(value, push_aps_identity_get_device_token(identity));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_aps_identity_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
   PushApsIdentity *identity = PUSH_APS_IDENTITY(object);

   switch (prop_id) {
   case PROP_DEVICE_TOKEN:
      push_aps_identity_set_device_token(identity, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_aps_identity_class_init (PushApsIdentityClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_aps_identity_finalize;
   object_class->get_property = push_aps_identity_get_property;
   object_class->set_property = push_aps_identity_set_property;
   g_type_class_add_private(object_class, sizeof(PushApsIdentityPrivate));

   gParamSpecs[PROP_DEVICE_TOKEN] =
      g_param_spec_string("device-token",
                          _("Device Token"),
                          _("The APS device token that was registered."),
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_DEVICE_TOKEN,
                                   gParamSpecs[PROP_DEVICE_TOKEN]);

   EXIT;
}

static void
push_aps_identity_init (PushApsIdentity *identity)
{
   ENTRY;
   identity->priv = G_TYPE_INSTANCE_GET_PRIVATE(identity,
                                                PUSH_TYPE_APS_IDENTITY,
                                                PushApsIdentityPrivate);
   EXIT;
}
