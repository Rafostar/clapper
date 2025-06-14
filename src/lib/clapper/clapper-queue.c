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

/**
 * ClapperQueue:
 *
 * A queue of media to be played.
 */

#include <gio/gio.h>

#include "clapper-queue-private.h"
#include "clapper-media-item-private.h"
#include "clapper-player-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-reactables-manager-private.h"
#include "clapper-features-manager-private.h"

#define CLAPPER_QUEUE_GET_REC_LOCK(obj) (&CLAPPER_QUEUE_CAST(obj)->rec_lock)
#define CLAPPER_QUEUE_REC_LOCK(obj) g_rec_mutex_lock (CLAPPER_QUEUE_GET_REC_LOCK(obj))
#define CLAPPER_QUEUE_REC_UNLOCK(obj) g_rec_mutex_unlock (CLAPPER_QUEUE_GET_REC_LOCK(obj))

#define DEFAULT_PROGRESSION_MODE CLAPPER_QUEUE_PROGRESSION_NONE
#define DEFAULT_GAPLESS FALSE
#define DEFAULT_INSTANT FALSE

#define GST_CAT_DEFAULT clapper_queue_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperQueue
{
  GstObject parent;

  GRecMutex rec_lock;

  GPtrArray *items;
  ClapperMediaItem *current_item;
  guint current_index;

  ClapperQueueProgressionMode progression_mode;
  gboolean gapless;
  gboolean instant;

  /* Avoid scenario when "gapless" prop is changed
   * between "about-to-finish" and "EOS" */
  gboolean handled_gapless;
};

enum
{
  PROP_0,
  PROP_CURRENT_ITEM,
  PROP_CURRENT_INDEX,
  PROP_N_ITEMS,
  PROP_PROGRESSION_MODE,
  PROP_GAPLESS,
  PROP_INSTANT,
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

  CLAPPER_QUEUE_REC_LOCK (self);
  n_items = self->items->len;
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return n_items;
}

static gpointer
clapper_queue_list_model_get_item (GListModel *model, guint index)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (model);
  ClapperMediaItem *item = NULL;

  CLAPPER_QUEUE_REC_LOCK (self);
  if (G_LIKELY (index < self->items->len)) {
    GST_LOG_OBJECT (self, "Reading queue item: %u", index);
    item = g_object_ref (g_ptr_array_index (self->items, index));
  }
  CLAPPER_QUEUE_REC_UNLOCK (self);

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
_announce_model_update (ClapperQueue *self, guint index, guint removed, guint added,
    ClapperMediaItem *changed_item)
{
  GST_DEBUG_OBJECT (self, "Announcing model update, index: %u, removed: %u, added: %u",
      index, removed, added);

  /* We handle reposition separately */
  if (removed != added) {
    ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

    if (player) {
      gboolean have_features = clapper_player_get_have_features (player);

      if (added == 1) { // addition
        if (player->reactables_manager)
          clapper_reactables_manager_trigger_queue_item_added (player->reactables_manager, changed_item, index);
        if (have_features)
          clapper_features_manager_trigger_queue_item_added (player->features_manager, changed_item, index);
      } else if (removed == 1) { // removal
        if (player->reactables_manager)
          clapper_reactables_manager_trigger_queue_item_removed (player->reactables_manager, changed_item, index);
        if (have_features)
          clapper_features_manager_trigger_queue_item_removed (player->features_manager, changed_item, index);
      } else if (removed > 1 && added == 0) { // queue cleared
        if (player->reactables_manager)
          clapper_reactables_manager_trigger_queue_cleared (player->reactables_manager);
        if (have_features)
          clapper_features_manager_trigger_queue_cleared (player->features_manager);
      } else {
        g_assert_not_reached ();
      }
    }

    gst_clear_object (&player);
  }

  g_list_model_items_changed (G_LIST_MODEL (self), index, removed, added);

  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_N_ITEMS]);
}

static void
_announce_reposition (ClapperQueue *self, guint before, guint after)
{
  ClapperPlayer *player;

  GST_DEBUG_OBJECT (self, "Announcing item reposition: %u -> %u", before, after);

  if ((player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self)))) {
    if (player->reactables_manager)
      clapper_reactables_manager_trigger_queue_item_repositioned (player->reactables_manager, before, after);
    if (clapper_player_get_have_features (player))
      clapper_features_manager_trigger_queue_item_repositioned (player->features_manager, before, after);

    gst_object_unref (player);
  }
}

