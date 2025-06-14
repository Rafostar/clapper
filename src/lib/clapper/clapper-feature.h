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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-media-item.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_FEATURE (clapper_feature_get_type())
#define CLAPPER_FEATURE_CAST(obj) ((ClapperFeature *)(obj))

CLAPPER_DEPRECATED_FOR(ClapperReactable)
G_DECLARE_DERIVABLE_TYPE (ClapperFeature, clapper_feature, CLAPPER, FEATURE, GstObject)

/**
 * ClapperFeatureClass:
 * @parent_class: The object class structure.
 * @prepare: Prepare feature for operation (optional).
 * @unprepare: Revert the changes done in @prepare (optional).
 * @property_changed: A property of @feature changed its value.
 * @state_changed: Player state was changed.
 * @position_changed: Player position was changed.
 * @speed_changed: Player speed was changed.
 * @volume_changed: Player volume was changed.
 * @mute_changed: Player mute state was changed.
 * @played_item_changed: Currently playing media item got changed.
 * @item_updated: An item in queue got updated.
 * @queue_item_added: An item was added to the queue.
 * @queue_item_removed: An item was removed from queue.
 * @queue_item_reposition: An item changed position within queue.
 * @queue_cleared: All items were removed from queue.
 * @queue_progression_changed: Progression mode of the queue was changed.
 */
struct _ClapperFeatureClass
{
  GstObjectClass parent_class;

  /**
   * ClapperFeatureClass::prepare:
   * @feature: a #ClapperFeature
   *
   * Prepare feature for operation (optional).
   *
   * This is different from init() as its called from features thread once
   * feature is added to the player, so it can already access it parent using
   * gst_object_get_parent(). If it fails, no other method will be called.
   *
   * Returns: %TRUE on success, %FALSE otherwise.
   */
  gboolean (* prepare) (ClapperFeature *feature);

  /**
   * ClapperFeatureClass::unprepare:
   * @feature: a #ClapperFeature
   *
   * Revert the changes done in @prepare (optional).
   *
   * Returns: %TRUE on success, %FALSE otherwise.
   */
  gboolean (* unprepare) (ClapperFeature *feature);

  /**
   * ClapperFeatureClass::property_changed:
   * @feature: a #ClapperFeature
   * @pspec: a #GParamSpec
   *
   * A property of @feature changed its value.
   *
   * Useful for reconfiguring @feature, since unlike "notify" signal
   * this is always called from the thread that feature works on and
   * only after feature was prepared.
   */
  void (* property_changed) (ClapperFeature *feature, GParamSpec *pspec);

  /**
   * ClapperFeatureClass::state_changed:
   * @feature: a #ClapperFeature
   * @state: a #ClapperPlayerState
   *
   * Player state was changed.
   */
  void (* state_changed) (ClapperFeature *feature, ClapperPlayerState state);

  /**
   * ClapperFeatureClass::position_changed:
   * @feature: a #ClapperFeature
   * @position: a decimal number with current position in seconds
   *
   * Player position was changed.
   */
  void (* position_changed) (ClapperFeature *feature, gdouble position);

  /**
   * ClapperFeatureClass::speed_changed:
   * @feature: a #ClapperFeature
   * @speed: the playback speed multiplier
   *
   * Player speed was changed.
   */
  void (* speed_changed) (ClapperFeature *feature, gdouble speed);

  /**
   * ClapperFeatureClass::volume_changed:
   * @feature: a #ClapperFeature
   * @volume: the volume level
   *
   * Player volume was changed.
   */
  void (* volume_changed) (ClapperFeature *feature, gdouble volume);

  /**
   * ClapperFeatureClass::mute_changed:
   * @feature: a #ClapperFeature
   * @mute: %TRUE if player is muted, %FALSE otherwise
   *
   * Player mute state was changed.
   */
  void (* mute_changed) (ClapperFeature *feature, gboolean mute);

  /**
   * ClapperFeatureClass::played_item_changed:
   * @feature: a #ClapperFeature
   * @item: a #ClapperMediaItem that is now playing
   *
   * New media item started playing. All following events (such as position changes)
   * will be related to this @item from now on.
   */
  void (* played_item_changed) (ClapperFeature *feature, ClapperMediaItem *item);

  /**
   * ClapperFeatureClass::item_updated:
   * @feature: a #ClapperFeature
   * @item: a #ClapperMediaItem that was updated
   *
   * An item in queue got updated. This might be (or not) currently
   * played item. Implementations can get parent player object
   * if they want to check that from its queue.
   */
  void (* item_updated) (ClapperFeature *feature, ClapperMediaItem *item);

  /**
   * ClapperFeatureClass::queue_item_added:
   * @feature: a #ClapperFeature
   * @item: a #ClapperMediaItem that was added
   * @index: position at which @item was placed in queue
   *
   * An item was added to the queue.
   */
  void (* queue_item_added) (ClapperFeature *feature, ClapperMediaItem *item, guint index);

  /**
   * ClapperFeatureClass::queue_item_removed:
   * @feature: a #ClapperFeature
   * @item: a #ClapperMediaItem that was removed
   * @index: position from which @item was removed in queue
   *
   * An item was removed from queue.
   */
  void (* queue_item_removed) (ClapperFeature *feature, ClapperMediaItem *item, guint index);

  /**
   * ClapperFeatureClass::queue_item_repositioned:
   * @feature: a #ClapperFeature
   * @before: position from which #ClapperMediaItem was removed
   * @after: position at which #ClapperMediaItem was inserted after removal
   *
   * An item changed position within queue.
   */
  void (* queue_item_repositioned) (ClapperFeature *feature, guint before, guint after);

  /**
   * ClapperFeatureClass::queue_cleared:
   * @feature: a #ClapperFeature
   *
   * All items were removed from queue. Note that in such event
   * @queue_item_removed will NOT be called for each item for performance reasons.
   * You probably want to implement this function if you also implemented item removal.
   */
  void (* queue_cleared) (ClapperFeature *feature);

  /**
   * ClapperFeatureClass::queue_progression_changed:
   * @feature: a #ClapperFeature
   * @mode: a #ClapperQueueProgressionMode
   *
   * Progression mode of the queue was changed.
   */
  void (* queue_progression_changed) (ClapperFeature *feature, ClapperQueueProgressionMode mode);

  /*< private >*/
  gpointer padding[8];
};

G_END_DECLS
