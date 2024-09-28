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

/**
 * ClapperExtractor:
 *
 * An optional Extractor feature to be added to the player.
 *
 * #ClapperExtractor is a feature that allows transforming URI that
 * does not lead to media directly into something playable.
 *
 * This feature does not have any actual extraction capability on its own,
 * instead it relies on `libpeas` based addons system to load and
 * use external objects that implement [iface@Clapper.Extractable] interface.
 *
 * Use [const@Clapper.HAVE_EXTRACTOR] macro to check if Clapper API
 * was compiled with this feature.
 *
 * Since: 0.8
 */

#include "config.h"

#include <gst/gst.h>
#include <libpeas.h>

#include "clapper-extractor.h"
#include "../shared/clapper-addons-loader-private.h"

#define GST_CAT_DEFAULT clapper_extractor_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperExtractor
{
  ClapperFeature parent;

  //ClapperExtractorDirector *director;
};

#define parent_class clapper_extractor_parent_class
G_DEFINE_TYPE (ClapperExtractor, clapper_extractor, CLAPPER_TYPE_FEATURE);

static void
clapper_extractor_queue_item_added (ClapperFeature *feature, ClapperMediaItem *item, guint index)
{
  ClapperExtractor *self = CLAPPER_EXTRACTOR_CAST (feature);
  GUri *uri;
  const PeasPluginInfo *info;

  GST_DEBUG_OBJECT (self, "Queue item added %" GST_PTR_FORMAT, item);

  uri = g_uri_parse (clapper_media_item_get_uri (item),
      G_URI_FLAGS_ENCODED, NULL);

  if (G_UNLIKELY (uri == NULL))
    return;

  if ((info = clapper_addons_loader_get_info (uri, CLAPPER_TYPE_EXTRACTOR))) {
    if (!self->director)
      self->director = clapper_extractor_director_new ();

    clapper_extractor_director_enqueue (self->director, uri);
  }

  g_uri_unref (uri);
}

static void
clapper_extractor_queue_item_removed (ClapperFeature *feature, ClapperMediaItem *item, guint index)
{
  ClapperExtractor *self = CLAPPER_EXTRACTOR_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item removed %" GST_PTR_FORMAT, item);
}

static void
clapper_extractor_queue_cleared (ClapperFeature *feature)
{
  ClapperExtractor *self = CLAPPER_EXTRACTOR_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue cleared");
}

static gboolean
clapper_extractor_prepare (ClapperFeature *feature)
{
  ClapperExtractor *self = CLAPPER_EXTRACTOR_CAST (feature);

  GST_DEBUG_OBJECT (self, "Prepare");

  clapper_addons_loader_init ();

  return TRUE;
}

static gboolean
clapper_extractor_unprepare (ClapperFeature *feature)
{
  ClapperExtractor *self = CLAPPER_EXTRACTOR_CAST (feature);

  GST_DEBUG_OBJECT (self, "Unprepare");

  /* Do what we also do when queue is cleared */
  //clapper_discoverer_queue_cleared (feature);

  return TRUE;
}

/**
 * clapper_extractor_new:
 *
 * Creates a new #ClapperExtractor instance.
 *
 * Returns: (transfer full): a new #ClapperExtractor instance.
 *
 * Since: 0.8
 */
ClapperExtractor *
clapper_extractor_new (void)
{
  ClapperExtractor *extractor = g_object_new (CLAPPER_TYPE_EXTRACTOR, NULL);
  gst_object_ref_sink (extractor);

  return extractor;
}

static void
clapper_extractor_init (ClapperExtractor *self)
{
}

static void
clapper_extractor_finalize (GObject *object)
{
  ClapperExtractor *self = CLAPPER_EXTRACTOR_CAST (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_extractor_class_init (ClapperExtractorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperFeatureClass *feature_class = (ClapperFeatureClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperextractor", 0,
      "Clapper Extractor");

  gobject_class->finalize = clapper_extractor_finalize;

  feature_class->prepare = clapper_extractor_prepare;
  feature_class->unprepare = clapper_extractor_unprepare;
  feature_class->queue_item_added = clapper_extractor_queue_item_added;
  feature_class->queue_item_removed = clapper_extractor_queue_item_removed;
  feature_class->queue_cleared = clapper_extractor_queue_cleared;
}
