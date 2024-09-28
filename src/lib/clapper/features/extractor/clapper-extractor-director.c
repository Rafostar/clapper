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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>

#include "clapper-extractor-director-private.h"
#include "clapper-extractable-private.h"
#include "clapper-media-item-private.h"
#include "clapper-harvest-private.h"
#include "../shared/clapper-addons-loader-private.h"

#define GST_CAT_DEFAULT clapper_extractor_director_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperExtractorDirector
{
  ClapperThreadedObject parent;

  GQueue *queue;
  GPtrArray *results;

  gboolean running;

  GCancellable *cancellable;
};

#define parent_class clapper_extractor_director_parent_class
G_DEFINE_TYPE (ClapperExtractorDirector, clapper_extractor_director, CLAPPER_TYPE_THREADED_OBJECT);

typedef struct
{
  ClapperMediaItem *item;
  ClapperHarvest *harvest;
  GError *error;
  gboolean cancelled;
} ClapperExtractionResult;

static void
clapper_extraction_result_free (ClapperExtractionResult *result)
{
  gst_object_unref (result->item);
  gst_clear_object (&result->harvest);
  g_clear_error (&result->error);

  g_free (result);
}

static gboolean
_extract_invoke_func (ClapperExtractorDirector *self)
{
  ClapperExtractable *extractable = NULL;
  ClapperMediaItem *item;
  ClapperExtractionResult *result;
  GUri *current_uri = NULL;
  GCancellable *cancellable;
  gboolean run_again = FALSE, success = FALSE;

beginning:
  GST_OBJECT_LOCK (self);
  cancellable = g_object_ref (self->cancellable);
  item = CLAPPER_MEDIA_ITEM_CAST (g_queue_pop_tail (self->queue));
  GST_OBJECT_UNLOCK (self);

  /* Queue is empty (item(s) removed from feature thread) */
  if (!item)
    goto finish;

  result = g_new (ClapperExtractionResult, 1);
  result->item = item;
  result->harvest = clapper_harvest_new ();
  result->error = NULL;
  result->cancelled = FALSE;

  current_uri = g_uri_parse (clapper_media_item_get_uri (result->item),
      G_URI_FLAGS_ENCODED, NULL);

start_extract:
  /* Remove held extractable if any */
  gst_clear_object (&extractable);

  /* Cancelled during thread switching or when URI changed */
  if (g_cancellable_is_cancelled (cancellable))
    goto stop_extract;

  extractable = CLAPPER_EXTRACTABLE_CAST (clapper_addons_loader_create_extension_for_uri (
      current_uri, CLAPPER_TYPE_EXTRACTABLE));

  /* It can be %NULL if extractable redirects to a different (unsupported) URI */
  if (!extractable) {
    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_ERROR) {
      gchar *uri_str = g_uri_to_string (current_uri);

      GST_ERROR_OBJECT (self, "No addon for URI: %s", uri_str);
      g_set_error (&result->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_NOT_FOUND,
          "None of the installed addons could handle URI: %s", uri_str);
      g_free (uri_str);
    }

    goto stop_extract;
  }

  success = clapper_extractable_extract (extractable, current_uri,
      result->harvest, cancellable, &result->error,);

  /* Cancelled */
  if (g_cancellable_is_cancelled (cancellable)) {
    success = FALSE;
    goto stop_extract;
  }

/* FIXME: Handle URI change request
  if (uri_changed) {
    g_uri_unref (current_uri);
    current_uri = _get_uri (extractor);

    goto start_extract;
  }
*/
stop_extract:
  if (success) {
/*
    guint interval = clapper_extractor_get_update_interval (extractor);

    if (interval > 0) {
      // FIXME: Start timeout
      g_ptr_array_add (self->extractables, gst_object_ref (extractor));
    }
*/
  } else {
    gst_clear_object (&result->harvest);
    result->cancelled = g_cancellable_is_cancelled (cancellable);

    /* Ensure we have some error set on failure */
    if (!error) {
      const gchar *err_msg = (result->cancelled)
          ? "Extraction was cancelled"
          : "Extraction failed";
      g_set_error (&result->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_FAILED, "%s", err_msg);
    }
  }

  g_ptr_array_add (self->results, result);

