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

#include <stdio.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include "clapper-tube-cache.h"
#include "clapper-tube-cache-private.h"
#include "clapper-tube-loader-private.h"
#include "clapper-tube-config.h"
#include "clapper-tube-version.h"

#define CLAPPER_TUBE_HEADER_NAME "CLAPPER_TUBE"
#define CLAPPER_TUBE_CACHE_BASENAME "clapper_tube_cache.bin"

#define GST_CAT_DEFAULT clapper_tube_cache_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* CACHE CONTENTS:
 * ClapperTubeCacheHeader;
 *
 * gint64 config_mod_time;
 * guint config_n_files;
 *
 * multiple dir data
 */

/* DIR DATA:
 * guint len;
 * gchar *dir_path;
 *
 * gint64 mod_time;
 * guint n_plugins;
 *
 * multiple plugins data
 */

/* PLUGIN DATA:
 * guint len;
 * gchar *module_name;
 *
 * guint n_strings;
 * guint len;
 * gchar *string;
 * ...
 */

/* PLUGIN CACHE CONTENTS:
 * ClapperTubeCacheHeader;
 *
 * guint key_len;
 * gchar *key;
 * guint val_len;
 * gchar *val;
 */

typedef struct
{
  gchar name[13];
  guint version_hex;
} ClapperTubeCacheHeader;

typedef struct
{
  gchar *module_name;
  GPtrArray *schemes;
  GPtrArray *hosts;
} ClapperTubeCachePluginCompatData;

typedef struct
{
  gchar *dir_path;
  GPtrArray *plugins;
} ClapperTubeCachePluginDirData;

/* Mutex protecting IO and plugins data */
static GMutex cache_lock;
static GPtrArray *plugins_cache = NULL;

static ClapperTubeCachePluginCompatData *
clapper_tube_cache_plugin_compat_data_new_take (gchar *module_name)
{
  ClapperTubeCachePluginCompatData *data;

  data = g_new (ClapperTubeCachePluginCompatData, 1);
  data->module_name = module_name;
  data->schemes = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
  data->hosts = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

  return data;
}

static ClapperTubeCachePluginCompatData *
clapper_tube_cache_plugin_compat_data_new (const gchar *module_name)
{
  return clapper_tube_cache_plugin_compat_data_new_take (g_strdup (module_name));
}

static void
clapper_tube_cache_plugin_compat_data_free (ClapperTubeCachePluginCompatData *data)
{
  g_free (data->module_name);
  g_ptr_array_unref (data->schemes);
  g_ptr_array_unref (data->hosts);

  g_free (data);
}

static ClapperTubeCachePluginDirData *
clapper_tube_cache_plugin_dir_data_new_take (gchar *dir_path)
{
  ClapperTubeCachePluginDirData *data;

  data = g_new (ClapperTubeCachePluginDirData, 1);
  data->dir_path = dir_path;
  data->plugins = g_ptr_array_new_with_free_func (
      (GDestroyNotify) clapper_tube_cache_plugin_compat_data_free);

  return data;
}

static ClapperTubeCachePluginDirData *
clapper_tube_cache_plugin_dir_data_new (const gchar *dir_path)
{
  return clapper_tube_cache_plugin_dir_data_new_take (g_strdup (dir_path));
}

static void
clapper_tube_cache_plugin_dir_data_free (ClapperTubeCachePluginDirData *data)
{
  g_free (data->dir_path);
  g_ptr_array_unref (data->plugins);

  g_free (data);
}

static void
clapper_tube_cache_take_dir_data (ClapperTubeCachePluginDirData *data)
{
  g_ptr_array_add (plugins_cache, data);
}

static void
clapper_tube_cache_remove_content (void)
{
  g_ptr_array_remove_range (plugins_cache, 0, plugins_cache->len);
}

static void
clapper_tube_cache_plugin_dir_data_take_plugin_data (ClapperTubeCachePluginDirData *data,
    ClapperTubeCachePluginCompatData *plugin_data)
{
  g_ptr_array_add (data->plugins, plugin_data);
}

static void
clapper_tube_cache_plugin_compat_data_add_scheme (ClapperTubeCachePluginCompatData *data,
    const gchar *scheme)
{
  g_ptr_array_add (data->schemes, g_strdup (scheme));
}

