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
#include <gst/gst.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-player.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_REACTABLE (clapper_reactable_get_type())
#define CLAPPER_REACTABLE_CAST(obj) ((ClapperReactable *)(obj))

CLAPPER_API
G_DECLARE_INTERFACE (ClapperReactable, clapper_reactable, CLAPPER, REACTABLE, GstObject)

/**
 * ClapperReactableInterface:
 * @parent_iface: The parent interface structure.
 * @state_changed: Player state changed.
 * @position_changed: Player position changed.
 * @speed_changed: Player speed changed.
 * @volume_changed: Player volume changed.
 * @mute_changed: Player mute state changed.
 * @played_item_changed: New media item started playing.
 * @item_updated: An item in queue got updated.
 * @queue_item_added: An item was added to the queue.
 * @queue_item_removed: An item was removed from queue.
 * @queue_item_repositioned: An item changed position within queue.
 * @queue_cleared: All items were removed from queue.
 * @queue_progression_changed: Progression mode of the queue was changed.
 */
struct _ClapperReactableInterface
{
  GTypeInterface parent_iface;

  /**
   * ClapperReactableInterface::state_changed:
   * @reactable: a #ClapperReactable
   * @state: a #ClapperPlayerState
   *
   * Player state changed.
   *
   * Since: 0.10
   */
  void (* state_changed) (ClapperReactable *reactable, ClapperPlayerState state);

  /**
   * ClapperReactableInterface::position_changed:
   * @reactable: a #ClapperReactable
   * @position: a decimal number with current position in seconds
   *
   * Player position changed.
   *
   * Since: 0.10
   */
  void (* position_changed) (ClapperReactable *reactable, gdouble position);

  /**
   * ClapperReactableInterface::speed_changed:
   * @reactable: a #ClapperReactable
   * @speed: the playback speed multiplier
   *
   * Player speed changed.
   *
   * Since: 0.10
   */
  void (* speed_changed) (ClapperReactable *reactable, gdouble speed);

  /**
   * ClapperReactableInterface::volume_changed:
   * @reactable: a #ClapperReactable
   * @volume: the volume level
   *
   * Player volume changed.
   *
   * Since: 0.10
   */
  void (* volume_changed) (ClapperReactable *reactable, gdouble volume);

  /**
   * ClapperReactableInterface::mute_changed:
   * @reactable: a #ClapperReactable
   * @mute: %TRUE if player is muted, %FALSE otherwise
   *
   * Player mute state changed.
   *
   * Since: 0.10
   */
  void (* mute_changed) (ClapperReactable *reactable, gboolean mute);

  /**
   * ClapperReactableInterface::played_item_changed:
   * @reactable: a #ClapperReactable
   * @item: a #ClapperMediaItem that is now playing
   *
   * New media item started playing. All following events (such as position changes)
   * will be related to this @item from now on.
   *
   * Since: 0.10
   */
  void (* played_item_changed) (ClapperReactable *reactable, ClapperMediaItem *item);

  /**
   * ClapperReactableInterface::item_updated:
   * @reactable: a #ClapperReactable
   * @item: a #ClapperMediaItem that was updated
   *
   * An item in queue got updated.
   *
   * This might be (or not) currently played item.
   * Implementations can compare it against the last item from
   * [vfunc@Clapper.Reactable.played_item_changed] if they
   * need to know that.
   *
   * Since: 0.10
   */
  void (* item_updated) (ClapperReactable *reactable, ClapperMediaItem *item);

  /**
   * ClapperReactableInterface::queue_item_added:
   * @reactable: a #ClapperReactable
   * @item: a #ClapperMediaItem that was added
   * @index: position at which @item was placed in queue
   *
   * An item was added to the queue.
   *
   * Since: 0.10
   */
  void (* queue_item_added) (ClapperReactable *reactable, ClapperMediaItem *item, guint index);

  /**
   * ClapperReactableInterface::queue_item_removed:
   * @reactable: a #ClapperReactable
   * @item: a #ClapperMediaItem that was removed
   * @index: position from which @item was removed in queue
   *
   * An item was removed from queue.
   *
   * Implementations that are interested in queue items removal
   * should also implement [vfunc@Clapper.Reactable.queue_cleared].
   *
   * Since: 0.10
   */
  void (* queue_item_removed) (ClapperReactable *reactable, ClapperMediaItem *item, guint index);

  /**
   * ClapperReactableInterface::queue_item_repositioned:
   * @reactable: a #ClapperReactable
   * @before: position from which #ClapperMediaItem was removed
   * @after: position at which #ClapperMediaItem was inserted after removal
   *
   * An item changed position within queue.
   *
   * Since: 0.10
   */
  void (* queue_item_repositioned) (ClapperReactable *reactable, guint before, guint after);

  /**
   * ClapperReactableInterface::queue_cleared:
   * @reactable: a #ClapperReactable
   *
   * All items were removed from queue.
   *
   * Note that in such event [vfunc@Clapper.Reactable.queue_item_removed]
   * will NOT be called for each item for performance reasons. You probably
   * want to implement this function if you also implemented item removal.
   *
   * Since: 0.10
   */
  void (* queue_cleared) (ClapperReactable *reactable);

  /**
   * ClapperReactableInterface::queue_progression_changed:
   * @reactable: a #ClapperReactable
   * @mode: a #ClapperQueueProgressionMode
   *
   * Progression mode of the queue was changed.
   *
   * Since: 0.10
   */
  void (* queue_progression_changed) (ClapperReactable *reactable, ClapperQueueProgressionMode mode);

  /*< private >*/
  gpointer padding[8];
};

CLAPPER_API
ClapperPlayer * clapper_reactable_get_player (ClapperReactable *reactable);

CLAPPER_API
void clapper_reactable_queue_append_sync (ClapperReactable *reactable, ClapperMediaItem *item);

CLAPPER_API
void clapper_reactable_queue_insert_sync (ClapperReactable *reactable, ClapperMediaItem *item, ClapperMediaItem *after_item);

CLAPPER_API
void clapper_reactable_queue_remove_sync (ClapperReactable *reactable, ClapperMediaItem *item);

CLAPPER_API
void clapper_reactable_queue_clear_sync (ClapperReactable *reactable);

G_END_DECLS
