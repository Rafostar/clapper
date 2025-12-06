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
 * ClapperTimeline:
 *
 * A media timeline filled with point markers.
 */

#include <gio/gio.h>

#include "clapper-enums.h"
#include "clapper-timeline-private.h"
#include "clapper-marker-private.h"
#include "clapper-player-private.h"
#include "clapper-reactables-manager-private.h"
#include "clapper-features-manager-private.h"
#include "clapper-utils-private.h"

#define GST_CAT_DEFAULT clapper_timeline_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperTimeline
{
  GstObject parent;

  GSequence *markers_seq;

  GstToc *toc;
  GPtrArray *pending_markers;
  gboolean needs_refresh;
};

enum
{
  PROP_0,
  PROP_N_MARKERS,
  PROP_LAST
};

static GType
clapper_timeline_list_model_get_item_type (GListModel *model)
{
  return CLAPPER_TYPE_MARKER;
}

static guint
clapper_timeline_list_model_get_n_items (GListModel *model)
{
  ClapperTimeline *self = CLAPPER_TIMELINE_CAST (model);
  guint n_markers;

  GST_OBJECT_LOCK (self);
  n_markers = g_sequence_get_length (self->markers_seq);
  GST_OBJECT_UNLOCK (self);

  return n_markers;
}

static gpointer
clapper_timeline_list_model_get_item (GListModel *model, guint index)
{
  ClapperTimeline *self = CLAPPER_TIMELINE_CAST (model);
  GSequenceIter *iter;
  ClapperMarker *marker = NULL;

  GST_OBJECT_LOCK (self);
  iter = g_sequence_get_iter_at_pos (self->markers_seq, index);
  if (!g_sequence_iter_is_end (iter))
    marker = gst_object_ref (g_sequence_get (iter));
  GST_OBJECT_UNLOCK (self);

  return marker;
}

static void
clapper_timeline_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = clapper_timeline_list_model_get_item_type;
  iface->get_n_items = clapper_timeline_list_model_get_n_items;
  iface->get_item = clapper_timeline_list_model_get_item;
}

#define parent_class clapper_timeline_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperTimeline, clapper_timeline, GST_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, clapper_timeline_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
clapper_timeline_post_item_updated (ClapperTimeline *self)
{
  ClapperPlayer *player;

  if ((player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self)))) {
    ClapperMediaItem *item;

    if ((item = CLAPPER_MEDIA_ITEM_CAST (gst_object_get_parent (GST_OBJECT_CAST (self))))) {
      ClapperFeaturesManager *features_manager;

      if (player->reactables_manager) {
        clapper_reactables_manager_trigger_item_updated (player->reactables_manager, item,
            CLAPPER_REACTABLE_ITEM_UPDATED_TIMELINE);
      }
      if ((features_manager = clapper_player_get_features_manager (player)))
        clapper_features_manager_trigger_item_updated (features_manager, item);

      gst_object_unref (item);
    }

    gst_object_unref (player);
  }
}

static gint
_markers_compare_func (gconstpointer marker_a, gconstpointer marker_b,
    gpointer user_data G_GNUC_UNUSED)
{
  gint64 val_a, val_b, result;

  /* Can happen if someone tries to insert already
   * inserted marker pointer */
  if (marker_a == marker_b)
    return 0;

  /* 1 millisecond accuracy should be enough */
  val_a = clapper_marker_get_start (CLAPPER_MARKER_CAST (marker_a)) * 1000;
  val_b = clapper_marker_get_start (CLAPPER_MARKER_CAST (marker_b)) * 1000;

  /* If start time is the same, sort by earliest end time */
  if (val_a == val_b) {
    val_a = clapper_marker_get_end (CLAPPER_MARKER_CAST (marker_a)) * 1000;
    val_b = clapper_marker_get_end (CLAPPER_MARKER_CAST (marker_b)) * 1000;

    /* If both times are the same, check type and if they also are
     * the same, we will assume that this is the same marker overall */
    if (val_a == val_b) {
      val_a = clapper_marker_get_marker_type (CLAPPER_MARKER_CAST (marker_a));
      val_b = clapper_marker_get_marker_type (CLAPPER_MARKER_CAST (marker_b));
    }
  }

  result = val_a - val_b;

  return (result > 0) ? 1 : (result < 0) ? -1 : 0;
}

