/* push-aps-message.c
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

#include "push-aps-message.h"
#include "push-debug.h"

/**
 * SECTION:push-aps-message
 * @title: PushApsMessage
 * @short_description: An APS notification.
 *
 * #PushApsMessage represents a notification that is to be delivered to
 * an identity via #PushApsClient. It contains the typical "alert",
 * "badge", and "sound" fields. Additionally, you may specify extra fields
 * for the notification using push_aps_message_add_extra() and similar
 * functions.
 *
 * Since APS uses a JSON encoded string in the protocol, to add generic
 * data to the message, you should provided a JsonNode. Alternatively,
 * you can use simple helpers such as push_aps_message_add_extra_string()
 * for common cases.
 *
 * Use push_aps_client_deliver_async() to deliver a message to a
 * #PushApsIdentity.
 */

G_DEFINE_TYPE(PushApsMessage, push_aps_message, G_TYPE_OBJECT)

struct _PushApsMessagePrivate
{
   GHashTable *extra;
   GDateTime *expires_at;
   gboolean badge_set;
   gchar *alert;
   guint badge;
   gchar *sound;
   gchar *json;
};

enum
{
   PROP_0,
   PROP_ALERT,
   PROP_BADGE,
   PROP_EXPIRES_AT,
   PROP_JSON,
   PROP_SOUND,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

PushApsMessage *
push_aps_message_new (void)
{
   return g_object_new(PUSH_TYPE_APS_MESSAGE, NULL);
}

PushApsMessage *
push_aps_message_new_from_json (JsonObject *object)
{
   PushApsMessage *message;
   JsonNode *node;
   GList *list;
   GList *iter;

   ENTRY;

   message = push_aps_message_new();

   if (!object) {
      RETURN(message);
   }

   list = json_object_get_members(object);

   for (iter = list; iter; iter = iter->next) {
      node = json_object_get_member(object, iter->data);
      if (!g_strcmp0(iter->data, "alert")) {
         if (json_node_get_value_type(node) == G_TYPE_STRING) {
            push_aps_message_set_alert(message, json_node_get_string(node));
         } else {
            /*
             * TODO: Handle JSON sub-items.
             */
         }
      } else if (!g_strcmp0(iter->data, "badge")) {
         if (json_node_get_value_type(node) == G_TYPE_INT64) {
            push_aps_message_set_badge(message, json_node_get_int(node));
         }
      } else if (!g_strcmp0(iter->data, "sound")) {
         if (json_node_get_value_type(node) == G_TYPE_STRING) {
            push_aps_message_set_sound(message, json_node_get_string(node));
         }
      } else {
         push_aps_message_add_extra(message, iter->data, node);
      }
   }

   g_list_free(list);

   RETURN(message);
}

/**
 * push_aps_message_get_json:
 * @message: (in): A #PushApsMessage.
 *
 * Retrieves the message as a JSON encoded message. The resulting string
 * is owned by the #PushApsMessage instance and should not be freed.
 *
 * The string may be cached for future calls to this function.
 *
 * Returns: The message as a JSON encoded string.
 */
const gchar *
push_aps_message_get_json (PushApsMessage *message)
{
   PushApsMessagePrivate *priv;
   GHashTableIter iter;
   JsonGenerator *json;
   JsonObject *obj;
   JsonObject *aps;
   JsonNode *root;
   gpointer key;
   gpointer value;
   gchar *ret = NULL;

   ENTRY;

   g_return_val_if_fail(PUSH_IS_APS_MESSAGE(message), NULL);

   priv = message->priv;

   if (priv->json) {
      RETURN(priv->json);
   }

   obj = json_object_new();
   root = json_node_new(JSON_NODE_OBJECT);
   json_node_take_object(root, obj);

   if (priv->extra) {
      g_hash_table_iter_init(&iter, priv->extra);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         json_object_set_member(obj, key, json_node_copy(value));
      }
   }

   aps = json_object_new();
   if (priv->alert) {
      json_object_set_string_member(aps, "alert", priv->alert);
   }
   if (priv->badge_set || (!priv->alert && !priv->sound)) {
      json_object_set_int_member(aps, "badge", priv->badge);
   }
   if (priv->sound) {
      json_object_set_string_member(aps, "sound", priv->sound);
   }
   json_object_set_object_member(obj, "aps", aps);

   json = json_generator_new();
   json_generator_set_root(json, root);
   ret = json_generator_to_data(json, NULL);

   json_node_free(root);
   g_object_unref(json);

   g_free(priv->json);
   priv->json = ret;

   RETURN(ret);
}

/**
 * push_aps_message_add_extra:
 * @message: A #PushApsMessage.
 * @key: The key to associate with @value.
 * @value: (transfer none): A #JsonNode to send with the message.
 *
 * Adds a JsonValue to the JSON portion of a push notification. @key MUST NOT
 * be "aps", as that is reserved for internal use by the APS message.
 *
 * The extra value will be sent inside the JSON encoded message using @key
 * as the field name in the dictionary.
 */
