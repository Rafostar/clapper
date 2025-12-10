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
#include <gst/gst.h>

#include "clapper-enums-private.h"
#include "clapper-player.h"
#include "clapper-media-item.h"

G_BEGIN_DECLS

void clapper_playbin_bus_initialize (void);

gboolean clapper_playbin_bus_message_func (GstBus *bus, GstMessage *msg, ClapperPlayer *player);

void clapper_playbin_bus_post_set_volume (GstBus *bus, GstElement *playbin, gdouble volume);

void clapper_playbin_bus_post_set_prop (GstBus *bus, GstObject *src, const gchar *name, GValue *value);

void clapper_playbin_bus_post_set_play_flag (GstBus *bus, ClapperPlayerPlayFlags flag, gboolean enabled);

void clapper_playbin_bus_post_request_state (GstBus *bus, ClapperPlayer *player, GstState state);

void clapper_playbin_bus_post_seek (GstBus *bus, gdouble position, ClapperPlayerSeekMethod flags);

void clapper_playbin_bus_post_rate_change (GstBus *bus, gdouble rate);

void clapper_playbin_bus_post_advance_frame (GstBus *bus);

void clapper_playbin_bus_post_stream_change (GstBus *bus);

void clapper_playbin_bus_post_current_item_change (GstBus *bus, ClapperMediaItem *current_item, ClapperQueueItemChangeMode mode);

void clapper_playbin_bus_post_item_suburi_change (GstBus *bus, ClapperMediaItem *item);

void clapper_playbin_bus_post_user_message (GstBus *bus, GstMessage *msg);

G_END_DECLS
