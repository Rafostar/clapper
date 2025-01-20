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

#define ENHANCER_INTERFACES "X-Interfaces"
#define ENHANCER_SCHEMES "X-Schemes"
#define ENHANCER_HOSTS "X-Hosts"
#define ENHANCER_IFACE_NAME_FROM_TYPE(type) (g_type_name (type) + 7) // strip "Clapper" prefix

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
clapper_enhancers_loader_initialize (void)
{
  const gchar *enhancers_path;
  gchar *custom_path = NULL;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancersloader", 0,
      "Clapper Enhancer Loader");

  enhancers_path = g_getenv ("CLAPPER_ENHANCERS_PATH");

#ifdef G_OS_WIN32
  if (!enhancers_path || *enhancers_path == '\0') {
    gchar *win_base_dir;

    win_base_dir = g_win32_get_package_installation_directory_of_module (
        _enhancers_dll_handle);
    /* FIXME: Avoid hardcoded major version */
    custom_path = g_build_filename (win_base_dir,
        "lib", "clapper-0.0", "enhancers", NULL);
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

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_INFO) {
    GListModel *list = (GListModel *) _engine;
    guint i, n_items = g_list_model_get_n_items (list);

    for (i = 0; i < n_items; ++i) {
      PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item (list, i);
      GST_INFO ("Found enhancer: %s (%s)", peas_plugin_info_get_name (info),
          peas_plugin_info_get_external_data (info, ENHANCER_INTERFACES));
      g_object_unref (info);
    }

    GST_INFO ("Clapper enhancers initialized, found: %u", n_items);
  }

  g_free (custom_path);
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
 * clapper_enhancers_loader_get_info:
 * @iface_type: an interface #GType
 * @scheme: an URI scheme
 * @host: (nullable): an URI host
 *
 * Returns: (transfer full) (nullable): available #PeasPluginInfo or %NULL.
 */
static PeasPluginInfo *
clapper_enhancers_loader_get_info (GType iface_type, const gchar *scheme, const gchar *host)
{
  GListModel *list = (GListModel *) _engine;
  PeasPluginInfo *found_info = NULL;
  guint i, n_plugins = g_list_model_get_n_items (list);
  const gchar *iface_name;
  gboolean is_https;

  if (n_plugins == 0) {
    GST_INFO ("No Clapper enhancers found");
    return NULL;
  }

  iface_name = ENHANCER_IFACE_NAME_FROM_TYPE (iface_type);

  /* Strip common subdomains, so plugins do not
   * have to list all combinations */
  if (host) {
    if (g_str_has_prefix (host, "www."))
      host += 4;
    else if (g_str_has_prefix (host, "m."))
      host += 2;
  }

  GST_INFO ("Enhancer check, iface: %s, scheme: %s, host: %s",
      iface_name, scheme, GST_STR_NULL (host));

  is_https = (g_str_has_prefix (scheme, "http")
      && (scheme[4] == '\0' || (scheme[4] == 's' && scheme[5] == '\0')));

  if (!host && is_https)
    return NULL;

  for (i = 0; i < n_plugins; ++i) {
    PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item (list, i);
    const gchar *iface_names, *schemes, *hosts;

    if (!(iface_names = peas_plugin_info_get_external_data (info, ENHANCER_INTERFACES))) {
      GST_DEBUG ("Skipping enhancer without interfaces: %s", peas_plugin_info_get_name (info));
      g_object_unref (info);
      continue;
    }
    if (!_is_name_listed (iface_name, iface_names)) {
      g_object_unref (info);
      continue;
    }
    if (!(schemes = peas_plugin_info_get_external_data (info, ENHANCER_SCHEMES))) {
      GST_DEBUG ("Skipping enhancer without schemes: %s", peas_plugin_info_get_name (info));
      g_object_unref (info);
      continue;
    }
    if (!_is_name_listed (scheme, schemes)) {
      g_object_unref (info);
      continue;
    }
    if (is_https) {
      if (!(hosts = peas_plugin_info_get_external_data (info, ENHANCER_HOSTS))) {
        GST_DEBUG ("Skipping enhancer without hosts: %s", peas_plugin_info_get_name (info));
        g_object_unref (info);
        continue;
      }
      if (!_is_name_listed (host, hosts)) {
        g_object_unref (info);
        continue;
      }
    }

    found_info = info;
    break;
  }

  return found_info;
}

/*
 * clapper_enhancers_loader_has_enhancers:
 * @iface_type: an interface #GType
 *
 * Check if any enhancer implementing given interface type is available.
 *
 * Returns: whether any valid enhancer was found.
 */
gboolean
clapper_enhancers_loader_has_enhancers (GType iface_type)
{
  GListModel *list = (GListModel *) _engine;
  const gchar *iface_name = ENHANCER_IFACE_NAME_FROM_TYPE (iface_type);
  guint i, n_plugins;

  GST_DEBUG ("Checking for any enhancers of type: \"%s\"", iface_name);

  n_plugins = g_list_model_get_n_items (list);

  for (i = 0; i < n_plugins; ++i) {
    PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item (list, i);
    const gchar *iface_names;

    if (!(iface_names = peas_plugin_info_get_external_data (info, ENHANCER_INTERFACES))) {
      g_object_unref (info);
      continue;
    }
    if (!_is_name_listed (iface_name, iface_names)) {
      g_object_unref (info);
      continue;
    }

    /* Additional validation */
    if (!peas_plugin_info_get_external_data (info, ENHANCER_SCHEMES)
        || !peas_plugin_info_get_external_data (info, ENHANCER_HOSTS)) {
      g_object_unref (info);
      continue;
    }

    GST_DEBUG ("Found valid enhancers of type: \"%s\"", iface_name);
    g_object_unref (info);

    return TRUE;
  }

  GST_DEBUG ("No available enhancers of type: \"%s\"", iface_name);

  return FALSE;
}

