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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <clapper/clapper-media-item.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_FEATURE (clapper_feature_get_type())
#define CLAPPER_FEATURE_CAST(obj) ((ClapperFeature *)(obj))

/**
 * ClapperFeature:
 *
 * Represents an additional feature.
 *
 * Feature objects are meant for adding additional functionalities that
 * are supposed to either act on playback/properties changes and/or change
 * them themselves due to some external signal/event.
 *
 * For reacting to playback changes subclass should override this class
 * virtual functions logic, while for controlling playback implementation
 * may call gst_object_get_parent() to acquire a weak reference on a parent
 * player object feature was added to.
 */
G_DECLARE_DERIVABLE_TYPE (ClapperFeature, clapper_feature, CLAPPER, FEATURE, GstObject)

/*
 * ClapperFeatureClass:
 * @parent_class: The object class structure.
 * @prepare: Prepare feature for operation (optional). This is different from init()
 *   as its called from features thread once feature is added to the player, so
 *   it can already access it parent using gst_object_get_parent(). Should return
 *   %TRUE on success, %FALSE otherwise. If it fails, no other method will be called.
 * @unprepare: Revert the changes done in @prepare (optional).
 * @state_changed: Player state was changed.
 * @position_changed: Player position was changed.
 * @speed_changed: Player speed was changed.
 * @volume_changed: Player volume was changed.
 * @mute_changed: Player mute state was changed.
 * @current_media_item_changed: Currently playing media item got changed.
 *   Item will be %NULL if no new item was selected.
 * @media_item_updated: An item in queue got updated. This might be (or not)
 *   currently played item. Implementations can get parent player object
 *   if they want to check that from its queue.
 * @queue_item_added: An item was added to the queue.
 * @queue_item_removed: An item was removed from queue.
 * @queue_cleared: All items were removed from queue. Note that in such event
 *   @queue_item_removed will NOT be called for each item for performance reasons.
 *   You probably want to implement this function if you also implemented item removal.
 * @queue_progression_changed: #ClapperQueueProgressionMode of the queue was changed.
 */
struct _ClapperFeatureClass
{
  GstObjectClass parent_class;

  gboolean (* prepare) (ClapperFeature *feature);

  gboolean (* unprepare) (ClapperFeature *feature);

  void (* state_changed) (ClapperFeature *feature, ClapperPlayerState state);

  void (* position_changed) (ClapperFeature *feature, gfloat position);

  void (* speed_changed) (ClapperFeature *feature, gfloat speed);

  void (* volume_changed) (ClapperFeature *feature, gfloat volume);

  void (* mute_changed) (ClapperFeature *feature, gboolean mute);

  void (* current_media_item_changed) (ClapperFeature *feature, ClapperMediaItem *current_item);

  void (* media_item_updated) (ClapperFeature *feature, ClapperMediaItem *item);

  void (* queue_item_added) (ClapperFeature *feature, ClapperMediaItem *item, guint index);

  void (* queue_item_removed) (ClapperFeature *feature, ClapperMediaItem *item);

  void (* queue_cleared) (ClapperFeature *feature);

  void (* queue_progression_changed) (ClapperFeature *feature, ClapperQueueProgressionMode mode);
};

G_END_DECLS
