/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "clapper-utils-private.h"
#include "clapper-player-private.h"

#define GST_CAT_DEFAULT clapper_utils_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

void
clapper_utils_initialize (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperutils", 0,
      "Clapper Utilities");
}

void
clapper_utils_prop_notify (GstObject *src, GParamSpec *pspec)
{
  ClapperPlayer *player = clapper_player_get_from_ancestor (src);

  if (G_UNLIKELY (!player)) {
    GST_WARNING ("Could not do prop notify of %" GST_PTR_FORMAT, src);
    return;
  }

  clapper_app_bus_post_prop_notify (player->app_bus, src, pspec);
  gst_object_unref (player);
}

gboolean
clapper_utils_replace_string (gchar **dst, gchar *src)
{
  gboolean changed = (g_strcmp0 (*dst, src) != 0);

  g_free (*dst);
  *dst = src;

  return changed;
}
