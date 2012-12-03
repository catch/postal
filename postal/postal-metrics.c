/* postal-metrics.c
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

#include <glib/gi18n.h>

#include "postal-debug.h"
#include "postal-metrics.h"
#ifdef ENABLE_REDIS
#include "postal-redis.h"
#endif

G_DEFINE_TYPE(PostalMetrics, postal_metrics, NEO_TYPE_SERVICE_BASE)

struct _PostalMetricsPrivate
{
#ifdef ENABLE_REDIS
   PostalRedis *redis;
#endif

   guint64 devices_added;
   guint64 devices_removed;
   guint64 devices_updated;
};

enum
{
   PROP_0,
   PROP_DEVICES_ADDED,
   PROP_DEVICES_REMOVED,
   PROP_DEVICES_UPDATED,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

PostalMetrics *
postal_metrics_new (void)
{
   return g_object_new(POSTAL_TYPE_METRICS,
                       "name", "metrics",
                       NULL);
}

guint64
postal_metrics_get_devices_added (PostalMetrics *metrics)
{
   g_return_val_if_fail(POSTAL_IS_METRICS(metrics), 0);
   return metrics->priv->devices_added;
}

guint64
postal_metrics_get_devices_removed (PostalMetrics *metrics)
{
   g_return_val_if_fail(POSTAL_IS_METRICS(metrics), 0);
   return metrics->priv->devices_removed;
}

guint64
postal_metrics_get_devices_updated (PostalMetrics *metrics)
{
   g_return_val_if_fail(POSTAL_IS_METRICS(metrics), 0);
   return metrics->priv->devices_updated;
}

void
postal_metrics_device_added (PostalMetrics *metrics,
                             PostalDevice  *device)
{
   g_return_if_fail(POSTAL_IS_METRICS(metrics));
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   __sync_fetch_and_add(&metrics->priv->devices_added, 1);
#ifdef ENABLE_REDIS
   postal_redis_device_added(metrics->priv->redis, device);
#endif
}

void
postal_metrics_device_removed (PostalMetrics *metrics,
                               PostalDevice  *device)
{
   g_return_if_fail(POSTAL_IS_METRICS(metrics));
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   __sync_fetch_and_add(&metrics->priv->devices_removed, 1);
#ifdef ENABLE_REDIS
   postal_redis_device_removed(metrics->priv->redis, device);
#endif
}

void
postal_metrics_device_updated (PostalMetrics *metrics,
                               PostalDevice  *device)
{
   g_return_if_fail(POSTAL_IS_METRICS(metrics));
   g_return_if_fail(POSTAL_IS_DEVICE(device));
   __sync_fetch_and_add(&metrics->priv->devices_updated, 1);
#ifdef ENABLE_REDIS
   postal_redis_device_updated(metrics->priv->redis, device);
#endif
}

static void
postal_metrics_start (NeoServiceBase *service_base,
                      GKeyFile       *config)
{
#ifdef ENABLE_REDIS
   PostalMetricsPrivate *priv;
   PostalMetrics *metrics = (PostalMetrics *)service;
   NeoService *service = (NeoService *)service_base;
   NeoService *redis;

   g_return_if_fail(NEO_IS_SERVICE(service));
   g_return_if_fail(POSTAL_IS_METRICS(metrics));

   priv = metrics->priv;

   if ((redis = neo_service_get_peer(service, "redis"))) {
      g_assert(POSTAL_IS_REDIS(redis));
      priv->redis = g_object_ref(redis);
   }
#endif
}

static void
postal_metrics_finalize (GObject *object)
{
#ifdef ENABLE_REDIS
   PostalMetricsPrivate *priv = POSTAL_METRICS(object)->priv;
   g_clear_object(&priv->redis);
#endif
   G_OBJECT_CLASS(postal_metrics_parent_class)->finalize(object);
}

static void
postal_metrics_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
   PostalMetrics *metrics = POSTAL_METRICS(object);

   switch (prop_id) {
   case PROP_DEVICES_ADDED:
      g_value_set_uint64(value, postal_metrics_get_devices_added(metrics));
      break;
   case PROP_DEVICES_REMOVED:
      g_value_set_uint64(value, postal_metrics_get_devices_removed(metrics));
      break;
   case PROP_DEVICES_UPDATED:
      g_value_set_uint64(value, postal_metrics_get_devices_updated(metrics));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
postal_metrics_class_init (PostalMetricsClass *klass)
{
   GObjectClass *object_class;
   NeoServiceBaseClass *base_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = postal_metrics_finalize;
   object_class->get_property = postal_metrics_get_property;
   g_type_class_add_private(object_class, sizeof(PostalMetricsPrivate));

   base_class = NEO_SERVICE_BASE_CLASS(klass);
   base_class->start = postal_metrics_start;

   gParamSpecs[PROP_DEVICES_ADDED] =
      g_param_spec_uint64("devices-added",
                          _("Devices Added"),
                          _("The number of devices added."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DEVICES_ADDED,
                                   gParamSpecs[PROP_DEVICES_ADDED]);

   gParamSpecs[PROP_DEVICES_REMOVED] =
      g_param_spec_uint64("devices-removed",
                          _("Devices Removed"),
                          _("The number of devices removed."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DEVICES_REMOVED,
                                   gParamSpecs[PROP_DEVICES_REMOVED]);

   gParamSpecs[PROP_DEVICES_UPDATED] =
      g_param_spec_uint64("devices-updated",
                          _("Devices Updated"),
                          _("The number of devices updated."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_DEVICES_UPDATED,
                                   gParamSpecs[PROP_DEVICES_UPDATED]);
}

static void
postal_metrics_init (PostalMetrics *metrics)
{
   ENTRY;
   metrics->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(metrics,
                                  POSTAL_TYPE_METRICS,
                                  PostalMetricsPrivate);
   EXIT;
}
