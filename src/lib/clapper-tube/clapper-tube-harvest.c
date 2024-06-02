/* ClapperTube Library
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

#include "clapper-tube-harvest.h"

#define GST_CAT_DEFAULT clapper_tube_harvest_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperTubeHarvest
{
  GstObject parent;

  GstCaps *caps;
  GstBuffer *buffer;

  GstTagList *tags;
  GstToc *toc;

  GstStructure *request_headers;
};

enum
{
  PROP_0,
  PROP_CAPS,
  PROP_BUFFER,
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
 * clapper_tube_harvest_set_caps:
 * @harvest: a #ClapperTubeHarvest
 * @caps: (transfer none) (nullable): a #GstCaps
 *
 * Set caps describing data in the [property@ClapperTube.Harvest:buffer].
 */
void
clapper_tube_harvest_set_caps (ClapperTubeHarvest *self, GstCaps *caps)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (caps == NULL || GST_IS_CAPS (caps));

  gst_caps_replace (&self->caps, caps);
}

/**
 * clapper_tube_harvest_take_caps: (skip)
 * @harvest: a #ClapperTubeHarvest
 * @caps: (transfer full) (nullable): a #GstCaps
 *
 * Same as [method@ClapperTube.Harvest.set_caps], but takes ownership of @caps.
 */
void
clapper_tube_harvest_take_caps (ClapperTubeHarvest *self, GstCaps *caps)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (caps == NULL || GST_IS_CAPS (caps));

  gst_caps_take (&self->caps, caps);
}

/**
 * clapper_tube_harvest_get_caps:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get the previously set caps describing data in the [property@ClapperTube.Harvest:buffer].
 *
 * Returns: (transfer none) (nullable): a #GstCaps
 */
GstCaps *
clapper_tube_harvest_get_caps (ClapperTubeHarvest *self)
{
  g_return_val_if_fail (CLAPPER_TUBE_IS_HARVEST (self), NULL);

  return self->caps;
}

/**
 * clapper_tube_harvest_set_buffer:
 * @harvest: a #ClapperTubeHarvest
 * @buffer: (transfer none) (nullable): a #GstBuffer
 *
 * Set buffer filled with extracted data. It can be anything that
 * GStreamer can parse and play.
 */
void
clapper_tube_harvest_set_buffer (ClapperTubeHarvest *self, GstBuffer *buffer)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (GST_IS_BUFFER (buffer));

  gst_clear_buffer (&self->buffer);
  self->buffer = buffer;
}

/**
 * clapper_tube_harvest_take_buffer: (skip)
 * @harvest: a #ClapperTubeHarvest
 * @buffer: (transfer full) (nullable): a #GstBuffer
 *
 * Same as [method@ClapperTube.Harvest.set_buffer], but takes ownership of @buffer.
 */
void
clapper_tube_harvest_take_buffer (ClapperTubeHarvest *self, GstBuffer *buffer)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (GST_IS_BUFFER (buffer));

  gst_clear_buffer (&self->buffer);
  self->buffer = buffer;
}

/**
 * clapper_tube_harvest_set_tags:
 * @harvest: a #ClapperTubeHarvest
 * @tags: (transfer none) (nullable): a #GstTagList
 *
 * Set tags with extracted media metadata.
 */
void
clapper_tube_harvest_set_tags (ClapperTubeHarvest *self, GstTagList *tags)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (caps == NULL || GST_IS_TAG_LIST (tags));

  gst_tag_list_replace (&self->tags, tags);
}

/**
 * clapper_tube_harvest_take_tags: (skip)
 * @harvest: a #ClapperTubeHarvest
 * @tags: (transfer full) (nullable): a #GstTagList
 *
 * Same as [method@ClapperTube.Harvest.set_tags], but takes ownership of @tags.
 */
void
clapper_tube_harvest_take_caps (ClapperTubeHarvest *self, GstTagList *tags)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (caps == NULL || GST_IS_TAG_LIST (tags));

  gst_tag_list_take (&self->tags, tags);
}

/**
 * clapper_tube_harvest_get_tags:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get the previously set media tags.
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
 * clapper_tube_harvest_set_toc:
 * @harvest: a #ClapperTubeHarvest
 * @toc: (transfer none) (nullable): a #GstToc
 *
 * Set media table of contents.
 */