static void
clapper_tube_cache_plugin_compat_data_take_scheme (ClapperTubeCachePluginCompatData *data,
    gchar *scheme)
{
  g_ptr_array_add (data->schemes, scheme);
}

static void
clapper_tube_cache_plugin_compat_data_add_host (ClapperTubeCachePluginCompatData *data,
    const gchar *host)
{
  g_ptr_array_add (data->hosts, g_strdup (host));
}

static void
clapper_tube_cache_plugin_compat_data_take_host (ClapperTubeCachePluginCompatData *data,
    gchar *host)
{
  g_ptr_array_add (data->hosts, host);
}

static gchar *
clapper_tube_cache_obtain_cache_path (const gchar *basename)
{
  return g_build_filename (g_get_user_cache_dir (),
      CLAPPER_TUBE_API_NAME, basename, NULL);
}

static gboolean
write_ptr_to_file (FILE *file, gconstpointer ptr, gsize size)
{
  return fwrite (ptr, size, 1, file) == 1;
}

static gboolean
read_file_to_ptr (FILE *file, gpointer ptr, gsize size)
{
  return fread (ptr, size, 1, file) == 1;
}

static void
write_string (FILE *file, const gchar *str)
{
  guint len = strlen (str) + 1;

  write_ptr_to_file (file, &len, sizeof (guint));
  write_ptr_to_file (file, str, len);
}

static gchar *
read_next_string (FILE *file)
{
  guint len;
  gchar *str = NULL;

  read_file_to_ptr (file, &len, sizeof (guint));
  if (len > 0) {
    str = g_new (gchar, len);
    read_file_to_ptr (file, str, len);
  }

  return str;
}

static gboolean
write_n_elems (FILE *file, const gchar *const *arr)
{
  guint n_elems = 0;

  while (arr && arr[n_elems])
    n_elems++;

  write_ptr_to_file (file, &n_elems, sizeof (guint));

  return n_elems > 0;
}

static gint64
clapper_tube_cache_get_file_mod_time (GFileInfo *info)
{
  GDateTime *date_time;
  gint64 unix_time = 0;

  date_time = g_file_info_get_modification_date_time (info);
  if (date_time) {
    unix_time = g_date_time_to_unix (date_time);
    g_date_time_unref (date_time);
  }

  return unix_time;
}

static gboolean
clapper_tube_cache_prepare (GError **error)
{
  GFile *db_dir;
  gchar *db_path;
  gboolean res = FALSE;

  db_path = clapper_tube_cache_obtain_cache_path (NULL);
  db_dir = g_file_new_for_path (db_path);
  g_free (db_path);

  res = (g_file_query_exists (db_dir, NULL) || g_file_make_directory (db_dir, NULL, error));
  g_object_unref (db_dir);

  return res;
}

static FILE *
clapper_tube_cache_open_write (const gchar *basename)
{
  FILE *file = NULL;
  ClapperTubeCacheHeader *header;
  gchar *filepath;

  filepath = clapper_tube_cache_obtain_cache_path (basename);
  GST_DEBUG ("Opening cache file for writing: %s", filepath);

  file = fopen (filepath, "wb");
  if (!file) {
    GST_ERROR ("Could not open file: %s", filepath);
    goto finish;
  }

  header = g_new (ClapperTubeCacheHeader, 1);
  g_strlcpy (header->name, CLAPPER_TUBE_HEADER_NAME, sizeof (header->name));
  header->version_hex = CLAPPER_TUBE_VERSION_HEX;

  write_ptr_to_file (file, header, sizeof (ClapperTubeCacheHeader));
  g_free (header);

finish:
  g_free (filepath);

  return file;
}

