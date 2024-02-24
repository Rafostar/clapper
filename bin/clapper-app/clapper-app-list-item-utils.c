/*
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

#include "clapper-app-list-item-utils.h"

gchar *
clapper_app_list_item_make_stream_group_title (GtkListItem *list_item, ClapperStream *stream)
{
  ClapperStreamType stream_type = CLAPPER_STREAM_TYPE_UNKNOWN;
  guint position = gtk_list_item_get_position (list_item);
  gchar *title = NULL;

  if (stream)
    stream_type = clapper_stream_get_stream_type (stream);

  /* FIXME: Translations */

  switch (stream_type) {
    case CLAPPER_STREAM_TYPE_VIDEO:
      title = g_strdup_printf ("Video [%u]", position);
      break;
    case CLAPPER_STREAM_TYPE_AUDIO:
      title = g_strdup_printf ("Audio [%u]", position);
      break;
    case CLAPPER_STREAM_TYPE_SUBTITLE:
      title = g_strdup_printf ("Subtitle [%u]", position);
      break;
    default:
      break;
  }

  return title;
}

gchar *
clapper_app_list_item_make_resolution (GtkListItem *list_item,
    gint width, gint height)
{
  return g_strdup_printf ("%ix%i", width, height);
}

gchar *
clapper_app_list_item_make_bitrate (GtkListItem *list_item, guint bitrate)
{
  if (bitrate >= 1000000)
    return g_strdup_printf ("%.3lf Mbps", (gdouble) bitrate / 1000000);

  return g_strdup_printf ("%u kbps", bitrate / 1000);
}

gchar *
clapper_app_list_item_convert_int (GtkListItem *list_item, gint value)
{
  return g_strdup_printf ("%i", value);
}

gchar *
clapper_app_list_item_convert_uint (GtkListItem *list_item, guint value)
{
  return g_strdup_printf ("%u", value);
}

gchar *
clapper_app_list_item_convert_double (GtkListItem *list_item, gdouble value)
{
  return g_strdup_printf ("%.3lf", value);
}
