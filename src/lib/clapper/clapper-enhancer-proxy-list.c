/* Clapper Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * ClapperEnhancerProxyList:
 *
 * A list of enhancer proxies.
 *
 * Since: 0.10
 */

#include <gio/gio.h>

#include "clapper-basic-functions.h"
#include "clapper-enhancer-proxy-list-private.h"
#include "clapper-enhancer-proxy-private.h"

#define GST_CAT_DEFAULT clapper_enhancer_proxy_list_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperEnhancerProxyList
{
  GstObject parent;

  GPtrArray *proxies;
};

enum
{
  PROP_0,
  PROP_N_PROXIES,
  PROP_LAST
};

static void clapper_enhancer_proxy_list_model_iface_init (GListModelInterface *iface);

#define parent_class clapper_enhancer_proxy_list_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperEnhancerProxyList, clapper_enhancer_proxy_list, GST_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, clapper_enhancer_proxy_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GType
clapper_enhancer_proxy_list_model_get_item_type (GListModel *model)
{
  return CLAPPER_TYPE_ENHANCER_PROXY;
}

static guint
clapper_enhancer_proxy_list_model_get_n_items (GListModel *model)
{
  return CLAPPER_ENHANCER_PROXY_LIST_CAST (model)->proxies->len;
}

static gpointer
clapper_enhancer_proxy_list_model_get_item (GListModel *model, guint index)
{
  ClapperEnhancerProxyList *self = CLAPPER_ENHANCER_PROXY_LIST_CAST (model);
  ClapperEnhancerProxy *proxy = NULL;

  if (G_LIKELY (index < self->proxies->len))
    proxy = gst_object_ref (g_ptr_array_index (self->proxies, index));

  return proxy;
}

static void
clapper_enhancer_proxy_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = clapper_enhancer_proxy_list_model_get_item_type;
  iface->get_n_items = clapper_enhancer_proxy_list_model_get_n_items;
  iface->get_item = clapper_enhancer_proxy_list_model_get_item;
}

/*
 * clapper_enhancer_proxy_list_new_named:
 * @name: (nullable): name of the #GstObject
 *
 * Returns: (transfer full): a new #ClapperEnhancerProxyList instance
 */
ClapperEnhancerProxyList *
clapper_enhancer_proxy_list_new_named (const gchar *name)
{
  ClapperEnhancerProxyList *list;

  list = g_object_new (CLAPPER_TYPE_ENHANCER_PROXY_LIST,
      "name", name, NULL);
  gst_object_ref_sink (list);

  return list;
}

void
clapper_enhancer_proxy_list_take_proxy (ClapperEnhancerProxyList *self, ClapperEnhancerProxy *proxy)
{
  g_ptr_array_add (self->proxies, proxy);
  gst_object_set_parent (GST_OBJECT_CAST (proxy), GST_OBJECT_CAST (self));
}

/*
 * clapper_enhancer_proxy_list_fill_from_global_proxies:
 *
 * Fill list with unconfigured proxies from global proxies list.
 */
void
clapper_enhancer_proxy_list_fill_from_global_proxies (ClapperEnhancerProxyList *self)
{
  ClapperEnhancerProxyList *global_list = clapper_get_global_enhancer_proxies ();
  static guint _list_id = 0;
  guint i;

  for (i = 0; i < global_list->proxies->len; ++i) {
    ClapperEnhancerProxy *proxy, *proxy_copy;
    gchar obj_name[64];

    proxy = clapper_enhancer_proxy_list_peek_proxy (global_list, i);

    /* Name newly created proxy, very useful for debugging. Keep index per
     * list, so it will be the same as the player that proxy belongs to. */
    g_snprintf (obj_name, sizeof (obj_name), "%s-proxy%u",
        clapper_enhancer_proxy_get_friendly_name (proxy), _list_id);
    proxy_copy = clapper_enhancer_proxy_copy (proxy, obj_name);

    clapper_enhancer_proxy_list_take_proxy (self, proxy_copy);
  }
  _list_id++;
}

static gint
_sort_values_by_name (ClapperEnhancerProxy *proxy_a, ClapperEnhancerProxy *proxy_b)
{
  return g_ascii_strcasecmp (
      clapper_enhancer_proxy_get_friendly_name (proxy_a),
      clapper_enhancer_proxy_get_friendly_name (proxy_b));
}

/*
 * clapper_enhancer_proxy_list_sort:
 *
 * Sort all list elements by enhancer friendly name.
 */
void
clapper_enhancer_proxy_list_sort (ClapperEnhancerProxyList *self)
{
  g_ptr_array_sort_values (self->proxies, (GCompareFunc) _sort_values_by_name);
}

/*
 * clapper_enhancer_proxy_list_has_proxy_with_interface:
 * @iface_type: an interface #GType
 *
 * Check if any enhancer implementing given interface type is available.
 *
 * Returns: whether any enhancer proxy was found.
 */
