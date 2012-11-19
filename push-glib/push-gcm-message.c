/* push-gcm-message.c
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

#include "push-gcm-message.h"

G_DEFINE_TYPE(PushGcmMessage, push_gcm_message, G_TYPE_OBJECT)

struct _PushGcmMessagePrivate
{
   gchar      *collapse_key;
   JsonObject *data;
   gboolean    delay_while_idle;
   gboolean    dry_run;
   guint       time_to_live;
};

enum
{
   PROP_0,
   PROP_COLLAPSE_KEY,
   PROP_DATA,
   PROP_DELAY_WHILE_IDLE,
   PROP_DRY_RUN,
   PROP_TIME_TO_LIVE,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

const gchar *
push_gcm_message_get_collapse_key (PushGcmMessage *message)
{
   g_return_val_if_fail(PUSH_IS_GCM_MESSAGE(message), NULL);
   return message->priv->collapse_key;
}

void
push_gcm_message_set_collapse_key (PushGcmMessage *message,
                                   const gchar    *collapse_key)
{
   g_return_if_fail(PUSH_IS_GCM_MESSAGE(message));
   g_free(message->priv->collapse_key);
   message->priv->collapse_key = g_strdup(collapse_key);
   g_object_notify_by_pspec(G_OBJECT(message), gParamSpecs[PROP_COLLAPSE_KEY]);
}

/**
 * push_gcm_message_get_data:
 * @message: (in): A #PushGcmMessage.
 *
 * Fetches the :data property. This corresponds to the "data" field
 * in a GCM notification.
 *
 * Returns: (transfer none): A #JsonObject or %NULL.
 */
JsonObject *
push_gcm_message_get_data (PushGcmMessage *message)
{
   g_return_val_if_fail(PUSH_IS_GCM_MESSAGE(message), NULL);
   return message->priv->data;
}

void
push_gcm_message_set_data (PushGcmMessage *message,
                           JsonObject     *data)
{
   PushGcmMessagePrivate *priv;

   g_return_if_fail(PUSH_IS_GCM_MESSAGE(message));

   priv = message->priv;

   if (priv->data) {
      json_object_unref(priv->data);
      priv->data = NULL;
   }

   if (data) {
      priv->data = json_object_ref(data);
   }

   g_object_notify_by_pspec(G_OBJECT(message), gParamSpecs[PROP_DATA]);
}

gboolean
push_gcm_message_get_delay_while_idle (PushGcmMessage *message)
{
   g_return_val_if_fail(PUSH_IS_GCM_MESSAGE(message), FALSE);
   return message->priv->delay_while_idle;
}

void
push_gcm_message_set_delay_while_idle (PushGcmMessage *message,
                                       gboolean        delay_while_idle)
{
   g_return_if_fail(PUSH_IS_GCM_MESSAGE(message));
   message->priv->delay_while_idle = delay_while_idle;
   g_object_notify_by_pspec(G_OBJECT(message),
                            gParamSpecs[PROP_DELAY_WHILE_IDLE]);
}

gboolean
push_gcm_message_get_dry_run (PushGcmMessage *message)
{
   g_return_val_if_fail(PUSH_IS_GCM_MESSAGE(message), FALSE);
   return message->priv->dry_run;
}

void
push_gcm_message_set_dry_run (PushGcmMessage *message,
                              gboolean        dry_run)
{
   g_return_if_fail(PUSH_IS_GCM_MESSAGE(message));
   message->priv->dry_run = dry_run;
   g_object_notify_by_pspec(G_OBJECT(message), gParamSpecs[PROP_DRY_RUN]);
}

guint
push_gcm_message_get_time_to_live (PushGcmMessage *message)
{
   g_return_val_if_fail(PUSH_IS_GCM_MESSAGE(message), 0);
   return message->priv->time_to_live;
}

void
push_gcm_message_set_time_to_live (PushGcmMessage *message,
                                   guint           time_to_live)
{
   g_return_if_fail(PUSH_IS_GCM_MESSAGE(message));
   message->priv->time_to_live = time_to_live;
   g_object_notify_by_pspec(G_OBJECT(message),
                            gParamSpecs[PROP_TIME_TO_LIVE]);
}

