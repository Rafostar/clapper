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
 * ClapperTubeHarvest:
 *
 * An object storing all extracted data.
 */

#include "clapper-tube-harvest-private.h"

#define GST_CAT_DEFAULT clapper_tube_harvest_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperTubeHarvest
{
  GstObject parent;

  GstCaps *caps;
  GstBuffer *buffer;

  /* props */
  GstTagList *tags;
  GstToc *toc;
  GstStructure *request_headers;
};

enum
{
  PROP_0,
  PROP_TAGS,
  PROP_TOC,
  PROP_REQUEST_HEADERS,
  PROP_LAST
};

#define parent_class clapper_tube_harvest_parent_class
G_DEFINE_TYPE (ClapperTubeHarvest, clapper_tube_harvest, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

ClapperTubeHarvest *
clapper_tube_harvest_new (void)
{
  ClapperTubeHarvest *harvest;

  harvest = g_object_new (CLAPPER_TUBE_TYPE_HARVEST, NULL);
  gst_object_ref_sink (harvest);

  return harvest;
}

/**
 * clapper_tube_harvest_fill:
 * @harvest: a #ClapperTubeHarvest
 * @data: (transfer full): a %NULL terminated string to fill @harvest
 *
 * Fill harvest with extracted data. It can be anything that GStreamer
 * can parse and play such as single URI or a streaming manifest data.
 *
 * Calling again this function will replace previously filled content.
 *
 * Returns: %TRUE if taken data is valid and usable, %FALSE otherwise.
 */
gboolean
clapper_tube_harvest_fill (ClapperTubeHarvest *self, gchar *data)
{
  gsize len;
  const gchar *media_type = NULL;

  g_return_val_if_fail (CLAPPER_TUBE_IS_HARVEST (self), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  gst_clear_buffer (&self->buffer);
  gst_clear_caps (&self->caps);

  len = strlen (data);

  if (len == 0) {
    g_free (data);
    return FALSE;
  }

  /* FIXME: Improve data parsing */
  if (data[0] == '<') {
    media_type = "application/dash+xml";
  } else if (data[0] == '#') {
    media_type = "application/x-hls";
  } else if (gst_uri_is_valid (data)) {
    media_type = "text/uri-list";
  }

  if (!media_type) {
    g_free (data);
    return FALSE;
  }

  self->buffer = gst_buffer_new_wrapped (data, len);
  self->caps = gst_caps_new_simple (media_type,
      "source", G_TYPE_STRING, "clapper-tube", NULL);

  return TRUE;
}

/**
 * clapper_tube_harvest_get_tags:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get media tags.
 *
 * Returns: (transfer none) (nullable): a #GstTagList
 */
GstTagList *
clapper_tube_harvest_get_tags (ClapperTubeHarvest *self)
{
  g_return_val_if_fail (CLAPPER_TUBE_IS_HARVEST (self), NULL);

  return self->tags;
}

/**
 * clapper_tube_harvest_get_toc:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get media table of contents.
 *
 * Returns: (transfer none) (nullable): a #GstToc
 */
GstToc *
clapper_tube_harvest_get_toc (ClapperTubeHarvest *self)
{
  g_return_val_if_fail (CLAPPER_TUBE_IS_HARVEST (self), NULL);

  return self->toc;
}

/**
 * clapper_tube_harvest_get_request_headers:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get request headers to be used when streaming.
 *
 * Returns: (transfer none) (nullable): a #GstStructure
 */
GstStructure *
clapper_tube_harvest_get_request_headers (ClapperTubeHarvest *self)
{
  g_return_val_if_fail (CLAPPER_TUBE_IS_HARVEST (self), NULL);

  return self->request_headers;
}

static void
clapper_tube_harvest_init (ClapperTubeHarvest *self)
{
  self->tags = gst_tag_list_new_empty ();
  gst_tag_list_set_scope (self->tags, GST_TAG_SCOPE_GLOBAL);

  self->toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);

  /* Create request headers with some defaults set */
  self->request_headers = gst_structure_new ("request-headers",
      "User-Agent", "Mozilla/5.0 (Windows NT 10.0; rv:78.0) Gecko/20100101 Firefox/78.0",
      NULL);
}

static void
clapper_tube_harvest_finalize (GObject *object)
{
  ClapperTubeHarvest *self = CLAPPER_TUBE_HARVEST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_caps (&self->caps);
  gst_clear_buffer (&self->buffer);

  gst_tag_list_unref (self->tags);
  gst_toc_unref (self->toc);
  gst_structure_free (self->request_headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}
/*
static void
clapper_tube_harvest_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperTubeHarvest *self = CLAPPER_TUBE_HARVEST_CAST (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
*/
static void
clapper_tube_harvest_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperTubeHarvest *self = CLAPPER_TUBE_HARVEST_CAST (object);

  switch (prop_id) {
    case PROP_TAGS:
      g_value_set_boxed (value, clapper_tube_harvest_get_tags (self));
      break;
    case PROP_TOC:
      g_value_set_boxed (value, clapper_tube_harvest_get_toc (self));
      break;
    case PROP_REQUEST_HEADERS:
      g_value_set_boxed (value, clapper_tube_harvest_get_request_headers (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_tube_harvest_class_init (ClapperTubeHarvestClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubeharvest", 0,
      "Clapper Tube Harvest");

  //gobject_class->set_property = clapper_tube_harvest_set_property;
  gobject_class->get_property = clapper_tube_harvest_get_property;
  gobject_class->finalize = clapper_tube_harvest_finalize;

  /**
   * ClapperTubeHarvest:tags:
   *
   * Media tags.
   */
  param_specs[PROP_TAGS] = g_param_spec_boxed ("tags",
      NULL, NULL, GST_TYPE_TAG_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperTubeHarvest:toc:
   *
   * Media table of contents.
   */
  param_specs[PROP_TOC] = g_param_spec_boxed ("toc",
      NULL, NULL, GST_TYPE_TOC,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperTubeHarvest:request-headers:
   *
   * Streaming request headers.
   */
  param_specs[PROP_REQUEST_HEADERS] = g_param_spec_boxed ("request-headers",
      NULL, NULL, GST_TYPE_STRUCTURE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