gboolean
clapper_enhancer_proxy_list_has_proxy_with_interface (ClapperEnhancerProxyList *self, GType iface_type)
{
  guint i;

  for (i = 0; i < self->proxies->len; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (self, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, iface_type))
      return TRUE;
  }

  return FALSE;
}

/**
 * clapper_enhancer_proxy_list_get_proxy:
 * @list: a #ClapperEnhancerProxyList
 * @index: an enhancer proxy index
 *
 * Get the #ClapperEnhancerProxy at index.
 *
 * This behaves the same as [method@Gio.ListModel.get_item], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: (transfer full) (nullable): The #ClapperEnhancerProxy at @index.
 *
 * Since: 0.10
 */
ClapperEnhancerProxy *
clapper_enhancer_proxy_list_get_proxy (ClapperEnhancerProxyList *self, guint index)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY_LIST (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * clapper_enhancer_proxy_list_peek_proxy: (skip)
 * @list: a #ClapperEnhancerProxyList
 * @index: an enhancer proxy index
 *
 * Get the #ClapperEnhancerProxy at index.
 *
 * Similar to [method@Clapper.EnhancerProxyList.get_proxy], but does not take
 * a new reference on proxy.
 *
 * Proxies in a list are only removed when a [class@Clapper.Player] instance
 * they originate from is destroyed, so do not use returned object afterwards
 * unless you take an additional reference on it.
 *
 * Returns: (transfer none) (nullable): The #ClapperEnhancerProxy at @index.
 *
 * Since: 0.10
 */
ClapperEnhancerProxy *
clapper_enhancer_proxy_list_peek_proxy (ClapperEnhancerProxyList *self, guint index)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY_LIST (self), NULL);

  return g_ptr_array_index (self->proxies, index);
}

/**
 * clapper_enhancer_proxy_list_get_proxy_by_module:
 * @list: a #ClapperEnhancerProxyList
 * @module_name: an enhancer module name
 *
 * Get the #ClapperEnhancerProxy by module name as defined in its plugin file.
 *
 * A convenience function to find a #ClapperEnhancerProxy by its unique
 * module name in the list.
 *
 * Returns: (transfer full) (nullable): The #ClapperEnhancerProxy with requested module name.
 *
 * Since: 0.10
 */
ClapperEnhancerProxy *
clapper_enhancer_proxy_list_get_proxy_by_module (ClapperEnhancerProxyList *self, const gchar *module_name)
{
  guint i;

  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY_LIST (self), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  for (i = 0; i < self->proxies->len; ++i) {
    ClapperEnhancerProxy *proxy = g_ptr_array_index (self->proxies, i);

    if (strcmp (clapper_enhancer_proxy_get_module_name (proxy), module_name) == 0)
      return gst_object_ref (proxy);
  }

  return NULL;
}

/**
 * clapper_enhancer_proxy_list_get_n_proxies:
 * @list: a #ClapperEnhancerProxyList
 *
 * Get the number of proxies in #ClapperEnhancerProxyList.
 *
 * This behaves the same as [method@Gio.ListModel.get_n_items], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: The number of proxies in #ClapperEnhancerProxyList.
 *
 * Since: 0.10
 */
guint
clapper_enhancer_proxy_list_get_n_proxies (ClapperEnhancerProxyList *self)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY_LIST (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

static void
_proxy_remove_func (ClapperEnhancerProxy *proxy)
{
  gst_object_unparent (GST_OBJECT_CAST (proxy));
  gst_object_unref (proxy);
}

static void
clapper_enhancer_proxy_list_init (ClapperEnhancerProxyList *self)
{
  self->proxies = g_ptr_array_new_with_free_func ((GDestroyNotify) _proxy_remove_func);
}

static void
clapper_enhancer_proxy_list_finalize (GObject *object)
{
  ClapperEnhancerProxyList *self = CLAPPER_ENHANCER_PROXY_LIST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_ptr_array_unref (self->proxies);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_enhancer_proxy_list_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperEnhancerProxyList *self = CLAPPER_ENHANCER_PROXY_LIST_CAST (object);

  switch (prop_id) {
    case PROP_N_PROXIES:
      g_value_set_uint (value, clapper_enhancer_proxy_list_get_n_proxies (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_enhancer_proxy_list_class_init (ClapperEnhancerProxyListClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancerproxylist", 0,
      "Clapper Enhancer Proxy List");

  gobject_class->get_property = clapper_enhancer_proxy_list_get_property;
  gobject_class->finalize = clapper_enhancer_proxy_list_finalize;

  /**
   * ClapperEnhancerProxyList:n-proxies:
   *
   * Number of proxies in the list.
   *
   * Since: 0.10
   */
  param_specs[PROP_N_PROXIES] = g_param_spec_uint ("n-proxies",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
