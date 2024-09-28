/*
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * ClapperAddonsLoader:
 *
 * An optional Addons loader feature to be added to the player.
 *
 * #ClapperAddonsLoader is a feature that wraps around #PeasEngine in order
 * to provide application extensibility that usually goes out of scope
 * of #GStreamer plugin system.
 *
 * Possible addons types include:
 *
 * * [iface@Clapper.Extractor] - extracting data from non-media URI into something playable
 *
 * * [iface@Clapper.PlaylistParser] - creating individual media items out of playlist URI
 *
 * Use [const@Clapper.HAVE_ADDONS_LOADER] macro to check if Clapper API
 * was compiled with this feature.
 *
 * Since: 0.8
 */

#include "config.h"

#include <gst/gst.h>
#include <libpeas.h>

#include "clapper-addons-loader.h"
#include "clapper-media-item-private.h"
#include "../shared/clapper-shared-utils-private.h"

#define PLUGIN_SCHEMES_DATA "X-Schemes"
#define PLUGIN_HOSTS_DATA "X-Hosts"

#define GST_CAT_DEFAULT clapper_addons_loader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAddonsLoader
{
  ClapperFeature parent;

  PeasEngine *engine;

  GPtrArray *extractors;
};

#define parent_class clapper_addons_loader_parent_class
G_DEFINE_TYPE (ClapperAddonsLoader, clapper_addons_loader, CLAPPER_TYPE_FEATURE);

static inline gboolean
_is_name_listed (const gchar *name, const gchar *list_str)
{
  gsize name_len = strlen (name);
  guint i = 0;

  while (list_str[i] != '\0') {
    guint end = i;

    while (list_str[end] != ';' && list_str[end] != '\0')
      ++end;

    /* Compare letters count until separator and prefix of whole string */
    if (end - i == name_len && g_str_has_prefix (list_str + i, name))
      return TRUE;

    i = end;

    /* Move to the next letter after ';' */
    if (list_str[i] != '\0')
      ++i;
  }

  return FALSE;
}

static const PeasPluginInfo *
clapper_addons_loader_get_info (ClapperAddonsLoader *self, GUri *uri, const GType extension_type)
{
  GListModel *list = (GListModel *) self->engine;
  const PeasPluginInfo *found_info = NULL;
  guint i, n_plugins = g_list_model_get_n_items (list);
  const gchar *scheme, *host;

  if (n_plugins == 0) {
    GST_INFO_OBJECT (self, "No Clapper addons found");
    return NULL;
  }

  scheme = g_uri_get_scheme (uri);
  host = g_uri_get_host (uri);

  for (i = 0; i < n_plugins; ++i) {
    PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item (list, i);
    const gchar *schemes, *hosts;

    if (!(schemes = peas_plugin_info_get_external_data (info, PLUGIN_SCHEMES_DATA))) {
      GST_INFO_OBJECT (self, "Skipping addon without schemes: %s", peas_plugin_info_get_name (info));
      continue;
    }
    if (!_is_name_listed (scheme, schemes))
      continue;
    if (!(hosts = peas_plugin_info_get_external_data (info, PLUGIN_HOSTS_DATA))) {
      GST_INFO_OBJECT (self, "Skipping addon without hosts: %s", peas_plugin_info_get_name (info));
      continue;
    }
    if (!_is_name_listed (host, hosts))
      continue;
    if (!peas_plugin_info_is_loaded (info) && !peas_engine_load_plugin (self->engine, info)) {
      GST_WARNING_OBJECT (self, "Skipping addon that cannot be loaded: %s", peas_plugin_info_get_name (info));
      continue;
    }
    if (!peas_engine_provides_extension (self->engine, info, extension_type))
      continue;

    found_info = info;
    break;
  }

  return found_info;
}

/*
 * clapper_addons_loader_create_extension:
 * @info: a #PeasPluginInfo
 * @extension_type: a requested extension #GType
 *
 * Returns: (transfer full) (nullable): a new extractor instance.
 */
static GObject *
clapper_addons_loader_create_extension (ClapperAddonsLoader *self,
    const PeasPluginInfo *info, const GType extension_type)
{
  return peas_engine_create_extension (self->engine, (PeasPluginInfo *) info, extension_type, NULL);
}

