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
#include <gio/gio.h>

#include "clapper-queue.h"
#include "clapper-media-item.h"
#include "clapper-player.h"
#include "clapper-app-bus-private.h"

G_BEGIN_DECLS

ClapperQueue * clapper_queue_new (void);

void clapper_queue_handle_played_item_changed (ClapperQueue *queue, ClapperMediaItem *played_item, ClapperAppBus *app_bus);

void clapper_queue_handle_playlist (ClapperQueue *queue, ClapperMediaItem *playlist_item, GListStore *playlist);

void clapper_queue_handle_about_to_finish (ClapperQueue *queue, ClapperPlayer *player);

gboolean clapper_queue_handle_eos (ClapperQueue *queue, ClapperPlayer *player);

G_END_DECLS
