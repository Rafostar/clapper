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

#include <glib.h>

#include "clapper-tube-extractor.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void clapper_tube_loader_init_internal (void);

G_GNUC_INTERNAL
ClapperTubeExtractor * clapper_tube_loader_get_extractor_for_uri (GUri *guri);

G_GNUC_INTERNAL
gboolean clapper_tube_loader_check_plugin_compat (const gchar *module_path, const gchar *const **schemes, const gchar *const **hosts);

G_GNUC_INTERNAL
gchar ** clapper_tube_loader_obtain_plugin_dir_paths (void);

G_END_DECLS
