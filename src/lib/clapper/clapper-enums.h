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

#include <clapper/clapper-enum-types.h>

G_BEGIN_DECLS

/**
 * ClapperPlayerState:
 * @CLAPPER_PLAYER_STATE_STOPPED: Player is stopped.
 * @CLAPPER_PLAYER_STATE_BUFFERING: Player is buffering.
 * @CLAPPER_PLAYER_STATE_PAUSED: Player is paused.
 * @CLAPPER_PLAYER_STATE_PLAYING: Player is playing.
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
 * @CLAPPER_PLAYER_SEEK_METHOD_ACCURATE: Seek to exact position (slow).
 * @CLAPPER_PLAYER_SEEK_METHOD_NORMAL: Seek to approximated position.
 * @CLAPPER_PLAYER_SEEK_METHOD_FAST: Seek to position of nearest keyframe (fast).
 */
typedef enum
{
  CLAPPER_PLAYER_SEEK_METHOD_ACCURATE = 0,
  CLAPPER_PLAYER_SEEK_METHOD_NORMAL,
  CLAPPER_PLAYER_SEEK_METHOD_FAST,
} ClapperPlayerSeekMethod;

/**
 * ClapperQueueProgressionMode:
 * @CLAPPER_QUEUE_PROGRESSION_NONE: Queue will not change current item after playback finishes.
 * @CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE: Queue selects items one after another until the end.
 *   When end of queue is reached, this mode will continue one another item is added to the queue,
 *   playing it if player autoplay property is set, otherwise current player state is kept.
 * @CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM: Queue keeps repeating current media item.
 * @CLAPPER_QUEUE_PROGRESSION_CAROUSEL: Queue starts from beginning after last media item.
 * @CLAPPER_QUEUE_PROGRESSION_SHUFFLE: Queue selects a random media item after current one.
 *   Shuffle mode will avoid reselecting previously shuffled items as long as possible.
 *   After it runs out of unused items, shuffling begins anew.
 */
typedef enum
{
  CLAPPER_QUEUE_PROGRESSION_NONE = 0,
  CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE,
  CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM,
  CLAPPER_QUEUE_PROGRESSION_CAROUSEL,
  CLAPPER_QUEUE_PROGRESSION_SHUFFLE,
} ClapperQueueProgressionMode;

/**
 * ClapperMarkerType:
 * @CLAPPER_MARKER_TYPE_UNKNOWN: Unknown marker type.
 * @CLAPPER_MARKER_TYPE_TITLE: A title marker in timeline.
 * @CLAPPER_MARKER_TYPE_CHAPTER: A chapter marker in timeline.
 * @CLAPPER_MARKER_TYPE_TRACK: A track marker in timeline.
 * @CLAPPER_STREAM_TYPE_CUSTOM_1: A custom marker 1 for free usage by application.
 * @CLAPPER_STREAM_TYPE_CUSTOM_2: A custom marker 2 for free usage by application.
 * @CLAPPER_STREAM_TYPE_CUSTOM_3: A custom marker 3 for free usage by application.
 */
typedef enum
{
  CLAPPER_MARKER_TYPE_UNKNOWN = 0,
  CLAPPER_MARKER_TYPE_TITLE,
  CLAPPER_MARKER_TYPE_CHAPTER,
  CLAPPER_MARKER_TYPE_TRACK,
  CLAPPER_MARKER_TYPE_CUSTOM_1 = 101,
  CLAPPER_MARKER_TYPE_CUSTOM_2 = 102,
  CLAPPER_MARKER_TYPE_CUSTOM_3 = 103,
} ClapperMarkerType;

/**
 * ClapperStreamType:
 * @CLAPPER_STREAM_TYPE_UNKNOWN: Unknown stream type.
 * @CLAPPER_STREAM_TYPE_VIDEO: Stream is a #ClapperVideoStream.
 * @CLAPPER_STREAM_TYPE_AUDIO: Stream is a #ClapperAudioStream.
 * @CLAPPER_STREAM_TYPE_SUBTITLE: Stream is a #ClapperSubtitleStream.
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

/* NOTE: GStreamer uses param flags 8-16, so start with 17. */
/**
 * ClapperEnhancerParamFlags:
 * @CLAPPER_ENHANCER_PARAM_GLOBAL: Use this flag for enhancer properties that should have global access scope.
 *   Such are meant for application `USER` to configure.
 * @CLAPPER_ENHANCER_PARAM_LOCAL: Use this flag for enhancer properties that should have local access scope.
 *   Such are meant for `APPLICATION` to configure.
 * @CLAPPER_ENHANCER_PARAM_FILEPATH: Use this flag for enhancer properties that store string with a file path.
 *   Applications can use this as a hint to show file selection instead of a text entry.
 * @CLAPPER_ENHANCER_PARAM_DIRPATH: Use this flag for enhancer properties that store string with a directory path.
 *   Applications can use this as a hint to show directory selection instead of a text entry.
 *
 * Additional [flags@GObject.ParamFlags] to be set in enhancer plugins implementations.
 *
 * Since: 0.10
 */
typedef enum
{
  CLAPPER_ENHANCER_PARAM_GLOBAL = 1 << 17,
  CLAPPER_ENHANCER_PARAM_LOCAL = 1 << 18,
  CLAPPER_ENHANCER_PARAM_FILEPATH = 1 << 19,
  CLAPPER_ENHANCER_PARAM_DIRPATH = 1 << 20,
} ClapperEnhancerParamFlags;

/**
 * ClapperReactableItemUpdatedFlags:
 * @CLAPPER_REACTABLE_ITEM_UPDATED_TITLE: Media item title was updated.
 * @CLAPPER_REACTABLE_ITEM_UPDATED_DURATION: Media item duration was updated.
 * @CLAPPER_REACTABLE_ITEM_UPDATED_TIMELINE: Media item timeline was updated.
 * @CLAPPER_REACTABLE_ITEM_UPDATED_TAGS: Media item tags were updated.
 *
 * Flags informing which properties were updated within [class@Clapper.MediaItem].
 *
 * Since: 0.10
 */
typedef enum
{
  CLAPPER_REACTABLE_ITEM_UPDATED_TITLE = 1 << 0,
  CLAPPER_REACTABLE_ITEM_UPDATED_DURATION = 1 << 1,
  CLAPPER_REACTABLE_ITEM_UPDATED_TIMELINE = 1 << 2,
  CLAPPER_REACTABLE_ITEM_UPDATED_TAGS = 1 << 3,
} ClapperReactableItemUpdatedFlags;

G_END_DECLS
