/* neo-logger-unix.c
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

#include <glib/gi18n.h>
#include <unistd.h>

#include "neo-logger-unix.h"

G_DEFINE_TYPE(NeoLoggerUnix, neo_logger_unix, NEO_TYPE_LOGGER)

enum
{
   PROP_0,
   PROP_CLOSE_ON_UNREF,
   PROP_FILENO,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

NeoLogger *
neo_logger_unix_new (gint fileno,
                     gboolean close_on_unref)
{
   return g_object_new(NEO_TYPE_LOGGER_UNIX,
                       "close-on-unref", close_on_unref,
                       "fileno", fileno,
                       NULL);
}

static void
neo_logger_unix_log (NeoLogger      *logger,
                     GTimeVal       *event_time,
                     const gchar    *log_domain,
                     const gchar    *hostname,
                     GPid            pid,
                     guint           tid,
                     GLogLevelFlags  log_level,
                     const gchar    *message,
                     const gchar    *formatted)
{
   NeoLoggerUnix *ux = (NeoLoggerUnix *)logger;
   if (ux->fileno >= 0) {
      write(ux->fileno, formatted, strlen(formatted));
   }
}

static void
neo_logger_unix_finalize (GObject *object)
{
   NeoLoggerUnix *ux = (NeoLoggerUnix *)object;

   if ((ux->close_on_unref) && (ux->fileno >= 0)) {
      close(ux->fileno);
   }

   ux->close_on_unref = FALSE;
   ux->fileno = -1;

   G_OBJECT_CLASS(neo_logger_unix_parent_class)->finalize(object);
}

static void
neo_logger_unix_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
   NeoLoggerUnix *ux = NEO_LOGGER_UNIX(object);

   switch (prop_id) {
   case PROP_CLOSE_ON_UNREF:
      g_value_set_boolean(value, ux->close_on_unref);
      break;
   case PROP_FILENO:
      g_value_set_int(value, ux->fileno);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_logger_unix_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
   NeoLoggerUnix *ux = NEO_LOGGER_UNIX(object);

   switch (prop_id) {
   case PROP_FILENO:
      ux->fileno = g_value_get_int(value);
      break;
   case PROP_CLOSE_ON_UNREF:
      ux->close_on_unref = g_value_get_boolean(value);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_logger_unix_class_init (NeoLoggerUnixClass *klass)
{
   GObjectClass *object_class;
   NeoLoggerClass *logger_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = neo_logger_unix_finalize;
   object_class->get_property = neo_logger_unix_get_property;
   object_class->set_property = neo_logger_unix_set_property;

   logger_class = NEO_LOGGER_CLASS(klass);
   logger_class->log = neo_logger_unix_log;

   gParamSpecs[PROP_CLOSE_ON_UNREF] =
      g_param_spec_boolean("close-on-unref",
                          _("Close on unref"),
                          _("If the file-descriptor should be closed"
                            " when the object is finalized."),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_CLOSE_ON_UNREF,
                                   gParamSpecs[PROP_CLOSE_ON_UNREF]);

   gParamSpecs[PROP_FILENO] =
      g_param_spec_int("fileno",
                       _("Fileno"),
                       _("A UNIX file-descriptor."),
                       -1,
                       G_MAXINT32,
                       -1,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_FILENO,
                                   gParamSpecs[PROP_FILENO]);
}

static void
neo_logger_unix_init (NeoLoggerUnix *ux)
{
   ux->fileno = -1;
   ux->close_on_unref = FALSE;
}
