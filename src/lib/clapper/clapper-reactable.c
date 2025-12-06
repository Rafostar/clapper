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

/**
 * ClapperReactable:
 *
 * An interface for creating enhancers that react to the
 * playback and/or events that should influence it.
 *
 * Since: 0.10
 */

#include "clapper-reactable.h"
#include "clapper-utils-private.h"

#define CLAPPER_REACTABLE_DO_WITH_QUEUE(reactable, _queue_dst, ...) {  \
    ClapperPlayer *_player = clapper_reactable_get_player (reactable); \
    if (G_LIKELY (_player != NULL)) {                                  \
      *_queue_dst = clapper_player_get_queue (_player);                \
      __VA_ARGS__                                                      \
      gst_object_unref (_player); }}

G_DEFINE_INTERFACE (ClapperReactable, clapper_reactable, GST_TYPE_OBJECT);

static void
clapper_reactable_default_init (ClapperReactableInterface *iface)
{
}

/**
 * clapper_reactable_get_player:
 * @reactable: a #ClapperReactable
 *
 * Get the [class@Clapper.Player] that this reactable is reacting to.
 *
 * This is meant to be used in implementations where reaction goes the
 * other way around (from enhancer plugin to the player). For example
 * some external event needs to influence parent player object like
 * changing its state, seeking, etc.
 *
 * Note that enhancers are working in a non-main application thread, thus
 * if you need to do operations on a [class@Clapper.Queue] such as adding/removing
 * items, you need to switch thread first. Otherwise this will not be thread safe
 * for applications that use single threaded toolkits such as #GTK. You can do this
 * manually or use provided reactable convenience functions.
 *
 * Due to the threaded nature, you should also avoid comparisons to the current
 * properties values in the player or its queue. While these are thread safe, there
 * is no guarantee that values/objects between threads are still the same in both
 * (or still exist). For example, instead of using [property@Clapper.Queue:current_item],
 * monitor it with implemented [vfunc@Clapper.Reactable.played_item_changed] instead,
 * as these functions are all serialized into your implementation thread.
 *
 * Returns: (transfer full) (nullable): A reference to the parent #ClapperPlayer.
 *
 * Since: 0.10
 */
ClapperPlayer *
clapper_reactable_get_player (ClapperReactable *self)
{
  g_return_val_if_fail (CLAPPER_IS_REACTABLE (self), NULL);

  return CLAPPER_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (self)));
}

/**
 * clapper_reactable_queue_append_sync:
 * @reactable: a #ClapperReactable
 * @item: a #ClapperMediaItem
 *
 * A convenience function that within application main thread synchronously appends
 * an @item to the playback queue of the player that @reactable belongs to.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
clapper_reactable_queue_append_sync (ClapperReactable *self, ClapperMediaItem *item)
{
  ClapperQueue *queue;

  g_return_if_fail (CLAPPER_IS_REACTABLE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  CLAPPER_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    clapper_utils_queue_append_on_main_sync (queue, item);
  });
}

/**
 * clapper_reactable_queue_insert_sync:
 * @reactable: a #ClapperReactable
 * @item: a #ClapperMediaItem
 * @after_item: a #ClapperMediaItem after which to insert or %NULL to prepend
 *
 * A convenience function that within application main thread synchronously inserts
 * an @item to the playback queue position after @after_item of the player that
 * @reactable belongs to.
 *
 * This function uses @after_item instead of position index in order to ensure
 * desired position does not change during thread switching.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
clapper_reactable_queue_insert_sync (ClapperReactable *self,
    ClapperMediaItem *item, ClapperMediaItem *after_item)
{
  ClapperQueue *queue;

  g_return_if_fail (CLAPPER_IS_REACTABLE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));
  g_return_if_fail (after_item == NULL || CLAPPER_IS_MEDIA_ITEM (after_item));

  CLAPPER_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    clapper_utils_queue_insert_on_main_sync (queue, item, after_item);
  });
}

/**
 * clapper_reactable_queue_remove_sync:
 * @reactable: a #ClapperReactable
 * @item: a #ClapperMediaItem
 *
 * A convenience function that within application main thread synchronously removes
 * an @item from the playback queue of the player that @reactable belongs to.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
clapper_reactable_queue_remove_sync (ClapperReactable *self, ClapperMediaItem *item)
{
  ClapperQueue *queue;

  g_return_if_fail (CLAPPER_IS_REACTABLE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  CLAPPER_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    clapper_utils_queue_remove_on_main_sync (queue, item);
  });
}

/**
 * clapper_reactable_queue_clear_sync:
 * @reactable: a #ClapperReactable
 *
 * A convenience function that within application main thread synchronously clears
 * the playback queue of the player that @reactable belongs to.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
clapper_reactable_queue_clear_sync (ClapperReactable *self)
{
  ClapperQueue *queue;

  g_return_if_fail (CLAPPER_IS_REACTABLE (self));

  CLAPPER_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    clapper_utils_queue_clear_on_main_sync (queue);
  });
}

/**
 * clapper_reactable_timeline_insert_sync:
 * @reactable: a #ClapperReactable
 * @timeline: a #ClapperTimeline
 * @marker: a #ClapperMarker
 *
 * A convenience function that within application main thread synchronously
 * inserts @marker into @timeline.
 *
 * Reactable enhancers should only modify timeline of an item that is already
 * in queue from the application main thread, switching thread either themselves
 * or using this convenience function that does so.
 *
 * Since: 0.10
 */
void
clapper_reactable_timeline_insert_sync (ClapperReactable *self,
    ClapperTimeline *timeline, ClapperMarker *marker)
{
  g_return_if_fail (CLAPPER_IS_REACTABLE (self));
  g_return_if_fail (CLAPPER_IS_TIMELINE (timeline));
  g_return_if_fail (CLAPPER_IS_MARKER (marker));

  clapper_utils_timeline_insert_on_main_sync (timeline, marker);
}

/**
 * clapper_reactable_timeline_remove_sync:
 * @reactable: a #ClapperReactable
 * @timeline: a #ClapperTimeline
 * @marker: a #ClapperMarker
 *
 * A convenience function that within application main thread synchronously
 * removes @marker from @timeline.
 *
 * Reactable enhancers should only modify timeline of an item that is already
 * in queue from the application main thread, switching thread either themselves
 * or using this convenience function that does so.
 *
 * Since: 0.10
 */
void
clapper_reactable_timeline_remove_sync (ClapperReactable *self,
    ClapperTimeline *timeline, ClapperMarker *marker)
{
  g_return_if_fail (CLAPPER_IS_REACTABLE (self));
  g_return_if_fail (CLAPPER_IS_TIMELINE (timeline));
  g_return_if_fail (CLAPPER_IS_MARKER (marker));

  clapper_utils_timeline_remove_on_main_sync (timeline, marker);
}
