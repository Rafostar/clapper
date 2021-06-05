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

#ifndef __GST_CLAPPER_PLAYLIST_ITEM_PRIVATE_H__
#define __GST_CLAPPER_PLAYLIST_ITEM_PRIVATE_H__

#include "gstclapper-playlist.h"

struct _GstClapperPlaylistItem
{
  GstObject parent;

  /* ID of the playlist this item belongs to */
  gchar *owner_uuid;
  gint id;

  gchar *uri;
  gchar *suburi;
  gchar *custom_title;

  /* Signals */
  gulong activated_signal_id;
};

struct _GstClapperPlaylistItemClass
{
  GstObjectClass parent_class;
};

G_GNUC_INTERNAL
void gst_clapper_playlist_item_mark_added (GstClapperPlaylistItem *item, GstClapperPlaylist *playlist);

#endif /* __GST_CLAPPER_PLAYLIST_ITEM_PRIVATE_H__ */
