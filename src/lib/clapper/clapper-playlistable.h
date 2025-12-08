/* Clapper Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <clapper/clapper-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_PLAYLISTABLE (clapper_playlistable_get_type())
#define CLAPPER_PLAYLISTABLE_CAST(obj) ((ClapperPlaylistable *)(obj))

CLAPPER_API
G_DECLARE_INTERFACE (ClapperPlaylistable, clapper_playlistable, CLAPPER, PLAYLISTABLE, GObject)

/**
 * ClapperPlaylistableInterface:
 * @parent_iface: The parent interface structure.
 * @parse: Parse bytes and fill playlist.
 */
struct _ClapperPlaylistableInterface
{
  GTypeInterface parent_iface;

  /**
   * ClapperPlaylistableInterface::parse:
   * @playlistable: a #ClapperPlaylistable
   * @uri: a source #GUri
   * @bytes: a #GBytes
   * @playlist: a #GListStore for media items
   * @cancellable: (not nullable): a #GCancellable object
   * @error: (not nullable): a #GError
   *
   * Parse @bytes and fill @playlist with [class@Clapper.MediaItem] objects.
   *
   * If implementation returns %FALSE, whole @playlist content will be discarded.
   *
   * Returns: whether parsing was successful.
   *
   * Since: 0.10
   */
  gboolean (* parse) (ClapperPlaylistable *playlistable, GUri *uri, GBytes *bytes, GListStore *playlist, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[8];
};

G_END_DECLS