/*
 * clapper_timeline_new:
 *
 * Returns: (transfer full): a new #ClapperTimeline instance
 */
ClapperTimeline *
clapper_timeline_new (void)
{
  ClapperTimeline *timeline;

  timeline = g_object_new (CLAPPER_TYPE_TIMELINE, NULL);
  gst_object_ref_sink (timeline);

  return timeline;
}

static inline gint
_take_marker_unlocked (ClapperTimeline *self, ClapperMarker *marker)
{
  GSequenceIter *iter;

  iter = g_sequence_insert_sorted (self->markers_seq, marker,
      (GCompareDataFunc) _markers_compare_func, NULL);
  gst_object_set_parent (GST_OBJECT_CAST (marker), GST_OBJECT_CAST (self));

  return g_sequence_iter_get_position (iter);
}

/**
 * clapper_timeline_insert_marker:
 * @timeline: a #ClapperTimeline
 * @marker: a #ClapperMarker
 *
 * Insert the #ClapperMarker into @timeline.
 */
void
clapper_timeline_insert_marker (ClapperTimeline *self, ClapperMarker *marker)
{
  gboolean success;
  gint position = 0;

  g_return_if_fail (CLAPPER_IS_TIMELINE (self));
  g_return_if_fail (CLAPPER_IS_MARKER (marker));

  GST_OBJECT_LOCK (self);

  if ((success = !g_sequence_lookup (self->markers_seq, marker,
      (GCompareDataFunc) _markers_compare_func, NULL)))
    position = _take_marker_unlocked (self, gst_object_ref (marker));

  GST_OBJECT_UNLOCK (self);

  if (success) {
    g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_N_MARKERS]);

    clapper_timeline_post_item_updated (self);
  }
}

/**
 * clapper_timeline_remove_marker:
 * @timeline: a #ClapperTimeline
 * @marker: a #ClapperMarker
 *
 * Removes #ClapperMarker from the timeline if present.
 */
void
clapper_timeline_remove_marker (ClapperTimeline *self, ClapperMarker *marker)
{
  GSequenceIter *iter;
  gint position = 0;
  gboolean success = FALSE;

  g_return_if_fail (CLAPPER_IS_TIMELINE (self));
  g_return_if_fail (CLAPPER_IS_MARKER (marker));

  GST_OBJECT_LOCK (self);

  if ((iter = g_sequence_lookup (self->markers_seq, marker,
      (GCompareDataFunc) _markers_compare_func, NULL))) {
    position = g_sequence_iter_get_position (iter);
    g_sequence_remove (iter);

    success = TRUE;
  }

  GST_OBJECT_UNLOCK (self);

  if (success) {
    g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_N_MARKERS]);

    clapper_timeline_post_item_updated (self);
  }
}

/**
 * clapper_timeline_get_marker:
 * @timeline: a #ClapperTimeline
 * @index: a marker index
 *
 * Get the #ClapperMarker at index.
 *
 * This behaves the same as [method@Gio.ListModel.get_item], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: (transfer full) (nullable): The #ClapperMarker at @index.
 */