static FILE *
clapper_tube_cache_open_read (const gchar *basename)
{
  FILE *file;
  ClapperTubeCacheHeader *header;
  gchar *filepath;
  gboolean success = FALSE;

  filepath = clapper_tube_cache_obtain_cache_path (basename);
  GST_DEBUG ("Opening cache file for reading: %s", filepath);

  file = fopen (filepath, "rb");
  g_free (filepath);

  if (!file) {
    GST_DEBUG ("Could not open file: %s", basename);
    return NULL;
  }

  header = g_new (ClapperTubeCacheHeader, 1);
  read_file_to_ptr (file, header, sizeof (ClapperTubeCacheHeader));

  GST_DEBUG ("Cache header, name: %s, version_hex: %u",
      header->name, header->version_hex);

  if (g_strcmp0 (header->name, CLAPPER_TUBE_HEADER_NAME)
      || header->version_hex != CLAPPER_TUBE_VERSION_HEX) {
    GST_DEBUG ("Cache header mismatch");
    goto finish;
  }

  GST_DEBUG ("Cache opened successfully");
  success = TRUE;

finish:
  g_free (header);

  if (success)
    return file;

  fclose (file);
  return NULL;
}

static void
clapper_tube_cache_enumerate_configs (GFile *dir, gint64 *config_mod_time,
    guint *config_n_files, GError **error)
{
  GFileEnumerator *dir_enum;

  dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error);
  if (!dir_enum)
    return;

  while (TRUE) {
    GFileInfo *info = NULL;
    gint64 file_mod_time;

    if (!g_file_enumerator_iterate (dir_enum, &info,
        NULL, NULL, error) || !info)
      break;

    if (config_n_files)
      *config_n_files += 1;

    file_mod_time = clapper_tube_cache_get_file_mod_time (info);

    if (config_mod_time && *config_mod_time < file_mod_time)
      *config_mod_time = file_mod_time;
  }

  g_object_unref (dir_enum);
}

static gboolean
clapper_tube_cache_read_config (FILE *file, GError **error)
{
  GFile *dir;
  gint64 config_mod_time, latest_time = 0;
  guint config_n_files, n_files = 0;

  read_file_to_ptr (file, &config_mod_time, sizeof (gint64));
  read_file_to_ptr (file, &config_n_files, sizeof (guint));

  dir = g_file_new_for_path (clapper_tube_config_get_dir_path ());

  /* Leaves latest_time as zero when config dir does not exists.
   * This allows to detect if dir was removed/created since last usage. */
  if (g_file_query_exists (dir, NULL))
    clapper_tube_cache_enumerate_configs (dir, &latest_time, &n_files, error);

  g_object_unref (dir);

  if (error && *error != NULL)
    return FALSE;

  GST_DEBUG ("Config compared, mod_time: %"
      G_GINT64_FORMAT " %s %" G_GINT64_FORMAT ", n_files: %u %s %u",
      config_mod_time, (config_mod_time != latest_time) ? "!=" : "==", latest_time,
      config_n_files, (config_n_files != n_files) ? "!=" : "==", n_files);

  return (config_mod_time == latest_time && config_n_files == n_files);
}

static gboolean
clapper_tube_cache_write_config (FILE *file, GError **error)
{
  GFile *dir;
  gint64 config_mod_time = 0;
  guint config_n_files = 0;

  dir = g_file_new_for_path (clapper_tube_config_get_dir_path ());

  if (g_file_query_exists (dir, NULL))
    clapper_tube_cache_enumerate_configs (dir, &config_mod_time, &config_n_files, error);

  g_object_unref (dir);

  if (error && *error != NULL)
    return FALSE;

  GST_DEBUG ("Writing config dir data, config_mod_time: %"
      G_GINT64_FORMAT ", config_n_files: %u",
      config_mod_time, config_n_files);

  write_ptr_to_file (file, &config_mod_time, sizeof (gint64));
  write_ptr_to_file (file, &config_n_files, sizeof (guint));

  return TRUE;
}

static void
clapper_tube_cache_enumerate_plugins (GFile *dir, GPtrArray *modules,
    gint64 *mod_time, guint *n_plugins, GError **error)
{
  GFileEnumerator *dir_enum;

  if (!(dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME ","
      G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error)))
    return;

  while (TRUE) {
    GFileInfo *info = NULL;
    gint64 plugin_mod_time;
    const gchar *module_name;

    if (!g_file_enumerator_iterate (dir_enum, &info,
        NULL, NULL, error) || !info)
      break;

    module_name = g_file_info_get_name (info);

    if (!g_str_has_suffix (module_name, "." G_MODULE_SUFFIX))
      continue;

    if (modules)
      g_ptr_array_add (modules, g_strdup (module_name));
    if (n_plugins)
      *n_plugins += 1;

    plugin_mod_time = clapper_tube_cache_get_file_mod_time (info);

    if (mod_time && *mod_time < plugin_mod_time)
      *mod_time = plugin_mod_time;
  }

  g_object_unref (dir_enum);
}

