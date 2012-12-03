/* postal-notification.c
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
#include <json-glib/json-glib.h>

#include "postal-debug.h"
#include "postal-notification.h"

G_DEFINE_TYPE(PostalNotification, postal_notification, G_TYPE_OBJECT)

struct _PostalNotificationPrivate
{
   JsonObject *aps;
   JsonObject *c2dm;
   JsonObject *gcm;
   gchar *collapse_key;
};

enum
{
   PROP_0,
   PROP_APS,
   PROP_C2DM,
   PROP_COLLAPSE_KEY,
   PROP_GCM,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

JsonObject *
postal_notification_get_aps (PostalNotification *notification)
{
   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);
   return notification->priv->aps;
}

void
postal_notification_set_aps (PostalNotification *notification,
                             JsonObject         *aps)
{
   PostalNotificationPrivate *priv;

   g_return_if_fail(POSTAL_IS_NOTIFICATION(notification));

   priv = notification->priv;

   if (priv->aps) {
      json_object_unref(priv->aps);
      priv->aps = NULL;
   }

   if (aps) {
      priv->aps = json_object_ref(aps);
   }

   g_object_notify_by_pspec(G_OBJECT(notification), gParamSpecs[PROP_APS]);
}

JsonObject *
postal_notification_get_c2dm (PostalNotification *notification)
{
   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);
   return notification->priv->c2dm;
}

void
postal_notification_set_c2dm (PostalNotification *notification,
                              JsonObject         *c2dm)
{
   PostalNotificationPrivate *priv;

   g_return_if_fail(POSTAL_IS_NOTIFICATION(notification));

   priv = notification->priv;

   if (priv->c2dm) {
      json_object_unref(priv->c2dm);
      priv->c2dm = NULL;
   }

   if (c2dm) {
      priv->c2dm = json_object_ref(c2dm);
   }

   g_object_notify_by_pspec(G_OBJECT(notification), gParamSpecs[PROP_C2DM]);
}

const gchar *
postal_notification_get_collapse_key (PostalNotification *notification)
{
   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);
   return notification->priv->collapse_key;
}

void
postal_notification_set_collapse_key (PostalNotification *notification,
                                      const gchar        *collapse_key)
{
   g_return_if_fail(POSTAL_IS_NOTIFICATION(notification));
   g_free(notification->priv->collapse_key);
   notification->priv->collapse_key = g_strdup(collapse_key);
   g_object_notify_by_pspec(G_OBJECT(notification),
                            gParamSpecs[PROP_COLLAPSE_KEY]);
}

JsonObject *
postal_notification_get_gcm (PostalNotification *notification)
{
   g_return_val_if_fail(POSTAL_IS_NOTIFICATION(notification), NULL);
   return notification->priv->gcm;
}

void
postal_notification_set_gcm (PostalNotification *notification,
                             JsonObject         *gcm)
{
   PostalNotificationPrivate *priv;

   g_return_if_fail(POSTAL_IS_NOTIFICATION(notification));

   priv = notification->priv;

   if (priv->gcm) {
      json_object_unref(priv->gcm);
      priv->gcm = NULL;
   }

   if (gcm) {
      priv->gcm = json_object_ref(gcm);
   }

   g_object_notify_by_pspec(G_OBJECT(notification), gParamSpecs[PROP_GCM]);
}

static void
postal_notification_finalize (GObject *object)
{
   PostalNotificationPrivate *priv;

   ENTRY;

   priv = POSTAL_NOTIFICATION(object)->priv;

   json_object_unref(priv->aps);
   json_object_unref(priv->c2dm);
   json_object_unref(priv->gcm);
   g_free(priv->collapse_key);

   G_OBJECT_CLASS(postal_notification_parent_class)->finalize(object);

   EXIT;
}

static void
postal_notification_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
   PostalNotification *notification = POSTAL_NOTIFICATION(object);

   switch (prop_id) {
   case PROP_APS:
      g_value_set_boxed(value, postal_notification_get_aps(notification));
      break;
   case PROP_C2DM:
      g_value_set_boxed(value, postal_notification_get_c2dm(notification));
      break;
   case PROP_GCM:
      g_value_set_boxed(value, postal_notification_get_gcm(notification));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_notification_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
   PostalNotification *notification = POSTAL_NOTIFICATION(object);

   switch (prop_id) {
   case PROP_APS:
      postal_notification_set_aps(notification, g_value_get_boxed(value));
      break;
   case PROP_C2DM:
      postal_notification_set_c2dm(notification, g_value_get_boxed(value));
      break;
   case PROP_GCM:
      postal_notification_set_gcm(notification, g_value_get_boxed(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_notification_class_init (PostalNotificationClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_notification_finalize;
   object_class->get_property = postal_notification_get_property;
   object_class->set_property = postal_notification_set_property;
   g_type_class_add_private(object_class, sizeof(PostalNotificationPrivate));

   gParamSpecs[PROP_APS] =
      g_param_spec_boxed("aps",
                         _("Aps"),
                         _("The APS notification message."),
                         JSON_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_APS,
                                   gParamSpecs[PROP_APS]);

   gParamSpecs[PROP_C2DM] =
      g_param_spec_boxed("c2dm",
                         _("C2DM"),
                         _("The C2DM notification message."),
                         JSON_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_C2DM,
                                   gParamSpecs[PROP_C2DM]);

   gParamSpecs[PROP_GCM] =
      g_param_spec_boxed("gcm",
                         _("Gcm"),
                         _("The GCM notification message."),
                         JSON_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_GCM,
                                   gParamSpecs[PROP_GCM]);

   EXIT;
}

static void
postal_notification_init (PostalNotification *notification)
{
   ENTRY;

   notification->priv = G_TYPE_INSTANCE_GET_PRIVATE(notification,
                                                    POSTAL_TYPE_NOTIFICATION,
                                                    PostalNotificationPrivate);

   EXIT;
}
