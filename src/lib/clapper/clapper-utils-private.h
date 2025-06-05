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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include "clapper-utils.h"
#include "clapper-queue.h"
#include "clapper-media-item.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void clapper_utils_initialize (void);

G_GNUC_INTERNAL
void clapper_utils_queue_append_on_main_sync (ClapperQueue *queue, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_utils_queue_insert_on_main_sync (ClapperQueue *queue, ClapperMediaItem *item, ClapperMediaItem *after_item);

G_GNUC_INTERNAL
void clapper_utils_queue_remove_on_main_sync (ClapperQueue *queue, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_utils_queue_clear_on_main_sync (ClapperQueue *queue);

G_GNUC_INTERNAL
void clapper_utils_prop_notify_on_main_sync (GObject *object, GParamSpec *pspec);

G_GNUC_INTERNAL
gchar * clapper_utils_uri_from_file (GFile *file);

G_GNUC_INTERNAL
gchar * clapper_utils_title_from_uri (const gchar *uri);

G_GNUC_INTERNAL
gboolean clapper_utils_set_value_from_variant (GValue *value, GVariant *variant);

G_END_DECLS
