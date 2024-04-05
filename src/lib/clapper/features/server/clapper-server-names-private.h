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

G_BEGIN_DECLS

#define CLAPPER_SERVER_WS_EVENT_STATE "state"
#define CLAPPER_SERVER_WS_EVENT_POSITION "position"
#define CLAPPER_SERVER_WS_EVENT_SPEED "speed"
#define CLAPPER_SERVER_WS_EVENT_VOLUME "volume"
#define CLAPPER_SERVER_WS_EVENT_MUTED "muted"
#define CLAPPER_SERVER_WS_EVENT_UNMUTED "unmuted"
#define CLAPPER_SERVER_WS_EVENT_PLAYED_INDEX "played_index"
#define CLAPPER_SERVER_WS_EVENT_QUEUE_CHANGED "queue_changed"
#define CLAPPER_SERVER_WS_EVENT_QUEUE_PROGRESSION "queue_progression"

#define CLAPPER_SERVER_PLAYER_STATE_STOPPED "stopped"
#define CLAPPER_SERVER_PLAYER_STATE_BUFFERING "buffering"
#define CLAPPER_SERVER_PLAYER_STATE_PAUSED "paused"
#define CLAPPER_SERVER_PLAYER_STATE_PLAYING "playing"

#define CLAPPER_SERVER_QUEUE_PROGRESSION_NONE "none"
#define CLAPPER_SERVER_QUEUE_PROGRESSION_CONSECUTIVE "consecutive"
#define CLAPPER_SERVER_QUEUE_PROGRESSION_REPEAT_ITEM "repeat_item"
#define CLAPPER_SERVER_QUEUE_PROGRESSION_CAROUSEL "carousel"
#define CLAPPER_SERVER_QUEUE_PROGRESSION_SHUFFLE "shuffle"

G_END_DECLS