/*
 * clapper_enhancers_loader_get_schemes:
 * @iface_type: an interface #GType
 *
 * Get all supported schemes for a given interface type.
 * The returned array consists of unique strings (no duplicates).
 *
 * Returns: (transfer full): all supported schemes by enhancers of type.
 */
gchar **
clapper_enhancers_loader_get_schemes (GType iface_type)
{
  GListModel *list = (GListModel *) _engine;
  GSList *found_schemes = NULL, *fs;
  const gchar *iface_name = ENHANCER_IFACE_NAME_FROM_TYPE (iface_type);
  gchar **schemes_strv;
  guint i, n_plugins, n_schemes;

  GST_DEBUG ("Checking supported URI schemes for \"%s\"", iface_name);

  n_plugins = g_list_model_get_n_items (list);

  for (i = 0; i < n_plugins; ++i) {
    PeasPluginInfo *info = (PeasPluginInfo *) g_list_model_get_item (list, i);
    const gchar *iface_names, *schemes;
    gchar **tmp_strv;
    gint j;

    if (!(iface_names = peas_plugin_info_get_external_data (info, ENHANCER_INTERFACES))) {
      g_object_unref (info);
      continue;
    }
    if (!_is_name_listed (iface_name, iface_names)) {
      g_object_unref (info);
      continue;
    }
    if (!(schemes = peas_plugin_info_get_external_data (info, ENHANCER_SCHEMES))) {
      g_object_unref (info);
      continue;
    }

    tmp_strv = g_strsplit (schemes, ";", 0);

    for (j = 0; tmp_strv[j]; ++j) {
      const gchar *scheme = tmp_strv[j];

      if (!found_schemes || !g_slist_find_custom (found_schemes,
          scheme, (GCompareFunc) strcmp)) {
        found_schemes = g_slist_append (found_schemes, g_strdup (scheme));
        GST_INFO ("Found supported URI scheme: %s", scheme);
      }
    }

    g_strfreev (tmp_strv);
    g_object_unref (info);
  }

  n_schemes = g_slist_length (found_schemes);
  schemes_strv = g_new0 (gchar *, n_schemes + 1);

  fs = found_schemes;
  for (i = 0; i < n_schemes; ++i) {
    schemes_strv[i] = fs->data;
    fs = fs->next;
  }

  GST_DEBUG ("Total found URI schemes: %u", n_schemes);

  /* Since string pointers were taken,
   * free list without content */
  g_slist_free (found_schemes);

  return schemes_strv;
}

/*
 * clapper_enhancers_loader_check:
 * @iface_type: a requested #GType
 * @scheme: an URI scheme
 * @host: (nullable): an URI host
 * @name: (out) (optional) (transfer none): return location for found enhancer name
 *
 * Checks if any enhancer can handle @uri without initializing loader
 * or creating enhancer instance, thus this can be used freely from any thread.
 *
 * Returns: whether enhancer for given scheme and host is available.
 */
gboolean
clapper_enhancers_loader_check (GType iface_type,
    const gchar *scheme, const gchar *host, const gchar **name)
{
  PeasPluginInfo *info;

  if ((info = clapper_enhancers_loader_get_info (iface_type, scheme, host))) {
    if (name)
      *name = peas_plugin_info_get_name (info);

    g_object_unref (info);

    return TRUE;
  }

  return FALSE;
}

/*
 * clapper_enhancers_loader_create_enhancer_for_uri:
 * @iface_type: a requested #GType
 * @uri: a #GUri
 *
 * Creates a new enhancer object for given URI.
 *
 * Enhancer should only be created and used within single thread.
 *
 * Returns: (transfer full) (nullable): a new enhancer instance.
 */
GObject *
clapper_enhancers_loader_create_enhancer_for_uri (GType iface_type, GUri *uri)
{
  GObject *enhancer = NULL;
  PeasPluginInfo *info;
  const gchar *scheme = g_uri_get_scheme (uri);
  const gchar *host = g_uri_get_host (uri);

  if ((info = clapper_enhancers_loader_get_info (iface_type, scheme, host))) {
    g_mutex_lock (&load_lock);

    if (!peas_plugin_info_is_loaded (info) && !peas_engine_load_plugin (_engine, info)) {
      GST_ERROR ("Could not load enhancer: %s", peas_plugin_info_get_name (info));
    } else if (!peas_engine_provides_extension (_engine, info, iface_type)) {
      GST_ERROR ("No \"%s\" enhancer in plugin: %s", ENHANCER_IFACE_NAME_FROM_TYPE (iface_type),
          peas_plugin_info_get_name (info));
    } else {
      enhancer = peas_engine_create_extension (_engine, info, iface_type, NULL);
    }

    g_mutex_unlock (&load_lock);
    g_object_unref (info);
  }

  return enhancer;
}
