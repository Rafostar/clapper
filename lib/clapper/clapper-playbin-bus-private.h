/*
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
#include <gst/gst.h>
#include <clapper/clapper-player.h>
#include <clapper/clapper-media-item.h>

#include "clapper-enums-private.h"

G_BEGIN_DECLS

void clapper_playbin_bus_initialize (void);

gboolean clapper_playbin_bus_message_func (GstBus *bus, GstMessage *msg, ClapperPlayer *player);

void clapper_playbin_bus_post_set_volume (GstBus *bus, GstElement *playbin, gfloat volume);

void clapper_playbin_bus_post_set_prop (GstBus *bus, GstObject *src, const gchar *name, GValue *value);

void clapper_playbin_bus_post_request_state (GstBus *bus, ClapperPlayer *player, GstState state);

void clapper_playbin_bus_post_seek (GstBus *bus, gfloat position, ClapperPlayerSeekMethod flags);

void clapper_playbin_bus_post_rate_change (GstBus *bus, gfloat rate);

void clapper_playbin_bus_post_stream_change (GstBus *bus, ClapperMediaItem *item);

void clapper_playbin_bus_post_current_item_change (GstBus *bus, ClapperMediaItem *current_item, ClapperQueueItemChangeMode mode);

G_END_DECLS