ClapperMarker *
clapper_timeline_get_marker (ClapperTimeline *self, guint index)
{
  g_return_val_if_fail (CLAPPER_IS_TIMELINE (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * clapper_timeline_get_n_markers:
 * @timeline: a #ClapperTimeline
 *
 * Get the number of markers in #ClapperTimeline.
 *
 * This behaves the same as [method@Gio.ListModel.get_n_items], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: The number of markers in #ClapperTimeline.
 */
guint
clapper_timeline_get_n_markers (ClapperTimeline *self)
{
  g_return_val_if_fail (CLAPPER_IS_TIMELINE (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

static void
_append_marker_from_toc_entry (ClapperTimeline *self, GstTocEntry *entry, GList **markers)
{
  ClapperMarker *marker;
  ClapperMarkerType marker_type;
  GstTagList *tags;
  gchar *title = NULL;
  gint64 start = 0, stop = 0;
  gdouble marker_start = 0, marker_end = CLAPPER_MARKER_NO_END;

  switch (gst_toc_entry_get_entry_type (entry)) {
    case GST_TOC_ENTRY_TYPE_TITLE:
      marker_type = CLAPPER_MARKER_TYPE_TITLE;
      break;
    case GST_TOC_ENTRY_TYPE_TRACK:
      marker_type = CLAPPER_MARKER_TYPE_TRACK;
      break;
    case GST_TOC_ENTRY_TYPE_CHAPTER:
      marker_type = CLAPPER_MARKER_TYPE_CHAPTER;
      break;
    default:
      return;
  }

  /* Start time is required */
  if (G_UNLIKELY (!gst_toc_entry_get_start_stop_times (entry, &start, NULL)))
    return;

  marker_start = (gdouble) start / GST_SECOND;
  if (gst_toc_entry_get_start_stop_times (entry, NULL, &stop))
    marker_end = (gdouble) stop / GST_SECOND;

  if ((tags = gst_toc_entry_get_tags (entry)))
    gst_tag_list_get_string_index (tags, GST_TAG_TITLE, 0, &title);

  marker = clapper_marker_new_internal (marker_type,
      title, marker_start, marker_end);
  *markers = g_list_append (*markers, marker);

  g_free (title);
}

static void
_iterate_toc_entries (ClapperTimeline *self, GList *entries, GList **markers)
{
  GList *en;

  for (en = entries; en != NULL; en = en->next) {
    GstTocEntry *entry = (GstTocEntry *) en->data;

    if (gst_toc_entry_is_alternative (entry))
      _iterate_toc_entries (self, gst_toc_entry_get_sub_entries (entry), markers);
    else if (gst_toc_entry_is_sequence (entry))
      _append_marker_from_toc_entry (self, entry, markers);
  }
}

static inline void
_prepare_markers (ClapperTimeline *self, GstToc *toc)
{
  GList *entries = gst_toc_get_entries (toc);
  GList *ma, *markers = NULL;

  GST_DEBUG_OBJECT (self, "Preparing markers from TOC: %" GST_PTR_FORMAT, toc);
  _iterate_toc_entries (self, entries, &markers);

  GST_OBJECT_LOCK (self);

  g_ptr_array_remove_range (self->pending_markers, 0, self->pending_markers->len);
  for (ma = markers; ma != NULL; ma = ma->next)
    g_ptr_array_add (self->pending_markers, CLAPPER_MARKER_CAST (ma->data));

  self->needs_refresh = TRUE;

  GST_OBJECT_UNLOCK (self);

  if (markers)
    g_list_free (markers);
}

gboolean
clapper_timeline_set_toc (ClapperTimeline *self, GstToc *toc, gboolean updated)
{
  gboolean changed;

  if (gst_toc_get_scope (toc) != GST_TOC_SCOPE_GLOBAL)
    return FALSE;

  GST_OBJECT_LOCK (self);

  if (self->toc == toc) {
    changed = updated;
  } else {
    /* FIXME: Iterate and compare entries and their amount
     * one by one, so we can avoid update between discovery and playback
     * (and also when playing the same media item again) */
    changed = TRUE;
  }

  if (changed) {
    if (self->toc)
      gst_toc_unref (self->toc);

    self->toc = gst_toc_ref (toc);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    _prepare_markers (self, toc);

  return changed;
}

/* Must be called from main thread */
void
clapper_timeline_refresh (ClapperTimeline *self)
{
  GSequenceIter *iter;
  GList *rec, *rec_markers = NULL;
  gpointer *stolen_markers;
  gsize n_pending = 0;
  guint i, n_before, n_after;

  GST_OBJECT_LOCK (self);

  /* This prevents us from incorrect behaviour when there were multiple
   * TOC objects set in a row before we reached main thread handling
   * for them here and refresh will be now invoked in a row, possibly
   * erasing markers on its second run */
  if (!self->needs_refresh) {
    GST_OBJECT_UNLOCK (self);
    return;
  }

  GST_DEBUG_OBJECT (self, "Timeline refresh");

  n_before = g_sequence_get_length (self->markers_seq);

  /* Recover markers that should remain */
  iter = g_sequence_get_begin_iter (self->markers_seq);
  while (!g_sequence_iter_is_end (iter)) {
    ClapperMarker *marker = CLAPPER_MARKER_CAST (g_sequence_get (iter));

    if (!clapper_marker_is_internal (marker))
      rec_markers = g_list_append (rec_markers, gst_object_ref (marker));

    iter = g_sequence_iter_next (iter);
  }

  /* Clear sequence */
  g_sequence_remove_range (
      g_sequence_get_begin_iter (self->markers_seq),
      g_sequence_get_end_iter (self->markers_seq));

  /* Transfer pending markers into sequence */
  stolen_markers = g_ptr_array_steal (self->pending_markers, &n_pending);
  for (i = 0; i < n_pending; ++i) {
    g_sequence_append (self->markers_seq, CLAPPER_MARKER_CAST (stolen_markers[i]));
    gst_object_set_parent (GST_OBJECT_CAST (stolen_markers[i]), GST_OBJECT_CAST (self));
  }
  g_free (stolen_markers);

  /* Transfer recovered markers back into sequence */
  for (rec = rec_markers; rec != NULL; rec = rec->next) {
    ClapperMarker *marker = CLAPPER_MARKER_CAST (rec->data);

    g_sequence_append (self->markers_seq, marker);
    gst_object_set_parent (GST_OBJECT_CAST (marker), GST_OBJECT_CAST (self));
  }
  if (rec_markers)
    g_list_free (rec_markers);

  /* Sort once after all appends (this way is faster according to documentation) */
  g_sequence_sort (self->markers_seq, _markers_compare_func, NULL);

  n_after = g_sequence_get_length (self->markers_seq);
  self->needs_refresh = FALSE;

  GST_OBJECT_UNLOCK (self);

  GST_DEBUG_OBJECT (self, "Timeline refreshed, n_before: %u, n_after: %u",
      n_before, n_after);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_before, n_after);
  if (n_before != n_after)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_N_MARKERS]);

  clapper_timeline_post_item_updated (self);
}

static void
_marker_remove_func (ClapperMarker *marker)
{
  gst_object_unparent (GST_OBJECT_CAST (marker));
  gst_object_unref (marker);
}

static void
clapper_timeline_init (ClapperTimeline *self)
{
  self->markers_seq = g_sequence_new ((GDestroyNotify) _marker_remove_func);
  self->pending_markers = g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);
}

static void
clapper_timeline_finalize (GObject *object)
{
  ClapperTimeline *self = CLAPPER_TIMELINE_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_sequence_free (self->markers_seq);

  if (self->toc)
    gst_toc_unref (self->toc);

  g_ptr_array_unref (self->pending_markers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_timeline_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperTimeline *self = CLAPPER_TIMELINE_CAST (object);

  switch (prop_id) {
    case PROP_N_MARKERS:
      g_value_set_uint (value, clapper_timeline_get_n_markers (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_timeline_class_init (ClapperTimelineClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertimeline", 0,
      "Clapper Timeline");

  gobject_class->get_property = clapper_timeline_get_property;
  gobject_class->finalize = clapper_timeline_finalize;

  /**
   * ClapperTimeline:n-markers:
   *
   * Number of markers in the timeline.
   */
  param_specs[PROP_N_MARKERS] = g_param_spec_uint ("n-markers",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
