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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>

#include "clapper-enhancer-director-private.h"
#include "../clapper-enhancers-loader-private.h"
#include "../clapper-extractable-private.h"
#include "../clapper-harvest-private.h"
#include "../../shared/clapper-shared-utils-private.h"

#define GST_CAT_DEFAULT clapper_enhancer_director_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperEnhancerDirector
{
  ClapperThreadedObject parent;
};

#define parent_class clapper_enhancer_director_parent_class
G_DEFINE_TYPE (ClapperEnhancerDirector, clapper_enhancer_director, CLAPPER_TYPE_THREADED_OBJECT);

typedef struct
{
  GUri *uri;
  GCancellable *cancellable;
  GError **error;
} ClapperEnhancerDirectorData;

static gpointer
clapper_enhancer_director_extract_in_thread (ClapperEnhancerDirectorData *data)
{
  ClapperExtractable *extractable = NULL;
  ClapperHarvest *harvest = clapper_harvest_new ();
  gboolean success = FALSE, cached = FALSE;

  /* Cancelled during thread switching */
  if (g_cancellable_is_cancelled (data->cancellable))
    goto finish;

  /* TODO: Cache lookup */
  if (cached) {
    // success = fill harvest from cache
    goto finish;
  }

  extractable = CLAPPER_EXTRACTABLE_CAST (clapper_enhancers_loader_create_enhancer_for_uri (
      CLAPPER_TYPE_EXTRACTABLE, data->uri));

  /* Check just before extract */
  if (g_cancellable_is_cancelled (data->cancellable))
    goto finish;

  success = clapper_extractable_extract (extractable, data->uri,
      harvest, data->cancellable, data->error);

  /* Cancelled during extract */
  if (g_cancellable_is_cancelled (data->cancellable)) {
    success = FALSE;
    goto finish;
  }

finish:
  if (success) {
    if (!cached) {
      /* TODO: Store in cache */
    }
  } else {
    gst_clear_object (&harvest);

    /* Ensure we have some error set on failure */
    if (*data->error == NULL) {
      const gchar *err_msg = (g_cancellable_is_cancelled (data->cancellable))
          ? "Extraction was cancelled"
          : "Extraction failed";
      g_set_error (data->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_FAILED, "%s", err_msg);
    }
  }

  gst_clear_object (&extractable);

  return harvest;
}

/*
 * clapper_enhancer_director_new:
 *
 * Returns: (transfer full): a new #ClapperEnhancerDirector instance.
 */
ClapperEnhancerDirector *
clapper_enhancer_director_new (void)
{
  ClapperEnhancerDirector *director;

  director = g_object_new (CLAPPER_TYPE_ENHANCER_DIRECTOR, NULL);
  gst_object_ref_sink (director);

  return director;
}

ClapperHarvest *
clapper_enhancer_director_extract (ClapperEnhancerDirector *self, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  ClapperEnhancerDirectorData *data = g_new (ClapperEnhancerDirectorData, 1);

  data->uri = uri;
  data->cancellable = cancellable;
  data->error = error;

  return CLAPPER_HARVEST_CAST (clapper_shared_utils_context_invoke_sync_full (
      clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self)),
      (GThreadFunc) clapper_enhancer_director_extract_in_thread,
      data, (GDestroyNotify) g_free));
}

static void
clapper_enhancer_director_thread_start (ClapperThreadedObject *threaded_object)
{
  GST_TRACE_OBJECT (threaded_object, "Enhancer director thread start");
}

static void
clapper_enhancer_director_thread_stop (ClapperThreadedObject *threaded_object)
{
  GST_TRACE_OBJECT (threaded_object, "Enhancer director thread stop");
}

static void
clapper_enhancer_director_init (ClapperEnhancerDirector *self)
{
}

static void
clapper_enhancer_director_finalize (GObject *object)
{
  GST_TRACE_OBJECT (object, "Finalize");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_enhancer_director_class_init (ClapperEnhancerDirectorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancerdirector", 0,
      "Clapper Enhancer Director");

  gobject_class->finalize = clapper_enhancer_director_finalize;

  threaded_object->thread_start = clapper_enhancer_director_thread_start;
  threaded_object->thread_stop = clapper_enhancer_director_thread_stop;
}
