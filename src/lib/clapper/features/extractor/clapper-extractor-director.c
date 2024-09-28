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

#include "clapper-extractor-director-private.h"
#include "clapper-extractable-private.h"
#include "clapper-media-item-private.h"

#define GST_CAT_DEFAULT clapper_extractor_director_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperExtractorDirector
{
  ClapperThreadedObject parent;

  GQueue *queue;
  GPtrArray *extractables;

  gboolean running;
};

#define parent_class clapper_extractor_director_parent_class
G_DEFINE_TYPE (ClapperExtractorDirector, clapper_extractor_director, CLAPPER_TYPE_THREADED_OBJECT);

typedef struct
{
  ClapperExtractorDirector *director;
  GUri *uri;
  GCancellable *cancellable;
  GError **error;
} ClapperExtractorDirectorData;

static gboolean
_extract_invoke_func (ClapperExtractorDirector *self)
{
  ClapperExtractable *extractable = NULL;
  ClapperMediaItem *item;
  ClapperHarvest *harvest = clapper_harvest_new ();
  GUri *current_uri;
  const PeasPluginInfo *info;
  gboolean success = FALSE;

  GST_OBJECT_LOCK (self);
  item = CLAPPER_MEDIA_ITEM_CAST (g_queue_pop_tail (self->queue));
  GST_OBJECT_UNLOCK (self);

  current_uri = g_uri_parse (clapper_media_item_get_uri (item),
      G_URI_FLAGS_ENCODED, NULL);

beginning:
  /* Remove held extractor if any */
  gst_clear_object (&extractable);

  /* Cancelled during thread switching or when URI changed */
  if (g_cancellable_is_cancelled (self->cancellable))
    goto finish;

  /* Get a hold of new extractor */
  if ((info = clapper_addons_loader_get_info (uri, CLAPPER_TYPE_EXTRACTOR))) {
    extractable = CLAPPER_EXTRACTABLE_CAST (
        clapper_addons_loader_create_extension (info, CLAPPER_TYPE_EXTRACTOR));
  }

  if (!extractable) {
    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_ERROR) {
      gchar *uri_str = g_uri_to_string (current_uri);

      GST_ERROR_OBJECT (self, "No plugin for URI: %s", uri_str);
      g_set_error (data->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_NOT_FOUND,
          "None of the installed addons could handle URI: %s", uri_str);
      g_free (uri_str);
    }

    goto finish;
  }

  if (!clapper_extractor_query (extractor, current_uri)) {
    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_INFO) {
      gchar *uri_str = g_uri_to_string (current_uri);

      GST_INFO_OBJECT (self, "Extractor %" GST_PTR_FORMAT, " refused URI: \"%s\"",
          extractor, current_uri);
      g_free (uri_str);
    }

    goto finish;
  }

  if (!g_cancellable_is_cancelled (data->cancellable))
    success = clapper_extractor_extract (extractor, harvest, data->cancellable, data->error);

  /* Cancelled */
  if (g_cancellable_is_cancelled (data->cancellable)) {
    success = FALSE;
    goto finish;
  }

/* FIXME: Handle URI change request
  if (flow == CLAPPER_FLOW_RESTART) {
    GUri *pending_uri = _get_uri (extractor);

    // Request to change URI and run again
    if (pending_uri != current_uri) {
      g_uri_unref (current_uri);
      current_uri = g_uri_ref (pending_uri);

      goto beginning;
    }

    goto run_extract;
  }
*/
finish:
  if (success) {
    guint interval = clapper_extractor_get_update_interval (extractor);

    if (interval > 0) {
      /* FIXME: Start timeout */
      g_ptr_array_add (self->extractables, gst_object_ref (extractor));
    }
  } else {
    gst_clear_object (&harvest);

    /* Ensure we have some error set on failure */
    if (*data->error == NULL) {
      const gchar *err_msg = (g_cancellable_is_cancelled (data->cancellable))
          ? "Extraction was cancelled"
          : "Extraction failed";
      g_set_error (data->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_FAILED, err_msg);
    }
  }

  g_uri_unref (current_uri);
  gst_clear_object (&extractor);

  GST_OBJECT_LOCK (self);
  self->running = FALSE;
  GST_OBJECT_UNLOCK (self);

  return G_SOURCE_REMOVE;
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
  ClapperExtractorDirectorData *data;
  GMainContext *context;

  data = g_new (ClapperExtractorDirectorData, 1);
  data->director = self;
  data->uri = gst_object_ref (item);
  //data->cancellable = cancellable;
  //data->error = error;

  context = clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self));

  GST_OBJECT_LOCK (self);

  g_queue_push_tail (self->queue, gst_object_ref (item));

  if (!self->running) {
    g_main_context_invoke (context, (GSourceFunc) _extract_invoke_func, self);
    self->running = TRUE;
  }

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
}

static void
clapper_extractor_director_finalize (GObject *object)
{
  g_queue_free_full (self->queue, (GDestroyNotify) g_uri_unref);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_extractor_director_class_init (ClapperExtractorDirectorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperextractordirector", 0,
      "Clapper Extractor Director");

  gobject_class->finalize = clapper_extractor_finalize;

  threaded_object->thread_start = clapper_extractor_director_thread_start;
  threaded_object->thread_stop = clapper_extractor_director_thread_stop;
}
