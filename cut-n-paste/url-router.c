/* url-router.c
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
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
 */

#include "url-router.h"

typedef struct
{
   const gchar      *key;
   gboolean          catchall;
   UrlRouterHandler  handler;
   gpointer          handler_data;
} UrlNodeData;

UrlNodeData *
url_node_data_new (const gchar      *key,
                   gboolean          catchall,
                   UrlRouterHandler  handler,
                   gpointer          user_data)
{
   UrlNodeData *data;
   GQuark qkey;

   data = g_new0(UrlNodeData, 1);
   qkey = g_quark_from_string(key);
   data->key = g_quark_to_string(qkey);
   data->catchall = catchall;
   data->handler = handler;
   data->handler_data = user_data;

   return data;
}

static gboolean
url_node_data_free_func (GNode    *node,
                         gpointer  user_data)
{
   g_free(node->data);
   return FALSE;
}

static gboolean
url_node_data_match (UrlNodeData *data,
                     const gchar *key,
                     gsize        key_len)
{
   const gchar *key2;

   for (key2 = data->key; key_len && *key2; key2++, key++, key_len--) {
      if (*key2 != *key) {
         return FALSE;
      }
   }

   return (key_len == 0) && (*key2 == '\0');
}

UrlRouter *
url_router_new (void)
{
   UrlNodeData *data;

   data = url_node_data_new("", FALSE, NULL, NULL);
   return g_node_new(data);
}

void
url_router_add_handler (UrlRouter        *router,
                        const gchar      *signature,
                        UrlRouterHandler  handler,
                        gpointer          user_data)
{
   UrlNodeData *data;
   gchar **parts;
   GNode *parent = NULL;
   GNode *node;
   guint i;

   g_return_if_fail(router);
   g_return_if_fail(signature);
   g_return_if_fail(*signature == '/');
   g_return_if_fail(handler);

   parent = (GNode *)router;

   parts = g_strsplit(signature, "/", 0);

   for (i = 1; parts[i]; i++) {
      for (node = parent->children; node; node = node->next) {
         data = node->data;
         if (!g_strcmp0(data->key, parts[i])) {
            parent = node;
            break;
         } else if ((*data->key == ':') && (*(parts[i]) == ':')) {
            g_warning("Wildcard params starting with : must match names!");
            parent = node;
            break;
         }
      }
      if (!node) {
         data = url_node_data_new(parts[i],
                                  *(parts[i]) == ':',
                                  (!parts[i+1]) ? handler : NULL,
                                  (!parts[i+1]) ? user_data : NULL);
         parent = g_node_append_data(parent, data);
      }
   }

   g_strfreev(parts);
}

gboolean
url_router_route (UrlRouter         *router,
                  SoupServer        *server,
                  SoupMessage       *message,
                  const gchar       *path,
                  GHashTable        *query,
                  SoupClientContext *client)
{
   UrlNodeData *data;
   const gchar *cur;
   const gchar *end;
   GHashTable *params = NULL;
   gboolean ret = FALSE;
   GNode *node = (GNode *)router;
   gchar *part;

   g_return_val_if_fail(router, FALSE);
   g_return_val_if_fail(SOUP_IS_SERVER(server), FALSE);
   g_return_val_if_fail(SOUP_IS_MESSAGE(message), FALSE);
   g_return_val_if_fail(path, FALSE);
   g_return_val_if_fail(client, FALSE);

   cur = path;
   if (*path != '/') {
      return FALSE;
   }
   cur++;

   while (node && *cur) {
      if (!(node = node->children)) {
         break;
      }

      end = cur;
      while (*end && *end != '/') end++;

      for (; node; node = node->next) {
         data = node->data;
         if (data->catchall) {
            if (!params) {
               params = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              NULL, g_free);
            }
            part = g_strndup(cur, end - cur);
            g_hash_table_insert(params, (gchar *)data->key + 1, part);
            break;
         } else if (url_node_data_match(data, cur, end - cur)) {
            break;
         }
      }

      cur = end;
      if (*cur == '/') cur++;
   }

   if (node && (data = node->data) && data->handler) {
      data->handler(router,
                    server,
                    message,
                    path,
                    params,
                    query,
                    client,
                    data->handler_data);
      ret = TRUE;
   }

   if (params) {
      g_hash_table_unref(params);
   }

   return ret;
}

void
url_router_free (UrlRouter *router)
{
   g_node_traverse(router,
                   G_POST_ORDER,
                   G_TRAVERSE_ALL,
                   -1,
                   url_node_data_free_func,
                   NULL);
   g_node_destroy(router);
}
