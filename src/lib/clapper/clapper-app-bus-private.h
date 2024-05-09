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
#include <gst/gst.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_APP_BUS (clapper_app_bus_get_type())
#define CLAPPER_APP_BUS_CAST(obj) ((ClapperAppBus *)(obj))

/**
 * ClapperAppBus:
 */
G_DECLARE_FINAL_TYPE (ClapperAppBus, clapper_app_bus, CLAPPER, APP_BUS, GstBus)

void clapper_app_bus_initialize (void);

ClapperAppBus * clapper_app_bus_new (void);

void clapper_app_bus_forward_message (ClapperAppBus *app_bus, GstMessage *msg);

void clapper_app_bus_post_prop_notify (ClapperAppBus *app_bus, GstObject *src, GParamSpec *pspec);

void clapper_app_bus_post_refresh_streams (ClapperAppBus *app_bus, GstObject *src);

void clapper_app_bus_post_refresh_timeline (ClapperAppBus *app_bus, GstObject *src);

void clapper_app_bus_post_simple_signal (ClapperAppBus *app_bus, GstObject *src, guint signal_id);

void clapper_app_bus_post_object_desc_signal (ClapperAppBus *app_bus, GstObject *src, guint signal_id, GstObject *object, const gchar *desc);

void clapper_app_bus_post_desc_with_details_signal (ClapperAppBus *app_bus, GstObject *src, guint signal_id, const gchar *desc, const gchar *details);

void clapper_app_bus_post_error_signal (ClapperAppBus *app_bus, GstObject *src, guint signal_id, GError *error, const gchar *debug_info);

G_END_DECLS
