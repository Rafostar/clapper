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

/**
 * SECTION:clapper-queue
 * @title: ClapperQueue
 * @short_description: A queue consisting of #ClapperMediaItem objects
 */

#include "clapper-queue-private.h"
#include "clapper-media-item-private.h"
#include "clapper-player-private.h"
#include "clapper-playbin-bus-private.h"

#define DEFAULT_PROGRESSION_MODE CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE

#define GST_CAT_DEFAULT clapper_queue_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperQueue
{
  GstObject parent;

  GPtrArray *items;
  ClapperMediaItem *current_item;

  ClapperQueueProgressionMode progression_mode;
};

enum
{
  PROP_0,
  PROP_CURRENT_ITEM,
  PROP_N_ITEMS,
  PROP_PROGRESSION_MODE,
  PROP_LAST
};

static GType
clapper_queue_list_model_get_item_type (GListModel *model)
{
  return CLAPPER_TYPE_MEDIA_ITEM;
}

static guint
clapper_queue_list_model_get_n_items (GListModel *model)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (model);
  guint n_items;

  GST_OBJECT_LOCK (self);
  n_items = self->items->len;
  GST_OBJECT_UNLOCK (self);

  return n_items;
}

static gpointer
clapper_queue_list_model_get_item (GListModel *model, guint index)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (model);
  ClapperMediaItem *item = NULL;

  GST_DEBUG_OBJECT (self, "Reading queue item: %u", index);

  GST_OBJECT_LOCK (self);
  if (G_LIKELY (index < self->items->len))
    item = g_object_ref (g_ptr_array_index (self->items, index));
  GST_OBJECT_UNLOCK (self);

  return item;
}

static void
clapper_queue_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = clapper_queue_list_model_get_item_type;
  iface->get_n_items = clapper_queue_list_model_get_n_items;
  iface->get_item = clapper_queue_list_model_get_item;
}

#define parent_class clapper_queue_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperQueue, clapper_queue, GST_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, clapper_queue_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_handle_items_changed (ClapperQueue *self, guint index, guint removed, guint added,
    gboolean current_changed, ClapperMediaItem *changed_item)
{
  ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

  if (G_UNLIKELY (player == NULL))
    return;

  if (removed > 0 || added > 0) {
    g_list_model_items_changed (G_LIST_MODEL (self), index, removed, added);

    if (clapper_player_get_have_features (player)) {
      if (added == 1)
        clapper_features_manager_trigger_queue_item_added (player->features_manager, changed_item);
      else if (removed == 1)
        clapper_features_manager_trigger_queue_item_removed (player->features_manager, changed_item);
      else if (removed > 1)
        clapper_features_manager_trigger_queue_cleared (player->features_manager);
      else
        g_assert_not_reached ();
    }

    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_N_ITEMS]);
  }
  if (current_changed) {
    clapper_playbin_bus_post_current_item_change (player->bus, changed_item);
    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_ITEM]);
  }

  gst_object_unref (player);
}

static inline gboolean
_replace_current_item_unlocked (ClapperQueue *self, ClapperMediaItem *item)
{
  return gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (item));
}

/*
 * clapper_queue_new:
 *
 * Returns: (transfer full): a new #ClapperQueue instance
 */
ClapperQueue *
clapper_queue_new (void)
{
  ClapperQueue *queue;

  queue = g_object_new (CLAPPER_TYPE_QUEUE, NULL);
  gst_object_ref_sink (queue);

  return queue;
}

/**
 * clapper_queue_add_item:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem
 *
 * Add another #ClapperMediaItem to the end of queue.
 *
 * If item is already in queue, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 */
void
clapper_queue_add_item (ClapperQueue *self, ClapperMediaItem *item)
{
  clapper_queue_insert_item (self, item, -1);
}

/**
 * clapper_queue_insert_item:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem
 * @index: the index to place @item in queue, -1 to append
 *
 * Insert another #ClapperMediaItem at @index position to the queue.
 *
 * If item is already in queue, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 */
void
clapper_queue_insert_item (ClapperQueue *self, ClapperMediaItem *item, gint index)
{
  guint arr_index = 0;
  gboolean added, select;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  GST_OBJECT_LOCK (self);

  if ((added = !g_ptr_array_find (self->items, item, NULL))) {
    arr_index = self->items->len;
    g_ptr_array_insert (self->items, index, gst_object_ref (item));
    gst_object_set_parent (GST_OBJECT (item), GST_OBJECT (self));
  }

  /* If queue was empty, auto select first item */
  select = (added && arr_index == 0);

  if (select)
    _replace_current_item_unlocked (self, item);

  GST_OBJECT_UNLOCK (self);

  if (added)
    _handle_items_changed (self, arr_index, 0, 1, select, item);
}