/*
 * Notify about current index change. This is needed only if some items
 * are added/removed before current selection, otherwise if selection
 * also changes use _announce_current_item_and_index_change() instead.
 */
static void
_announce_current_index_change (ClapperQueue *self)
{
  gboolean is_main_thread = g_main_context_is_owner (g_main_context_default ());

  GST_DEBUG_OBJECT (self, "Announcing current index change from %smain thread, now: %u",
      (is_main_thread) ? "" : "non-", self->current_index);

  if (is_main_thread) {
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_INDEX]);
  } else {
    ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

    if (G_LIKELY (player != NULL)) {
      clapper_app_bus_post_prop_notify (player->app_bus,
          GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);

      gst_object_unref (player);
    }
  }
}

/*
 * Notify about both current item and its index changes.
 * Needs to be called while holding CLAPPER_QUEUE_REC_LOCK.
 */
static void
_announce_current_item_and_index_change (ClapperQueue *self)
{
  ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));
  gboolean instant, is_main_thread;

  if (G_UNLIKELY (player == NULL))
    return;

  is_main_thread = g_main_context_is_owner (g_main_context_default ());

  GST_DEBUG_OBJECT (self, "Announcing current item change from %smain thread,"
      " now: %" GST_PTR_FORMAT " (index: %u)",
      (is_main_thread) ? "" : "non-", self->current_item, self->current_index);

  GST_OBJECT_LOCK (self);
  instant = self->instant;
  GST_OBJECT_UNLOCK (self);

  clapper_playbin_bus_post_current_item_change (player->bus, self->current_item,
      (instant) ? CLAPPER_QUEUE_ITEM_CHANGE_INSTANT : CLAPPER_QUEUE_ITEM_CHANGE_NORMAL);

  if (is_main_thread) {
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_ITEM]);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_INDEX]);
  } else {
    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_ITEM]);
    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);
  }

  gst_object_unref (player);
}

static inline gboolean
_replace_current_item_unlocked (ClapperQueue *self, ClapperMediaItem *item, guint index)
{
  if (gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (item))) {
    self->current_index = index;

    if (self->current_item)
      clapper_media_item_set_used (self->current_item, TRUE);

    GST_TRACE_OBJECT (self, "Current item replaced, now: %" GST_PTR_FORMAT, self->current_item);

    return TRUE;
  }

  return FALSE;
}

static void
_reset_shuffle_unlocked (ClapperQueue *self)
{
  guint i;

  for (i = 0; i < self->items->len; ++i) {
    ClapperMediaItem *item = g_ptr_array_index (self->items, i);
    clapper_media_item_set_used (item, FALSE);
  }
}

static ClapperMediaItem *
_get_next_item_unlocked (ClapperQueue *self, ClapperQueueProgressionMode mode)
{
  ClapperMediaItem *next_item = NULL;

  GST_DEBUG_OBJECT (self, "Handling progression mode: %u", mode);

  if (self->current_index == CLAPPER_QUEUE_INVALID_POSITION) {
    GST_DEBUG_OBJECT (self, "No current item, can not advance");
    return NULL;
  }

  switch (mode) {
    case CLAPPER_QUEUE_PROGRESSION_NONE:
      break;
    case CLAPPER_QUEUE_PROGRESSION_CAROUSEL:
      next_item = g_ptr_array_index (self->items, 0);
      G_GNUC_FALLTHROUGH;
    case CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE:
      if (self->current_index + 1 < self->items->len)
        next_item = g_ptr_array_index (self->items, self->current_index + 1);
      break;
    case CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM:
      next_item = self->current_item;
      break;
    case CLAPPER_QUEUE_PROGRESSION_SHUFFLE:{
      GList *unused = NULL;
      GRand *rand = g_rand_new ();
      guint i;

      for (i = 0; i < self->items->len; ++i) {
        ClapperMediaItem *item = g_ptr_array_index (self->items, i);

        if (!clapper_media_item_get_used (item))
          unused = g_list_append (unused, item);
      }

      if (unused) {
        next_item = g_list_nth_data (unused,
            g_rand_int_range (rand, 0, g_list_length (unused)));
        g_list_free (unused);
      } else {
        _reset_shuffle_unlocked (self);
        next_item = g_ptr_array_index (self->items,
            g_rand_int_range (rand, 0, self->items->len));
      }

      g_rand_free (rand);
      break;
    }
    default:
      g_assert_not_reached ();
      break;
  }

  if (next_item)
    gst_object_ref (next_item);

  return next_item;
}

