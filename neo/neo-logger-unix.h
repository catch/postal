/* neo-logger-unix.h
 *
 * Copyright (C) 2012 Catch.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NEO_LOGGER_UNIX_H
#define NEO_LOGGER_UNIX_H

#include "neo-logger.h"

G_BEGIN_DECLS

#define NEO_TYPE_LOGGER_UNIX            (neo_logger_unix_get_type())
#define NEO_LOGGER_UNIX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_LOGGER_UNIX, NeoLoggerUnix))
#define NEO_LOGGER_UNIX_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_LOGGER_UNIX, NeoLoggerUnix const))
#define NEO_LOGGER_UNIX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NEO_TYPE_LOGGER_UNIX, NeoLoggerUnixClass))
#define NEO_IS_LOGGER_UNIX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEO_TYPE_LOGGER_UNIX))
#define NEO_IS_LOGGER_UNIX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NEO_TYPE_LOGGER_UNIX))
#define NEO_LOGGER_UNIX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NEO_TYPE_LOGGER_UNIX, NeoLoggerUnixClass))

typedef struct _NeoLoggerUnix        NeoLoggerUnix;
typedef struct _NeoLoggerUnixClass   NeoLoggerUnixClass;
typedef struct _NeoLoggerUnixPrivate NeoLoggerUnixPrivate;

struct _NeoLoggerUnix
{
   NeoLogger parent;

   /*< private >*/
   gint fileno;
   gboolean close_on_unref;
};

struct _NeoLoggerUnixClass
{
   NeoLoggerClass parent_class;
};

GType      neo_logger_unix_get_type (void) G_GNUC_CONST;
NeoLogger *neo_logger_unix_new      (gint     fd,
                                     gboolean close_on_unref);

G_END_DECLS

#endif /* NEO_LOGGER_UNIX_H */
