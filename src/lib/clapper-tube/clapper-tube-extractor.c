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

#include "clapper-tube-extractor.h"
#include "clapper-tube-harvest-private.h"
#include "../shared/clapper-shared-utils-private.h"

#define GST_CAT_DEFAULT clapper_tube_extractor_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperTubeExtractorPrivate
{
  GUri *uri;
};

#define parent_class clapper_tube_extractor_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperTubeExtractor, clapper_tube_extractor, CLAPPER_TYPE_THREADED_OBJECT,
    G_ADD_PRIVATE (ClapperTubeExtractor));

typedef struct
{
  ClapperTubeExtractor *extractor;
  GUri *uri;
  GCancellable *cancellable;
  GError **error;
} ClapperTubeExtractorData;

/**
 * clapper_tube_extractor_error_quark:
 *
 * An error quark for [class@ClapperTube.Extractor] errors.
 *
 * Returns: the error quark.
 */
G_DEFINE_QUARK (clapper-tube-extractor-error-quark, clapper_tube_extractor_error);

/**
 * clapper_tube_extractor_get_uri:
 * @website: a #ClapperTubeExtractor
 *
 * Returns: (transfer none): current requested URI.
 */
GUri *
clapper_tube_extractor_get_uri (ClapperTubeExtractor *self)
{
  ClapperTubeExtractorPrivate *priv;

  g_return_val_if_fail (CLAPPER_TUBE_IS_EXTRACTOR (self), NULL);

  priv = clapper_tube_extractor_get_instance_private (self);

  return priv->uri;
}

/**
 * clapper_tube_extractor_set_uri:
 * @website: a #ClapperTubeExtractor
 * @uri: a media source URI
 *
 * Sets new URI for handling. URI cannot be %NULL.
 *
 * After setting new URI, [vfunc@ClapperTube.Extractor::extract] should
 * return %FALSE which will trigger another extractor query for updated URI.
 */
void
clapper_tube_extractor_set_uri (ClapperTubeExtractor *self, GUri *uri)
{
  ClapperTubeExtractorPrivate *priv;

  g_return_if_fail (CLAPPER_TUBE_IS_EXTRACTOR (self));
  g_return_if_fail (uri != NULL);

  priv = clapper_tube_extractor_get_instance_private (self);

  if (priv->uri)
    g_uri_unref (priv->uri);

  priv->uri = g_uri_ref (uri);
}

static gpointer
clapper_tube_extractor_run_in_thread (ClapperTubeExtractorData *data)
{
  ClapperTubeExtractorClass *extractor_class = CLAPPER_TUBE_EXTRACTOR_GET_CLASS (data->extractor);
  ClapperTubeHarvest *harvest = NULL;

  /* Cancelled during thread switching */
  if (g_cancellable_is_cancelled (data->cancellable))
    return NULL;

  harvest = clapper_tube_harvest_new ();

  clapper_tube_extractor_set_uri (data->extractor, data->uri);
  extractor_class->extract (data->extractor, harvest, data->cancellable, data->error);

  /* Extraction error or cancelled in middle of it */
  if (*data->error != NULL || g_cancellable_is_cancelled (data->cancellable))
    gst_clear_object (&harvest);

  return harvest;
}

ClapperTubeHarvest *
clapper_tube_extractor_run (ClapperTubeExtractor *self, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  ClapperTubeExtractorData *data = g_new (ClapperTubeExtractorData, 1);

  data->extractor = self;
  data->uri = uri;
  data->cancellable = cancellable;
  data->error = error;

  return CLAPPER_TUBE_HARVEST_CAST (clapper_shared_utils_context_invoke_sync_full (
      clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self)),
      (GThreadFunc) clapper_tube_extractor_run,
      data, (GDestroyNotify) g_free));
}

static void
clapper_tube_extractor_init (ClapperTubeExtractor *self)
{
}

static gboolean
clapper_tube_extractor_extract (ClapperTubeExtractor *self, ClapperTubeHarvest *harvest,
    GCancellable *cancellable, GError **error)
{
  GST_ERROR_OBJECT (self, "Extraction is not implemented!");

  return CLAPPER_TUBE_EXTRACTION_FAILED;
}

static void
clapper_tube_extractor_finalize (GObject *object)
{
  ClapperTubeExtractor *self = CLAPPER_TUBE_EXTRACTOR_CAST (object);
  ClapperTubeExtractorPrivate *priv = clapper_tube_extractor_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Finalize");

  if (priv->uri)
    g_uri_unref (priv->uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_tube_extractor_class_init (ClapperTubeExtractorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperTubeExtractorClass *extractor_class = (ClapperTubeExtractorClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubeextractor", 0,
      "Clapper Tube Extractor");

  gobject_class->finalize = clapper_tube_extractor_finalize;

  extractor_class->extract = clapper_tube_extractor_extract;
}
