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

#include "config.h"

#include <gst/gst.h>
#include <libpeas.h>

#include "clapper-addons-loader-private.h"

#define ADDON_SCHEMES "X-Schemes"
#define ADDON_HOSTS "X-Hosts"

#define GST_CAT_DEFAULT clapper_addons_loader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static PeasEngine *_engine = NULL;
static GMutex init_lock;

/*
 * clapper_addons_loader_init:
 *
 * Initializes a #PeasEngine with directories that store addons.
 * If already initialized, this function does nothing.
 */
void
clapper_addons_loader_init (void)
{
  g_mutex_lock (&init_lock);

  if (!_engine) {
    const gchar *addons_path;
    gchar **dir_paths;
    guint i;

    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperaddonsloader", 0,
        "Clapper Addons Loader");

    addons_path = g_getenv ("CLAPPER_ADDONS_PATH");
    if (!addons_path || *addons_path == '\0')
      addons_path = CLAPPER_ADDONS_PATH;

    GST_DEBUG ("Initializing Clapper addons with path: \"%s\"", addons_path);

    _engine = peas_engine_new ();

    /* Peas loaders are loaded lazily, so it should be fine
     * to just enable them all here (even if not installed) */
    peas_engine_enable_loader (_engine, "python");
    peas_engine_enable_loader (_engine, "gjs");

    dir_paths = g_strsplit (addons_path, G_SEARCHPATH_SEPARATOR_S, 0);

    for (i = 0; dir_paths[i]; ++i)
      peas_engine_add_search_path (_engine, dir_paths[i], NULL);

    g_strfreev (dir_paths);

    GST_DEBUG ("Clapper addons initialized, found: %u",
        g_list_model_get_n_items ((GListModel *) _engine));
  }

  g_mutex_unlock (&init_lock);
}

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

/*
 * clapper_addons_loader_get_info:
 * @uri: a #GUri
 * @extension_type: a requested extension #GType
 *
 * Returns: (transfer none) (nullable): available #PeasPluginInfo or %NULL.
 */
static PeasPluginInfo *
clapper_addons_loader_get_info (GUri *uri, const GType extension_type)
{
  GListModel *list = (GListModel *) _engine;
  PeasPluginInfo *found_info = NULL;
  guint i, n_plugins = g_list_model_get_n_items (list);
  const gchar *scheme, *host;

  if (n_plugins == 0) {
    GST_INFO ("No Clapper addons found");
    return NULL;
  }

  scheme = g_uri_get_scheme (uri);
  host = g_uri_get_host (uri);

  for (i = 0; i < n_plugins; ++i) {
    PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item (list, i);
    const gchar *schemes, *hosts;

    if (!(schemes = peas_plugin_info_get_external_data (info, ADDON_SCHEMES))) {
      GST_INFO ("Skipping addon without schemes: %s", peas_plugin_info_get_name (info));
      continue;
    }
    if (!_is_name_listed (scheme, schemes))
      continue;
    if (!(hosts = peas_plugin_info_get_external_data (info, ADDON_HOSTS))) {
      GST_INFO ("Skipping addon without hosts: %s", peas_plugin_info_get_name (info));
      continue;
    }
    if (!_is_name_listed (host, hosts))
      continue;
    if (!peas_plugin_info_is_loaded (info) && !peas_engine_load_plugin (_engine, info)) {
      GST_WARNING ("Skipping addon that cannot be loaded: %s", peas_plugin_info_get_name (info));
      continue;
    }
    if (!peas_engine_provides_extension (_engine, info, extension_type))
      continue;

    found_info = info;
    break;
  }

  return found_info;
}

/*
 * clapper_addons_loader_has_extension_for_uri:
 * @uri: a #GUri
 * @extension_type: a requested extension #GType
 *
 * Checks if any addon can handle @uri without
 * initializing loader or creating extension instance,
 * thus this can be used freely from any thread.
 *
 * Returns: whether any addon has extension for given URI.
 */
gboolean
clapper_addons_loader_has_extension_for_uri (GUri *uri, const GType extension_type)
{
  return (clapper_addons_loader_get_info (uri, extension_type) != NULL);
}

/*
 * clapper_addons_loader_create_extension:
 * @info: a #PeasPluginInfo
 * @extension_type: a requested extension #GType
 *
 * Creates a new extension object for given addon info.
 *
 * Returns: (transfer full) (nullable): a new extension instance.
 */
GObject *
clapper_addons_loader_create_extension (const PeasPluginInfo *info, const GType extension_type)
{
  GObject *extension = NULL;
  const PeasPluginInfo *info;

  if ((info = clapper_addons_loader_get_info (uri, extension_type)))
    extension = peas_engine_create_extension (_engine, (PeasPluginInfo *) info, extension_type, NULL);

  return extension;
}
