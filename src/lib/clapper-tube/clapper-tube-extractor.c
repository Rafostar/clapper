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

/**
 * ClapperTubeExtractor:
 *
 * A base class for creating extractor modules.
 */

#include "clapper-tube-extractor.h"
#include "clapper-tube-harvest-private.h"
#include "../shared/clapper-shared-utils-private.h"

#define GST_CAT_DEFAULT clapper_tube_extractor_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperTubeExtractorPrivate ClapperTubeExtractorPrivate;

struct _ClapperTubeExtractorPrivate
{
  GUri *uri;
  ClapperTubeHarvest *harvest;
};

#define parent_class clapper_tube_extractor_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperTubeExtractor, clapper_tube_extractor, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (ClapperTubeExtractor));

enum
{
  SIGNAL_TAGS_UPDATED,
  SIGNAL_TOC_UPDATED,
  SIGNAL_LAST
};

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
 * @extractor: a #ClapperTubeExtractor
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
 * @extractor: a #ClapperTubeExtractor
 * @uri: a media source URI
 *
 * Sets new URI for handling. URI cannot be %NULL.
 *
 * After setting new URI, [vfunc@ClapperTube.Extractor.extract] should
 * return [enum@ClapperTube.Flow.RESTART] which will trigger another
 * extractor query for updated URI.
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

/**
 * clapper_tube_extractor_get_harvest:
 * @extractor: a #ClapperTubeExtractor
 *
 * Returns: (transfer none): a #ClapperTubeHarvest.
 */
ClapperTubeHarvest *
clapper_tube_extractor_get_harvest (ClapperTubeExtractor *self)
{
  ClapperTubeExtractorPrivate *priv;

  g_return_val_if_fail (CLAPPER_TUBE_IS_EXTRACTOR (self), NULL);

  priv = clapper_tube_extractor_get_instance_private (self);

  return priv->harvest;
}

/**
 * clapper_tube_extractor_emit_harvest_updated:
 * @extractor: a #ClapperTubeExtractor
 *
 * A convenience function that will emit [signal@ClapperTube.Extractor::tags-updated]
 * with current harvest.
 *
 * Use this if @extractor updates its tags, toc or request-headers after
 * initial extraction. Useful for dealing with live sources where
 * e.g. media title changes during playback.
 */
void
clapper_tube_extractor_emit_harvest_updated (ClapperTubeExtractor *self)
{
  ClapperTubeExtractorPrivate *priv;

  g_return_if_fail (CLAPPER_TUBE_IS_EXTRACTOR (self));

  priv = clapper_tube_extractor_get_instance_private (self);

  g_signal_emit (G_OBJECT (self), signals[SIGNAL_HARVEST_UPDATED], 0, priv->harvest);
}

static void
clapper_tube_extractor_init (ClapperTubeExtractor *self)
{
  ClapperTubeExtractorPrivate *priv = clapper_tube_extractor_get_instance_private (self);

  priv->harvest = clapper_tube_harvest_new ();
}

static ClapperTubeFlow
clapper_tube_extractor_extract (ClapperTubeExtractor *self,
    GCancellable *cancellable, GError **error)
{
  GST_ERROR_OBJECT (self, "Extraction is not implemented!");

  return CLAPPER_TUBE_FLOW_ERROR;
}

static void
clapper_tube_extractor_finalize (GObject *object)
{
  ClapperTubeExtractor *self = CLAPPER_TUBE_EXTRACTOR_CAST (object);
  ClapperTubeExtractorPrivate *priv = clapper_tube_extractor_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Finalize");

  if (priv->uri)
    g_uri_unref (priv->uri);

  gst_object_unref (priv->harvest);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_tube_extractor_class_init (ClapperTubeExtractorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperTubeExtractorClass *extractor_class = (ClapperTubeExtractorClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubeextractor", 0,
      "Clapper Tube Extractor");

  /**
   * ClapperTubeExtractor::harvest-updated:
   * @extractor: a #ClapperTubeExtractor
   * @harvest: a #ClapperTubeHarvest
   *
   * Implementations emit this signal when harvest tags, toc or request-headers
   * at a later point (after initial extraction).
   */
  signals[SIGNAL_HARVEST_UPDATED] = g_signal_new ("harvest-updated",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, CLAPPER_TUBE_TYPE_HARVEST);

  gobject_class->finalize = clapper_tube_extractor_finalize;

  extractor_class->extract = clapper_tube_extractor_extract;
}