/**
 * clapper_queue_remove_item:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem
 *
 * Removes #ClapperMediaItem from the queue.
 *
 * If item either was never in the queue or was removed from
 * it earlier, this function will do nothing, so it is safe
 * to call multiple times if unsure.
 */
void
clapper_queue_remove_item (ClapperQueue *self, ClapperMediaItem *item)
{
  ClapperMediaItem *removed_item = NULL;
  guint index = 0;
  gboolean is_current = FALSE;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  GST_OBJECT_LOCK (self);

  if (g_ptr_array_find (self->items, item, &index)) {
    removed_item = g_ptr_array_steal_index (self->items, index);
    gst_object_unparent (GST_OBJECT (removed_item));

    if ((is_current = removed_item == self->current_item))
      gst_clear_object (&self->current_item);
  }

  GST_OBJECT_UNLOCK (self);

  if (removed_item) {
    _handle_items_changed (self, index, 1, 0, is_current, removed_item);
    gst_object_unref (removed_item);
  }
}

/**
 * clapper_queue_clear:
 * @queue: a #ClapperQueue
 *
 * Removes all media items from the queue.
 *
 * If queue is empty, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 */
void
clapper_queue_clear (ClapperQueue *self)
{
  guint n_items;
  gboolean changed;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);

  n_items = self->items->len;

  if ((changed = n_items > 0)) {
    g_ptr_array_remove_range (self->items, 0, n_items);
    gst_clear_object (&self->current_item);
  }

  GST_OBJECT_UNLOCK (self);

  if (changed)
    _handle_items_changed (self, 0, n_items, 0, TRUE, NULL);
}

/**
 * clapper_queue_select_item:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem
 *
 * Selects #ClapperMediaItem from playlist as current one and
 * signals #ClapperPlayer to play it.
 */
void
clapper_queue_select_item (ClapperQueue *self, ClapperMediaItem *item)
{
  gboolean queued;
  guint index = 0;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  GST_OBJECT_LOCK (self);
  if ((queued = g_ptr_array_find (self->items, item, &index)))
    _replace_current_item_unlocked (self, item);
  GST_OBJECT_UNLOCK (self);

  g_return_if_fail (queued);

  if (queued)
    _handle_items_changed (self, index, 0, 0, TRUE, item);
}

static gboolean
_select_item_with_offset (ClapperQueue *self, gint offset)
{
  ClapperMediaItem *item;
  guint index = 0;

  GST_OBJECT_LOCK (self);

  if (G_UNLIKELY (!self->current_item
      || !g_ptr_array_find (self->items, self->current_item, &index))) {
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }

  index += offset;

  if (index < 0 || index > self->items->len - 1) {
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }

  item = gst_object_ref (g_ptr_array_index (self->items, index));
  _replace_current_item_unlocked (self, item);

  GST_OBJECT_UNLOCK (self);

  GST_DEBUG_OBJECT (self, "Changing current item index by offset: %i", offset);

  _handle_items_changed (self, index, 0, 0, TRUE, item);
  gst_object_unref (item);

  return TRUE;
}

/**
 * clapper_queue_select_next_item:
 * @queue: a #ClapperQueue
 *
 * Selects next #ClapperMediaItem from playlist for playback.
 *
 * Note that this will try to select next item in the order
 * of the queue, regardless of #ClapperQueueProgressionMode set.
 *
 * Returns: %TRUE if there was another media item in queue, %FALSE otherwise.
 */
gboolean
clapper_queue_select_next_item (ClapperQueue *self)
{
  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  return _select_item_with_offset (self, 1);
}

/**
 * clapper_queue_select_previous_item:
 * @queue: a #ClapperQueue
 *
 * Selects previous #ClapperMediaItem from playlist for playback.
 *
 * Note that this will try to select previous item in the order
 * of the queue, regardless of #ClapperQueueProgressionMode set.
 *
 * Returns: %TRUE if there was previous media item in queue, %FALSE otherwise.
 */
gboolean
clapper_queue_select_previous_item (ClapperQueue *self)
{
  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  return _select_item_with_offset (self, -1);
}

/**
 * clapper_queue_get_item: (skip)
 * @queue: a #ClapperQueue
 * @index: an item index
 *
 * Get the #ClapperMediaItem at index.
 *
 * This behaves the same as g_list_model_get_item(), and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * This function is not available in bindings as they already
 * inherit get_item() method from #GListModel interface.
 *
 * Returns: (transfer full): The #ClapperMediaItem at @index.
 */
