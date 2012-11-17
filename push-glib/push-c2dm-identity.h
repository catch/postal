/* push-c2dm-identity.h
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

#ifndef PUSH_C2DM_IDENTITY_H
#define PUSH_C2DM_IDENTITY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PUSH_TYPE_C2DM_IDENTITY            (push_c2dm_identity_get_type())
#define PUSH_C2DM_IDENTITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_C2DM_IDENTITY, PushC2dmIdentity))
#define PUSH_C2DM_IDENTITY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_C2DM_IDENTITY, PushC2dmIdentity const))
#define PUSH_C2DM_IDENTITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_C2DM_IDENTITY, PushC2dmIdentityClass))
#define PUSH_IS_C2DM_IDENTITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_C2DM_IDENTITY))
#define PUSH_IS_C2DM_IDENTITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_C2DM_IDENTITY))
#define PUSH_C2DM_IDENTITY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_C2DM_IDENTITY, PushC2dmIdentityClass))

typedef struct _PushC2dmIdentity        PushC2dmIdentity;
typedef struct _PushC2dmIdentityClass   PushC2dmIdentityClass;
typedef struct _PushC2dmIdentityPrivate PushC2dmIdentityPrivate;

struct _PushC2dmIdentity
{
   GObject parent;

   /*< private >*/
   PushC2dmIdentityPrivate *priv;
};

struct _PushC2dmIdentityClass
{
   GObjectClass parent_class;
};

const gchar      *push_c2dm_identity_get_registration_id (PushC2dmIdentity *identity);
GType             push_c2dm_identity_get_type            (void) G_GNUC_CONST;
PushC2dmIdentity *push_c2dm_identity_new                 (const gchar      *registration_id);
void              push_c2dm_identity_set_registration_id (PushC2dmIdentity *identity,
                                                          const gchar      *registration_id);

G_END_DECLS

#endif /* PUSH_C2DM_IDENTITY_H */
