/* push-c2dm-message.h
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

#ifndef PUSH_C2DM_MESSAGE_H
#define PUSH_C2DM_MESSAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PUSH_TYPE_C2DM_MESSAGE            (push_c2dm_message_get_type())
#define PUSH_C2DM_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_C2DM_MESSAGE, PushC2dmMessage))
#define PUSH_C2DM_MESSAGE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_C2DM_MESSAGE, PushC2dmMessage const))
#define PUSH_C2DM_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_C2DM_MESSAGE, PushC2dmMessageClass))
#define PUSH_IS_C2DM_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_C2DM_MESSAGE))
#define PUSH_IS_C2DM_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_C2DM_MESSAGE))
#define PUSH_C2DM_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_C2DM_MESSAGE, PushC2dmMessageClass))

typedef struct _PushC2dmMessage        PushC2dmMessage;
typedef struct _PushC2dmMessageClass   PushC2dmMessageClass;
typedef struct _PushC2dmMessagePrivate PushC2dmMessagePrivate;

struct _PushC2dmMessage
{
   GObject parent;

   /*< private >*/
   PushC2dmMessagePrivate *priv;
};

struct _PushC2dmMessageClass
{
   GObjectClass parent_class;
};

void             push_c2dm_message_add_param            (PushC2dmMessage *message,
                                                         const gchar     *param,
                                                         const gchar     *value);
GHashTable      *push_c2dm_message_build_params         (PushC2dmMessage *message);
gboolean         push_c2dm_message_get_delay_while_idle (PushC2dmMessage *message);
const gchar     *push_c2dm_message_get_collapse_key     (PushC2dmMessage *message);
GType            push_c2dm_message_get_type             (void) G_GNUC_CONST;
PushC2dmMessage *push_c2dm_message_new                  (void);
void             push_c2dm_message_set_collapse_key     (PushC2dmMessage *message,
                                                         const gchar     *collapse_key);
void             push_c2dm_message_set_delay_while_idle (PushC2dmMessage *message,
                                                         gboolean         delay_while_idle);

G_END_DECLS

#endif /* PUSH_C2DM_MESSAGE_H */
