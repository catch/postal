/* postal-metrics.h
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
 *
 * Authors:
 *    Christian Hergert <christian@catch.com>
 */

#ifndef POSTAL_METRICS_H
#define POSTAL_METRICS_H

#include <neo.h>

#include "postal-device.h"

G_BEGIN_DECLS

#define POSTAL_TYPE_METRICS            (postal_metrics_get_type())
#define POSTAL_METRICS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_METRICS, PostalMetrics))
#define POSTAL_METRICS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), POSTAL_TYPE_METRICS, PostalMetrics const))
#define POSTAL_METRICS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  POSTAL_TYPE_METRICS, PostalMetricsClass))
#define POSTAL_IS_METRICS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), POSTAL_TYPE_METRICS))
#define POSTAL_IS_METRICS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  POSTAL_TYPE_METRICS))
#define POSTAL_METRICS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  POSTAL_TYPE_METRICS, PostalMetricsClass))

typedef struct _PostalMetrics        PostalMetrics;
typedef struct _PostalMetricsClass   PostalMetricsClass;
typedef struct _PostalMetricsPrivate PostalMetricsPrivate;

struct _PostalMetrics
{
   NeoServiceBase parent;

   /*< private >*/
   PostalMetricsPrivate *priv;
};

struct _PostalMetricsClass
{
   NeoServiceBaseClass parent_class;
};

PostalMetrics *postal_metrics_new                 (void);
GType          postal_metrics_get_type            (void) G_GNUC_CONST;
guint64        postal_metrics_get_devices_added   (PostalMetrics *metrics);
guint64        postal_metrics_get_devices_removed (PostalMetrics *metrics);
guint64        postal_metrics_get_devices_updated (PostalMetrics *metrics);
void           postal_metrics_device_added        (PostalMetrics *metrics,
                                                   PostalDevice  *device);
void           postal_metrics_device_removed      (PostalMetrics *metrics,
                                                   PostalDevice  *device);
void           postal_metrics_device_updated      (PostalMetrics *metrics,
                                                   PostalDevice  *device);
void           postal_metrics_device_notified     (PostalMetrics *metrics,
                                                   PostalDevice  *device);

G_END_DECLS

#endif /* POSTAL_METRICS_H */
