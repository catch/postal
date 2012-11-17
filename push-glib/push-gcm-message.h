/* push-gcm-message.h
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

#ifndef PUSH_GCM_MESSAGE_H
#define PUSH_GCM_MESSAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PUSH_TYPE_GCM_MESSAGE            (push_gcm_message_get_type())
#define PUSH_GCM_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_GCM_MESSAGE, PushGcmMessage))
#define PUSH_GCM_MESSAGE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_GCM_MESSAGE, PushGcmMessage const))
#define PUSH_GCM_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_GCM_MESSAGE, PushGcmMessageClass))
#define PUSH_IS_GCM_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_GCM_MESSAGE))
#define PUSH_IS_GCM_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_GCM_MESSAGE))
#define PUSH_GCM_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_GCM_MESSAGE, PushGcmMessageClass))

typedef struct _PushGcmMessage        PushGcmMessage;
typedef struct _PushGcmMessageClass   PushGcmMessageClass;
typedef struct _PushGcmMessagePrivate PushGcmMessagePrivate;

struct _PushGcmMessage
{
   GObject parent;

   /*< private >*/
   PushGcmMessagePrivate *priv;
};

struct _PushGcmMessageClass
{
   GObjectClass parent_class;
};

GType push_gcm_message_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* PUSH_GCM_MESSAGE_H */
