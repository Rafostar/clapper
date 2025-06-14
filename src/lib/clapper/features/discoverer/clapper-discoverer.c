/*
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
 * ClapperDiscoverer:
 *
 * An optional Discoverer feature to be added to the player.
 *
 * #ClapperDiscoverer is a feature that wraps around #GstDiscoverer API
 * to automatically discover items within [class@Clapper.Queue]. Once media
 * is scanned, all extra information of it will be filled within media item,
 * this includes title, duration, chapters, etc.
 *
 * Please note that media items are also discovered during their playback
 * by the player itself. #ClapperDiscoverer is useful in situations where
 * one wants to present to the user an updated media item before its
 * playback, such as an UI that displays playback queue.
 *
 * Depending on your application, you can select an optimal
 * [enum@Clapper.DiscovererDiscoveryMode] that best suits your needs.
 *
 * Use [const@Clapper.HAVE_DISCOVERER] macro to check if Clapper API
 * was compiled with this feature.
 */

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "clapper-discoverer.h"
#include "clapper-queue.h"
#include "clapper-media-item-private.h"
#include "../shared/clapper-shared-utils-private.h"

#define DEFAULT_DISCOVERY_MODE CLAPPER_DISCOVERER_DISCOVERY_NONCURRENT

#define GST_CAT_DEFAULT clapper_discoverer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperDiscoverer
{
  ClapperFeature parent;

  GstDiscoverer *discoverer;

  GPtrArray *pending_items;
  ClapperMediaItem *discovered_item;

  gboolean running;
  GSource *timeout_source;

  ClapperDiscovererDiscoveryMode discovery_mode;
};

enum
{
  PROP_0,
  PROP_DISCOVERY_MODE,
  PROP_LAST
};

#define parent_class clapper_discoverer_parent_class
G_DEFINE_TYPE (ClapperDiscoverer, clapper_discoverer, CLAPPER_TYPE_FEATURE);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static inline void
_clear_timeout_source (ClapperDiscoverer *self)
{
  if (self->timeout_source) {
    g_source_destroy (self->timeout_source);
    g_clear_pointer (&self->timeout_source, g_source_unref);
  }
}

static inline void
_unqueue_discovery (ClapperDiscoverer *self, ClapperMediaItem *item)
{
  guint index = 0;

  /* Removing item that is being discovered */
  if (item == self->discovered_item) {
    GST_DEBUG_OBJECT (self, "Ignoring discovery of current item %" GST_PTR_FORMAT, item);
    gst_clear_object (&self->discovered_item);
  } else if (g_ptr_array_find (self->pending_items, item, &index)) {
    GST_DEBUG_OBJECT (self, "Removing discovery of pending item %" GST_PTR_FORMAT, item);
    g_ptr_array_remove_index (self->pending_items, index);
  }
}

static inline void
_start_discovery (ClapperDiscoverer *self)
{
  if (!self->running) {
    gst_discoverer_start (self->discoverer);
    self->running = TRUE;
    GST_INFO_OBJECT (self, "Discoverer started");
  }
}

static inline void
_stop_discovery (ClapperDiscoverer *self)
{
  if (self->running) {
    gst_discoverer_stop (self->discoverer);
    self->running = FALSE;
    GST_INFO_OBJECT (self, "Discoverer stopped");
  }
}

static void
_run_discovery (ClapperDiscoverer *self)
{
  ClapperMediaItem *item;
  ClapperQueue *queue;
  ClapperDiscovererDiscoveryMode discovery_mode;
  const gchar *uri;
  gboolean success = FALSE;

  if (self->pending_items->len == 0) {
    GST_DEBUG_OBJECT (self, "No more pending items");
    return;
  }

  item = g_ptr_array_steal_index (self->pending_items, 0);

  GST_DEBUG_OBJECT (self, "Investigating discovery of %" GST_PTR_FORMAT, item);

  queue = CLAPPER_QUEUE_CAST (gst_object_get_parent (GST_OBJECT_CAST (item)));

  if (G_UNLIKELY (queue == NULL)) {
    GST_DEBUG_OBJECT (self, "Queued item %" GST_PTR_FORMAT
        " does not appear to be in queue anymore", item);
    goto finish;
  }

  discovery_mode = clapper_discoverer_get_discovery_mode (self);

  if (discovery_mode == CLAPPER_DISCOVERER_DISCOVERY_NONCURRENT
      && clapper_queue_item_is_current (queue, item)) {
    GST_DEBUG_OBJECT (self, "Queued %" GST_PTR_FORMAT
        " is current item, ignoring discovery", item);
    goto finish;
  }

  uri = clapper_media_item_get_uri (item);
  GST_DEBUG_OBJECT (self, "Starting discovery of %"
      GST_PTR_FORMAT "(%s)", item, uri);

  /* Need to start first, then append URI */
  _start_discovery (self);

  if ((success = gst_discoverer_discover_uri_async (self->discoverer, uri))) {
    gst_object_replace ((GstObject **) &self->discovered_item, GST_OBJECT_CAST (item));
    GST_DEBUG_OBJECT (self, "Running discovery of %"
        GST_PTR_FORMAT "(%s)", self->discovered_item, uri);
  } else {
    GST_ERROR_OBJECT (self, "Could not run discovery of %"
        GST_PTR_FORMAT "(%s)", item, uri);
  }

finish:
  gst_clear_object (&item);
  gst_clear_object (&queue);

  /* Continue until we run out of pending items */
  if (!success)
    _run_discovery (self);
}

