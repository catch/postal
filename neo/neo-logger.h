/* neo-logger.h
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

#ifndef NEO_LOGGER_H
#define NEO_LOGGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NEO_TYPE_LOGGER            (neo_logger_get_type())
#define NEO_LOGGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_LOGGER, NeoLogger))
#define NEO_LOGGER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_LOGGER, NeoLogger const))
#define NEO_LOGGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NEO_TYPE_LOGGER, NeoLoggerClass))
#define NEO_IS_LOGGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEO_TYPE_LOGGER))
#define NEO_IS_LOGGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NEO_TYPE_LOGGER))
#define NEO_LOGGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NEO_TYPE_LOGGER, NeoLoggerClass))

typedef struct _NeoLogger        NeoLogger;
typedef struct _NeoLoggerClass   NeoLoggerClass;
typedef struct _NeoLoggerPrivate NeoLoggerPrivate;

struct _NeoLogger
{
   GObject parent;

   /*< private >*/
   NeoLoggerPrivate *priv;
};

struct _NeoLoggerClass
{
   GObjectClass parent_class;

   void (*log) (NeoLogger      *logger,
                GTimeVal       *event_time,
                const gchar    *log_domain,
                const gchar    *hostname,
                GPid            pid,
                guint           tid,
                GLogLevelFlags  log_level,
                const gchar    *message,
                const gchar    *formatted);
};

GType neo_logger_get_type (void) G_GNUC_CONST;
void  neo_logger_log      (NeoLogger      *logger,
                           GTimeVal       *event_time,
                           const gchar    *log_domain,
                           const gchar    *hostname,
                           GPid            pid,
                           guint           tid,
                           GLogLevelFlags  log_level,
                           const gchar    *message,
                           const gchar    *formatted);

G_END_DECLS

#endif /* NEO_LOGGER_H */
