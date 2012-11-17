/* neo-logger.c
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

#include "neo-logger.h"

G_DEFINE_ABSTRACT_TYPE(NeoLogger, neo_logger, G_TYPE_OBJECT)

void
neo_logger_log (NeoLogger      *logger,
                GTimeVal       *event_time,
                const gchar    *log_domain,
                const gchar    *hostname,
                GPid            pid,
                guint           tid,
                GLogLevelFlags  log_level,
                const gchar    *message,
                const gchar    *formatted)
{
   NEO_LOGGER_GET_CLASS(logger)->log(logger,
                                     event_time,
                                     log_domain,
                                     hostname,
                                     pid,
                                     tid,
                                     log_level,
                                     message,
                                     formatted);
}

static void
neo_logger_class_init (NeoLoggerClass *klass)
{
}

static void
neo_logger_init (NeoLogger *logger)
{
}
