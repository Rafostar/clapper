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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "clapper-enums-private.h"
#include "clapper-threaded-object.h"
#include "clapper-feature.h"

G_BEGIN_DECLS

#define CLAPPER_TYPE_FEATURES_MANAGER (clapper_features_manager_get_type())
#define CLAPPER_FEATURES_MANAGER_CAST(obj) ((ClapperFeaturesManager *)(obj))

G_DECLARE_FINAL_TYPE (ClapperFeaturesManager, clapper_features_manager, CLAPPER, FEATURES_MANAGER, ClapperThreadedObject)

G_GNUC_INTERNAL
ClapperFeaturesManager * clapper_features_manager_new (void);

G_GNUC_INTERNAL
void clapper_features_manager_add_feature (ClapperFeaturesManager *features, ClapperFeature *feature, GstObject *parent);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_property_changed (ClapperFeaturesManager *self, ClapperFeature *feature, GParamSpec *pspec);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_state_changed (ClapperFeaturesManager *features, ClapperPlayerState state);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_position_changed (ClapperFeaturesManager *features, gdouble position);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_speed_changed (ClapperFeaturesManager *features, gdouble speed);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_volume_changed (ClapperFeaturesManager *features, gdouble volume);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_mute_changed (ClapperFeaturesManager *features, gboolean mute);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_played_item_changed (ClapperFeaturesManager *features, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_item_updated (ClapperFeaturesManager *features, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_queue_item_added (ClapperFeaturesManager *features, ClapperMediaItem *item, guint index);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_queue_item_removed (ClapperFeaturesManager *features, ClapperMediaItem *item, guint index);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_queue_item_repositioned (ClapperFeaturesManager *features, guint before, guint after);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_queue_cleared (ClapperFeaturesManager *features);

G_GNUC_INTERNAL
void clapper_features_manager_trigger_queue_progression_changed (ClapperFeaturesManager *features, ClapperQueueProgressionMode mode);

G_GNUC_INTERNAL
void clapper_features_manager_handle_event (ClapperFeaturesManager *features, ClapperFeaturesManagerEvent event, const GValue *value, const GValue *extra_value);

G_END_DECLS
