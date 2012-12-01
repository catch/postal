/* neo-service-base.c
 *
 * Copyright (C) 2012 Catch.com
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Christian Hergert <christian@catch.com>
 */

#include <glib/gi18n.h>

#include "neo-debug.h"
#include "neo-service-base.h"

struct _NeoServiceBasePrivate
{
   GHashTable *children;
   gchar      *name;
   NeoService *parent;
   gboolean    is_running;
};

enum
{
   PROP_0,
   PROP_IS_RUNNING,
   PROP_NAME,
   PROP_PARENT,
   LAST_PROP
};

enum
{
   START,
   STOP,
   LAST_SIGNAL
};

static void neo_service_init (NeoServiceIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(NeoServiceBase,
                                 neo_service_base,
                                 G_TYPE_OBJECT,
                                 G_IMPLEMENT_INTERFACE(NEO_TYPE_SERVICE,
                                                       neo_service_init))

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];

gboolean
neo_service_base_get_is_running (NeoService *service)
{
   NeoServiceBase *base = (NeoServiceBase *)service;
   g_return_val_if_fail(NEO_IS_SERVICE_BASE(base), FALSE);
   return base->priv->is_running;
}

const gchar *
neo_service_base_get_name (NeoService *service)
{
   NeoServiceBase *base = (NeoServiceBase *)service;
   g_return_val_if_fail(NEO_IS_SERVICE_BASE(base), NULL);
   return base->priv->name;
}

void
neo_service_base_set_name (NeoService  *service,
                           const gchar *name)
{
   NeoServiceBase *base = (NeoServiceBase *)service;
   NEO_ENTRY;
   g_return_if_fail(NEO_IS_SERVICE_BASE(base));
   g_free(base->priv->name);
   base->priv->name = g_strdup(name);
   g_object_notify_by_pspec(G_OBJECT(service), gParamSpecs[PROP_NAME]);
   NEO_EXIT;
}

NeoService *
neo_service_base_get_parent (NeoService *service)
{
   NeoServiceBase *base = (NeoServiceBase *)service;
   g_return_val_if_fail(NEO_IS_SERVICE_BASE(base), NULL);
   return base->priv->parent;
}

void
neo_service_base_set_parent (NeoService *service,
                             NeoService *parent)
{
   NeoServiceBasePrivate *priv;
   NeoServiceBase *base = (NeoServiceBase *)service;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE_BASE(base));

   priv = base->priv;

   if (priv->parent) {
      g_object_remove_weak_pointer(G_OBJECT(priv->parent),
                                   (gpointer *)&priv->parent);
   }

   priv->parent = parent;

   if (priv->parent) {
      g_object_add_weak_pointer(G_OBJECT(priv->parent),
                                (gpointer *)&priv->parent);
   }

   g_object_notify_by_pspec(G_OBJECT(base), gParamSpecs[PROP_PARENT]);

   NEO_EXIT;
}

void
neo_service_base_add_child (NeoService *service,
                            NeoService *child)
{
   NeoServiceBasePrivate *priv;
   NeoServiceBase *base = (NeoServiceBase *)service;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE_BASE(base));
   g_return_if_fail(NEO_IS_SERVICE(child));

   priv = base->priv;

   g_hash_table_insert(priv->children,
                       g_strdup(neo_service_get_name(child)),
                       g_object_ref(child));
   neo_service_set_parent(child, service);

   NEO_EXIT;
}

NeoService *
neo_service_base_get_child (NeoService  *service,
                            const gchar *name)
{
   NeoServiceBase *base = (NeoServiceBase *)service;
   g_return_val_if_fail(NEO_IS_SERVICE_BASE(base), NULL);
   g_return_val_if_fail(name, NULL);
   return g_hash_table_lookup(base->priv->children, name);
}

void
neo_service_base_start (NeoService *service,
                        GKeyFile   *config)
{
   NeoServiceBasePrivate *priv;
   NeoServiceBase *base = (NeoServiceBase *)service;
   GHashTableIter iter;
   gpointer value;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE_BASE(base));

   priv = base->priv;

   if (priv->is_running) {
      /*
       * TODO: Get service path. /parent.../name
       */
      g_warning("Service \"%s\" is already running!", priv->name);
      return;
   }

   priv->is_running = TRUE;

   g_signal_emit(base, gSignals[START], 0, config);

   if (priv->is_running) {
      g_hash_table_iter_init(&iter, priv->children);
      while (g_hash_table_iter_next(&iter, NULL, &value)) {
         neo_service_start(value, config);
      }
   }

   NEO_EXIT;
}