void
clapper_tube_harvest_set_toc (ClapperTubeHarvest *self, GstToc *toc)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (toc == NULL || GST_IS_TOC (toc));

  gst_mini_object_replace ((GstMiniObject **) &self->toc, GST_MINI_OBJECT_CAST (toc));
}

/**
 * clapper_tube_harvest_take_toc: (skip)
 * @harvest: a #ClapperTubeHarvest
 * @toc: (transfer full) (nullable): a #GstToc
 *
 * Same as [method@ClapperTube.Harvest.set_toc], but takes ownership of @toc.
 */
void
clapper_tube_harvest_take_toc (ClapperTubeHarvest *self, GstToc *toc)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (toc == NULL || GST_IS_TOC (toc));

  gst_mini_object_take ((GstMiniObject **) &self->toc, GST_MINI_OBJECT_CAST (toc));
}

/**
 * clapper_tube_harvest_get_toc:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get the previously set media table of contents.
 *
 * Returns: (transfer none) (nullable): a #GstToc
 */
GstTagList *
clapper_tube_harvest_get_toc (ClapperTubeHarvest *self)
{
  g_return_val_if_fail (CLAPPER_TUBE_IS_HARVEST (self), NULL);

  return self->toc;
}

/**
 * clapper_tube_harvest_set_request_headers:
 * @harvest: a #ClapperTubeHarvest
 * @structure: (transfer none) (nullable): a #GstStructure
 *
 * Set request headers that should be applied during streaming.
 * The @structure must be named "request-headers" for this to work.
 */
void
clapper_tube_harvest_set_request_headers (ClapperTubeHarvest *self, GstStructure *structure)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (structure == NULL || GST_IS_STRUCTURE (structure));

  gst_structure_take (&self->request_headers, gst_structure_copy (structure));
}

/**
 * clapper_tube_harvest_take_request_headers: (skip)
 * @harvest: a #ClapperTubeHarvest
 * @structure: (transfer full) (nullable): a #GstStructure
 *
 * Same as [method@ClapperTube.Harvest.set_request_headers], but takes ownership of @structure.
 */
void
clapper_tube_harvest_take_request_headers (ClapperTubeHarvest *self, GstStructure *structure)
{
  g_return_if_fail (CLAPPER_TUBE_IS_HARVEST (self));
  g_return_if_fail (structure == NULL || GST_IS_STRUCTURE (structure));

  gst_structure_take (&self->request_headers, structure);
}

/**
 * clapper_tube_harvest_get_request_headers:
 * @harvest: a #ClapperTubeHarvest
 *
 * Get the previously set streaming request_headers.
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
}

static void
clapper_tube_harvest_finalize (GObject *object)
{
  ClapperTubeHarvest *self = CLAPPER_TUBE_HARVEST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_caps (&self->caps);
  gst_clear_buffer (&self->buffer);

  gst_clear_tag_list (&self->tags);
  gst_clear_mini_object (GST_MINI_OBJECT_CAST (self->toc));

  gst_clear_structure (&self->request_headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_tube_harvest_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperTubeHarvest *self = CLAPPER_TUBE_HARVEST_CAST (object);

  switch (prop_id) {
    case PROP_CAPS:
      
      break;
    case PROP_BUFFER:
      
      break;
    case PROP_TAGS:
      
      break;
    case PROP_TOC:
      
      break;
    case PROP_REQUEST_HEADERS:
      
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_tube_harvest_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperTubeHarvest *self = CLAPPER_TUBE_HARVEST_CAST (object);

  switch (prop_id) {
    case PROP_CAPS:
      g_value_set_boxed (value, clapper_tube_harvest_get_caps (self));
      break;
    case PROP_BUFFER:
      g_value_set_boxed (value, clapper_tube_harvest_get_buffer (self));
      break;
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
      "ClapperTube Harvest");

  gobject_class->set_property = clapper_media_item_set_property;
  gobject_class->get_property = clapper_media_item_get_property;
  gobject_class->finalize = clapper_media_item_finalize;

  /**
   * ClapperTubeHarvest:caps:
   *
   * Caps describing data in buffer.
   */
  param_specs[PROP_CAPS] = g_param_spec_boxed ("caps",
      NULL, NULL, GST_TYPE_CAPS,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperTubeHarvest:buffer:
   *
   * Buffer filled with extracted data.
   */
  param_specs[PROP_BUFFER] = g_param_spec_boxed ("buffer",
      NULL, NULL, GST_TYPE_BUFFER,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
