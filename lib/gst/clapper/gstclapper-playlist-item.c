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

#include "gstclapper-playlist-item.h"
#include "gstclapper-playlist-item-private.h"
#include "gstclapper-playlist-private.h"

enum
{
  PROP_0,
  PROP_URI,
  PROP_SUBURI,
  PROP_CUSTOM_TITLE,
  PROP_LAST
};

enum
{
  SIGNAL_ACTIVATED,
  SIGNAL_LAST
};

#define parent_class gst_clapper_playlist_item_parent_class
G_DEFINE_TYPE (GstClapperPlaylistItem, gst_clapper_playlist_item, GST_TYPE_OBJECT);

static guint signals[SIGNAL_LAST] = { 0, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_clapper_playlist_item_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_clapper_playlist_item_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_clapper_playlist_item_dispose (GObject * object);
static void gst_clapper_playlist_item_finalize (GObject * object);

static void
gst_clapper_playlist_item_init (GstClapperPlaylistItem * self)
{
  self->owner_uuid = NULL;
  self->id = -1;

  self->uri = NULL;
  self->suburi = NULL;
  self->custom_title = NULL;
}

static void
gst_clapper_playlist_item_class_init (GstClapperPlaylistItemClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_clapper_playlist_item_set_property;
  gobject_class->get_property = gst_clapper_playlist_item_get_property;
  gobject_class->dispose = gst_clapper_playlist_item_dispose;
  gobject_class->finalize = gst_clapper_playlist_item_finalize;

  param_specs[PROP_URI] = g_param_spec_string ("uri",
      "URI", "Playlist Item URI", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SUBURI] = g_param_spec_string ("suburi",
      "Subtitle URI", "Playlist Item Subtitle URI", NULL,
       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CUSTOM_TITLE] = g_param_spec_string ("custom-title",
      "Custom Title", "Playlist Item Custom Title", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  signals[SIGNAL_ACTIVATED] =
      g_signal_new ("activated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);
}

static void
gst_clapper_playlist_item_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstClapperPlaylistItem *self = GST_CLAPPER_PLAYLIST_ITEM (object);

  switch (prop_id) {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;
    case PROP_SUBURI:
      g_free (self->suburi);
      self->suburi = g_value_dup_string (value);
      break;
    case PROP_CUSTOM_TITLE:
      self->custom_title = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_playlist_item_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstClapperPlaylistItem *self = GST_CLAPPER_PLAYLIST_ITEM (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;
    case PROP_SUBURI:
      g_value_set_string (value, self->suburi);
      break;
    case PROP_CUSTOM_TITLE:
      g_value_set_string (value, self->custom_title);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_playlist_item_dispose (GObject * object)
{
  GstClapperPlaylistItem *self = GST_CLAPPER_PLAYLIST_ITEM (object);

  if (self->activated_signal_id) {
    g_signal_handler_disconnect (self, self->activated_signal_id);
    self->activated_signal_id = 0;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_clapper_playlist_item_finalize (GObject * object)
{
  GstClapperPlaylistItem *self = GST_CLAPPER_PLAYLIST_ITEM (object);

  g_free (self->owner_uuid);

  g_free (self->uri);
  g_free (self->suburi);
  g_free (self->custom_title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
item_activate_cb (GstClapperPlaylistItem * self, GParamSpec * pspec,
    GstClapperPlaylist * playlist)
{
  gst_clapper_playlist_emit_item_activated (playlist, self);
}

void
gst_clapper_playlist_item_mark_added (GstClapperPlaylistItem * self,
    GstClapperPlaylist * playlist)
{
  GST_OBJECT_LOCK (self);

  self->owner_uuid = g_strdup (playlist->uuid);
  self->id = playlist->id_count;

  self->activated_signal_id = g_signal_connect (self, "activated",
      G_CALLBACK (item_activate_cb), playlist);

  GST_OBJECT_UNLOCK (self);
}

/**
 * gst_clapper_playlist_item_new:
 *
 * Creates a new #GstClapperPlaylistItem.
 *
 * Returns: (transfer full): a new #GstClapperPlaylistItem object.
 */
GstClapperPlaylistItem *
gst_clapper_playlist_item_new (const gchar * uri)
{
  return g_object_new (GST_TYPE_CLAPPER_PLAYLIST_ITEM, "uri", uri, NULL);
}

/**
 * gst_clapper_playlist_item_new_titled:
 * @uri: An URI pointing to media
 * @custom_title: A custom title for this item
 *
 * Creates a new #GstClapperPlaylistItem with a custom title.
 *
 * Normally item title is obtained from media info or local filename,
 * use this function for online sources where media title cannot be
 * determined or if you want to override original title for some reason.
 *
 * Returns: (transfer full): a new #GstClapperPlaylistItem object.
 */
GstClapperPlaylistItem *
gst_clapper_playlist_item_new_titled (const gchar * uri,
    const gchar * custom_title)
{
  return g_object_new (GST_TYPE_CLAPPER_PLAYLIST_ITEM, "uri", uri,
      "custom_title", custom_title, NULL);
}

/**
 * gst_clapper_playlist_item_copy:
 * @item: #GstClapperPlaylistItem
 *
 * Duplicates a #GstClapperPlaylistItem.
 *
 * Duplicated items do not belong to any playlist.
 * Use this function if you want to append the same
 * media into another #GstClapperPlaylist instance.
 *
 * Returns: (transfer full): a new #GstClapperPlaylistItem object.
 */
GstClapperPlaylistItem *
gst_clapper_playlist_item_copy (GstClapperPlaylistItem * source)
{
  g_return_val_if_fail (GST_IS_CLAPPER_PLAYLIST_ITEM (source), NULL);

  return g_object_new (GST_TYPE_CLAPPER_PLAYLIST_ITEM, "uri", source->uri,
      "suburi", source->suburi, "custom-title", source->custom_title, NULL);
}

/**
 * gst_clapper_playlist_item_set_suburi:
 * @item: #GstClapperPlaylistItem
 * @suburi: subtitle URI
 *
 * Sets the external subtitle URI.
 */
void
gst_clapper_playlist_item_set_suburi (GstClapperPlaylistItem * self,
    const gchar * suburi)
{
/* TODO: When setting this property for an item that is currently active,
 * it should be combined with a call to
 * gst_clapper_set_subtitle_track_enabled(Clapper, TRUE),
 * so the subtitles are actually rendered.
 */
  g_return_if_fail (GST_IS_CLAPPER_PLAYLIST_ITEM (self));

  g_object_set (self, "suburi", suburi, NULL);
}

/**
 * gst_clapper_playlist_item_activate:
 * @item: #GstClapperPlaylistItem
 *
 * Activates the #GstClapperPlaylistItem.
 */
void
gst_clapper_playlist_item_activate (GstClapperPlaylistItem * self)
{
  g_return_if_fail (GST_IS_CLAPPER_PLAYLIST_ITEM (self));

  g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0);
}
