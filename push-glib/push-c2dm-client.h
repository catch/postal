/* push-c2dm-client.h
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

#ifndef PUSH_C2DM_CLIENT_H
#define PUSH_C2DM_CLIENT_H

#include <glib-object.h>
#include <libsoup/soup.h>

#include "push-c2dm-identity.h"
#include "push-c2dm-message.h"

G_BEGIN_DECLS

#define PUSH_TYPE_C2DM_CLIENT            (push_c2dm_client_get_type())
#define PUSH_C2DM_CLIENT_ERROR           (push_c2dm_client_error_quark())
#define PUSH_C2DM_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_C2DM_CLIENT, PushC2dmClient))
#define PUSH_C2DM_CLIENT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PUSH_TYPE_C2DM_CLIENT, PushC2dmClient const))
#define PUSH_C2DM_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  PUSH_TYPE_C2DM_CLIENT, PushC2dmClientClass))
#define PUSH_IS_C2DM_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PUSH_TYPE_C2DM_CLIENT))
#define PUSH_IS_C2DM_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PUSH_TYPE_C2DM_CLIENT))
#define PUSH_C2DM_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  PUSH_TYPE_C2DM_CLIENT, PushC2dmClientClass))

typedef struct _PushC2dmClient        PushC2dmClient;
typedef struct _PushC2dmClientClass   PushC2dmClientClass;
typedef struct _PushC2dmClientPrivate PushC2dmClientPrivate;
typedef enum   _PushC2dmClientError   PushC2dmClientError;

enum _PushC2dmClientError
{
   PUSH_C2DM_CLIENT_ERROR_UNKNOWN,
   PUSH_C2DM_CLIENT_ERROR_QUOTA_EXCEEDED,
   PUSH_C2DM_CLIENT_ERROR_DEVICE_QUOTA_EXCEEDED,
   PUSH_C2DM_CLIENT_ERROR_MISSING_REGISTRATION,
   PUSH_C2DM_CLIENT_ERROR_INVALID_REGISTRATION,
   PUSH_C2DM_CLIENT_ERROR_MISMATCH_SENDER_ID,
   PUSH_C2DM_CLIENT_ERROR_NOT_REGISTERED,
   PUSH_C2DM_CLIENT_ERROR_MESSAGE_TOO_BIG,
   PUSH_C2DM_CLIENT_ERROR_MISSING_COLLAPSE_KEY,
};

struct _PushC2dmClient
{
   SoupSessionAsync parent;

   /*< private >*/
   PushC2dmClientPrivate *priv;
};

struct _PushC2dmClientClass
{
   SoupSessionAsyncClass parent_class;
};

void     push_c2dm_client_deliver_async  (PushC2dmClient       *client,
                                          PushC2dmIdentity     *identity,
                                          PushC2dmMessage      *message,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean push_c2dm_client_deliver_finish (PushC2dmClient       *client,
                                          GAsyncResult         *result,
                                          GError              **error);
GQuark   push_c2dm_client_error_quark    (void) G_GNUC_CONST;
GType    push_c2dm_client_get_type       (void) G_GNUC_CONST;

G_END_DECLS

#endif /* PUSH_C2DM_CLIENT_H */
