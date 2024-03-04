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

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

typedef void (* ClapperAppUtilsIterRanks) (const gchar *feature_name, GstRank rank, gboolean from_env, gpointer user_data);

G_GNUC_INTERNAL
gboolean clapper_app_utils_uri_is_valid (const gchar *uri);

G_GNUC_INTERNAL
gboolean clapper_app_utils_value_for_item_is_valid (const GValue *value);

G_GNUC_INTERNAL
ClapperMediaItem * clapper_app_utils_media_item_from_value (const GValue *value);

G_GNUC_INTERNAL
void clapper_app_utils_iterate_plugin_feature_ranks (GSettings *settings, ClapperAppUtilsIterRanks callback, gpointer user_data);

G_END_DECLS