finish:
  if (current_uri)
    g_uri_unref (current_uri);

  g_object_unref (cancellable);
  gst_clear_object (&extractable);

  GST_OBJECT_LOCK (self);
  if (!(run_again = !g_queue_is_empty (self->queue)))
    self->running = FALSE;
  GST_OBJECT_UNLOCK (self);

  if (run_again)
    goto beginning;

  return G_SOURCE_REMOVE;
}

static
_start_extracting_unlocked (ClapperExtractorDirector *self)
{
  /* Ensure fresh cancellable and push item
   * to queue within single mutex locking */
  if (g_cancellable_is_cancelled (self->cancellable)) {
    g_object_unref (self->cancellable);
    self->cancellable = g_cancellable_new ();
  }

  /* FIXME: Re-enqueue all previously cancelled extractions */

  if (!self->running) {
    g_main_context_invoke (context, (GSourceFunc) _extract_invoke_func, self);
    self->running = TRUE;
  }
}

/*
 * clapper_extractor_director_new:
 *
 * Returns: (transfer full): a new #ClapperExtractorDirector instance.
 */
ClapperExtractorDirector *
clapper_extractor_director_new (void)
{
  ClapperExtractorDirector *director;

  director = g_object_new (CLAPPER_TYPE_EXTRACTOR_DIRECTOR, NULL);
  gst_object_ref_sink (director);

  return director;
}

void
clapper_extractor_director_enqueue (ClapperExtractorDirector *self, ClapperMediaItem *item)
{
  GMainContext *context;

  context = clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self));

  GST_OBJECT_LOCK (self);

  g_queue_push_tail (self->queue, gst_object_ref (item));
  _start_extracting_unlocked (self);

  GST_OBJECT_UNLOCK (self);
}

void
clapper_extractor_director_dequeue (ClapperExtractorDirector *self, ClapperMediaItem *item)
{
  GST_OBJECT_LOCK (self);

  /* Remove and unref, since queue does not do that */
  if (g_queue_remove (self->queue, item))
    gst_object_unref (item);
  //else if on finished array find and remove

  GST_OBJECT_UNLOCK (self);
}

void
clapper_extractor_director_dequeue_all (ClapperExtractorDirector *self)
{
  /* Cancel ongoing extraction if any, then clear queue */
  clapper_extractor_director_cancel (self->director);

  GST_OBJECT_LOCK (self);
  g_queue_clear_full (self->queue, (GDestroyNotify) gst_object_unref);
  GST_OBJECT_UNLOCK (self);
}

void
clapper_extractor_director_prioritize_item (ClapperExtractorDirector *self, ClapperMediaItem *item)
{
  GST_OBJECT_LOCK (self);

  /* FIXME: Find if item is queued, then move it to the top of queue */

  _start_extracting_unlocked (self);

  GST_OBJECT_UNLOCK (self);
}

void
clapper_extractor_director_cancel (ClapperExtractorDirector *self)
{
  g_cancellable_cancel (self->cancellable);
}

static void
clapper_extractor_director_thread_start (ClapperThreadedObject *threaded_object)
{
  ClapperExtractorDirector *self = CLAPPER_EXTRACTOR_DIRECTOR_CAST (threaded_object);

  GST_TRACE_OBJECT (self, "Extractor director thread start");
}

static void
clapper_extractor_director_thread_stop (ClapperThreadedObject *threaded_object)
{
  ClapperExtractorDirector *self = CLAPPER_EXTRACTOR_DIRECTOR_CAST (threaded_object);

  GST_TRACE_OBJECT (self, "Extractor director thread stop");
}

static void
clapper_extractor_director_init (ClapperExtractorDirector *self)
{
  self->queue = g_queue_new ();

  self->cancellable = g_cancellable_new ();
}

static void
clapper_extractor_director_finalize (GObject *object)
{
  ClapperExtractorDirector *self = CLAPPER_EXTRACTOR_DIRECTOR_CAST (object);

  g_queue_free_full (self->queue, (GDestroyNotify) gst_object_unref);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_extractor_director_class_init (ClapperExtractorDirectorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperextractordirector", 0,
      "Clapper Extractor Director");

  gobject_class->finalize = clapper_extractor_director_finalize;

  threaded_object->thread_start = clapper_extractor_director_thread_start;
  threaded_object->thread_stop = clapper_extractor_director_thread_stop;
}