/*
 * For gapless we need to manually replace current item in queue when it starts
 * playing and emit notify about change, this function will do that if necessary
 */
void
clapper_queue_handle_played_item_changed (ClapperQueue *self, ClapperMediaItem *played_item,
    ClapperAppBus *app_bus)
{
  guint index = 0;
  gboolean changed = FALSE;

  CLAPPER_QUEUE_REC_LOCK (self);

  /* Item is often the same here (when selected from queue),
   * so compare pointers first to avoid iterating queue */
  if (played_item != self->current_item
      && g_ptr_array_find (self->items, played_item, &index))
    changed = _replace_current_item_unlocked (self, played_item, index);

  CLAPPER_QUEUE_REC_UNLOCK (self);

  if (changed) {
    clapper_app_bus_post_prop_notify (app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_ITEM]);
    clapper_app_bus_post_prop_notify (app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);
  }
}

void
clapper_queue_handle_about_to_finish (ClapperQueue *self, ClapperPlayer *player)
{
  ClapperMediaItem *next_item;
  ClapperQueueProgressionMode progression_mode;

  GST_INFO_OBJECT (self, "Handling \"about-to-finish\"");

  GST_OBJECT_LOCK (self);
  if (!(self->handled_gapless = self->gapless)) {
    GST_OBJECT_UNLOCK (self);
    return;
  }
  progression_mode = self->progression_mode;
  GST_OBJECT_UNLOCK (self);

  CLAPPER_QUEUE_REC_LOCK (self);
  next_item = _get_next_item_unlocked (self, progression_mode);
  CLAPPER_QUEUE_REC_UNLOCK (self);

  if (next_item) {
    clapper_player_set_pending_item (player, next_item, CLAPPER_QUEUE_ITEM_CHANGE_GAPLESS);
    gst_object_unref (next_item);
  }
}

gboolean
clapper_queue_handle_eos (ClapperQueue *self, ClapperPlayer *player)
{
  ClapperMediaItem *next_item = NULL;
  ClapperQueueProgressionMode progression_mode;
  gboolean handled_eos = FALSE;

  /* On gapless "about-to-finish" selects next item instead and
   * we can reach EOS only if there was either nothing to select or
   * some playback error ocurred */

  GST_INFO_OBJECT (self, "Handling EOS");

  GST_OBJECT_LOCK (self);
  if (self->handled_gapless) {
    self->handled_gapless = FALSE; // reset
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }
  progression_mode = self->progression_mode;
  GST_OBJECT_UNLOCK (self);

  CLAPPER_QUEUE_REC_LOCK (self);
  if ((next_item = _get_next_item_unlocked (self, progression_mode))) {
    if (next_item == self->current_item)
      clapper_player_seek (player, 0);
    else
      clapper_queue_select_item (self, next_item);

    handled_eos = TRUE;
    gst_object_unref (next_item);
  }
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return handled_eos;
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
  g_return_if_fail (CLAPPER_IS_QUEUE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));
  g_return_if_fail (index >= -1);

  CLAPPER_QUEUE_REC_LOCK (self);

  if (!g_ptr_array_find (self->items, item, NULL)) {
    guint prev_length = self->items->len;

    g_ptr_array_insert (self->items, index, gst_object_ref (item));
    gst_object_set_parent (GST_OBJECT_CAST (item), GST_OBJECT_CAST (self));

    /* In append we inserted at array length */
    if (index < 0)
      index = prev_length;

    _announce_model_update (self, index, 0, 1, item);

    /* If has selection and inserting before it */
    if (self->current_index != CLAPPER_QUEUE_INVALID_POSITION
        && (guint) index <= self->current_index) {
      self->current_index++;
      _announce_current_index_change (self);
    } else if (prev_length == 0 && _replace_current_item_unlocked (self, item, 0)) {
      /* If queue was empty, auto select first item and announce it */
      _announce_current_item_and_index_change (self);
    } else if (self->current_index == prev_length - 1
        && clapper_queue_get_progression_mode (self) == CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE) {
      ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));
      gboolean after_eos = (gboolean) g_atomic_int_get (&player->eos);

      /* In consecutive progression automatically select next item
       * if we were after EOS of last queue item */
      if (after_eos && _replace_current_item_unlocked (self, item, index))
        _announce_current_item_and_index_change (self);

      gst_object_unref (player);
    }
  }

  CLAPPER_QUEUE_REC_UNLOCK (self);
}

