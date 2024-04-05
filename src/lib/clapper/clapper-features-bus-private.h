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

#include <gst/gst.h>
#include <glib-object.h>

#include "clapper-features-manager-private.h"
#include "clapper-enums-private.h"

G_BEGIN_DECLS

#define CLAPPER_TYPE_FEATURES_BUS (clapper_features_bus_get_type())
#define CLAPPER_FEATURES_BUS_CAST(obj) ((ClapperFeaturesBus *)(obj))

/**
 * ClapperFeaturesBus:
 */
G_DECLARE_FINAL_TYPE (ClapperFeaturesBus, clapper_features_bus, CLAPPER, FEATURES_BUS, GstBus)

void clapper_features_bus_initialize (void);

ClapperFeaturesBus * clapper_features_bus_new (void);

void clapper_features_bus_post_event (ClapperFeaturesBus *features_bus, ClapperFeaturesManager *src, ClapperFeaturesManagerEvent event, GValue *value, GValue *extra_value);

G_END_DECLS
