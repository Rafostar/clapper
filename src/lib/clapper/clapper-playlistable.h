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
#include <gio/gio.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-media-item.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_PLAYLISTABLE (clapper_playlistable_get_type())
#define CLAPPER_PLAYLISTABLE_CAST(obj) ((ClapperPlaylistable *)(obj))

CLAPPER_API
G_DECLARE_INTERFACE (ClapperPlaylistable, clapper_playlistable, CLAPPER, PLAYLISTABLE, GObject)

/**
 * ClapperPlaylistableInterface:
 * @parent_iface: The parent interface structure.
 * @parse_playlist: Parse playlist URI into individual media items.
 */
struct _ClapperPlaylistableInterface
{
  GTypeInterface parent_iface;

  /**
   * ClapperPlaylistableInterface::parse_playlist:
   * @playlistable: a #ClapperPlaylistable
   * @uri: a #GUri
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Parse playlist URI into individual media items.
   *
   * Returns: (transfer full) (nullable) (element-type ClapperMediaItem): a #GList of media items
   *   or %NULL on failure with @error set.
   */
  GList * (* parse_playlist) (ClapperPlaylistable *playlistable, GUri *uri, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[4];
};

G_END_DECLS