void
push_aps_message_add_extra (PushApsMessage *message,
                            const gchar    *key,
                            JsonNode       *value)
{
   PushApsMessagePrivate *priv;

   ENTRY;

   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));
   g_return_if_fail(key);
   g_return_if_fail(!g_str_equal(key, "aps"));
   g_return_if_fail(value);

   priv = message->priv;

   if (!priv->extra) {
      priv->extra =
         g_hash_table_new_full(g_str_hash, g_str_equal,
                               g_free, (GDestroyNotify)json_node_free);
   }

   g_hash_table_insert(priv->extra, g_strdup(key), json_node_copy(value));

   g_free(priv->json);
   priv->json = NULL;

   EXIT;
}

/**
 * push_aps_message_add_extra_string:
 * @message: A #PushApsMessage.
 * @key: The key for @value.
 * @value: The value for @key.
 *
 * Adds a key/value pair to the resulting JSON encoded string that is
 * delivered to a #PushApsIdentity.
 *
 * This simply creates a JsonNode containing @value and calls
 * push_aps_message_add_extra().
 */
void
push_aps_message_add_extra_string (PushApsMessage *message,
                                   const gchar    *key,
                                   const gchar    *value)
{
   JsonNode *node;

   ENTRY;

   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));
   g_return_if_fail(key);

   node = json_node_new(JSON_NODE_VALUE);
   json_node_set_string(node, value);
   push_aps_message_add_extra(message, key, node);
   json_node_free(node);

   EXIT;
}

/**
 * push_aps_message_get_alert:
 * @message: (in): A #PushApsMessage.
 *
 * Fetches the "alert" property, containing the text of the alert message
 * or %NULL.
 *
 * Returns: A string or %NULL if not set.
 */
const gchar *
push_aps_message_get_alert (PushApsMessage *message)
{
   g_return_val_if_fail(PUSH_IS_APS_MESSAGE(message), NULL);
   return message->priv->alert;
}

/**
 * push_aps_message_get_badge:
 * @message: A #PushApsMessage.
 *
 * Retrieves the "badge" property, containing the badge number that should
 * be displayed once the message is received by an identity.
 *
 * Returns: The badge number.
 */
guint
push_aps_message_get_badge (PushApsMessage *message)
{
   g_return_val_if_fail(PUSH_IS_APS_MESSAGE(message), 0);
   return message->priv->badge;
}

/**
 * push_aps_message_get_expires_at:
 * @message: A #PushApsMessage.
 *
 * Retrieves the "expires-at" property, containing the time that the message
 * should expire if it could not be delivered.
 *
 * Returns: (transfer none): A #GDateTime, or %NULL.
 */
GDateTime *
push_aps_message_get_expires_at (PushApsMessage *message)
{
   g_return_val_if_fail(PUSH_IS_APS_MESSAGE(message), NULL);
   return message->priv->expires_at;
}

/**
 * push_aps_message_get_sound:
 * @message: (in): A #PushApsMessage.
 *
 * Retrieves the "sound" property, containing the sound to be played
 * upon receipt of the message.
 *
 * Returns: A string or %NULL if not set.
 */
const gchar *
push_aps_message_get_sound (PushApsMessage *message)
{
   g_return_val_if_fail(PUSH_IS_APS_MESSAGE(message), NULL);
   return message->priv->sound;
}

/**
 * push_aps_message_set_alert:
 * @message: A #PushApsMessage.
 * @alert: (allow-none): The alert text to display.
 *
 * Sets the "alert" property of @message. This contains the text that should
 * be displayed in the alert message, or %NULL for no alert text.
 */
void
push_aps_message_set_alert (PushApsMessage *message,
                            const gchar    *alert)
{
   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));

   g_free(message->priv->alert);
   message->priv->alert = g_strdup(alert);
   g_free(message->priv->json);
   message->priv->json = NULL;
   g_object_notify_by_pspec(G_OBJECT(message), gParamSpecs[PROP_ALERT]);
}

/**
 * push_aps_message_set_badge:
 * @message: A #PushApsMessage.
 * @badge: The badge number to display or zero to unset.
 *
 * Sets the "badge" property. This contains the number to be displayed on
 * the applications badge on an iOS device. To unset the badge, set this
 * property to zero.
 */
void
push_aps_message_set_badge (PushApsMessage *message,
                            guint           badge)
{
   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));

   message->priv->badge = badge;
   message->priv->badge_set = TRUE;
   g_free(message->priv->json);
   message->priv->json = NULL;
   g_object_notify_by_pspec(G_OBJECT(message), gParamSpecs[PROP_BADGE]);
}

