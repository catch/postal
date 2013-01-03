/* push-aps-identity.h
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

#ifndef PUSH_APS_IDENTITY_H
#define PUSH_APS_IDENTITY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PUSH_TYPE_APS_IDENTITY            (push_aps_identity_get_type())
#define PUSH_APS_IDENTITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_APS_IDENTITY, PushApsIdentity))
#define PUSH_APS_IDENTITY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_APS_IDENTITY, PushApsIdentity const))
#define PUSH_APS_IDENTITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_APS_IDENTITY, PushApsIdentityClass))
#define PUSH_IS_APS_IDENTITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_APS_IDENTITY))
#define PUSH_IS_APS_IDENTITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_APS_IDENTITY))
#define PUSH_APS_IDENTITY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_APS_IDENTITY, PushApsIdentityClass))

typedef struct _PushApsIdentity        PushApsIdentity;
typedef struct _PushApsIdentityClass   PushApsIdentityClass;
typedef struct _PushApsIdentityPrivate PushApsIdentityPrivate;

struct _PushApsIdentity
{
   GObject parent;

   /*< private >*/
   PushApsIdentityPrivate *priv;
};

struct _PushApsIdentityClass
{
   GObjectClass parent_class;
};

PushApsIdentity *push_aps_identity_new              (const gchar     *device_token);
GType            push_aps_identity_get_type         (void) G_GNUC_CONST;
const gchar     *push_aps_identity_get_device_token (PushApsIdentity *identity);
void             push_aps_identity_set_device_token (PushApsIdentity *identity,
                                                     const gchar     *device_token);

G_END_DECLS

#endif /* PUSH_APS_IDENTITY_H */