/**
 * clapper_queue_reposition_item:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem
 * @index: the index to place @item in queue, -1 to place at the end
 *
 * Change position of one #ClapperMediaItem within the queue.
 *
 * Note that the @index is the new position you expect item to be
 * after whole reposition operation is finished.
 *
 * If item is not in the queue, this function will do nothing.
 */
void
clapper_queue_reposition_item (ClapperQueue *self, ClapperMediaItem *item, gint index)
{
  guint index_old = 0;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));
  g_return_if_fail (index >= -1);

  CLAPPER_QUEUE_REC_LOCK (self);

  if (g_ptr_array_find (self->items, item, &index_old)) {
    ClapperMediaItem *removed_item;
    guint index_new, start_index, end_index, n_changed;

    index_new = (index < 0)
        ? self->items->len - 1
        : (guint) index;

    GST_DEBUG_OBJECT (self, "Reposition item %u -> %u, is_current: %s",
        index_old, index_new, (item == self->current_item) ? "yes" : "no");

    removed_item = g_ptr_array_steal_index (self->items, index_old);
    g_ptr_array_insert (self->items, index_new, removed_item);

    _announce_reposition (self, index_old, index_new);

    if (self->current_index != CLAPPER_QUEUE_INVALID_POSITION) {
      guint before = self->current_index;

      if (index_old > self->current_index && index_new <= self->current_index)
        self->current_index++; // Moved before current item
      else if (index_old < self->current_index && index_new >= self->current_index)
        self->current_index--; // Moved after current item
      else if (index_old == self->current_index)
        self->current_index = index_new; // Moved current item

      if (self->current_index != before)
        _announce_current_index_change (self);
    }

    start_index = MIN (index_old, index_new);
    end_index = MAX (index_old, index_new);
    n_changed = end_index - start_index + 1;

    _announce_model_update (self, start_index, n_changed, n_changed, item);
  }

  CLAPPER_QUEUE_REC_UNLOCK (self);
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
  guint index = 0;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  CLAPPER_QUEUE_REC_LOCK (self);

  if (g_ptr_array_find (self->items, item, &index))
    clapper_queue_remove_index (self, index);

  CLAPPER_QUEUE_REC_UNLOCK (self);
}

/**
 * clapper_queue_remove_index:
 * @queue: a #ClapperQueue
 * @index: an item index
 *
 * Removes #ClapperMediaItem at @index from the queue.
 */
void
clapper_queue_remove_index (ClapperQueue *self, guint index)
{
  ClapperMediaItem *item = clapper_queue_steal_index (self, index);
  gst_clear_object (&item);
}

/**
 * clapper_queue_steal_index:
 * @queue: a #ClapperQueue
 * @index: an item index
 *
 * Removes #ClapperMediaItem at @index from the queue.
 *
 * Returns: (transfer full) (nullable): The removed #ClapperMediaItem at @index.
 */
ClapperMediaItem *
clapper_queue_steal_index (ClapperQueue *self, guint index)
{
  ClapperMediaItem *removed_item = NULL;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), NULL);
  g_return_val_if_fail (index != CLAPPER_QUEUE_INVALID_POSITION, NULL);

  CLAPPER_QUEUE_REC_LOCK (self);

  if (index < self->items->len) {
    if (index == self->current_index
        && _replace_current_item_unlocked (self, NULL, CLAPPER_QUEUE_INVALID_POSITION)) {
      _announce_current_item_and_index_change (self);
    } else if (self->current_index != CLAPPER_QUEUE_INVALID_POSITION
        && index < self->current_index) {
      /* If has selection and removed before it */
      self->current_index--;
      _announce_current_index_change (self);
    }

    removed_item = g_ptr_array_steal_index (self->items, index);
    gst_object_unparent (GST_OBJECT_CAST (removed_item));

    _announce_model_update (self, index, 1, 0, removed_item);
  }

  CLAPPER_QUEUE_REC_UNLOCK (self);

  return removed_item;
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

  g_return_if_fail (CLAPPER_IS_QUEUE (self));

  CLAPPER_QUEUE_REC_LOCK (self);

  n_items = self->items->len;

  if (n_items > 0) {
    if (_replace_current_item_unlocked (self, NULL, CLAPPER_QUEUE_INVALID_POSITION))
      _announce_current_item_and_index_change (self);

    g_ptr_array_remove_range (self->items, 0, n_items);
    _announce_model_update (self, 0, n_items, 0, NULL);
  }

  CLAPPER_QUEUE_REC_UNLOCK (self);
}