static gboolean
_run_discovery_delayed_cb (ClapperDiscoverer *self)
{
  GST_DEBUG_OBJECT (self, "Delayed discovery handler reached");

  _clear_timeout_source (self);
  _run_discovery (self);

  return G_SOURCE_REMOVE;
}

static void
_discovered_cb (GstDiscoverer *discoverer G_GNUC_UNUSED,
    GstDiscovererInfo *info, GError *error, ClapperDiscoverer *self)
{
  /* Can be NULL if removed while discovery of it was running */
  if (self->discovered_item) {
    const gchar *uri = clapper_media_item_get_uri (self->discovered_item);

    if (G_LIKELY (error == NULL)) {
      GST_DEBUG_OBJECT (self, "Finished discovery of %"
          GST_PTR_FORMAT "(%s)", self->discovered_item, uri);
      clapper_media_item_update_from_discoverer_info (self->discovered_item, info);
    } else {
      GST_ERROR_OBJECT (self, "Discovery of %" GST_PTR_FORMAT
          "(%s) failed, reason: %s", self->discovered_item, uri, error->message);
    }

    /* Clear so its NULL when replaced later */
    gst_clear_object (&self->discovered_item);
  }

  /* Try to discover next item */
  _run_discovery (self);
}

static void
_finished_cb (GstDiscoverer *discoverer G_GNUC_UNUSED, ClapperDiscoverer *self)
{
  if (G_LIKELY (self->pending_items->len == 0)) {
    GST_DEBUG_OBJECT (self, "Finished discovery of all items");
  } else {
    /* This should never happen, but if it does, then clear
     * pending items array so we can somewhat recover */
    GST_ERROR_OBJECT (self, "Discovery stopped, but still had %u pending items!",
        self->pending_items->len);
    g_ptr_array_remove_range (self->pending_items, 0, self->pending_items->len);
  }

  _stop_discovery (self);
}

static void
clapper_discoverer_played_item_changed (ClapperFeature *feature, ClapperMediaItem *item)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Played item changed to: %" GST_PTR_FORMAT, item);
  _unqueue_discovery (self, item);
}

static void
clapper_discoverer_queue_item_added (ClapperFeature *feature, ClapperMediaItem *item, guint index)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item added %" GST_PTR_FORMAT, item);

  g_ptr_array_add (self->pending_items, gst_object_ref (item));

  /* Already running, nothing more to do */
  if (self->running)
    return;

  /* Need to always clear timeout here, as mode may
   * have changed between adding multiple items */
  _clear_timeout_source (self);

  switch (clapper_discoverer_get_discovery_mode (self)) {
    case CLAPPER_DISCOVERER_DISCOVERY_NONCURRENT:
      /* We start running after small delay in this mode, so
       * application can select item after adding it to queue first */
      self->timeout_source = clapper_shared_utils_context_timeout_add_full (
          g_main_context_get_thread_default (),
          G_PRIORITY_DEFAULT_IDLE, 50,
          (GSourceFunc) _run_discovery_delayed_cb,
          self, NULL);
      break;
    case CLAPPER_DISCOVERER_DISCOVERY_ALWAYS:
      _run_discovery (self);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
clapper_discoverer_queue_item_removed (ClapperFeature *feature, ClapperMediaItem *item, guint index)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item removed %" GST_PTR_FORMAT, item);
  _unqueue_discovery (self, item);
}

static void
clapper_discoverer_queue_cleared (ClapperFeature *feature)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Discarding discovery of all pending items");

  if (self->pending_items->len > 0)
    g_ptr_array_remove_range (self->pending_items, 0, self->pending_items->len);

  gst_clear_object (&self->discovered_item);

  _stop_discovery (self);
}