static void
neo_service_base_stop (NeoService *service)
{
   NeoServiceBasePrivate *priv;
   NeoServiceBase *base = (NeoServiceBase *)service;
   GHashTableIter iter;
   gpointer value;

   NEO_ENTRY;

   g_return_if_fail(NEO_IS_SERVICE_BASE(base));

   priv = base->priv;

   if (!priv->is_running) {
      /*
       * TODO: Get service path: /parent.../name.
       */
      g_warning("Service \"%s\" is already running!", priv->name);
      return;
   }

   priv->is_running = FALSE;

   g_signal_emit(base, gSignals[STOP], 0);

   g_hash_table_iter_init(&iter, priv->children);
   while (g_hash_table_iter_next(&iter, NULL, &value)) {
      neo_service_stop(value);
   }

   NEO_EXIT;
}

static void
neo_service_base_finalize (GObject *object)
{
   NeoServiceBasePrivate *priv;

   priv = NEO_SERVICE_BASE(object)->priv;

   g_free(priv->name);
   priv->name = NULL;

   g_hash_table_unref(priv->children);
   priv->children = NULL;

   if (priv->parent) {
      g_object_remove_weak_pointer(G_OBJECT(priv->parent),
                                   (gpointer *)&priv->parent);
      priv->parent = NULL;
   }

   G_OBJECT_CLASS(neo_service_base_parent_class)->finalize(object);
}

static void
neo_service_base_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
   NeoService *service = NEO_SERVICE(object);

   switch (prop_id) {
   case PROP_IS_RUNNING:
      g_value_set_boolean(value, neo_service_base_get_is_running(service));
      break;
   case PROP_NAME:
      g_value_set_string(value, neo_service_base_get_name(service));
      break;
   case PROP_PARENT:
      g_value_set_object(value, neo_service_base_get_parent(service));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_service_base_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
   NeoService *service = NEO_SERVICE(object);

   switch (prop_id) {
   case PROP_NAME:
      neo_service_base_set_name(service, g_value_get_string(value));
      break;
   case PROP_PARENT:
      neo_service_base_set_parent(service, g_value_get_object(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
neo_service_base_class_init (NeoServiceBaseClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = neo_service_base_finalize;
   object_class->get_property = neo_service_base_get_property;
   object_class->set_property = neo_service_base_set_property;
   g_type_class_add_private(object_class, sizeof(NeoServiceBasePrivate));

   gParamSpecs[PROP_IS_RUNNING] =
      g_param_spec_boolean("is-running",
                           _("Is Running"),
                           _("If the service is running."),
                           FALSE,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_IS_RUNNING,
                                   gParamSpecs[PROP_IS_RUNNING]);

   gParamSpecs[PROP_NAME] =
      g_param_spec_string("name",
                          _("Name"),
                          _("The service name."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_NAME,
                                   gParamSpecs[PROP_NAME]);

   gParamSpecs[PROP_PARENT] =
      g_param_spec_object("parent",
                          _("Parent"),
                          _("The parent service."),
                          NEO_TYPE_SERVICE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
   g_object_class_install_property(object_class, PROP_PARENT,
                                   gParamSpecs[PROP_PARENT]);

   gSignals[START] = g_signal_new("start",
                                  NEO_TYPE_SERVICE_BASE,
                                  G_SIGNAL_RUN_LAST,
                                  G_STRUCT_OFFSET(NeoServiceBaseClass,
                                                  start),
                                  NULL,
                                  NULL,
                                  g_cclosure_marshal_generic,
                                  G_TYPE_NONE,
                                  1,
                                  G_TYPE_KEY_FILE);

   gSignals[STOP] = g_signal_new("stop",
                                 NEO_TYPE_SERVICE_BASE,
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET(NeoServiceBaseClass,
                                                 stop),
                                 NULL,
                                 NULL,
                                 g_cclosure_marshal_generic,
                                 G_TYPE_NONE,
                                 0);
}

static void
neo_service_base_init (NeoServiceBase *base)
{
   base->priv = G_TYPE_INSTANCE_GET_PRIVATE(base,
                                            NEO_TYPE_SERVICE_BASE,
                                            NeoServiceBasePrivate);
   base->priv->children = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);
}

static void
neo_service_init (NeoServiceIface *iface)
{
   iface->add_child = neo_service_base_add_child;
   iface->get_child = neo_service_base_get_child;
   iface->get_is_running = neo_service_base_get_is_running;
   iface->get_name = neo_service_base_get_name;
   iface->get_parent = neo_service_base_get_parent;
   iface->set_parent = neo_service_base_set_parent;
   iface->start = neo_service_base_start;
   iface->stop = neo_service_base_stop;
}