/**
 * clapper_queue_select_item:
 * @queue: a #ClapperQueue
 * @item: (nullable): a #ClapperMediaItem or %NULL to unselect
 *
 * Selects #ClapperMediaItem from @queue as current one or
 * unselects currently selected item when @item is %NULL.
 *
 * Returns: %TRUE if item could be selected/unselected,
 *   %FALSE if it was not in the queue.
 */
gboolean
clapper_queue_select_item (ClapperQueue *self, ClapperMediaItem *item)
{
  gboolean success = FALSE;
  guint index = 0;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);
  g_return_val_if_fail (item == NULL || CLAPPER_IS_MEDIA_ITEM (item), FALSE);

  CLAPPER_QUEUE_REC_LOCK (self);
  if (!item)
    success = clapper_queue_select_index (self, CLAPPER_QUEUE_INVALID_POSITION);
  else if (g_ptr_array_find (self->items, item, &index))
    success = clapper_queue_select_index (self, index);
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * clapper_queue_select_index:
 * @queue: a #ClapperQueue
 * @index: an item index or [const@Clapper.QUEUE_INVALID_POSITION] to unselect
 *
 * Selects #ClapperMediaItem at @index from @queue as current one or
 * unselects currently selected index when @index is [const@Clapper.QUEUE_INVALID_POSITION].
 *
 * Returns: %TRUE if item at @index could be selected/unselected,
 *   %FALSE if index was out of queue range.
 */
gboolean
clapper_queue_select_index (ClapperQueue *self, guint index)
{
  ClapperMediaItem *item = NULL;
  gboolean success;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  CLAPPER_QUEUE_REC_LOCK (self);
  if (index != CLAPPER_QUEUE_INVALID_POSITION && index < self->items->len)
    item = g_ptr_array_index (self->items, index);
  if ((success = (index == CLAPPER_QUEUE_INVALID_POSITION
      || index < self->items->len))) {
    if (_replace_current_item_unlocked (self, item, index))
      _announce_current_item_and_index_change (self);
  }
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * clapper_queue_select_next_item:
 * @queue: a #ClapperQueue
 *
 * Selects next #ClapperMediaItem from @queue for playback.
 *
 * Note that this will try to select next item in the order
 * of the queue, regardless of [enum@Clapper.QueueProgressionMode] set.
 *
 * Returns: %TRUE if there was another media item in queue, %FALSE otherwise.
 */
gboolean
clapper_queue_select_next_item (ClapperQueue *self)
{
  gboolean success = FALSE;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  CLAPPER_QUEUE_REC_LOCK (self);

  if (self->current_index != CLAPPER_QUEUE_INVALID_POSITION
      && self->current_index < self->items->len - 1) {
    GST_DEBUG_OBJECT (self, "Selecting next queue item");
    success = clapper_queue_select_index (self, self->current_index + 1);
  }

  CLAPPER_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * clapper_queue_select_previous_item:
 * @queue: a #ClapperQueue
 *
 * Selects previous #ClapperMediaItem from @queue for playback.
 *
 * Note that this will try to select previous item in the order
 * of the queue, regardless of [enum@Clapper.QueueProgressionMode] set.
 *
 * Returns: %TRUE if there was previous media item in queue, %FALSE otherwise.
 */
gboolean
clapper_queue_select_previous_item (ClapperQueue *self)
{
  gboolean success = FALSE;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  CLAPPER_QUEUE_REC_LOCK (self);

  if (self->current_index != CLAPPER_QUEUE_INVALID_POSITION
      && self->current_index > 0) {
    GST_DEBUG_OBJECT (self, "Selecting previous queue item");
    success = clapper_queue_select_index (self, self->current_index - 1);
  }

  CLAPPER_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * clapper_queue_get_item: (skip)
 * @queue: a #ClapperQueue
 * @index: an item index
 *
 * Get the #ClapperMediaItem at index.
 *
 * This behaves the same as [method@Gio.ListModel.get_item], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * This function is not available in bindings as they already
 * inherit `get_item()` method from [iface@Gio.ListModel] interface.
 *
 * Returns: (transfer full) (nullable): The #ClapperMediaItem at @index.
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
 * Returns: (transfer full) (nullable): The current #ClapperMediaItem.
 */
ClapperMediaItem *
clapper_queue_get_current_item (ClapperQueue *self)
{
  ClapperMediaItem *item = NULL;

  /* XXX: For updating media item during playback we should
   * use `player->played_item` instead to not be racy when
   * changing and updating current item at the same time */

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), NULL);

  CLAPPER_QUEUE_REC_LOCK (self);
  if (self->current_item)
    item = gst_object_ref (self->current_item);
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return item;
}

/**
 * clapper_queue_get_current_index:
 * @queue: a #ClapperQueue
 *
 * Get index of the currently selected #ClapperMediaItem.
 *
 * Returns: Current item index or [const@Clapper.QUEUE_INVALID_POSITION]
 *   when nothing is selected.
 */
guint
clapper_queue_get_current_index (ClapperQueue *self)
{
  guint index;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), CLAPPER_QUEUE_INVALID_POSITION);

  CLAPPER_QUEUE_REC_LOCK (self);
  index = self->current_index;
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return index;
}

