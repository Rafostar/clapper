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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gst/tag/tag.h>

#include "clapper-harvest.h"
#include "clapper-enhancer-proxy.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
ClapperHarvest * clapper_harvest_new (void);

G_GNUC_INTERNAL
gboolean clapper_harvest_unpack (ClapperHarvest *harvest, GstBuffer **buffer, gsize *buf_size, GstCaps **caps, GstTagList **tags, GstToc **toc, GstStructure **headers);

G_GNUC_INTERNAL
gboolean clapper_harvest_fill_from_cache (ClapperHarvest *harvest, ClapperEnhancerProxy *proxy, const GstStructure *config, GUri *uri);

G_GNUC_INTERNAL
void clapper_harvest_export_to_cache (ClapperHarvest *harvest, ClapperEnhancerProxy *proxy, const GstStructure *config, GUri *uri);

G_END_DECLS