static void
clapper_addons_loader_queue_item_added (ClapperFeature *feature, ClapperMediaItem *item, guint index)
{
  ClapperAddonsLoader *self = CLAPPER_ADDONS_LOADER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item added %" GST_PTR_FORMAT, item);

  //g_ptr_array_add (self->pending_items, gst_object_ref (item));
}

static void
clapper_addons_loader_queue_item_removed (ClapperFeature *feature, ClapperMediaItem *item, guint index)
{
  ClapperAddonsLoader *self = CLAPPER_ADDONS_LOADER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item removed %" GST_PTR_FORMAT, item);
  //_unqueue_discovery (self, item);
}

static void
clapper_addons_loader_queue_cleared (ClapperFeature *feature)
{
  ClapperAddonsLoader *self = CLAPPER_ADDONS_LOADER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue cleared");
/*
  if (self->pending_items->len > 0)
    g_ptr_array_remove_range (self->pending_items, 0, self->pending_items->len);

  _stop_discovery (self);
*/
}

static gboolean
clapper_addons_loader_prepare (ClapperFeature *feature)
{
  ClapperAddonsLoader *self = CLAPPER_ADDONS_LOADER_CAST (feature);
  const gchar *addons_path;
  gchar **dir_paths;
  guint i, n_addons;

  GST_DEBUG_OBJECT (self, "Prepare");

  self->engine = peas_engine_new_with_nonglobal_loaders ();

  addons_path = g_getenv ("CLAPPER_ADDONS_PATH");
  if (!addons_path || *addons_path == '\0')
    addons_path = CLAPPER_ADDONS_PATH;

  GST_TRACE_OBJECT (self, "Initializing Clapper addons with path: \"%s\"", addons_path);

  dir_paths = g_strsplit (addons_path, G_SEARCHPATH_SEPARATOR_S, 0);

  for (i = 0; dir_paths[i]; ++i)
    peas_engine_add_search_path (self->engine, dir_paths[i], NULL);

  g_strfreev (dir_paths);

  n_addons = g_list_model_get_n_items (G_LIST_MODEL (self->engine));
  GST_TRACE_OBJECT (self, "Clapper addons initialized, found: %i", n_addons);

  /* FIXME: Should we return %FALSE if zero addons? */
  return TRUE;
}

static gboolean
clapper_addons_loader_unprepare (ClapperFeature *feature)
{
  ClapperAddonsLoader *self = CLAPPER_ADDONS_LOADER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Unprepare");

  /* Do what we also do when queue is cleared */
  //clapper_discoverer_queue_cleared (feature);

  g_clear_object (&self->engine);

  return TRUE;
}

/**
 * clapper_addons_loader_new:
 *
 * Creates a new #ClapperAddonsLoader instance.
 *
 * Returns: (transfer full): a new #ClapperAddonsLoader instance.
 *
 * Since: 0.8
 */
ClapperAddonsLoader *
clapper_addons_loader_new (void)
{
  ClapperAddonsLoader *loader = g_object_new (CLAPPER_TYPE_ADDONS_LOADER, NULL);
  gst_object_ref_sink (loader);

  return loader;
}

static void
clapper_addons_loader_init (ClapperAddonsLoader *self)
{
  self->extractors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
clapper_addons_loader_finalize (GObject *object)
{
  ClapperAddonsLoader *self = CLAPPER_ADDONS_LOADER_CAST (object);

  g_ptr_array_unref (self->extractors);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_addons_loader_class_init (ClapperAddonsLoaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperFeatureClass *feature_class = (ClapperFeatureClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperaddonsloader", 0,
      "Clapper Addons Loader");

  gobject_class->finalize = clapper_addons_loader_finalize;

  feature_class->prepare = clapper_addons_loader_prepare;
  feature_class->unprepare = clapper_addons_loader_unprepare;
  feature_class->queue_item_added = clapper_addons_loader_queue_item_added;
  feature_class->queue_item_removed = clapper_addons_loader_queue_item_removed;
  feature_class->queue_cleared = clapper_addons_loader_queue_cleared;
}
