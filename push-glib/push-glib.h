/* push-glib.h
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

#ifndef PUSH_GLIB_H
#define PUSH_GLIB_H

#include <glib.h>

G_BEGIN_DECLS

#define PUSH_INSIDE

#include "push-aps-client.h"
#include "push-aps-identity.h"
#include "push-aps-message.h"
#include "push-c2dm-client.h"
#include "push-c2dm-identity.h"
#include "push-c2dm-message.h"
#include "push-gcm-client.h"
#include "push-gcm-identity.h"
#include "push-gcm-message.h"

#undef PUSH_INSIDE

G_END_DECLS

#endif /* PUSH_GLIB_H */