/**
 * clapper_queue_item_is_current:
 * @queue: a #ClapperQueue
 * @item: a #ClapperMediaItem to check
 *
 * Checks if given #ClapperMediaItem is currently selected.
 *
 * Returns: %TRUE if @item is a current media item, %FALSE otherwise.
 */
gboolean
clapper_queue_item_is_current (ClapperQueue *self, ClapperMediaItem *item)
{
  gboolean is_current;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (item), FALSE);

  CLAPPER_QUEUE_REC_LOCK (self);
  is_current = (item == self->current_item);
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return is_current;
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

  CLAPPER_QUEUE_REC_LOCK (self);
  found = g_ptr_array_find (self->items, item, index);
  CLAPPER_QUEUE_REC_UNLOCK (self);

  return found;
}

/**
 * clapper_queue_get_n_items: (skip)
 * @queue: a #ClapperQueue
 *
 * Get the number of items in #ClapperQueue.
 *
 * This behaves the same as [method@Gio.ListModel.get_n_items], and is here
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
  gboolean changed;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->progression_mode != mode))
    self->progression_mode = mode;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

    /* Start shuffle from the current item, allowing
     * reselecting past items already used without it */
    if (mode == CLAPPER_QUEUE_PROGRESSION_SHUFFLE) {
      CLAPPER_QUEUE_REC_LOCK (self);

      _reset_shuffle_unlocked (self);
      if (self->current_item)
        clapper_media_item_set_used (self->current_item, TRUE);

      CLAPPER_QUEUE_REC_UNLOCK (self);
    }

    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_PROGRESSION_MODE]);
    if (player->reactables_manager)
      clapper_reactables_manager_trigger_queue_progression_changed (player->reactables_manager, mode);
    if (clapper_player_get_have_features (player))
      clapper_features_manager_trigger_queue_progression_changed (player->features_manager, mode);

    gst_object_unref (player);
  }
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

/**
 * clapper_queue_set_gapless:
 * @queue: a #ClapperQueue
 * @gapless: %TRUE to enable, %FALSE otherwise.
 *
 * Set #ClapperQueue progression to be gapless.
 *
 * Gapless playback will try to re-use as much as possible of underlying
 * GStreamer elements when #ClapperQueue progresses, removing any
 * potential gap in the data.
 *
 * Enabling this option mostly makes sense when used together with
 * [property@Clapper.Queue:progression-mode] property set to
 * [enum@Clapper.QueueProgressionMode.CONSECUTIVE].
 *
 * NOTE: This feature within GStreamer is rather new and
 * might still cause playback issues. Disabled by default.
 */
void
clapper_queue_set_gapless (ClapperQueue *self, gboolean gapless)
{
  gboolean changed;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->gapless != gapless))
    self->gapless = gapless;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_GAPLESS]);

    gst_object_unref (player);
  }
}

