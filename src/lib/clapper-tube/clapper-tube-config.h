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

#pragma once

#if !defined(__CLAPPER_TUBE_INSIDE__) && !defined(CLAPPER_TUBE_COMPILATION)
#error "Only <clapper-tube/clapper-tube.h> can be included directly."
#endif

#include <glib.h>
#include <gio/gio.h>

#include <clapper-tube/clapper-tube-visibility.h>

G_BEGIN_DECLS

CLAPPER_TUBE_API
gchar * clapper_tube_config_obtain_config_dir_path (void);

CLAPPER_TUBE_API
gchar * clapper_tube_config_obtain_config_file_path (const gchar *file_name);

CLAPPER_TUBE_API
GFile * clapper_tube_config_obtain_config_dir (void);

CLAPPER_TUBE_API
GFile * clapper_tube_config_obtain_config_dir_file (const gchar *file_name);

CLAPPER_TUBE_API
gchar ** clapper_tube_config_read_plugin_hosts_file (const gchar *file_name);

CLAPPER_TUBE_API
gchar ** clapper_tube_config_read_plugin_hosts_file_with_prepend (const gchar *file_name, ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS
