/* push-gcm-identity.c
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
 */

#include <glib/gi18n.h>

#include "push-debug.h"
#include "push-gcm-identity.h"

G_DEFINE_TYPE(PushGcmIdentity, push_gcm_identity, G_TYPE_OBJECT)

/**
 * SECTION:push-gcm-identity
 * @title: PushGcmIdentity
 * @short_description: An identity registered via Google Cloud Messaging.
 *
 * #PushGcmIdentity represents a device that has been registered via Google
 * Cloud Messaging service. They are represented by an :registration-id that
 * was provided to the device when it registered with Google's service.
 *
 * Push-GLib keeps this in an object to allow for future expansion if more
 * than :registration-id is someday supported.
 */

struct _PushGcmIdentityPrivate
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

PushGcmIdentity *
push_gcm_identity_new (const gchar *registration_id)
{
   return g_object_new(PUSH_TYPE_GCM_IDENTITY,
                       "registration-id", registration_id,
                       NULL);
}

const gchar *
push_gcm_identity_get_registration_id (PushGcmIdentity *identity)
{
   g_return_val_if_fail(PUSH_IS_GCM_IDENTITY(identity), NULL);
   return identity->priv->registration_id;
}

void
push_gcm_identity_set_registration_id (PushGcmIdentity *identity,
                                       const gchar     *registration_id)
{
   g_return_if_fail(PUSH_IS_GCM_IDENTITY(identity));
   g_free(identity->priv->registration_id);
   identity->priv->registration_id = g_strdup(registration_id);
   g_object_notify_by_pspec(G_OBJECT(identity),
                            gParamSpecs[PROP_REGISTRATION_ID]);
}

static void
push_gcm_identity_finalize (GObject *object)
{
   PushGcmIdentityPrivate *priv;

   ENTRY;
   priv = PUSH_GCM_IDENTITY(object)->priv;
   g_free(priv->registration_id);
   G_OBJECT_CLASS(push_gcm_identity_parent_class)->finalize(object);
   EXIT;
}

static void
push_gcm_identity_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
   PushGcmIdentity *identity = PUSH_GCM_IDENTITY(object);

   switch (prop_id) {
   case PROP_REGISTRATION_ID:
      g_value_set_string(value,
                         push_gcm_identity_get_registration_id(identity));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_identity_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
   PushGcmIdentity *identity = PUSH_GCM_IDENTITY(object);

   switch (prop_id) {
   case PROP_REGISTRATION_ID:
      push_gcm_identity_set_registration_id(identity,
                                            g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_identity_class_init (PushGcmIdentityClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_gcm_identity_finalize;
   object_class->get_property = push_gcm_identity_get_property;
   object_class->set_property = push_gcm_identity_set_property;
   g_type_class_add_private(object_class, sizeof(PushGcmIdentityPrivate));

   gParamSpecs[PROP_REGISTRATION_ID] =
      g_param_spec_string("registration-id",
                          _("Registration Id"),
                          _("The registration id for the GCM identity."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_REGISTRATION_ID,
                                   gParamSpecs[PROP_REGISTRATION_ID]);
}

static void
push_gcm_identity_init (PushGcmIdentity *identity)
{
   identity->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(identity,
                                  PUSH_TYPE_GCM_IDENTITY,
                                  PushGcmIdentityPrivate);
}
