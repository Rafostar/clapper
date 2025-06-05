/* Clapper Playback Library
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

#ifdef G_OS_WIN32
#include <windows.h>
static HMODULE _enhancers_dll_handle = NULL;
#endif

#include "clapper-enhancers-loader-private.h"
#include "clapper-enhancer-proxy-list-private.h"
#include "clapper-enhancer-proxy-private.h"

// Supported interfaces
#include "clapper-extractable.h"
#include "clapper-reactable.h"

#include <clapper-functionalities-availability.h>

#define GST_CAT_DEFAULT clapper_enhancers_loader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static PeasEngine *_engine = NULL;
static GMutex load_lock;

static inline void
_import_enhancers (const gchar *enhancers_path)
{
  gchar **dir_paths = g_strsplit (enhancers_path, G_SEARCHPATH_SEPARATOR_S, 0);
  guint i;

  for (i = 0; dir_paths[i]; ++i)
    peas_engine_add_search_path (_engine, dir_paths[i], NULL);

  g_strfreev (dir_paths);
}

/*
 * clapper_enhancers_loader_initialize:
 *
 * Initializes #PeasEngine with directories that store enhancers.
 */
void
clapper_enhancers_loader_initialize (ClapperEnhancerProxyList *proxies)
{
  const gchar *enhancers_path;
  gchar *custom_path = NULL;
  guint i, n_items;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancersloader", 0,
      "Clapper Enhancer Loader");

  enhancers_path = g_getenv ("CLAPPER_ENHANCERS_PATH");

#ifdef G_OS_WIN32
  if (!enhancers_path || *enhancers_path == '\0') {
    gchar *win_base_dir;

    win_base_dir = g_win32_get_package_installation_directory_of_module (
        _enhancers_dll_handle);
    custom_path = g_build_filename (win_base_dir,
        "lib", CLAPPER_API_NAME, "enhancers", NULL);
    enhancers_path = custom_path; // assign temporarily

    g_free (win_base_dir);
  }
#endif

  if (!enhancers_path || *enhancers_path == '\0')
    enhancers_path = CLAPPER_ENHANCERS_PATH;

  GST_INFO ("Initializing Clapper enhancers with path: \"%s\"", enhancers_path);

  _engine = peas_engine_new ();

  /* Peas loaders are loaded lazily, so it should be fine
   * to just enable them all here (even if not installed) */
  peas_engine_enable_loader (_engine, "python");
  peas_engine_enable_loader (_engine, "gjs");

  _import_enhancers (enhancers_path);

  /* Support loading additional enhancers from non-default directory */
  enhancers_path = g_getenv ("CLAPPER_ENHANCERS_EXTRA_PATH");
  if (enhancers_path && *enhancers_path != '\0') {
    GST_INFO ("Enhancers extra path: \"%s\"", enhancers_path);
    _import_enhancers (enhancers_path);
  }

  n_items = g_list_model_get_n_items ((GListModel *) _engine);
  for (i = 0; i < n_items; ++i) {
    PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item ((GListModel *) _engine, i);
    ClapperEnhancerProxy *proxy;
    gboolean filled;

    /* FIXME: 1.0: Remove together with features code and manager.
     * These would clash with each other, so avoid loading these
     * as enhancers when also compiled as part of the library. */
#if (CLAPPER_HAVE_MPRIS || CLAPPER_HAVE_DISCOVERER || CLAPPER_HAVE_SERVER)
    guint f_index;
    const gchar *module_name = peas_plugin_info_get_module_name (info);
    const gchar *ported_features[] = {
#if CLAPPER_HAVE_MPRIS
      "clapper-mpris",
#endif
#if CLAPPER_HAVE_DISCOVERER
      "clapper-discoverer",
#endif
#if CLAPPER_HAVE_SERVER
      "clapper-server",
#endif
    };

    for (f_index = 0; f_index < G_N_ELEMENTS (ported_features); ++f_index) {
      if (strcmp (module_name, ported_features[f_index]) == 0) {
        GST_INFO ("Skipped \"%s\" enhancer module, since its"
          " loaded from deprecated feature object", module_name);
        g_clear_object (&info);
      }
    }

    if (!info) // cleared when exists as feature
      continue;
#endif

    /* Clapper supports only 1 proxy per plugin. Each plugin can
     * ship 1 class, but it can implement more than 1 interface. */
    proxy = clapper_enhancer_proxy_new_global_take ((GObject *) info);

    /* Try to fill missing data from cache (fast).
     * Otherwise make an instance and fill missing data from it (slow). */
    if (!(filled = clapper_enhancer_proxy_fill_from_cache (proxy))) {
      GObject *enhancer;
      const GType main_types[] = { CLAPPER_TYPE_EXTRACTABLE, CLAPPER_TYPE_REACTABLE };
      guint j;

      /* We cannot ask libpeas for "any" of our main interfaces, so try each one until found */
      for (j = 0; j < G_N_ELEMENTS (main_types); ++j) {
        if ((enhancer = clapper_enhancers_loader_create_enhancer (proxy, main_types[j]))) {
          filled = clapper_enhancer_proxy_fill_from_instance (proxy, enhancer);
          g_object_unref (enhancer);

          clapper_enhancer_proxy_export_to_cache (proxy);
          break;
        }
      }
    }

    if (G_LIKELY (filled)) {
      GST_INFO ("Found enhancer: \"%s\" (%s)",
          clapper_enhancer_proxy_get_friendly_name (proxy),
          clapper_enhancer_proxy_get_module_name (proxy));
      clapper_enhancer_proxy_list_take_proxy (proxies, proxy);
    } else {
      GST_WARNING ("Enhancer init failed: \"%s\" (%s)",
          clapper_enhancer_proxy_get_friendly_name (proxy),
          clapper_enhancer_proxy_get_module_name (proxy));
      gst_object_unref (proxy);
    }
  }

  clapper_enhancer_proxy_list_sort (proxies);

  GST_INFO ("Clapper enhancers initialized, found: %u",
      clapper_enhancer_proxy_list_get_n_proxies (proxies));

  g_free (custom_path);
}

/*
 * clapper_enhancers_loader_create_enhancer:
 * @iface_type: a requested #GType
 * @info: a #PeasPluginInfo
 *
 * Creates a new enhancer object using @info.
 *
 * Enhancer should only be created and used within single thread.
 *
 * Returns: (transfer full) (nullable): a new enhancer instance.
 */
GObject *
clapper_enhancers_loader_create_enhancer (ClapperEnhancerProxy *proxy, GType iface_type)
{
  GObject *enhancer = NULL;
  PeasPluginInfo *info = (PeasPluginInfo *) clapper_enhancer_proxy_get_peas_info (proxy);

  g_mutex_lock (&load_lock);

  if (!peas_plugin_info_is_loaded (info) && !peas_engine_load_plugin (_engine, info)) {
    GST_ERROR ("Could not load enhancer: %s", peas_plugin_info_get_module_name (info));
  } else if (!peas_engine_provides_extension (_engine, info, iface_type)) {
    GST_LOG ("No \"%s\" enhancer in module: %s", g_type_name (iface_type),
        peas_plugin_info_get_module_name (info));
  } else {
    enhancer = peas_engine_create_extension (_engine, info, iface_type, NULL);
  }

  g_mutex_unlock (&load_lock);

  return enhancer;
}
