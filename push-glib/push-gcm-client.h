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

#ifndef PUSH_GCM_CLIENT_H
#define PUSH_GCM_CLIENT_H

#include <libsoup/soup.h>

#include "push-gcm-identity.h"
#include "push-gcm-message.h"

G_BEGIN_DECLS

#define PUSH_TYPE_GCM_CLIENT            (push_gcm_client_get_type())
#define PUSH_GCM_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_GCM_CLIENT, PushGcmClient))
#define PUSH_GCM_CLIENT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_GCM_CLIENT, PushGcmClient const))
#define PUSH_GCM_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_GCM_CLIENT, PushGcmClientClass))
#define PUSH_IS_GCM_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_GCM_CLIENT))
#define PUSH_IS_GCM_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_GCM_CLIENT))
#define PUSH_GCM_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_GCM_CLIENT, PushGcmClientClass))

typedef struct _PushGcmClient        PushGcmClient;
typedef struct _PushGcmClientClass   PushGcmClientClass;
typedef struct _PushGcmClientPrivate PushGcmClientPrivate;

struct _PushGcmClient
{
   SoupSessionAsync parent;

   /*< private >*/
   PushGcmClientPrivate *priv;
};

struct _PushGcmClientClass
{
   SoupSessionAsyncClass parent_class;
};

GType          push_gcm_client_get_type      (void) G_GNUC_CONST;
PushGcmClient *push_gcm_client_new           (const gchar          *auth_token);
void           push_gcm_client_deliver_async (PushGcmClient        *client,
                                              GList                *identities,
                                              PushGcmMessage       *message,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
gboolean       push_gcm_client_deliver_finish (PushGcmClient       *client,
                                               GAsyncResult        *result,
                                               GError             **error);

G_END_DECLS

#endif /* PUSH_GCM_CLIENT_H */
