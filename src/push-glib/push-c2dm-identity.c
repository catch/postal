/* push-c2dm-identity.c
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

#include "push-c2dm-identity.h"
#include "push-debug.h"

/**
 * SECTION:push-c2dm-identity
 * @title: PushC2dmIdentity
 * @short_description: A C2DM device that can be notified.
 *
 * PushC2dmIdentity represents a C2DM registered device that can be
 * notified using #PushC2dmClient. It contains the "registration-id"
 * provided by Android's C2DM intent.
 */

G_DEFINE_TYPE(PushC2dmIdentity, push_c2dm_identity, G_TYPE_OBJECT)

struct _PushC2dmIdentityPrivate
{
   gchar *registration_id;
};

enum
{
   PROP_0,
   PROP_REGISTRATION_ID,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * push_c2dm_identity_new:
 * @registration_id: (allow-none): The registration_id.
 *
 * Creates a new #PushC2dmIdentity.
 *
 * Returns: (transfer full): A #PushC2dmIdentity.
 */
PushC2dmIdentity *
push_c2dm_identity_new (const gchar *registration_id)
{
   return g_object_new(PUSH_TYPE_C2DM_IDENTITY,
                       "registration-id", registration_id,
                       NULL);
}

/**
 * push_c2dm_identity_get_registration_id:
 * @identity: (in): A #PushC2dmIdentity.
 *
 * Fetches the "registration-id" property. The registration id is provided
 * by a device that has registered with the C2DM service.
 *
 * Returns: A string containing the registration id.
 */
const gchar *
push_c2dm_identity_get_registration_id (PushC2dmIdentity *identity)
{
   g_return_val_if_fail(PUSH_IS_C2DM_IDENTITY(identity), NULL);
   return identity->priv->registration_id;
}

/**
 * push_c2dm_identity_set_registration_id:
 * @identity: (in): A #PushC2dmIdentity.
 * @registration_id: A string containing the registration_id.
 *
 * Sets the "registration-id" property. The registration id is provided
 * by a device that has registered with the C2DM service.
 */
void
push_c2dm_identity_set_registration_id (PushC2dmIdentity *identity,
                                        const gchar      *registration_id)
{
   ENTRY;
   g_return_if_fail(PUSH_IS_C2DM_IDENTITY(identity));
   g_free(identity->priv->registration_id);
   identity->priv->registration_id = g_strdup(registration_id);
   g_object_notify_by_pspec(G_OBJECT(identity),
                            gParamSpecs[PROP_REGISTRATION_ID]);
   EXIT;
}

static void
push_c2dm_identity_finalize (GObject *object)
{
   PushC2dmIdentityPrivate *priv;

   ENTRY;

   priv = PUSH_C2DM_IDENTITY(object)->priv;

   g_free(priv->registration_id);

   G_OBJECT_CLASS(push_c2dm_identity_parent_class)->finalize(object);

   EXIT;
}

static void
push_c2dm_identity_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
   PushC2dmIdentity *identity = PUSH_C2DM_IDENTITY(object);

   switch (prop_id) {
   case PROP_REGISTRATION_ID:
      g_value_set_string(value,
                         push_c2dm_identity_get_registration_id(identity));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_c2dm_identity_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
   PushC2dmIdentity *identity = PUSH_C2DM_IDENTITY(object);

   switch (prop_id) {
   case PROP_REGISTRATION_ID:
      push_c2dm_identity_set_registration_id(identity,
                                             g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_c2dm_identity_class_init (PushC2dmIdentityClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_c2dm_identity_finalize;
   object_class->get_property = push_c2dm_identity_get_property;
   object_class->set_property = push_c2dm_identity_set_property;
   g_type_class_add_private(object_class, sizeof(PushC2dmIdentityPrivate));

   /**
    * PushC2dmIdentity:registration-id:
    *
    * The "registration-id" property is the registration_id provided by
    * a client device registering for C2DM via the Android Intent.
    */
   gParamSpecs[PROP_REGISTRATION_ID] =
      g_param_spec_string("registration-id",
                          _("Registration ID"),
                          _("The registration id provided from a device."),
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_REGISTRATION_ID,
                                   gParamSpecs[PROP_REGISTRATION_ID]);

   EXIT;
}

static void
push_c2dm_identity_init (PushC2dmIdentity *identity)
{
   ENTRY;
   identity->priv = G_TYPE_INSTANCE_GET_PRIVATE(identity,
                                                PUSH_TYPE_C2DM_IDENTITY,
                                                PushC2dmIdentityPrivate);
   EXIT;
}