/**
 * push_aps_message_set_expires_at:
 * @message: A #PushApsMessage.
 * @expires_at: (allow-none): A #GDateTime, or %NULL.
 *
 * Sets the "expires-at" property, containing the time at which the message
 * should expire and further attempts to deliver should be aborted.
 */
void
push_aps_message_set_expires_at (PushApsMessage *message,
                                 GDateTime      *expires_at)
{
   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));

   if (message->priv->expires_at) {
      g_date_time_unref(message->priv->expires_at);
      message->priv->expires_at = NULL;
   }

   if (expires_at) {
      message->priv->expires_at = g_date_time_ref(expires_at);
   }
}

/**
 * push_aps_message_set_sound:
 * @message: A #PushApsMessage.
 * @sound: (allow-none): A string containing the sound to play or %NULL.
 *
 * Sest the "sound" property of the message. This contains the sound to be
 * played upon receipt of the message.
 */
void
push_aps_message_set_sound (PushApsMessage *message,
                            const gchar    *sound)
{
   g_return_if_fail(PUSH_IS_APS_MESSAGE(message));

   g_free(message->priv->sound);
   message->priv->sound = g_strdup(sound);
   g_free(message->priv->json);
   message->priv->json = NULL;
   g_object_notify_by_pspec(G_OBJECT(message), gParamSpecs[PROP_SOUND]);
}

static void
push_aps_message_finalize (GObject *object)
{
   PushApsMessagePrivate *priv;

   ENTRY;

   priv = PUSH_APS_MESSAGE(object)->priv;

   g_free(priv->alert);
   priv->alert = NULL;

   g_free(priv->sound);
   priv->sound = NULL;

   g_free(priv->json);
   priv->json = NULL;

   if (priv->expires_at) {
      g_date_time_unref(priv->expires_at);
      priv->expires_at = NULL;
   }

   if (priv->extra) {
      g_hash_table_unref(priv->extra);
      priv->extra = NULL;
   }

   G_OBJECT_CLASS(push_aps_message_parent_class)->finalize(object);

   EXIT;
}

static void
push_aps_message_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
   PushApsMessage *message = PUSH_APS_MESSAGE(object);

   switch (prop_id) {
   case PROP_ALERT:
      g_value_set_string(value, push_aps_message_get_alert(message));
      break;
   case PROP_BADGE:
      g_value_set_uint(value, push_aps_message_get_badge(message));
      break;
   case PROP_JSON:
      g_value_set_string(value, push_aps_message_get_json(message));
      break;
   case PROP_SOUND:
      g_value_set_string(value, push_aps_message_get_sound(message));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_aps_message_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
   PushApsMessage *message = PUSH_APS_MESSAGE(object);

   switch (prop_id) {
   case PROP_ALERT:
      push_aps_message_set_alert(message, g_value_get_string(value));
      break;
   case PROP_BADGE:
      push_aps_message_set_badge(message, g_value_get_uint(value));
      break;
   case PROP_SOUND:
      push_aps_message_set_sound(message, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
push_aps_message_class_init (PushApsMessageClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = push_aps_message_finalize;
   object_class->get_property = push_aps_message_get_property;
   object_class->set_property = push_aps_message_set_property;
   g_type_class_add_private(object_class, sizeof(PushApsMessagePrivate));

   gParamSpecs[PROP_ALERT] =
      g_param_spec_string("alert",
                          _("Alert"),
                          _("The alert text to display."),
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_ALERT,
                                   gParamSpecs[PROP_ALERT]);

   gParamSpecs[PROP_BADGE] =
      g_param_spec_uint("badge",
                        _("Badge"),
                        _("The badget number or zero to unset."),
                        0,
                        G_MAXUINT32,
                        0,
                        G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_BADGE,
                                   gParamSpecs[PROP_BADGE]);

   gParamSpecs[PROP_EXPIRES_AT] =
      g_param_spec_boxed("expires-at",
                         _("Expires At"),
                         _("The time at which the notification expires."),
                         G_TYPE_DATE_TIME,
                         G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_EXPIRES_AT,
                                   gParamSpecs[PROP_EXPIRES_AT]);

   gParamSpecs[PROP_JSON] =
      g_param_spec_string("json",
                          _("JSON"),
                          _("The message as a JSON encoded string."),
                          NULL,
                          G_PARAM_READABLE);
   g_object_class_install_property(object_class, PROP_JSON,
                                   gParamSpecs[PROP_JSON]);

   gParamSpecs[PROP_SOUND] =
      g_param_spec_string("sound",
                          _("Sound"),
                          _("The sound to play."),
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_SOUND,
                                   gParamSpecs[PROP_SOUND]);

   EXIT;
}

static void
push_aps_message_init (PushApsMessage *message)
{
   message->priv = G_TYPE_INSTANCE_GET_PRIVATE(message,
                                               PUSH_TYPE_APS_MESSAGE,
                                               PushApsMessagePrivate);
}
