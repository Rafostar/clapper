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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include <clapper/clapper-stream-list.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_MEDIA_ITEM (clapper_media_item_get_type())
#define CLAPPER_MEDIA_ITEM_CAST(obj) ((ClapperMediaItem *)(obj))

G_DECLARE_FINAL_TYPE (ClapperMediaItem, clapper_media_item, CLAPPER, MEDIA_ITEM, GstObject)

ClapperMediaItem * clapper_media_item_new (const gchar *uri);

ClapperMediaItem * clapper_media_item_new_take (gchar *uri);

ClapperMediaItem * clapper_media_item_new_from_file (GFile *file);

guint clapper_media_item_get_id (ClapperMediaItem *item);

const gchar * clapper_media_item_get_uri (ClapperMediaItem *item);

gchar * clapper_media_item_get_title (ClapperMediaItem *item);

gchar * clapper_media_item_get_container_format (ClapperMediaItem *item);

gfloat clapper_media_item_get_duration (ClapperMediaItem *item);

ClapperStreamList * clapper_media_item_get_video_streams (ClapperMediaItem *item);

ClapperStreamList * clapper_media_item_get_audio_streams (ClapperMediaItem *item);

ClapperStreamList * clapper_media_item_get_subtitle_streams (ClapperMediaItem *item);

G_END_DECLS
