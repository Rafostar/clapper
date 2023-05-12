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

#pragma once

#include <glib.h>

#include "clapper-media-item.h"
#include "clapper-app-bus-private.h"

G_BEGIN_DECLS

struct _ClapperMediaItem
{
  GstObject parent;

  gchar *uri;
  gchar **suburis;

  gboolean pending_discovery;
  gboolean discovered;

  gchar *title;
  gchar *container_format;
  gfloat duration;
};

G_GNUC_INTERNAL
void clapper_media_item_update_from_tag_list (ClapperMediaItem *item, const GstTagList *tags, ClapperAppBus *app_bus);

G_GNUC_INTERNAL
void clapper_media_item_update_from_container_tags (ClapperMediaItem *item, const GstTagList *tags, ClapperAppBus *app_bus);

G_GNUC_INTERNAL
void clapper_media_item_take_title (ClapperMediaItem *item, gchar *title, ClapperAppBus *app_bus);

G_GNUC_INTERNAL
void clapper_media_item_take_container_format (ClapperMediaItem *item, gchar *container_format, ClapperAppBus *app_bus);

G_GNUC_INTERNAL
void clapper_media_item_set_duration (ClapperMediaItem *item, gfloat duration, ClapperAppBus *app_bus);

G_END_DECLS
