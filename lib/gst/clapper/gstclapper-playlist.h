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

#ifndef __GST_CLAPPER_PLAYLIST_H__
#define __GST_CLAPPER_PLAYLIST_H__

#include <gst/clapper/clapper-prelude.h>
#include <gst/clapper/gstclapper-playlist-item.h>

G_BEGIN_DECLS

typedef struct _GstClapperPlaylist GstClapperPlaylist;
typedef struct _GstClapperPlaylistClass GstClapperPlaylistClass;

#define GST_TYPE_CLAPPER_PLAYLIST             (gst_clapper_playlist_get_type ())
#define GST_IS_CLAPPER_PLAYLIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_PLAYLIST))
#define GST_IS_CLAPPER_PLAYLIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_PLAYLIST))
#define GST_CLAPPER_PLAYLIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_PLAYLIST, GstClapperPlaylistClass))
#define GST_CLAPPER_PLAYLIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_PLAYLIST, GstClapperPlaylist))
#define GST_CLAPPER_PLAYLIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_PLAYLIST, GstClapperPlaylistClass))
#define GST_CLAPPER_PLAYLIST_CAST(obj)        ((GstClapperPlaylist*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstClapperPlaylist, g_object_unref)
#endif

GST_CLAPPER_API
GType gst_clapper_playlist_get_type                      (void);

GST_CLAPPER_API
GstClapperPlaylist * gst_clapper_playlist_new            (void);

GST_CLAPPER_API
gboolean gst_clapper_playlist_append                     (GstClapperPlaylist *playlist, GstClapperPlaylistItem *item);

GST_CLAPPER_API
guint gst_clapper_playlist_get_length                    (GstClapperPlaylist *playlist);

GST_CLAPPER_API
GstClapperPlaylistItem *
    gst_clapper_playlist_get_item_at_index               (GstClapperPlaylist *playlist, gint index);

GST_CLAPPER_API
GstClapperPlaylistItem *
    gst_clapper_playlist_get_active_item                 (GstClapperPlaylist *playlist);

GST_CLAPPER_API
gboolean gst_clapper_playlist_remove_item_at_index       (GstClapperPlaylist *playlist, guint index);

GST_CLAPPER_API
gboolean gst_clapper_playlist_remove_item                (GstClapperPlaylist *playlist, GstClapperPlaylistItem *item);

G_END_DECLS

#endif /* __GST_CLAPPER_PLAYLIST_H__ */
