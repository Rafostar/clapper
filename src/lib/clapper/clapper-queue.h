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

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <clapper/clapper-enums.h>
#include <clapper/clapper-media-item.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_QUEUE (clapper_queue_get_type())
#define CLAPPER_QUEUE_CAST(obj) ((ClapperQueue *)(obj))

G_DECLARE_FINAL_TYPE (ClapperQueue, clapper_queue, CLAPPER, QUEUE, GstObject)

/**
 * CLAPPER_QUEUE_INVALID_POSITION:
 *
 * The value used to refer to an invalid position in a #ClapperQueue
 */
#define CLAPPER_QUEUE_INVALID_POSITION ((guint) 0xffffffff)

void clapper_queue_add_item (ClapperQueue *queue, ClapperMediaItem *item);

void clapper_queue_insert_item (ClapperQueue *queue, ClapperMediaItem *item, gint index);

void clapper_queue_reposition_item (ClapperQueue *queue, ClapperMediaItem *item, gint index);

void clapper_queue_remove_item (ClapperQueue *queue, ClapperMediaItem *item);

void clapper_queue_remove_index (ClapperQueue *queue, guint index);

ClapperMediaItem * clapper_queue_steal_index (ClapperQueue *queue, guint index);

void clapper_queue_clear (ClapperQueue *queue);

gboolean clapper_queue_select_item (ClapperQueue *queue, ClapperMediaItem *item);

gboolean clapper_queue_select_index (ClapperQueue *queue, guint index);

gboolean clapper_queue_select_next_item (ClapperQueue *queue);

gboolean clapper_queue_select_previous_item (ClapperQueue *queue);

ClapperMediaItem * clapper_queue_get_item (ClapperQueue *queue, guint index);

ClapperMediaItem * clapper_queue_get_current_item (ClapperQueue *queue);

guint clapper_queue_get_current_index (ClapperQueue *queue);

gboolean clapper_queue_item_is_current (ClapperQueue *queue, ClapperMediaItem *item);

gboolean clapper_queue_find_item (ClapperQueue *queue, ClapperMediaItem *item, guint *index);

guint clapper_queue_get_n_items (ClapperQueue *queue);

void clapper_queue_set_progression_mode (ClapperQueue *queue, ClapperQueueProgressionMode mode);

ClapperQueueProgressionMode clapper_queue_get_progression_mode (ClapperQueue *queue);

void clapper_queue_set_gapless (ClapperQueue *queue, gboolean gapless);

gboolean clapper_queue_get_gapless (ClapperQueue *queue);

void clapper_queue_set_instant (ClapperQueue *queue, gboolean instant);

gboolean clapper_queue_get_instant (ClapperQueue *queue);

G_END_DECLS
