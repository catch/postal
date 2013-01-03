/* push-aps-message.h
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

#ifndef PUSH_APS_MESSAGE_H
#define PUSH_APS_MESSAGE_H

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define PUSH_TYPE_APS_MESSAGE            (push_aps_message_get_type())
#define PUSH_APS_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_APS_MESSAGE, PushApsMessage))
#define PUSH_APS_MESSAGE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_APS_MESSAGE, PushApsMessage const))
#define PUSH_APS_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_APS_MESSAGE, PushApsMessageClass))
#define PUSH_IS_APS_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_APS_MESSAGE))
#define PUSH_IS_APS_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_APS_MESSAGE))
#define PUSH_APS_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_APS_MESSAGE, PushApsMessageClass))

typedef struct _PushApsMessage        PushApsMessage;
typedef struct _PushApsMessageClass   PushApsMessageClass;
typedef struct _PushApsMessagePrivate PushApsMessagePrivate;

struct _PushApsMessage
{
   GObject parent;

   /*< private >*/
   PushApsMessagePrivate *priv;
};

struct _PushApsMessageClass
{
   GObjectClass parent_class;
};

void            push_aps_message_add_extra        (PushApsMessage *message,
                                                   const gchar    *key,
                                                   JsonNode       *value);
void            push_aps_message_add_extra_string (PushApsMessage *message,
                                                   const gchar    *key,
                                                   const gchar    *value);
GType           push_aps_message_get_type         (void) G_GNUC_CONST;
const gchar    *push_aps_message_get_alert        (PushApsMessage *message);
guint           push_aps_message_get_badge        (PushApsMessage *message);
GDateTime      *push_aps_message_get_expires_at   (PushApsMessage *message);
const gchar    *push_aps_message_get_json         (PushApsMessage *message);
const gchar    *push_aps_message_get_sound        (PushApsMessage *message);
PushApsMessage *push_aps_message_new              (void);
PushApsMessage *push_aps_message_new_from_json    (JsonObject     *object);
void            push_aps_message_set_alert        (PushApsMessage *message,
                                                   const gchar    *alert);
void            push_aps_message_set_badge        (PushApsMessage *message,
                                                   guint           badge);
void            push_aps_message_set_expires_at   (PushApsMessage *message,
                                                   GDateTime      *expires_at);
void            push_aps_message_set_sound        (PushApsMessage *message,
                                                   const gchar    *sound);

G_END_DECLS

#endif /* PUSH_APS_MESSAGE_H */