static gboolean
clapper_tube_cache_read_plugins_compat (FILE *file,
    const gchar *dir_path, GError **error)
{
  GFile *dir;
  gchar *cache_dir_path;
  gint64 cache_mod_time, latest_time = 0;
  guint cache_n_plugins, n_plugins = 0;
  gboolean changed;

  cache_dir_path = read_next_string (file);
  GST_DEBUG ("Read cache dir path: %s", cache_dir_path);

  /* Make sure the order in CLAPPER_TUBE_PLUGIN_PATH have not changed */
  changed = g_strcmp0 (dir_path, cache_dir_path) != 0;

  if (changed) {
    GST_DEBUG ("Plugin path has changed");
    goto fail;
  }

  dir = g_file_new_for_path (dir_path);
  if (!dir) {
    GST_ERROR ("Could not parse dir path: %s", dir_path);
    goto fail;
  }

  read_file_to_ptr (file, &cache_mod_time, sizeof (gint64));
  read_file_to_ptr (file, &cache_n_plugins, sizeof (guint));

  clapper_tube_cache_enumerate_plugins (dir, NULL, &latest_time, &n_plugins, error);
  g_object_unref (dir);

  changed = (cache_mod_time != latest_time
      || cache_n_plugins != n_plugins);

  GST_DEBUG ("Cache compared, mod_time: %"
      G_GINT64_FORMAT " %s %" G_GINT64_FORMAT ", n_plugins: %u %s %u",
      cache_mod_time, (cache_mod_time != latest_time) ? "!=" : "==", latest_time,
      cache_n_plugins, (cache_n_plugins != n_plugins) ? "!=" : "==", n_plugins);

  if (!changed) {
    ClapperTubeCachePluginDirData *dir_data;
    guint i;

    GST_DEBUG ("Reading plugins compat data for dir: %s", cache_dir_path);
    dir_data = clapper_tube_cache_plugin_dir_data_new_take (cache_dir_path);

    for (i = 0; i < cache_n_plugins; ++i) {
      ClapperTubeCachePluginCompatData *data;
      gchar *module_name;
      guint n_schemes, n_hosts, j;

      module_name = read_next_string (file);
      data = clapper_tube_cache_plugin_compat_data_new_take (module_name);

      read_file_to_ptr (file, &n_schemes, sizeof (guint));
      for (j = 0; j < n_schemes; ++j) {
        gchar *scheme;

        scheme = read_next_string (file);
        clapper_tube_cache_plugin_compat_data_take_scheme (data, scheme);
      }

      read_file_to_ptr (file, &n_hosts, sizeof (guint));
      for (j = 0; j < n_hosts; ++j) {
        gchar *host;

        host = read_next_string (file);
        clapper_tube_cache_plugin_compat_data_take_host (data, host);
      }

      clapper_tube_cache_plugin_dir_data_take_plugin_data (dir_data, data);
    }

    clapper_tube_cache_take_dir_data (dir_data);
    GST_DEBUG ("Read compat data for %u plugins", n_plugins);

    return TRUE;
  }

fail:
  g_free (cache_dir_path);

  return FALSE;
}

