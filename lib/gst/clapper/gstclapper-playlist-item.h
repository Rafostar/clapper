/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_CLAPPER_PLAYLIST_ITEM_H__
#define __GST_CLAPPER_PLAYLIST_ITEM_H__

#include <gst/clapper/clapper-prelude.h>

G_BEGIN_DECLS

typedef struct _GstClapperPlaylistItem GstClapperPlaylistItem;
typedef struct _GstClapperPlaylistItemClass GstClapperPlaylistItemClass;

#define GST_TYPE_CLAPPER_PLAYLIST_ITEM             (gst_clapper_playlist_item_get_type ())
#define GST_IS_CLAPPER_PLAYLIST_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_PLAYLIST_ITEM))
#define GST_IS_CLAPPER_PLAYLIST_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_PLAYLIST_ITEM))
#define GST_CLAPPER_PLAYLIST_ITEM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_PLAYLIST_ITEM, GstClapperPlaylistItemClass))
#define GST_CLAPPER_PLAYLIST_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_PLAYLIST_ITEM, GstClapperPlaylistItem))
#define GST_CLAPPER_PLAYLIST_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_PLAYLIST_ITEM, GstClapperPlaylistItemClass))
#define GST_CLAPPER_PLAYLIST_ITEM_CAST(obj)        ((GstClapperPlaylistItem*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstClapperPlaylistItem, gst_object_unref)
#endif

GST_CLAPPER_API
GType gst_clapper_playlist_item_get_type                          (void);

GST_CLAPPER_API
GstClapperPlaylistItem * gst_clapper_playlist_item_new            (const gchar *uri);

GST_CLAPPER_API
GstClapperPlaylistItem * gst_clapper_playlist_item_new_titled     (const gchar *uri, const gchar *custom_title);

GST_CLAPPER_API
GstClapperPlaylistItem * gst_clapper_playlist_item_copy           (GstClapperPlaylistItem *item);

GST_CLAPPER_API
void gst_clapper_playlist_item_set_suburi                         (GstClapperPlaylistItem *item, const gchar *suburi);

GST_CLAPPER_API
void gst_clapper_playlist_item_activate                           (GstClapperPlaylistItem *item);

G_END_DECLS

#endif /* __GST_CLAPPER_PLAYLIST_ITEM_H__ */
