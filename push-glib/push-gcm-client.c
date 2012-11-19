/* push-gcm-client.h
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
#include "push-gcm-client.h"

G_DEFINE_TYPE(PushGcmClient, push_gcm_client, SOUP_TYPE_SESSION_ASYNC)

struct _PushGcmClientPrivate
{
   gchar *auth_token;
};

enum
{
   PROP_0,
   PROP_AUTH_TOKEN,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

PushGcmClient *
push_gcm_client_new (const gchar *auth_token)
{
   return g_object_new(PUSH_TYPE_GCM_CLIENT,
                       "auth-token", auth_token,
                       NULL);
}

const gchar *
push_gcm_client_get_auth_token (PushGcmClient *client)
{
   g_return_val_if_fail(PUSH_IS_GCM_CLIENT(client), NULL);
   return client->priv->auth_token;
}

void
push_gcm_client_set_auth_token (PushGcmClient *client,
                                const gchar   *auth_token)
{
   g_return_if_fail(PUSH_IS_GCM_CLIENT(client));
   g_free(client->priv->auth_token);
   client->priv->auth_token = g_strdup(auth_token);
   g_object_notify_by_pspec(G_OBJECT(client), gParamSpecs[PROP_AUTH_TOKEN]);
}

static void
push_gcm_client_finalize (GObject *object)
{
   PushGcmClientPrivate *priv;

   ENTRY;
   priv = PUSH_GCM_CLIENT(object)->priv;
   g_free(priv->auth_token);
   G_OBJECT_CLASS(push_gcm_client_parent_class)->finalize(object);
   EXIT;
}

static void
push_gcm_client_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
   PushGcmClient *client = PUSH_GCM_CLIENT(object);

   switch (prop_id) {
   case PROP_AUTH_TOKEN:
      g_value_set_string(value, push_gcm_client_get_auth_token(client));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_client_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
   PushGcmClient *client = PUSH_GCM_CLIENT(object);

   switch (prop_id) {
   case PROP_AUTH_TOKEN:
      push_gcm_client_set_auth_token(client, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_client_class_init (PushGcmClientClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_gcm_client_finalize;
   object_class->get_property = push_gcm_client_get_property;
   object_class->set_property = push_gcm_client_set_property;
   g_type_class_add_private(object_class, sizeof(PushGcmClientPrivate));

   gParamSpecs[PROP_AUTH_TOKEN] =
      g_param_spec_string("auth-token",
                          _("Auth Token"),
                          _("The authorization token to use to authenticate "
                            "with Google's GCM service."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_AUTH_TOKEN,
                                   gParamSpecs[PROP_AUTH_TOKEN]);
}

static void
push_gcm_client_init (PushGcmClient *client)
{
   client->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(client,
                                  PUSH_TYPE_GCM_CLIENT,
                                  PushGcmClientPrivate);
}