static gboolean
clapper_tube_cache_write_plugins_compat (FILE *file,
    const gchar *dir_path, GError **error)
{
  GFile *dir;
  GPtrArray *module_names;
  ClapperTubeCachePluginDirData *dir_data;
  gint64 mod_time = 0;
  guint i, n_plugins = 0;
  gboolean success = TRUE;

  dir = g_file_new_for_path (dir_path);
  if (!dir) {
    GST_ERROR ("Malformed path in \"CLAPPER_TUBE_PLUGIN_PATH\" env: %s", dir_path);
    return FALSE;
  }

  write_string (file, dir_path);

  module_names = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
  clapper_tube_cache_enumerate_plugins (dir, module_names, &mod_time, &n_plugins, error);

  GST_DEBUG ("Writing plugin dir data, mod_time: %"
      G_GINT64_FORMAT ", n_plugins: %u", mod_time, n_plugins);

  write_ptr_to_file (file, &mod_time, sizeof (gint64));
  write_ptr_to_file (file, &n_plugins, sizeof (guint));

  dir_data = clapper_tube_cache_plugin_dir_data_new (dir_path);

  for (i = 0; i < module_names->len; ++i) {
    ClapperTubeCachePluginCompatData *data;
    gchar *module_path;
    guint j;

    const gchar *module_name;
    const gchar *const *plugin_schemes = NULL;
    const gchar *const *plugin_hosts = NULL;

    module_name = g_ptr_array_index (module_names, i);

    module_path = g_module_build_path (dir_path, module_name);
    GST_DEBUG ("Checking support: %s", module_path);

    success = clapper_tube_loader_check_plugin_compat (module_path,
        &plugin_schemes, &plugin_hosts);
    g_free (module_path);

    /* Instant failure, if we skip a write here number of plugins
     * in cache will not match number of plugin data objects */
    if (!success) {
      GST_ERROR ("Could not read plugin compat: %s", module_name);
      break;
    }

    write_string (file, module_name);
    data = clapper_tube_cache_plugin_compat_data_new (module_name);

    if (write_n_elems (file, plugin_schemes)) {
      for (j = 0; plugin_schemes[j]; ++j) {
        GST_DEBUG ("Supported scheme: %s", plugin_schemes[j]);

        write_string (file, plugin_schemes[j]);
        clapper_tube_cache_plugin_compat_data_add_scheme (data, plugin_schemes[j]);
      }
    }
    if (write_n_elems (file, plugin_hosts)) {
      for (j = 0; plugin_hosts[j]; ++j) {
        GST_DEBUG ("Supported host: %s", plugin_hosts[j]);

        write_string (file, plugin_hosts[j]);
        clapper_tube_cache_plugin_compat_data_add_host (data, plugin_hosts[j]);
      }
    }

    clapper_tube_cache_plugin_dir_data_take_plugin_data (dir_data, data);
  }

  g_ptr_array_unref (module_names);
  clapper_tube_cache_take_dir_data (dir_data);

  return success;
}

gchar **
clapper_tube_cache_create_supported_schemes (void)
{
  GPtrArray *arr;
  gchar **schemes;
  guint i;

  /* Plugin cache should be already initialized
   * before this function is called */
  if (G_UNLIKELY (!plugins_cache))
    return NULL;

  arr = g_ptr_array_new ();

  for (i = 0; i < plugins_cache->len; ++i) {
    ClapperTubeCachePluginDirData *dir_data;
    guint j;

    dir_data = g_ptr_array_index (plugins_cache, i);

    for (j = 0; j < dir_data->plugins->len; ++j) {
      ClapperTubeCachePluginCompatData *data;
      guint k;

      data = g_ptr_array_index (dir_data->plugins, j);

      for (k = 0; k < data->schemes->len; ++k) {
        const gchar *plugin_scheme;
        gboolean present = FALSE;
        guint l;

        plugin_scheme = g_ptr_array_index (data->schemes, k);

        for (l = 0; l < arr->len; ++l) {
          if ((present = strcmp (g_ptr_array_index (arr, l), plugin_scheme) == 0))
            break;
        }

        if (!present)
          g_ptr_array_add (arr, (gchar *) plugin_scheme);
      }
    }
  }

  schemes = g_new0 (gchar *, arr->len + 1);

  for (i = 0; i < arr->len; ++i)
    schemes[i] = g_ptr_array_index (arr, i);

  g_ptr_array_unref (arr);

  return schemes;
}

