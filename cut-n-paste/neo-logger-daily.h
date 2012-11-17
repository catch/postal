#ifndef NEO_LOGGER_DAILY_H
#define NEO_LOGGER_DAILY_H

#include "neo-logger.h"

G_BEGIN_DECLS

#define NEO_TYPE_LOGGER_DAILY            (neo_logger_daily_get_type())
#define NEO_LOGGER_DAILY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_LOGGER_DAILY, NeoLoggerDaily))
#define NEO_LOGGER_DAILY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEO_TYPE_LOGGER_DAILY, NeoLoggerDaily const))
#define NEO_LOGGER_DAILY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NEO_TYPE_LOGGER_DAILY, NeoLoggerDailyClass))
#define NEO_IS_LOGGER_DAILY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEO_TYPE_LOGGER_DAILY))
#define NEO_IS_LOGGER_DAILY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NEO_TYPE_LOGGER_DAILY))
#define NEO_LOGGER_DAILY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NEO_TYPE_LOGGER_DAILY, NeoLoggerDailyClass))

typedef struct _NeoLoggerDaily        NeoLoggerDaily;
typedef struct _NeoLoggerDailyClass   NeoLoggerDailyClass;
typedef struct _NeoLoggerDailyPrivate NeoLoggerDailyPrivate;

struct _NeoLoggerDaily
{
   NeoLogger parent;

   /*< private >*/
   NeoLoggerDailyPrivate *priv;
};

struct _NeoLoggerDailyClass
{
   NeoLoggerClass parent_class;
};

GType      neo_logger_daily_get_type (void) G_GNUC_CONST;
NeoLogger *neo_logger_daily_new      (const gchar *filename);

G_END_DECLS

#endif /* NEO_LOGGER_DAILY_H */