ClapperMediaItem *
clapper_queue_get_item (ClapperQueue *self, guint index)
{
  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * clapper_queue_get_current_item:
 * @queue: a #ClapperQueue
 *
 * Get the currently selected #ClapperMediaItem.
 *
 * Returns: (transfer full): The current #ClapperMediaItem.
 */
ClapperMediaItem *
clapper_queue_get_current_item (ClapperQueue *self)
{
  ClapperMediaItem *item = NULL;

  /* XXX: For updating media item during playback we should
   * use `player->played_item` instead to not be racy when
   * changing and updating current item at the same time */

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), NULL);

  GST_OBJECT_LOCK (self);
  if (self->current_item)
    item = gst_object_ref (self->current_item);
  GST_OBJECT_UNLOCK (self);

  return item;
}

/**
 * clapper_queue_find_item:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem to search for
 * @index: (optional) (out): return location for the index of
 *   the element, if found
 *
 * Get the index of #ClapperMediaItem within #ClapperQueue.
 *
 * Returns: %TRUE if @item is one of the elements of queue.
 */
gboolean
clapper_queue_find_item (ClapperQueue *self, ClapperMediaItem *item, guint *index)
{
  gboolean found;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (item), FALSE);

  GST_OBJECT_LOCK (self);
  found = g_ptr_array_find (self->items, item, index);
  GST_OBJECT_UNLOCK (self);

  return found;
}

/**
 * clapper_queue_get_n_items: (skip)
 * @queue: a #ClapperQueue
 *
 * Get the number of items in #ClapperQueue.
 *
 * This behave the same as g_list_model_get_n_items(), and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * This function is not available in bindings as they already
 * inherit get_n_items() method from #GListModel interface.
 *
 * Returns: The number of items in #ClapperQueue.
 */
guint
clapper_queue_get_n_items (ClapperQueue *self)
{
  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

/**
 * clapper_queue_set_progression_mode:
 * @queue: a #ClapperQueue
 * @mode: a #ClapperQueueProgressionMode
 *
 * Set the #ClapperQueueProgressionMode of the #ClapperQueue.
 *
 * Changing the mode set will alter next item selection at the
 * end of playback. For possible values and their descriptions,
 * see #ClapperQueueProgressionMode documentation.
 */
void
clapper_queue_set_progression_mode (ClapperQueue *self, ClapperQueueProgressionMode mode)
{
  g_return_if_fail (CLAPPER_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  self->progression_mode = mode;
  GST_OBJECT_UNLOCK (self);
}

/**
 * clapper_queue_get_progression_mode:
 * @queue: a #ClapperQueue
 *
 * Get the #ClapperQueueProgressionMode of the #ClapperQueue.
 *
 * Returns: a currently set #ClapperQueueProgressionMode.
 */
ClapperQueueProgressionMode
clapper_queue_get_progression_mode (ClapperQueue *self)
{
  ClapperQueueProgressionMode mode;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), DEFAULT_PROGRESSION_MODE);

  GST_OBJECT_LOCK (self);
  mode = self->progression_mode;
  GST_OBJECT_UNLOCK (self);

  return mode;
}

static void
_item_remove_func (ClapperMediaItem *item)
{
  gst_object_unparent (GST_OBJECT (item));
  gst_object_unref (item);
}

static void
clapper_queue_init (ClapperQueue *self)
{
  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify) _item_remove_func);

  self->progression_mode = DEFAULT_PROGRESSION_MODE;
}

static void
clapper_queue_finalize (GObject *object)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&self->current_item);
  g_ptr_array_unref (self->items);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_queue_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_ITEM:
      g_value_take_object (value, clapper_queue_get_current_item (self));
      break;
    case PROP_N_ITEMS:
      g_value_set_uint (value, clapper_queue_get_n_items (self));
      break;
    case PROP_PROGRESSION_MODE:
      g_value_set_enum (value, clapper_queue_get_progression_mode (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_queue_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (object);

  switch (prop_id) {
    case PROP_PROGRESSION_MODE:
      clapper_queue_set_progression_mode (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_queue_class_init (ClapperQueueClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperqueue", 0,
      "Clapper Queue");

  gobject_class->get_property = clapper_queue_get_property;
  gobject_class->set_property = clapper_queue_set_property;
  gobject_class->finalize = clapper_queue_finalize;

  /**
   * ClapperQueue:current-item:
   *
   * Currently selected media item for playback.
   */
  param_specs[PROP_CURRENT_ITEM] = g_param_spec_object ("current-item",
      NULL, NULL, CLAPPER_TYPE_MEDIA_ITEM,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperQueue:n-items:
   *
   * Number of media items in the queue.
   */
  param_specs[PROP_N_ITEMS] = g_param_spec_uint ("n-items",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperQueue:progression-mode:
   *
   * Queue progression mode.
   */
  param_specs[PROP_PROGRESSION_MODE] = g_param_spec_enum ("progression-mode",
      NULL, NULL, CLAPPER_TYPE_QUEUE_PROGRESSION_MODE, DEFAULT_PROGRESSION_MODE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
