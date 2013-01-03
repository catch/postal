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

#include <push-glib.h>
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
   guint64 aps_notified;
   guint64 c2dm_notified;
   guint64 gcm_notified;
};

enum
{
   PROP_0,
   PROP_DEVICES_ADDED,
   PROP_DEVICES_REMOVED,
   PROP_DEVICES_UPDATED,
   PROP_APS_NOTIFIED,
   PROP_C2DM_NOTIFIED,
   PROP_GCM_NOTIFIED,
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

void
postal_metrics_device_notified (PostalMetrics *metrics,
                                PostalDevice  *device)
{
   PostalMetricsPrivate *priv;
   PostalDeviceType device_type;

   g_return_if_fail(POSTAL_IS_METRICS(metrics));

   priv = metrics->priv;

   device_type = postal_device_get_device_type(device);

   switch (device_type) {
   case POSTAL_DEVICE_APS:
      __sync_fetch_and_add(&priv->aps_notified, 1);
      break;
   case POSTAL_DEVICE_C2DM:
      __sync_fetch_and_add(&priv->c2dm_notified, 1);
      break;
   case POSTAL_DEVICE_GCM:
      __sync_fetch_and_add(&priv->gcm_notified, 1);
      break;
   default:
      break;
   }

#ifdef ENABLE_REDIS
   postal_redis_device_notified(metrics->priv->redis, device);
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
   case PROP_APS_NOTIFIED:
      g_value_set_uint64(value, metrics->priv->aps_notified);
      break;
   case PROP_C2DM_NOTIFIED:
      g_value_set_uint64(value, metrics->priv->c2dm_notified);
      break;
   case PROP_DEVICES_ADDED:
      g_value_set_uint64(value, metrics->priv->devices_added);
      break;
   case PROP_DEVICES_REMOVED:
      g_value_set_uint64(value, metrics->priv->devices_removed);
      break;
   case PROP_DEVICES_UPDATED:
      g_value_set_uint64(value, metrics->priv->devices_updated);
      break;
   case PROP_GCM_NOTIFIED:
      g_value_set_uint64(value, metrics->priv->gcm_notified);
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

   gParamSpecs[PROP_APS_NOTIFIED] =
      g_param_spec_uint64("aps-notified",
                          _("Gcm Notified"),
                          _("Number of APS devices notified."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_APS_NOTIFIED,
                                   gParamSpecs[PROP_APS_NOTIFIED]);

   gParamSpecs[PROP_C2DM_NOTIFIED] =
      g_param_spec_uint64("c2dm-notified",
                          _("C2dm Notified"),
                          _("Number of C2DM devices notified."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_C2DM_NOTIFIED,
                                   gParamSpecs[PROP_C2DM_NOTIFIED]);

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

   gParamSpecs[PROP_GCM_NOTIFIED] =
      g_param_spec_uint64("gcm-notified",
                          _("Gcm Notified"),
                          _("Number of GCM devices notified."),
                          0,
                          G_MAXUINT64,
                          0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_GCM_NOTIFIED,
                                   gParamSpecs[PROP_GCM_NOTIFIED]);
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