/* Should be run only once with a global mutex taken */
gboolean
clapper_tube_cache_init (GError **error)
{
  FILE *file;
  gboolean success = FALSE;
  guint i;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubecache", 0,
      "Clapper Tube Cache");

  GST_INFO ("Initializing cache");
  plugins_cache = g_ptr_array_new_with_free_func (
      (GDestroyNotify) clapper_tube_cache_plugin_dir_data_free);

  if (!clapper_tube_cache_prepare (error))
    goto finish;

  file = clapper_tube_cache_open_read (CLAPPER_TUBE_CACHE_BASENAME);
  if (file) {
    if (clapper_tube_cache_read_config (file, error)) {
      gchar **dir_paths;

      dir_paths = clapper_tube_loader_obtain_plugin_dir_paths ();

      for (i = 0; dir_paths[i]; ++i) {
        if (!(success = clapper_tube_cache_read_plugins_compat (file, dir_paths[i], error))) {
          /* Since we failed at some index, remove everything that was added prior to it */
          clapper_tube_cache_remove_content ();
          break;
        }
      }

      g_strfreev (dir_paths);
    }

    fclose (file);
  }

  if (error && *error != NULL)
    goto finish;

  if (!success) {
    GST_INFO ("Plugin cache needs rewriting");

    file = clapper_tube_cache_open_write (CLAPPER_TUBE_CACHE_BASENAME);
    if (!file) {
      /* FIXME: Can we recover somehow? */
      g_assert_not_reached ();
    }

    if ((success = clapper_tube_cache_write_config (file, error))) {
      gchar **dir_paths;

      dir_paths = clapper_tube_loader_obtain_plugin_dir_paths ();

      for (i = 0; dir_paths[i]; ++i) {
        if (!(success = clapper_tube_cache_write_plugins_compat (file, dir_paths[i], error))) {
          /* Since we failed at some index, remove everything that was added prior to it */
          clapper_tube_cache_remove_content ();
          break;
        }
      }

      g_strfreev (dir_paths);
    }

    fclose (file);
    GST_INFO ("Plugin cache %srewritten", success ? "" : "could not be ");
  }

finish:
  if (success) {
    GST_INFO ("Initialized cache");
  } else {
    g_ptr_array_unref (plugins_cache);
    plugins_cache = NULL;

    GST_ERROR ("Could not initialize cache");
  }

  return success;
}

GPtrArray *
clapper_tube_cache_find_plugins_for_uri (GUri *guri)
{
  GPtrArray *compatible;
  const gchar *scheme, *host;
  guint i, offset = 0;

  compatible = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_free);

  /* Cache init failed, return empty array */
  if (!plugins_cache)
    return compatible;

  scheme = g_uri_get_scheme (guri);
  host = g_uri_get_host (guri);

  /* Skip common host prefixes */
  if (g_str_has_prefix (host, "www."))
    offset = 4;
  else if (g_str_has_prefix (host, "m."))
    offset = 2;

  GST_DEBUG ("Cache query, scheme: \"%s\", host: \"%s\"",
      scheme, host + offset);

  for (i = 0; i < plugins_cache->len; ++i) {
    ClapperTubeCachePluginDirData *dir_data;
    guint j;

    dir_data = g_ptr_array_index (plugins_cache, i);
    GST_DEBUG ("Searching in cached dir: %s", dir_data->dir_path);

    for (j = 0; j < dir_data->plugins->len; ++j) {
      ClapperTubeCachePluginCompatData *data;
      gboolean plausible = FALSE;
      guint k;

      data = g_ptr_array_index (dir_data->plugins, j);

      for (k = 0; k < data->schemes->len; ++k) {
        const gchar *plugin_scheme;

        plugin_scheme = g_ptr_array_index (data->schemes, k);
        if ((plausible = strcmp (plugin_scheme, scheme) == 0))
          break;
      }

      /* Wrong scheme */
      if (!plausible)
        continue;

      /* For common http(s) scheme, also check host */
      if (g_str_has_prefix (scheme, "http")) {
        /* Disallow http(s) with no hosts */
        plausible = FALSE;

        for (k = 0; k < data->hosts->len; ++k) {
          const gchar *plugin_host;

          plugin_host = g_ptr_array_index (data->hosts, k);
          if ((plausible = strcmp (plugin_host, host + offset) == 0))
            break;
        }
      }

      if (plausible) {
        gchar *module_path;

        module_path = g_module_build_path (dir_data->dir_path,
            data->module_name);

        GST_DEBUG ("Found plausible plugin: %s", module_path);
        g_ptr_array_add (compatible, module_path);
      }
    }
  }

  return compatible;
}