static gboolean
clapper_discoverer_prepare (ClapperFeature *feature)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (feature);
  GError *error = NULL;

  GST_DEBUG_OBJECT (self, "Prepare");

  self->discoverer = gst_discoverer_new (15 * GST_SECOND, &error);

  if (G_UNLIKELY (error != NULL)) {
    GST_ERROR_OBJECT (self, "Could not prepare, reason: %s", error->message);
    g_error_free (error);

    return FALSE;
  }

  GST_TRACE_OBJECT (self, "Created new GstDiscoverer: %" GST_PTR_FORMAT, self->discoverer);

  /* FIXME: Caching in GStreamer is broken. Does not save container tags, such as media title.
   * Disable it until completely fixed upsteam. Once fixed change to %TRUE. */
  g_object_set (self->discoverer, "use-cache", FALSE, NULL);

  g_signal_connect (self->discoverer, "discovered",
      G_CALLBACK (_discovered_cb), self);
  g_signal_connect (self->discoverer, "finished",
      G_CALLBACK (_finished_cb), self);

  return TRUE;
}

static gboolean
clapper_discoverer_unprepare (ClapperFeature *feature)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Unprepare");

  _clear_timeout_source (self);

  /* Do what we also do when queue is cleared */
  clapper_discoverer_queue_cleared (feature);

  gst_clear_object (&self->discoverer);

  return TRUE;
}

/**
 * clapper_discoverer_new:
 *
 * Creates a new #ClapperDiscoverer instance.
 *
 * Returns: (transfer full): a new #ClapperDiscoverer instance.
 */
ClapperDiscoverer *
clapper_discoverer_new (void)
{
  ClapperDiscoverer *discoverer = g_object_new (CLAPPER_TYPE_DISCOVERER, NULL);
  gst_object_ref_sink (discoverer);

  return discoverer;
}

/**
 * clapper_discoverer_set_discovery_mode:
 * @discoverer: a #ClapperDiscoverer
 * @mode: a #ClapperDiscovererDiscoveryMode
 *
 * Set the [enum@Clapper.DiscovererDiscoveryMode] of @discoverer.
 */
void
clapper_discoverer_set_discovery_mode (ClapperDiscoverer *self, ClapperDiscovererDiscoveryMode mode)
{
  gboolean changed;

  g_return_if_fail (CLAPPER_IS_DISCOVERER (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->discovery_mode != mode))
    self->discovery_mode = mode;
  GST_OBJECT_UNLOCK (self);

  if (changed)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_DISCOVERY_MODE]);
}

/**
 * clapper_discoverer_get_discovery_mode:
 * @discoverer: a #ClapperDiscoverer
 *
 * Get the [enum@Clapper.DiscovererDiscoveryMode] of @discoverer.
 *
 * Returns: a currently set #ClapperDiscovererDiscoveryMode.
 */
ClapperDiscovererDiscoveryMode
clapper_discoverer_get_discovery_mode (ClapperDiscoverer *self)
{
  ClapperDiscovererDiscoveryMode mode;

  g_return_val_if_fail (CLAPPER_IS_DISCOVERER (self), DEFAULT_DISCOVERY_MODE);

  GST_OBJECT_LOCK (self);
  mode = self->discovery_mode;
  GST_OBJECT_UNLOCK (self);

  return mode;
}

static void
clapper_discoverer_init (ClapperDiscoverer *self)
{
  self->pending_items = g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);

  self->discovery_mode = DEFAULT_DISCOVERY_MODE;
}

static void
clapper_discoverer_finalize (GObject *object)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (object);

  g_ptr_array_unref (self->pending_items);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_discoverer_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (object);

  switch (prop_id) {
    case PROP_DISCOVERY_MODE:
      clapper_discoverer_set_discovery_mode (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_discoverer_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (object);

  switch (prop_id) {
    case PROP_DISCOVERY_MODE:
      g_value_set_enum (value, clapper_discoverer_get_discovery_mode (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_discoverer_class_init (ClapperDiscovererClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperFeatureClass *feature_class = (ClapperFeatureClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperdiscoverer", 0,
      "Clapper Discoverer");

  gobject_class->get_property = clapper_discoverer_get_property;
  gobject_class->set_property = clapper_discoverer_set_property;
  gobject_class->finalize = clapper_discoverer_finalize;

  feature_class->prepare = clapper_discoverer_prepare;
  feature_class->unprepare = clapper_discoverer_unprepare;
  feature_class->played_item_changed = clapper_discoverer_played_item_changed;
  feature_class->queue_item_added = clapper_discoverer_queue_item_added;
  feature_class->queue_item_removed = clapper_discoverer_queue_item_removed;
  feature_class->queue_cleared = clapper_discoverer_queue_cleared;

  /**
   * ClapperDiscoverer:discovery-mode:
   *
   * Discoverer discovery mode.
   */
  param_specs[PROP_DISCOVERY_MODE] = g_param_spec_enum ("discovery-mode",
      NULL, NULL, CLAPPER_TYPE_DISCOVERER_DISCOVERY_MODE, DEFAULT_DISCOVERY_MODE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
