/* neo-service.c
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

#include "neo-debug.h"
#include "neo-service.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "neo"

G_DEFINE_TYPE(NeoService, neo_service, G_TYPE_OBJECT)

struct _NeoServicePrivate
{
   GKeyFile *config;
   gboolean  is_running;
   gchar    *name;
};

enum
{
   PROP_0,
   PROP_CONFIG,
   PROP_NAME,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

gboolean
neo_service_get_is_running (NeoService *service)
{
   g_return_val_if_fail(NEO_IS_SERVICE(service), FALSE);
   return service->priv->is_running;
}

GKeyFile *
neo_service_get_config (NeoService *service)
{
   g_return_val_if_fail(NEO_IS_SERVICE(service), NULL);
   return service->priv->config;
}

void
neo_service_set_config (NeoService *service,
                        GKeyFile   *config)
{
   NeoServicePrivate *priv;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));

   priv = service->priv;

   if (priv->config) {
      g_key_file_unref(priv->config);
      priv->config = NULL;
   }

   if (config) {
      priv->config = g_key_file_ref(config);
   }

   g_object_notify_by_pspec(G_OBJECT(service), gParamSpecs[PROP_CONFIG]);

   NEO_EXIT;
}

const gchar *
neo_service_get_name (NeoService *service)
{
   g_return_val_if_fail(NEO_IS_SERVICE(service), NULL);
   return service->priv->name;
}

void
neo_service_set_name (NeoService  *service,
                      const gchar *name)
{
   g_return_if_fail(NEO_IS_SERVICE(service));
   g_free(service->priv->name);
   service->priv->name = g_strdup(name);
   g_object_notify_by_pspec(G_OBJECT(service), gParamSpecs[PROP_NAME]);
}

void
neo_service_start (NeoService *service)
{
   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));

   if (neo_service_get_is_running(service)) {
      g_warning("Service \"%s\" is already running!", service->priv->name);
      NEO_EXIT;
   }

   service->priv->is_running = TRUE;

   if (NEO_SERVICE_GET_CLASS(service)->start) {
      NEO_SERVICE_GET_CLASS(service)->start(service);
   }

   NEO_EXIT;
}

void
neo_service_stop (NeoService *service)
{
   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE(service));

   if (NEO_SERVICE_GET_CLASS(service)->stop) {
      NEO_SERVICE_GET_CLASS(service)->stop(service);
   }

   service->priv->is_running = FALSE;

   NEO_EXIT;
}

static void
neo_service_dispose (GObject *object)
{
   NEO_ENTRY;

   if (neo_service_get_is_running(NEO_SERVICE(object))) {
      neo_service_stop(NEO_SERVICE(object));
   }

   if (G_OBJECT_CLASS(neo_service_parent_class)->dispose) {
      G_OBJECT_CLASS(neo_service_parent_class)->dispose(object);
   }

   NEO_EXIT;
}

static void
neo_service_finalize (GObject *object)
{
   NeoServicePrivate *priv;

   NEO_ENTRY;

   priv = NEO_SERVICE(object)->priv;

   if (priv->config) {
      g_key_file_unref(priv->config);
      priv->config = NULL;
   }

   G_OBJECT_CLASS(neo_service_parent_class)->finalize(object);

   NEO_EXIT;
}

static void
neo_service_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
   NeoService *service = NEO_SERVICE(object);

   switch (prop_id) {
   case PROP_CONFIG:
      g_value_set_boxed(value, neo_service_get_config(service));
      break;
   case PROP_NAME:
      g_value_set_string(value, neo_service_get_name(service));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_service_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
   NeoService *service = NEO_SERVICE(object);

   switch (prop_id) {
   case PROP_CONFIG:
      neo_service_set_config(service, g_value_get_boxed(value));
      break;
   case PROP_NAME:
      neo_service_set_name(service, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_service_class_init (NeoServiceClass *klass)
{
   GObjectClass *object_class;

   NEO_ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->dispose = neo_service_dispose;
   object_class->finalize = neo_service_finalize;
   object_class->get_property = neo_service_get_property;
   object_class->set_property = neo_service_set_property;
   g_type_class_add_private(object_class, sizeof(NeoServicePrivate));

   gParamSpecs[PROP_CONFIG] =
      g_param_spec_boxed("config",
                          _("Config"),
                          _("The configuration for the service."),
                          G_TYPE_KEY_FILE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_CONFIG,
                                   gParamSpecs[PROP_CONFIG]);

   gParamSpecs[PROP_NAME] =
      g_param_spec_string("name",
                          _("Name"),
                          _("The name of the service."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_NAME,
                                   gParamSpecs[PROP_NAME]);

   NEO_EXIT;
}

static void
neo_service_init (NeoService *service)
{
   NEO_ENTRY;
   service->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(service,
                                  NEO_TYPE_SERVICE,
                                  NeoServicePrivate);
   NEO_EXIT;
}
