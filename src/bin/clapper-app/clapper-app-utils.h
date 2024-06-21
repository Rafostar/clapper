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
#include <gio/gio.h>
#include <gst/gst.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

typedef void (* ClapperAppUtilsIterRanks) (const gchar *feature_name, GstRank rank, gboolean from_env, gpointer user_data);

#ifdef G_OS_WIN32
G_GNUC_INTERNAL
const gchar *const * clapper_app_utils_get_extensions (void);

G_GNUC_INTERNAL
const gchar *const * clapper_app_utils_get_subtitles_extensions (void);
#endif

G_GNUC_INTERNAL
const gchar *const * clapper_app_utils_get_mime_types (void);

G_GNUC_INTERNAL
const gchar *const * clapper_app_utils_get_subtitles_mime_types (void);

G_GNUC_INTERNAL
void clapper_app_utils_parse_progression (ClapperQueueProgressionMode mode, const gchar **icon, const gchar **label);

G_GNUC_INTERNAL
gboolean clapper_app_utils_is_subtitles_file (GFile *file);

G_GNUC_INTERNAL
gboolean clapper_app_utils_value_for_item_is_valid (const GValue *value);

G_GNUC_INTERNAL
gboolean clapper_app_utils_files_from_list_model (GListModel *files_model, GFile ***files, gint *n_files);

G_GNUC_INTERNAL
gboolean clapper_app_utils_files_from_slist (GSList *file_list, GFile ***files, gint *n_files);

G_GNUC_INTERNAL
gboolean clapper_app_utils_files_from_string (const gchar *string, GFile ***files, gint *n_files);

G_GNUC_INTERNAL
gboolean clapper_app_utils_files_from_command_line (GApplicationCommandLine *cmd_line, GFile ***files, gint *n_files);

G_GNUC_INTERNAL
gboolean clapper_app_utils_files_from_value (const GValue *value, GFile ***files, gint *n_files);

G_GNUC_INTERNAL
void clapper_app_utils_files_free (GFile **files);

G_GNUC_INTERNAL
void clapper_app_utils_iterate_plugin_feature_ranks (GSettings *settings, ClapperAppUtilsIterRanks callback, gpointer user_data);

G_GNUC_INTERNAL
GstElement * clapper_app_utils_make_element (const gchar *string);

G_END_DECLS