static void
push_gcm_message_finalize (GObject *object)
{
   PushGcmMessagePrivate *priv;

   priv = PUSH_GCM_MESSAGE(object)->priv;

   g_free(priv->collapse_key);

   if (priv->data) {
      json_object_unref(priv->data);
   }

   G_OBJECT_CLASS(push_gcm_message_parent_class)->finalize(object);
}

static void
push_gcm_message_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
   PushGcmMessage *message = PUSH_GCM_MESSAGE(object);

   switch (prop_id) {
   case PROP_COLLAPSE_KEY:
      g_value_set_string(value, push_gcm_message_get_collapse_key(message));
      break;
   case PROP_DATA:
      g_value_set_boxed(value, push_gcm_message_get_data(message));
      break;
   case PROP_DELAY_WHILE_IDLE:
      g_value_set_boolean(value,
                          push_gcm_message_get_delay_while_idle(message));
      break;
   case PROP_DRY_RUN:
      g_value_set_boolean(value, push_gcm_message_get_dry_run(message));
      break;
   case PROP_TIME_TO_LIVE:
      g_value_set_uint(value, push_gcm_message_get_time_to_live(message));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_message_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
   PushGcmMessage *message = PUSH_GCM_MESSAGE(object);

   switch (prop_id) {
   case PROP_COLLAPSE_KEY:
      push_gcm_message_set_collapse_key(message, g_value_get_string(value));
      break;
   case PROP_DATA:
      push_gcm_message_set_data(message, g_value_get_boxed(value));
      break;
   case PROP_DELAY_WHILE_IDLE:
      push_gcm_message_set_delay_while_idle(message,
                                            g_value_get_boolean(value));
      break;
   case PROP_DRY_RUN:
      push_gcm_message_set_dry_run(message, g_value_get_boolean(value));
      break;
   case PROP_TIME_TO_LIVE:
      push_gcm_message_set_time_to_live(message, g_value_get_uint(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_gcm_message_class_init (PushGcmMessageClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_gcm_message_finalize;
   object_class->get_property = push_gcm_message_get_property;
   object_class->set_property = push_gcm_message_set_property;
   g_type_class_add_private(object_class, sizeof(PushGcmMessagePrivate));

   gParamSpecs[PROP_COLLAPSE_KEY] =
      g_param_spec_string("collapse-key",
                          _("Collapse Key"),
                          _("The key used for collapsing messages."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_COLLAPSE_KEY,
                                   gParamSpecs[PROP_COLLAPSE_KEY]);

   gParamSpecs[PROP_DATA] =
      g_param_spec_boxed("data",
                         _("Data"),
                         _("The data field for the message."),
                         JSON_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DATA,
                                   gParamSpecs[PROP_DATA]);

   gParamSpecs[PROP_DELAY_WHILE_IDLE] =
      g_param_spec_boolean("delay-while-idle",
                           _("Delay While Idle"),
                           _("If the message should be delayed "
                             "until the device wakes up."),
                           TRUE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DELAY_WHILE_IDLE,
                                   gParamSpecs[PROP_DELAY_WHILE_IDLE]);

   gParamSpecs[PROP_DRY_RUN] =
      g_param_spec_boolean("dry-run",
                          _("Dry Run"),
                          _("If this message is only a test of the system "
                            "and no message should be delivered to a "
                            "device."),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DRY_RUN,
                                   gParamSpecs[PROP_DRY_RUN]);

   gParamSpecs[PROP_TIME_TO_LIVE] =
      g_param_spec_uint("time-to-live",
                        _("Time To Live"),
                        _("The number of seconds until the message "
                          "is no longer valid."),
                        0,
                        G_MAXUINT,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_TIME_TO_LIVE,
                                   gParamSpecs[PROP_TIME_TO_LIVE]);
}

static void
push_gcm_message_init (PushGcmMessage *message)
{
   message->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(message,
                                  PUSH_TYPE_GCM_MESSAGE,
                                  PushGcmMessagePrivate);
}
