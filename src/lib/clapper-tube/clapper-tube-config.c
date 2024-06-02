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

#include "clapper-tube-config.h"
#include "clapper-tube-config-private.h"

#define GST_CAT_DEFAULT clapper_tube_config_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

void
clapper_tube_config_debug_init (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubeconfig", 0,
      "Clapper Tube Config");
}

/**
 * clapper_tube_config_obtain_config_dir_path:
 *
 * Obtains a complete filename leading to clapper tube config
 * folder within user config directory.
 *
 * Returns: (transfer full): filename of config directory.
 */
gchar *
clapper_tube_config_obtain_config_dir_path (void)
{
  return clapper_tube_config_obtain_config_file_path (NULL);
}

/**
 * clapper_tube_config_obtain_config_file_path:
 * @file_name: name of the config file
 *
 * Obtains a complete filename leading to a file within
 * clapper tube config directory.
 *
 * Returns: (transfer full): filename of config file.
 */
gchar *
clapper_tube_config_obtain_config_file_path (const gchar *file_name)
{
  /* file_name can be NULL here */

  return g_build_filename (g_get_user_config_dir (),
      GTUBER_API_NAME, file_name, NULL);
}

/**
 * clapper_tube_config_obtain_config_dir:
 *
 * Obtains a #GFile from path leading to clapper tube config directory.
 *
 * Returns: (transfer full): a #GFile from clapper tube config directory.
 */
GFile *
clapper_tube_config_obtain_config_dir (void)
{
  return clapper_tube_config_obtain_config_dir_file (NULL);
}

/**
 * clapper_tube_config_obtain_config_dir_file:
 * @file_name: name of the config file
 *
 * Obtains a #GFile from file within clapper tube config directory.
 *
 * Returns: (transfer full): a #GFile from file within clapper tube config directory.
 */
GFile *
clapper_tube_config_obtain_config_dir_file (const gchar *file_name)
{
  GFile *file;
  gchar *path;

  /* file_name can be NULL here */

  path = clapper_tube_config_obtain_config_file_path (file_name);
  file = g_file_new_for_path (path);
  g_free (path);

  return file;
}

/**
 * clapper_tube_config_read_plugin_hosts_file:
 * @file_name: name of the config file
 *
 * Obtains a list of hosts read from file within clapper tube config directory.
 *
 * Returns: (transfer full): a list of hosts from config file.
 */
gchar **
clapper_tube_config_read_plugin_hosts_file (const gchar *file_name)
{
  GFile *file;
  gchar *file_path;
  gchar **hosts = NULL;

  g_return_val_if_fail (file_name != NULL, NULL);

  file_path = clapper_tube_config_obtain_config_file_path (file_name);
  file = g_file_new_for_path (file_path);
  GST_DEBUG ("Reading hosts file: %s", file_path);

  g_free (file_path);

  if (!g_file_query_exists (file, NULL)) {
    GST_DEBUG ("Hosts file does not exist");
  } else {
    GFileInputStream *stream;
    GDataInputStream *dstream;
    guint n_hosts = 0;

    stream = g_file_read (file, NULL, NULL);
    if (!stream)
      return NULL;

    dstream = g_data_input_stream_new (G_INPUT_STREAM (stream));

    if (dstream) {
      GStrvBuilder *builder;
      gchar *line;

      builder = g_strv_builder_new ();

      while ((line = g_data_input_stream_read_line (dstream, NULL, NULL, NULL))) {
        g_strstrip (line);

        if (strlen (line) > 0) {
          g_strv_builder_add (builder, line);
          n_hosts++;
        }

        g_free (line);
      }

      g_object_unref (dstream);

      hosts = g_strv_builder_end (builder);
      g_strv_builder_unref (builder);
    }

    g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
    g_object_unref (stream);

    GST_DEBUG ("Found hosts in file: %u", n_hosts);
  }

  g_object_unref (file);

  return hosts;
}

/**
 * clapper_tube_config_read_plugin_hosts_file_with_prepend:
 * @file_name: name of the config file
 * @...: %NULL terminated list of additional hosts to prepend
 *
 * Obtains a list of hosts read from file within clapper tube config directory.
 * Additional hosts are prepended to the list without modifying file contents.
 *
 * Returns: (transfer full): a combined list of hosts.
 */
gchar **
clapper_tube_config_read_plugin_hosts_file_with_prepend (const gchar *file_name, ...)
{
  GStrvBuilder *builder;
  gchar **file_hosts, **merged;
  const gchar *another_host;
  va_list args;

  g_return_val_if_fail (file_name != NULL, NULL);

  builder = g_strv_builder_new ();

  va_start (args, file_name);

  while ((another_host = va_arg (args, const gchar *)))
    g_strv_builder_add (builder, another_host);

  va_end (args);

  if ((file_hosts = clapper_tube_config_read_plugin_hosts_file (file_name))) {
    g_strv_builder_addv (builder, file_hosts);
    g_strfreev (file_hosts);
  }

  merged = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  return merged;
}
