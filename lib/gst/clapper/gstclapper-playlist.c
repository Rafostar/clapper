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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapper-playlist.h"
#include "gstclapper-playlist-private.h"
#include "gstclapper-playlist-item.h"
#include "gstclapper-playlist-item-private.h"

enum
{
  SIGNAL_ITEM_ACTIVATED,
  SIGNAL_LAST
};

#define parent_class gst_clapper_playlist_parent_class
G_DEFINE_TYPE (GstClapperPlaylist, gst_clapper_playlist, GST_TYPE_OBJECT);

static guint signals[SIGNAL_LAST] = { 0, };

static void gst_clapper_playlist_dispose (GObject * object);
static void gst_clapper_playlist_finalize (GObject * object);

static void
gst_clapper_playlist_init (GstClapperPlaylist * self)
{
  self->uuid = g_uuid_string_random ();
  self->id_count = 0;
  self->items = g_array_new (FALSE, FALSE, sizeof (GstClapperPlaylistItem));
  self->active_index = -1;

  g_array_set_clear_func (self->items, (GDestroyNotify) gst_object_unref);
}

static void
gst_clapper_playlist_class_init (GstClapperPlaylistClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_clapper_playlist_dispose;
  gobject_class->finalize = gst_clapper_playlist_finalize;

  signals[SIGNAL_ITEM_ACTIVATED] =
      g_signal_new ("item-activated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLAPPER_PLAYLIST_ITEM);
}

static void
gst_clapper_playlist_dispose (GObject * object)
{
  GstClapperPlaylist *self = GST_CLAPPER_PLAYLIST (object);

  /* FIXME: Need this for something? */

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_clapper_playlist_finalize (GObject * object)
{
  GstClapperPlaylist *self = GST_CLAPPER_PLAYLIST (object);

  g_free (self->uuid);
  g_array_unref (self->items);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
gst_clapper_playlist_emit_item_activated (GstClapperPlaylist * self,
    GstClapperPlaylistItem * item)
{
  g_signal_emit (self, signals[SIGNAL_ITEM_ACTIVATED], 0, item);
}

/**
 * gst_clapper_playlist_new:
 *
 * Creates a new #GstClapperPlaylist.
 *
 * Returns: (transfer full): a new #GstClapperPlaylist instance.
 */
GstClapperPlaylist *
gst_clapper_playlist_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_PLAYLIST, NULL);
}

/**
 * gst_clapper_playlist_append:
 * @playlist: #GstClapperPlaylist
 * @item: #GstClapperPlaylistItem to append
 *
 * Adds a new #GstClapperPlaylistItem to the end of playlist.
 *
 * Returns: %TRUE if the item was added successfully.
 */
gboolean
gst_clapper_playlist_append (GstClapperPlaylist * self, GstClapperPlaylistItem * item)
{
  gboolean added = FALSE;

  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST (self), FALSE);
  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST_ITEM (item), FALSE);
  g_return_val_if_fail (item->owner_uuid == NULL, FALSE);

  GST_OBJECT_LOCK (self);

  added = g_array_append_val (self->items, item) != NULL;
  if (added) {
    gst_clapper_playlist_item_mark_added (item, self);
    self->id_count++;
  }

  GST_OBJECT_UNLOCK (self);

  return added;
}

/**
 * gst_clapper_playlist_get_length:
 * @playlist: #GstClapperPlaylist
 *
 * Returns: Amount of items in playlist.
 */
guint
gst_clapper_playlist_get_length (GstClapperPlaylist * self)
{
  guint len;

  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST (self), 0);

  GST_OBJECT_LOCK (self);
  len = self->items->len;
  GST_OBJECT_UNLOCK (self);

  return len;
}

/**
 * gst_clapper_playlist_get_item_at_index:
 * @playlist: #GstClapperPlaylist
 *
 * Returns: (transfer none): A #GstClapperPlaylistItem at given index.
 */
GstClapperPlaylistItem *
gst_clapper_playlist_get_item_at_index (GstClapperPlaylist * self, gint index)
{
  GstClapperPlaylistItem *item = NULL;

  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST (self), NULL);

  GST_OBJECT_LOCK (self);

  if (index < self->items->len)
    goto out;

  item = &g_array_index (self->items, GstClapperPlaylistItem, index);

out:
  GST_OBJECT_UNLOCK (self);

  return item;
}

/**
 * gst_clapper_playlist_get_active_item:
 * @playlist: #GstClapperPlaylist
 *
 * Returns: (transfer none): A #GstClapperPlaylistItem that is
 * currently playing.
 */
GstClapperPlaylistItem *
gst_clapper_playlist_get_active_item (GstClapperPlaylist * self)
{
  gint active_index;

  GST_OBJECT_LOCK (self);
  active_index = self->active_index;
  GST_OBJECT_UNLOCK (self);

  return gst_clapper_playlist_get_item_at_index (self, active_index);
}

/**
 * gst_clapper_playlist_remove_item_at_index:
 * @playlist: #GstClapperPlaylist
 * @index: Index of #GstClapperPlaylistItem to remove
 *
 * Removes item at given index from playlist.
 *
 * Returns: %TRUE if the item was removed successfully.
 */
gboolean
gst_clapper_playlist_remove_item_at_index (GstClapperPlaylist * self, guint index)
{
  gboolean removed = FALSE;

  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST (self), FALSE);

  GST_OBJECT_LOCK (self);

  if (index >= self->items->len || index == self->active_index)
    goto out;

  removed = g_array_remove_index (self->items, index) != NULL;

out:
  GST_OBJECT_UNLOCK (self);

  return removed;
}

/**
 * gst_clapper_playlist_remove_item:
 * @playlist: #GstClapperPlaylist
 * @item: #GstClapperPlaylistItem object to remove
 *
 * Removes given playlist item from playlist.
 *
 * Returns: %TRUE if the item was removed successfully.
 */
gboolean
gst_clapper_playlist_remove_item (GstClapperPlaylist * self,
    GstClapperPlaylistItem * item)
{
  gint i;
  gboolean removed = FALSE;

  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST (self), FALSE);
  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST_ITEM (item), FALSE);

  GST_OBJECT_LOCK (self);

  if (strcmp (self->uuid, item->owner_uuid) != 0)
    goto out;

  for (i = 0; i < self->items->len; i++) {
    GstClapperPlaylistItem *curr_item;

    curr_item = &g_array_index (self->items, GstClapperPlaylistItem, i);
    if (!curr_item)
      goto out;

    if (item->id == curr_item->id) {
      removed = g_array_remove_index (self->items, i) != NULL;
      break;
    }
  }

out:
  GST_OBJECT_UNLOCK (self);

  return removed;
}
