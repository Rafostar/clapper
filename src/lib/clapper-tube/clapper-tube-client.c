/* Clapper Tube Library
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

#include "clapper-tube-enums.h"
#include "clapper-tube-client-private.h"
#include "clapper-tube-extractor.h"
#include "clapper-tube-harvest-private.h"
#include "clapper-tube-loader-private.h"
#include "../shared/clapper-shared-utils-private.h"

#define GST_CAT_DEFAULT clapper_tube_client_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperTubeClient
{
  ClapperThreadedObject parent;

  ClapperTubeExtractor *extractor;
};

#define parent_class clapper_tube_client_parent_class
G_DEFINE_TYPE (ClapperTubeClient, clapper_tube_client, CLAPPER_TYPE_THREADED_OBJECT);

typedef struct
{
  ClapperTubeClient *client;
  GUri *uri;
  GCancellable *cancellable;
  GError **error;
} ClapperTubeClientData;

static gpointer
clapper_tube_client_run_in_thread (ClapperTubeClientData *data)
{
  ClapperTubeClient *self = data->client;
  ClapperTubeHarvest *harvest = clapper_tube_harvest_new ();
  GUri *current_uri = g_uri_ref (data->uri);
  ClapperTubeFlow flow = CLAPPER_TUBE_FLOW_ERROR;

beginning:
  /* Remove held extractor if any */
  gst_clear_object (&self->extractor);

  /* Cancelled during thread switching or when URI changed */
  if (g_cancellable_is_cancelled (data->cancellable)) {
    flow = CLAPPER_TUBE_FLOW_ERROR;
    goto finish;
  }

  /* Get a hold of new extractor */
  self->extractor = clapper_tube_loader_get_extractor_for_uri (current_uri);

  if (!self->extractor) {
    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_ERROR) {
      gchar *uri_str = g_uri_to_string (current_uri);

      GST_ERROR_OBJECT (self, "No plugin for URI: %s", uri_str);
      g_set_error (data->error, CLAPPER_TUBE_EXTRACTOR_ERROR,
          CLAPPER_TUBE_EXTRACTOR_ERROR_OTHER,
          "None of the installed plugins could handle URI: %s", uri_str);

      g_free (uri_str);
    }

    flow = CLAPPER_TUBE_FLOW_ERROR;
    goto finish;
  }

  clapper_tube_extractor_set_uri (self->extractor, current_uri);

run_extract:
  if (!g_cancellable_is_cancelled (data->cancellable)) {
    ClapperTubeExtractorClass *extractor_class;

    extractor_class = CLAPPER_TUBE_EXTRACTOR_GET_CLASS (self->extractor);
    flow = extractor_class->extract (self->extractor, harvest, data->cancellable, data->error);
  }

  /* Extraction failed */
  if (flow == CLAPPER_TUBE_FLOW_ERROR)
    goto finish;

  /* Ignore error if no %CLAPPER_TUBE_FLOW_ERROR.
   * Assume that extractor handled situation somehow. */
  if (*data->error != NULL)
    g_clear_error (data->error);

  /* Cancelled */
  if (g_cancellable_is_cancelled (data->cancellable)) {
    flow = CLAPPER_TUBE_FLOW_ERROR;
    goto finish;
  }

  if (flow == CLAPPER_TUBE_FLOW_RESTART) {
    GUri *pending_uri = clapper_tube_extractor_get_uri (self->extractor);

    /* Request to change URI and run again */
    if (pending_uri != current_uri) {
      g_uri_unref (current_uri);
      current_uri = g_uri_ref (pending_uri);

      goto beginning;
    }

    goto run_extract;
  }

finish:
  if (flow == CLAPPER_TUBE_FLOW_ERROR) {
    gst_clear_object (&harvest);
    gst_clear_object (&self->extractor);

    /* Ensure we have some error set on failure */
    if (*data->error == NULL) {
      if (g_cancellable_is_cancelled (data->cancellable)) {
        g_set_error (data->error, CLAPPER_TUBE_EXTRACTOR_ERROR,
            CLAPPER_TUBE_EXTRACTOR_ERROR_OTHER,
            "Extraction was cancelled");
      } else {
        g_set_error (data->error, CLAPPER_TUBE_EXTRACTOR_ERROR,
            CLAPPER_TUBE_EXTRACTOR_ERROR_FAILED,
            "Extraction failed");
      }
    }
  } else if (G_LIKELY (self->extractor != NULL)) {
    /* TODO: Connect "*-updated" signals */
  }

  g_uri_unref (current_uri);

  return harvest;
}

ClapperTubeHarvest *
clapper_tube_client_run (ClapperTubeClient *self, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  ClapperTubeClientData *data = g_new (ClapperTubeClientData, 1);

  data->client = self;
  data->uri = uri;
  data->cancellable = cancellable;
  data->error = error;

  return CLAPPER_TUBE_HARVEST_CAST (clapper_shared_utils_context_invoke_sync_full (
      clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self)),
      (GThreadFunc) clapper_tube_client_run_in_thread,
      data, (GDestroyNotify) g_free));
}

static gpointer
clapper_tube_client_stop_in_thread (ClapperTubeClient *self)
{
  if (self->extractor) {
    /* TODO: Disconnect "*-updated" signals */

    gst_clear_object (&self->extractor);
  }

  return NULL;
}

void
clapper_tube_client_stop (ClapperTubeClient *self)
{
  clapper_shared_utils_context_invoke_sync (
      clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self)),
      (GThreadFunc) clapper_tube_client_stop_in_thread, self);
}

static void
clapper_tube_client_thread_stop (ClapperThreadedObject *threaded_object)
{
  ClapperTubeClient *self = CLAPPER_TUBE_CLIENT_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Client thread stop");

  gst_clear_object (&self->extractor);
}

static void
clapper_tube_client_init (ClapperTubeClient *self)
{
}

static void
clapper_tube_client_finalize (GObject *object)
{
  GST_TRACE_OBJECT (object, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_tube_client_class_init (ClapperTubeClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubeclient", 0,
      "Clapper Tube Client");

  gobject_class->finalize = clapper_tube_client_finalize;

  threaded_object->thread_stop = clapper_tube_client_thread_stop;
}
