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

/**
 * ClapperQueue:
 */
G_DECLARE_FINAL_TYPE (ClapperQueue, clapper_queue, CLAPPER, QUEUE, GstObject)

void clapper_queue_add_item (ClapperQueue *queue, ClapperMediaItem *item);

void clapper_queue_insert_item (ClapperQueue *queue, ClapperMediaItem *item, gint index);

void clapper_queue_remove_item (ClapperQueue *queue, ClapperMediaItem *item);

void clapper_queue_clear (ClapperQueue *queue);

gboolean clapper_queue_select_item (ClapperQueue *queue, ClapperMediaItem *item);

gboolean clapper_queue_select_next_item (ClapperQueue *queue);

gboolean clapper_queue_select_previous_item (ClapperQueue *queue);

ClapperMediaItem * clapper_queue_get_item (ClapperQueue *queue, guint index);

ClapperMediaItem * clapper_queue_get_current_item (ClapperQueue *queue);

gboolean clapper_queue_find_item (ClapperQueue *queue, ClapperMediaItem *item, guint *index);

guint clapper_queue_get_n_items (ClapperQueue *queue);

void clapper_queue_set_progression_mode (ClapperQueue *queue, ClapperQueueProgressionMode mode);

ClapperQueueProgressionMode clapper_queue_get_progression_mode (ClapperQueue *queue);

/* NOTE: Retrieving other items is possible through g_list_model_get_(item/object) APIs */

G_END_DECLS
