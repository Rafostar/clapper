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

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  CLAPPER_PLAYER_PLAY_FLAG_VIDEO             = (1 << 0),
  CLAPPER_PLAYER_PLAY_FLAG_AUDIO             = (1 << 1),
  CLAPPER_PLAYER_PLAY_FLAG_TEXT              = (1 << 2),
  CLAPPER_PLAYER_PLAY_FLAG_VIS               = (1 << 3),
  CLAPPER_PLAYER_PLAY_FLAG_SOFT_VOLUME       = (1 << 4),
  CLAPPER_PLAYER_PLAY_FLAG_NATIVE_AUDIO      = (1 << 5),
  CLAPPER_PLAYER_PLAY_FLAG_NATIVE_VIDEO      = (1 << 6),
  CLAPPER_PLAYER_PLAY_FLAG_DOWNLOAD          = (1 << 7),
  CLAPPER_PLAYER_PLAY_FLAG_BUFFERING         = (1 << 8),
  CLAPPER_PLAYER_PLAY_FLAG_DEINTERLACE       = (1 << 9),
  CLAPPER_PLAYER_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
  CLAPPER_PLAYER_PLAY_FLAG_FORCE_FILTERS     = (1 << 11),
  CLAPPER_PLAYER_PLAY_FLAG_FORCE_SW_DECODERS = (1 << 12)
} ClapperPlayerPlayFlags;

typedef enum
{
  CLAPPER_FEATURES_MANAGER_EVENT_UNKNOWN = 0,
  CLAPPER_FEATURES_MANAGER_EVENT_FEATURE_ADDED,
  CLAPPER_FEATURES_MANAGER_EVENT_FEATURE_PROPERTY_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_STATE_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_POSITION_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_SPEED_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_VOLUME_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_MUTE_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_PLAYED_ITEM_CHANGED,
  CLAPPER_FEATURES_MANAGER_EVENT_ITEM_UPDATED,
  CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_ADDED,
  CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REMOVED,
  CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REPOSITIONED,
  CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_CLEARED,
  CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED
} ClapperFeaturesManagerEvent;

typedef enum
{
  CLAPPER_QUEUE_ITEM_CHANGE_NORMAL = 1,
  CLAPPER_QUEUE_ITEM_CHANGE_INSTANT = 2,
  CLAPPER_QUEUE_ITEM_CHANGE_GAPLESS = 3,
} ClapperQueueItemChangeMode;

G_END_DECLS
