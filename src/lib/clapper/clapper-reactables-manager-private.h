/* Clapper Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "clapper-enums.h"
#include "clapper-threaded-object.h"
#include "clapper-enhancer-proxy.h"
#include "clapper-media-item.h"

G_BEGIN_DECLS

#define CLAPPER_TYPE_REACTABLES_MANAGER (clapper_reactables_manager_get_type())
#define CLAPPER_REACTABLES_MANAGER_CAST(obj) ((ClapperReactablesManager *)(obj))

G_DECLARE_FINAL_TYPE (ClapperReactablesManager, clapper_reactables_manager, CLAPPER, REACTABLES_MANAGER, ClapperThreadedObject)

G_GNUC_INTERNAL
void clapper_reactables_manager_initialize (void);

G_GNUC_INTERNAL
ClapperReactablesManager * clapper_reactables_manager_new (void);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_configure_take_config (ClapperReactablesManager *manager, ClapperEnhancerProxy *proxy, GstStructure *config);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_state_changed (ClapperReactablesManager *manager, ClapperPlayerState state);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_position_changed (ClapperReactablesManager *manager, gdouble position);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_speed_changed (ClapperReactablesManager *manager, gdouble speed);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_volume_changed (ClapperReactablesManager *manager, gdouble volume);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_mute_changed (ClapperReactablesManager *manager, gboolean mute);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_played_item_changed (ClapperReactablesManager *manager, ClapperMediaItem *item);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_item_updated (ClapperReactablesManager *manager, ClapperMediaItem *item, ClapperReactableItemUpdatedFlags flags);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_queue_item_added (ClapperReactablesManager *manager, ClapperMediaItem *item, guint index);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_queue_item_removed (ClapperReactablesManager *manager, ClapperMediaItem *item, guint index);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_queue_item_repositioned (ClapperReactablesManager *manager, guint before, guint after);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_queue_cleared (ClapperReactablesManager *manager);

G_GNUC_INTERNAL
void clapper_reactables_manager_trigger_queue_progression_changed (ClapperReactablesManager *manager, ClapperQueueProgressionMode mode);

G_END_DECLS
