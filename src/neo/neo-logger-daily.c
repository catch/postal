#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <sys/time.h>
#include <time.h>

#include "neo-logger-daily.h"

#define DATE_FORMAT "%Y-%m-%d"

G_DEFINE_TYPE(NeoLoggerDaily, neo_logger_daily, NEO_TYPE_LOGGER)

struct _NeoLoggerDailyPrivate
{
   GIOChannel *channel;
   gchar *filename;
   struct tm last_event;
};

enum
{
   PROP_0,
   PROP_FILENAME,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

NeoLogger *
neo_logger_daily_new (const gchar *filename)
{
   return g_object_new(NEO_TYPE_LOGGER_DAILY,
                       "filename", filename,
                       NULL);
}

const gchar *
neo_logger_daily_get_filename (NeoLoggerDaily *daily)
{
   g_return_val_if_fail(NEO_IS_LOGGER_DAILY(daily), NULL);
   return daily->priv->filename;
}

static void
neo_logger_daily_rotate (NeoLoggerDaily *daily,
                         GTimeVal       *as_day)
{
   NeoLoggerDailyPrivate *priv = daily->priv;
   GDateTime *dt;
   gchar *filename = NULL;
   gchar *date;
   guint counter = 0;

   dt = g_date_time_new_from_timeval_local(as_day);
   date = g_date_time_format(dt, DATE_FORMAT);

   do {
      g_free(filename);
      if (counter) {
         filename = g_strdup_printf("%s-%s.%d", priv->filename, date, counter);
      } else {
         filename = g_strdup_printf("%s-%s", priv->filename, date);
      }
      counter++;
   } while (g_file_test(filename, G_FILE_TEST_EXISTS));

   if (g_file_test(priv->filename, G_FILE_TEST_EXISTS)) {
      g_rename(priv->filename, filename);
   }

   g_free(filename);
   g_free(date);

   if (priv->channel) {
      g_io_channel_unref(priv->channel);
      priv->channel = NULL;
   }

   priv->channel = g_io_channel_new_file(priv->filename, "a", NULL);
   g_io_channel_set_close_on_unref(priv->channel, TRUE);
   g_io_channel_set_encoding(priv->channel, NULL, NULL);
   g_io_channel_set_buffered(priv->channel, FALSE);
}

static void
neo_logger_daily_set_filename (NeoLoggerDaily *daily,
                               const gchar    *filename)
{
   NeoLoggerDailyPrivate *priv;
   GTimeVal tv;

   g_return_if_fail(NEO_IS_LOGGER_DAILY(daily));

   priv = daily->priv;

   if ((priv->filename = g_strdup(filename))) {
      g_get_current_time(&tv);
      neo_logger_daily_rotate(daily, &tv);
   }
}

static void
neo_logger_daily_log (NeoLogger      *logger,
                      GTimeVal       *event_time,
                      const gchar    *log_domain,
                      const gchar    *hostname,
                      GPid            pid,
                      guint           tid,
                      GLogLevelFlags  log_level,
                      const gchar    *message,
                      const gchar    *formatted)
{
   NeoLoggerDaily *daily = (NeoLoggerDaily *)logger;
   GTimeVal rotate;
   time_t t = event_time->tv_sec;
   struct tm tt;

   localtime_r(&t, &tt);

   if (daily->priv->last_event.tm_year) {
      if ((tt.tm_mday != daily->priv->last_event.tm_mday) ||
          (tt.tm_mon != daily->priv->last_event.tm_mon) ||
          (tt.tm_year != daily->priv->last_event.tm_year)) {
         rotate.tv_sec = mktime(&daily->priv->last_event);
         rotate.tv_usec = 0;
         neo_logger_daily_rotate(daily, &rotate);
      }
   }

   daily->priv->last_event = tt;

   g_io_channel_write_chars(daily->priv->channel,
                            formatted,
                            strlen(formatted),
                            NULL,
                            NULL);
}

static void
neo_logger_daily_finalize (GObject *object)
{
   NeoLoggerDailyPrivate *priv = NEO_LOGGER_DAILY(object)->priv;
   g_free(priv->filename);
   priv->filename = NULL;
   if (priv->channel) {
      g_io_channel_unref(priv->channel);
      priv->channel = NULL;
   }
   G_OBJECT_CLASS(neo_logger_daily_parent_class)->finalize(object);
}

static void
neo_logger_daily_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
   NeoLoggerDaily *daily = NEO_LOGGER_DAILY(object);

   switch (prop_id) {
   case PROP_FILENAME:
      g_value_set_string(value, neo_logger_daily_get_filename(daily));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_logger_daily_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
   NeoLoggerDaily *daily = NEO_LOGGER_DAILY(object);

   switch (prop_id) {
   case PROP_FILENAME:
      neo_logger_daily_set_filename(daily, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_logger_daily_class_init (NeoLoggerDailyClass *klass)
{
   GObjectClass *object_class;
   NeoLoggerClass *logger_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = neo_logger_daily_finalize;
   object_class->get_property = neo_logger_daily_get_property;
   object_class->set_property = neo_logger_daily_set_property;
   g_type_class_add_private(object_class, sizeof(NeoLoggerDailyPrivate));

   logger_class = NEO_LOGGER_CLASS(klass);
   logger_class->log = neo_logger_daily_log;

   gParamSpecs[PROP_FILENAME] =
      g_param_spec_string("filename",
                          _("Filename"),
                          _("The filename to log to."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_FILENAME,
                                   gParamSpecs[PROP_FILENAME]);
}

static void
neo_logger_daily_init (NeoLoggerDaily *daily)
{
   daily->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(daily,
                                  NEO_TYPE_LOGGER_DAILY,
                                  NeoLoggerDailyPrivate);
}
