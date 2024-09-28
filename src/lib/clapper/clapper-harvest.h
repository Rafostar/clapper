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

#pragma once

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <clapper/clapper-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_HARVEST (clapper_harvest_get_type())
#define CLAPPER_HARVEST_CAST(obj) ((ClapperHarvest *)(obj))

G_DECLARE_FINAL_TYPE (ClapperHarvest, clapper_harvest, CLAPPER, HARVEST, GstObject)

CLAPPER_API
gboolean clapper_harvest_fill (ClapperHarvest *harvest, gchar *data);

CLAPPER_API
GstTagList * clapper_harvest_get_tags (ClapperHarvest *harvest);

CLAPPER_API
GstToc * clapper_harvest_get_toc (ClapperHarvest *harvest);

CLAPPER_API
GstStructure * clapper_harvest_get_request_headers (ClapperHarvest *harvest);

G_END_DECLS
