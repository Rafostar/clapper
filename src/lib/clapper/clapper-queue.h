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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-enums.h>
#include <clapper/clapper-media-item.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_QUEUE (clapper_queue_get_type())
#define CLAPPER_QUEUE_CAST(obj) ((ClapperQueue *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperQueue, clapper_queue, CLAPPER, QUEUE, GstObject)

/**
 * CLAPPER_QUEUE_INVALID_POSITION:
 *
 * The value used to refer to an invalid position in a #ClapperQueue
 */
#define CLAPPER_QUEUE_INVALID_POSITION ((guint) 0xffffffff)

CLAPPER_API
void clapper_queue_add_item (ClapperQueue *queue, ClapperMediaItem *item);

CLAPPER_API
void clapper_queue_insert_item (ClapperQueue *queue, ClapperMediaItem *item, gint index);

CLAPPER_API
void clapper_queue_insert_item_after (ClapperQueue *queue, ClapperMediaItem *item, ClapperMediaItem *after_item);

CLAPPER_API
void clapper_queue_reposition_item (ClapperQueue *queue, ClapperMediaItem *item, gint index);

CLAPPER_API
void clapper_queue_remove_item (ClapperQueue *queue, ClapperMediaItem *item);

CLAPPER_API
void clapper_queue_remove_index (ClapperQueue *queue, guint index);

CLAPPER_API
ClapperMediaItem * clapper_queue_steal_index (ClapperQueue *queue, guint index);

CLAPPER_API
void clapper_queue_clear (ClapperQueue *queue);

CLAPPER_API
gboolean clapper_queue_select_item (ClapperQueue *queue, ClapperMediaItem *item);

CLAPPER_API
gboolean clapper_queue_select_index (ClapperQueue *queue, guint index);

CLAPPER_API
gboolean clapper_queue_select_next_item (ClapperQueue *queue);

CLAPPER_API
gboolean clapper_queue_select_previous_item (ClapperQueue *queue);

CLAPPER_API
ClapperMediaItem * clapper_queue_get_item (ClapperQueue *queue, guint index);

CLAPPER_API
ClapperMediaItem * clapper_queue_get_current_item (ClapperQueue *queue);

CLAPPER_API
guint clapper_queue_get_current_index (ClapperQueue *queue);

CLAPPER_API
gboolean clapper_queue_item_is_current (ClapperQueue *queue, ClapperMediaItem *item);

CLAPPER_API
gboolean clapper_queue_find_item (ClapperQueue *queue, ClapperMediaItem *item, guint *index);

CLAPPER_API
guint clapper_queue_get_n_items (ClapperQueue *queue);

CLAPPER_API
void clapper_queue_set_progression_mode (ClapperQueue *queue, ClapperQueueProgressionMode mode);

CLAPPER_API
ClapperQueueProgressionMode clapper_queue_get_progression_mode (ClapperQueue *queue);

CLAPPER_API
void clapper_queue_set_gapless (ClapperQueue *queue, gboolean gapless);

CLAPPER_API
gboolean clapper_queue_get_gapless (ClapperQueue *queue);

CLAPPER_API
void clapper_queue_set_instant (ClapperQueue *queue, gboolean instant);

CLAPPER_API
gboolean clapper_queue_get_instant (ClapperQueue *queue);

G_END_DECLS
