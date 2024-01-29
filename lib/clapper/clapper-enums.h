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

/**
 * ClapperStreamType:
 * @CLAPPER_STREAM_TYPE_UNKNOWN: unknown stream type.
 * @CLAPPER_STREAM_TYPE_VIDEO: stream is a #ClapperVideoStream.
 * @CLAPPER_STREAM_TYPE_AUDIO: stream is a #ClapperAudioStream.
 * @CLAPPER_STREAM_TYPE_SUBTITLE: stream is a #ClapperSubtitleStream.
 */
typedef enum
{
  CLAPPER_STREAM_TYPE_UNKNOWN = 0,
  CLAPPER_STREAM_TYPE_VIDEO,
  CLAPPER_STREAM_TYPE_AUDIO,
  CLAPPER_STREAM_TYPE_SUBTITLE,
} ClapperStreamType;

/**
 * ClapperDiscovererDiscoveryMode:
 * @CLAPPER_DISCOVERER_DISCOVERY_ALWAYS: Run discovery for every single media item added to [class@Clapper.Queue].
 *   This mode is useful when application presents a list of items to select from to the user before playback.
 *   It will scan every single item in queue, so user can have an updated list of items when selecting what to play.
 * @CLAPPER_DISCOVERER_DISCOVERY_NONCURRENT: Only run discovery on an item if it is not a currently selected item in [class@Clapper.Queue].
 *   This mode is optimal when application always plays (or at least goes into paused) after selecting item from queue.
 *   It will skip discovery of such items since they will be discovered by [class@Clapper.Player] anyway.
 */
typedef enum
{
  CLAPPER_DISCOVERER_DISCOVERY_ALWAYS = 0,
  CLAPPER_DISCOVERER_DISCOVERY_NONCURRENT,
} ClapperDiscovererDiscoveryMode;

G_END_DECLS
