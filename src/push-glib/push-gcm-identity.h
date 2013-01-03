/* push-gcm-identity.h
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

#ifndef PUSH_GCM_IDENTITY_H
#define PUSH_GCM_IDENTITY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PUSH_TYPE_GCM_IDENTITY            (push_gcm_identity_get_type())
#define PUSH_GCM_IDENTITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_GCM_IDENTITY, PushGcmIdentity))
#define PUSH_GCM_IDENTITY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_GCM_IDENTITY, PushGcmIdentity const))
#define PUSH_GCM_IDENTITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_GCM_IDENTITY, PushGcmIdentityClass))
#define PUSH_IS_GCM_IDENTITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_GCM_IDENTITY))
#define PUSH_IS_GCM_IDENTITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_GCM_IDENTITY))
#define PUSH_GCM_IDENTITY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_GCM_IDENTITY, PushGcmIdentityClass))

typedef struct _PushGcmIdentity        PushGcmIdentity;
typedef struct _PushGcmIdentityClass   PushGcmIdentityClass;
typedef struct _PushGcmIdentityPrivate PushGcmIdentityPrivate;

struct _PushGcmIdentity
{
   GObject parent;

   /*< private >*/
   PushGcmIdentityPrivate *priv;
};

struct _PushGcmIdentityClass
{
   GObjectClass parent_class;
};

PushGcmIdentity *push_gcm_identity_new                 (const gchar     *registration_id);
GType            push_gcm_identity_get_type            (void) G_GNUC_CONST;
const gchar     *push_gcm_identity_get_registration_id (PushGcmIdentity *identity);
void             push_gcm_identity_set_registration_id (PushGcmIdentity *identity,
                                                        const gchar     *registration_id);

G_END_DECLS

#endif /* PUSH_GCM_IDENTITY_H */
