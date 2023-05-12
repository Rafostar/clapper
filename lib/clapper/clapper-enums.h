/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

/**
 * SECTION:clapper-enums
 * @title: Enumeration Types
 */

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <clapper/clapper-enum-types.h>

G_BEGIN_DECLS

/**
 * ClapperPlayerState:
 * @CLAPPER_PLAYER_STATE_STOPPED: player is stopped.
 * @CLAPPER_PLAYER_STATE_BUFFERING: player is buffering.
 * @CLAPPER_PLAYER_STATE_PAUSED: player is paused.
 * @CLAPPER_PLAYER_STATE_PLAYING: player is playing.
 */
typedef enum
{
  CLAPPER_PLAYER_STATE_STOPPED = 0,
  CLAPPER_PLAYER_STATE_BUFFERING,
  CLAPPER_PLAYER_STATE_PAUSED,
  CLAPPER_PLAYER_STATE_PLAYING,
} ClapperPlayerState;

/**
 * ClapperPlayerSeekMethod:
 * @CLAPPER_PLAYER_SEEK_METHOD_ACCURATE: seek to exact position (slow).
 * @CLAPPER_PLAYER_SEEK_METHOD_NORMAL: seek to approximated position.
 * @CLAPPER_PLAYER_SEEK_METHOD_FAST: seek to position of nearest keyframe (fast).
 */
typedef enum
{
  CLAPPER_PLAYER_SEEK_METHOD_ACCURATE = 0,
  CLAPPER_PLAYER_SEEK_METHOD_NORMAL,
  CLAPPER_PLAYER_SEEK_METHOD_FAST,
} ClapperPlayerSeekMethod;

/**
 * ClapperQueueProgressionMode:
 * @CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE: queue plays items one after another until the end.
 * @CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM: queue keeps repeating current media item.
 * @CLAPPER_QUEUE_PROGRESSION_CAROUSEL: queue starts from beginning after last media item.
 * @CLAPPER_QUEUE_PROGRESSION_SHUFFLE: queue selects a random media item after current one.
 */
typedef enum
{
  CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE = 0,
  CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM,
  CLAPPER_QUEUE_PROGRESSION_CAROUSEL,
  CLAPPER_QUEUE_PROGRESSION_SHUFFLE,
} ClapperQueueProgressionMode;

G_END_DECLS