static gchar *
clapper_tube_cache_plugin_encode_name (const gchar *plugin_name, const gchar *key)
{
  gchar *name, *encoded;

  name = g_strjoin (".", plugin_name, key, NULL);
  encoded = g_base64_encode ((guchar*) name, strlen (name));
  g_free (name);

  return encoded;
}

/**
 * clapper_tube_cache_plugin_read:
 * @plugin_name: short and unique name of plugin.
 * @key: name of the key this value is associated with.
 *
 * Reads the value of a given plugin name with key.
 *
 * Returns: (transfer full): cached value or %NULL if unavailable or expired.
 */
gchar *
clapper_tube_cache_plugin_read (const gchar *plugin_name, const gchar *key)
{
  FILE *file;
  gchar *encoded, *str = NULL;
  gboolean success = FALSE;

  g_return_val_if_fail (plugin_name != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  GST_DEBUG ("Reading from \"%s\" cache \"%s\" data",
      plugin_name, key);

  encoded = clapper_tube_cache_plugin_encode_name (plugin_name, key);

  g_mutex_lock (&cache_lock);

  file = clapper_tube_cache_open_read (encoded);
  g_free (encoded);

  if (file) {
    GDateTime *date_time;
    gint64 curr_time, exp_time;

    date_time = g_date_time_new_now_utc ();
    curr_time = g_date_time_to_unix (date_time);
    g_date_time_unref (date_time);

    read_file_to_ptr (file, &exp_time, sizeof (gint64));

    if ((success = exp_time > curr_time)) {
      str = read_next_string (file);
      GST_DEBUG ("Read cached value: %s", str);
    } else {
      GST_DEBUG ("Cache expired");
    }

    fclose (file);
  }

  g_mutex_unlock (&cache_lock);

  return str;
}

/**
 * clapper_tube_cache_plugin_write:
 * @plugin_name: short and unique name of plugin.
 * @key: name of the key this value is associated with.
 * @val: (nullable): value to store in cache file.
 * @exp: expire time in seconds from now.
 *
 * Writes the value of a given plugin name with key. This function
 * uses time in seconds to set how long cached value will stay valid.
 */
void
clapper_tube_cache_plugin_write (const gchar *plugin_name,
    const gchar *key, const gchar *val, gint64 exp)
{
  GDateTime *date_time;
  gint64 epoch;

  g_return_if_fail (exp > 0);

  date_time = g_date_time_new_now_utc ();
  epoch = g_date_time_to_unix (date_time);
  g_date_time_unref (date_time);

  epoch += exp;

  clapper_tube_cache_plugin_write_epoch (plugin_name,
      key, val, epoch);
}

/**
 * clapper_tube_cache_plugin_write_epoch:
 * @plugin_name: short and unique name of plugin.
 * @key: name of the key this value is associated with.
 * @val: (nullable): value to store in cache file.
 * @epoch: expire date in epoch time.
 *
 * Writes the value of a given plugin name with key. This function
 * uses epoch time to set date when cached value will expire.
 */
void
clapper_tube_cache_plugin_write_epoch (const gchar *plugin_name,
    const gchar *key, const gchar *val, gint64 epoch)
{
  gchar *encoded;

  g_return_if_fail (plugin_name != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (epoch > 0);

  GST_DEBUG ("Writing into \"%s\" cache \"%s\" data",
      plugin_name, key);

  encoded = clapper_tube_cache_plugin_encode_name (plugin_name, key);

  g_mutex_lock (&cache_lock);

  /* Write value if any, otherwise simply delete the file */
  if (val) {
    FILE *file = clapper_tube_cache_open_write (encoded);

    if (file) {
      write_ptr_to_file (file, &epoch, sizeof (gint64));
      write_string (file, val);
      GST_DEBUG ("Written cache value: %s, expires: %" G_GINT64_FORMAT,
          val, epoch);

      fclose (file);
    }
  } else {
    GFile *file;
    gchar *filepath;

    filepath = clapper_tube_cache_obtain_cache_path (encoded);
    file = g_file_new_for_path (filepath);

    if (g_file_delete (file, NULL, NULL))
      GST_DEBUG ("Deleted cache file");

    g_object_unref (file);
    g_free (filepath);
  }

  g_mutex_unlock (&cache_lock);

  g_free (encoded);
}
