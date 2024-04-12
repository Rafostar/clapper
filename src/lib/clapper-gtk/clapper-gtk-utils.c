/* Clapper GTK Integration Library
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

#include "config.h"

#include <glib/gi18n-lib.h>

#include "clapper-gtk-utils-private.h"
#include "clapper-gtk-video.h"

static gboolean initialized = FALSE;

/**
 * clapper_gtk_get_player_from_ancestor:
 * @widget: a #GtkWidget
 *
 * Get [class@Clapper.Player] used by [class@ClapperGtk.Video] ancestor of @widget.
 *
 * This utility is a convenience wrapper for calling [method@Gtk.Widget.get_ancestor]
 * of type `CLAPPER_GTK_TYPE_VIDEO` and [method@ClapperGtk.Video.get_player] with
 * additional %NULL checking and type casting.
 *
 * This is meant to be used mainly for custom widget development as an easy access to the
 * underlying parent [class@Clapper.Player] object. If you want to get the player from
 * [class@ClapperGtk.Video] widget itself, use [method@ClapperGtk.Video.get_player] instead.
 *
 * Rememeber that this function will return %NULL when widget does not have
 * a [class@ClapperGtk.Video] ancestor in widget hierarchy (widget is not yet placed).
 *
 * Returns: (transfer none) (nullable): a #ClapperPlayer from ancestor of a @widget.
 */
ClapperPlayer *
clapper_gtk_get_player_from_ancestor (GtkWidget *widget)
{
  GtkWidget *parent;
  ClapperPlayer *player = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if ((parent = gtk_widget_get_ancestor (widget, CLAPPER_GTK_TYPE_VIDEO)))
    player = clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (parent));

  return player;
}

void
clapper_gtk_init_translations (void)
{
  const gchar *clapper_gtk_ldir;

  if (initialized)
    return;

  if (!(clapper_gtk_ldir = g_getenv ("CLAPPER_GTK_OVERRIDE_LOCALEDIR")))
    clapper_gtk_ldir = LOCALEDIR;
  bindtextdomain (GETTEXT_PACKAGE, clapper_gtk_ldir);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  initialized = TRUE;
}

const gchar *
clapper_gtk_get_icon_name_for_volume (gfloat volume)
{
  return (volume <= 0.0f)
      ? "audio-volume-muted-symbolic"
      : (volume <= 0.3f)
      ? "audio-volume-low-symbolic"
      : (volume <= 0.7f)
      ? "audio-volume-medium-symbolic"
      : (volume <= 1.0f)
      ? "audio-volume-high-symbolic"
      : "audio-volume-overamplified-symbolic";
}

const gchar *
clapper_gtk_get_icon_name_for_speed (gfloat speed)
{
  return (speed < 1.0f)
      ? "power-profile-power-saver-symbolic"
      : (speed == 1.0f)
      ? "power-profile-balanced-symbolic"
      : "power-profile-performance-symbolic";
}
