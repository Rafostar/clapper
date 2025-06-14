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

#include <clapper/clapper-feature.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
void clapper_feature_call_prepare (ClapperFeature *feature);

G_GNUC_INTERNAL
void clapper_feature_call_unprepare (ClapperFeature *feature);

G_GNUC_INTERNAL
void clapper_feature_call_property_changed (ClapperFeature *feature, GParamSpec *pspec);

G_GNUC_INTERNAL
void clapper_feature_call_state_changed (ClapperFeature *feature, ClapperPlayerState state);

G_GNUC_INTERNAL
void clapper_feature_call_position_changed (ClapperFeature *feature, gdouble position);

G_GNUC_INTERNAL
void clapper_feature_call_speed_changed (ClapperFeature *feature, gdouble speed);

G_GNUC_INTERNAL
void clapper_feature_call_volume_changed (ClapperFeature *feature, gdouble volume);

G_GNUC_INTERNAL
void clapper_feature_call_mute_changed (ClapperFeature *feature, gboolean mute);

G_GNUC_INTERNAL
void clapper_feature_call_played_item_changed (ClapperFeature *feature, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_feature_call_item_updated (ClapperFeature *feature, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_feature_call_queue_item_added (ClapperFeature *feature, ClapperMediaItem *item, guint index);

G_GNUC_INTERNAL
void clapper_feature_call_queue_item_removed (ClapperFeature *feature, ClapperMediaItem *item, guint index);

G_GNUC_INTERNAL
void clapper_feature_call_queue_item_repositioned (ClapperFeature *self, guint before, guint after);

G_GNUC_INTERNAL
void clapper_feature_call_queue_cleared (ClapperFeature *feature);

G_GNUC_INTERNAL
void clapper_feature_call_queue_progression_changed (ClapperFeature *feature, ClapperQueueProgressionMode mode);

G_END_DECLS
