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

#include <glib-object.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

#define CLAPPER_APP_TYPE_QUEUE_PROGRESSION_ITEM (clapper_app_queue_progression_item_get_type())
#define CLAPPER_APP_QUEUE_PROGRESSION_ITEM_CAST(obj) ((ClapperAppQueueProgressionItem *)(obj))

G_DECLARE_FINAL_TYPE (ClapperAppQueueProgressionItem, clapper_app_queue_progression_item, CLAPPER_APP, QUEUE_PROGRESSION_ITEM, GObject)

ClapperAppQueueProgressionItem * clapper_app_queue_progression_item_new (const gchar *icon_name, const gchar *label);

G_END_DECLS
