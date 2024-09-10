/* Clapper Tube Library
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

#include <gmodule.h>
#include <gst/gst.h>

#include "clapper-tube-loader-private.h"
#include "clapper-tube-cache-private.h"

#define GST_CAT_DEFAULT clapper_tube_loader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef ClapperTubeExtractor* (* ExtractorQuery) (GUri *uri);
typedef const gchar *const * (* ExtractorHosts) (void);
typedef const gchar *const * (* ExtractorSchemes) (void);

static const gchar *const default_schemes[] = {
  "http", "https", NULL
};
static const gchar *const no_hosts[] = {
  NULL
};

void
clapper_tube_loader_init_internal (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubeloader", 0,
      "Clapper Tube Loader");
}

static const gchar *
clapper_tube_loader_get_plugin_dir_path_string (void)
{
  const gchar *env_path = g_getenv ("CLAPPER_TUBE_PLUGIN_PATH");

  return (env_path && env_path[0])
      ? env_path
      : CLAPPER_TUBE_PLUGIN_PATH;
}

gchar **
clapper_tube_loader_obtain_plugin_dir_paths (void)
{
  const gchar *path_str = clapper_tube_loader_get_plugin_dir_path_string ();

  return g_strsplit (path_str, G_SEARCHPATH_SEPARATOR_S, 0);
}

static GModule *
clapper_tube_loader_open_module (const gchar *module_path)
{
  GModule *module;
  GError *error = NULL;

  GST_DEBUG ("Opening module: %s", module_path);
  module = g_module_open_full (module_path,
      G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL, &error);

  if (module == NULL) {
    GST_ERROR ("Could not load plugin: %s, reason: %s",
        module_path, (error) ? error->message : "Unknown");
    g_clear_error (&error);

    return NULL;
  }

  GST_INFO ("Opened plugin module: %s", module_path);

  /* Make sure module stays loaded and just
   * decease reference counter afterwards */
  g_module_make_resident (module);
  g_module_close (module);

  return module;
}

gboolean
clapper_tube_loader_check_plugin_compat (const gchar *module_path,
    const gchar *const **schemes, const gchar *const **hosts)
{
  ExtractorSchemes extractor_get_schemes;
  ExtractorHosts extractor_get_hosts;
  GModule *module;

  if (!(module = clapper_tube_loader_open_module (module_path)))
    return FALSE;

  if (g_module_symbol (module, "extractor_get_schemes", (gpointer *) &extractor_get_schemes)
      && extractor_get_schemes != NULL) {
    *schemes = extractor_get_schemes ();
  }

  /* Schemes are required */
  if (*schemes == NULL || (*schemes)[0] == NULL)
    *schemes = default_schemes;

  if (g_module_symbol (module, "extractor_get_hosts", (gpointer *) &extractor_get_hosts)
      && extractor_get_hosts != NULL) {
    *hosts = extractor_get_hosts ();
  }

  /* Hosts may be empty in case of plugins
   * that use some unusual scheme */
  if (*hosts == NULL)
    *hosts = no_hosts;

  return TRUE;
}

ClapperTubeExtractor *
clapper_tube_loader_get_extractor_for_uri (GUri *guri)
{
  ClapperTubeExtractor *extractor = NULL;
  GPtrArray *compatible;
  guint i;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *uri = g_uri_to_string (guri);

    GST_DEBUG ("Searching for plugin that opens URI: %s", uri);
    g_free (uri);
  }

  compatible = clapper_tube_cache_find_plugins_for_uri (guri);

  for (i = 0; i < compatible->len; ++i) {
    const gchar *module_path = g_ptr_array_index (compatible, i);
    ExtractorQuery extractor_query;
    GModule *module;

    if (!(module = clapper_tube_loader_open_module (module_path)))
      continue;

    if (!g_module_symbol (module, "extractor_query", (gpointer *) &extractor_query)
        || extractor_query == NULL) {
      GST_ERROR ("Skipping extractor without query function: %s", module_path);
      continue;
    }

    if ((extractor = extractor_query (guri))) {
      GST_INFO ("Found compatible plugin: %s", module_path);
      break;
    }
  }

  g_ptr_array_unref (compatible);

  return extractor;
}
