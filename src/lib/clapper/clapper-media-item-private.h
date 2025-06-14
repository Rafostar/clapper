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
#include <gst/pbutils/pbutils.h>

#include "clapper-media-item.h"
#include "clapper-player.h"
#include "clapper-app-bus-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void clapper_media_item_update_from_tag_list (ClapperMediaItem *item, const GstTagList *tags, ClapperPlayer *player);

G_GNUC_INTERNAL
void clapper_media_item_update_from_discoverer_info (ClapperMediaItem *self, GstDiscovererInfo *info);

G_GNUC_INTERNAL
gboolean clapper_media_item_set_duration (ClapperMediaItem *item, gdouble duration, ClapperAppBus *app_bus);

G_GNUC_INTERNAL
void clapper_media_item_set_cache_location (ClapperMediaItem *item, const gchar *location);

G_GNUC_INTERNAL
const gchar * clapper_media_item_get_playback_uri (ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_media_item_set_used (ClapperMediaItem *item, gboolean used);

G_GNUC_INTERNAL
gboolean clapper_media_item_get_used (ClapperMediaItem *item);

G_END_DECLS
