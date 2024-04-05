/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

gchar * clapper_app_list_item_make_stream_group_title (GtkListItem *list_item, ClapperStream *stream);

gchar * clapper_app_list_item_make_resolution (GtkListItem *list_item, gint width, gint height);

gchar * clapper_app_list_item_make_bitrate (GtkListItem *list_item, guint value);

gchar * clapper_app_list_item_convert_int (GtkListItem *list_item, gint value);

gchar * clapper_app_list_item_convert_uint (GtkListItem *list_item, guint value);

gchar * clapper_app_list_item_convert_double (GtkListItem *list_item, gdouble value);

G_END_DECLS