/**
 * clapper_queue_get_gapless:
 * @queue: a #ClapperQueue
 *
 * Get if #ClapperQueue is set to use gapless progression.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
clapper_queue_get_gapless (ClapperQueue *self)
{
  gboolean gapless;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  GST_OBJECT_LOCK (self);
  gapless = self->gapless;
  GST_OBJECT_UNLOCK (self);

  return gapless;
}

/**
 * clapper_queue_set_instant:
 * @queue: a #ClapperQueue
 * @instant: %TRUE to enable, %FALSE otherwise.
 *
 * Set #ClapperQueue media item changes to be instant.
 *
 * Instant will try to re-use as much as possible of underlying
 * GStreamer elements when #ClapperMediaItem is selected, allowing
 * media item change requests to be faster.
 *
 * NOTE: This feature within GStreamer is rather new and
 * might still cause playback issues. Disabled by default.
 */
void
clapper_queue_set_instant (ClapperQueue *self, gboolean instant)
{
  gboolean changed;

  g_return_if_fail (CLAPPER_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->instant != instant))
    self->instant = instant;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_INSTANT]);

    gst_object_unref (player);
  }
}

/**
 * clapper_queue_get_instant:
 * @queue: a #ClapperQueue
 *
 * Get if #ClapperQueue is set to use instant media item changes.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
clapper_queue_get_instant (ClapperQueue *self)
{
  gboolean instant;

  g_return_val_if_fail (CLAPPER_IS_QUEUE (self), FALSE);

  GST_OBJECT_LOCK (self);
  instant = self->instant;
  GST_OBJECT_UNLOCK (self);

  return instant;
}

static void
_item_remove_func (ClapperMediaItem *item)
{
  gst_object_unparent (GST_OBJECT_CAST (item));
  gst_object_unref (item);
}

static void
clapper_queue_init (ClapperQueue *self)
{
  g_rec_mutex_init (&self->rec_lock);

  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify) _item_remove_func);

  self->current_index = CLAPPER_QUEUE_INVALID_POSITION;
  self->progression_mode = DEFAULT_PROGRESSION_MODE;
  self->gapless = DEFAULT_GAPLESS;
  self->instant = DEFAULT_INSTANT;
}

static void
clapper_queue_finalize (GObject *object)
{
  ClapperQueue *self = CLAPPER_QUEUE_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_rec_mutex_clear (&self->rec_lock);

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
    case PROP_CURRENT_INDEX:
      g_value_set_uint (value, clapper_queue_get_current_index (self));
      break;
    case PROP_N_ITEMS:
      g_value_set_uint (value, clapper_queue_get_n_items (self));
      break;
    case PROP_PROGRESSION_MODE:
      g_value_set_enum (value, clapper_queue_get_progression_mode (self));
      break;
    case PROP_GAPLESS:
      g_value_set_boolean (value, clapper_queue_get_gapless (self));
      break;
    case PROP_INSTANT:
      g_value_set_boolean (value, clapper_queue_get_instant (self));
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
    case PROP_CURRENT_INDEX:
      clapper_queue_select_index (self, g_value_get_uint (value));
      break;
    case PROP_PROGRESSION_MODE:
      clapper_queue_set_progression_mode (self, g_value_get_enum (value));
      break;
    case PROP_GAPLESS:
      clapper_queue_set_gapless (self, g_value_get_boolean (value));
      break;
    case PROP_INSTANT:
      clapper_queue_set_instant (self, g_value_get_boolean (value));
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
   * ClapperQueue:current-index:
   *
   * Index of currently selected media item for playback.
   */
  param_specs[PROP_CURRENT_INDEX] = g_param_spec_uint ("current-index",
      NULL, NULL, 0, G_MAXUINT, CLAPPER_QUEUE_INVALID_POSITION,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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

  /**
   * ClapperQueue:gapless:
   *
   * Use gapless progression.
   */
  param_specs[PROP_GAPLESS] = g_param_spec_boolean ("gapless",
      NULL, NULL, DEFAULT_GAPLESS,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperQueue:instant:
   *
   * Use instant media item changes.
   */
  param_specs[PROP_INSTANT] = g_param_spec_boolean ("instant",
      NULL, NULL, DEFAULT_INSTANT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
