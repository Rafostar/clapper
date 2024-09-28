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

/**
 * ClapperPlaylistable:
 *
 * An interface for creating addons that resolve given playlist URI
 * into individual media items.
 *
 * Since: 0.8
 */

#include <gst/gst.h>

#include "clapper-playlistable-private.h"

G_DEFINE_INTERFACE (ClapperPlaylistable, clapper_playlistable, G_TYPE_OBJECT);

static GList *
clapper_playlistable_default_parse_playlist (ClapperPlaylistable *self, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  if (*error == NULL) {
    g_set_error (error, GST_CORE_ERROR,
        GST_CORE_ERROR_NOT_IMPLEMENTED,
        "Playlistable object did not implement parse playlist function");
  }

  return FALSE;
}

static void
clapper_playlistable_default_init (ClapperPlaylistableInterface *iface)
{
  iface->parse_playlist = clapper_playlistable_default_parse_playlist;
}

GList *
clapper_playlistable_parse_playlist (ClapperPlaylistable *self, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  ClapperPlaylistableInterface *iface = CLAPPER_PLAYLISTABLE_GET_IFACE (self);

  return iface->parse_playlist (self, uri, cancellable, error);
}
