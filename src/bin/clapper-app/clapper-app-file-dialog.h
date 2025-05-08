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
#include <adwaita.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
void clapper_app_file_dialog_open_files (GtkApplication *gtk_app);

G_GNUC_INTERNAL
void clapper_app_file_dialog_open_subtitles (GtkApplication *gtk_app, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_app_file_dialog_select_prefs_file (GtkApplication *gtk_app, AdwActionRow *action_row);

G_GNUC_INTERNAL
void clapper_app_file_dialog_select_prefs_dir (GtkApplication *gtk_app, AdwActionRow *action_row);

G_END_DECLS
