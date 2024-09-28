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

/**
 * ClapperHarvest:
 *
 * An object storing data that extension implementing [iface@Clapper.Extractor] extracts.
 *
 * Any harvested data (including tags, toc and request-headers) can only be
 * added/changed from within virtual functions of [iface@Clapper.Extractor].
 *
 * Since: 0.8
 */

#include "clapper-harvest-private.h"

#define GST_CAT_DEFAULT clapper_harvest_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperHarvest
{
  GstObject parent;

  GstCaps *caps;
  GstBuffer *buffer;
  gsize buf_size;

  /* props */
  GstTagList *tags;
  GstToc *toc;
  GstStructure *request_headers;
};

#define parent_class clapper_harvest_parent_class
G_DEFINE_TYPE (ClapperHarvest, clapper_harvest, GST_TYPE_OBJECT);

ClapperHarvest *
clapper_harvest_new (void)
{
  ClapperHarvest *harvest;

  harvest = g_object_new (CLAPPER_TYPE_HARVEST, NULL);
  gst_object_ref_sink (harvest);

  return harvest;
}

gboolean
clapper_harvest_unpack (ClapperHarvest *self,
    GstBuffer **buffer, gsize *buf_size, GstCaps **caps)
{
  /* Not filled or already unpacked */
  if (!self->buffer)
    return FALSE;

  *buffer = self->buffer;
  self->buffer = NULL;

  *buf_size = self->buf_size;
  self->buf_size = 0;

  *caps = self->caps;
  self->caps = NULL;

  return TRUE;
}

/**
 * clapper_harvest_fill:
 * @harvest: a #ClapperHarvest
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
clapper_harvest_fill (ClapperHarvest *self, gchar *data)
{
  gsize len;
  const gchar *media_type = NULL;

  g_return_val_if_fail (CLAPPER_IS_HARVEST (self), FALSE);
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
  self->buf_size = len;
  self->caps = gst_caps_new_simple (media_type,
      "source", G_TYPE_STRING, "clapper-extractor", NULL);

  return TRUE;
}

/**
 * clapper_harvest_get_tags:
 * @harvest: a #ClapperHarvest
 *
 * Get media tags.
 *
 * Returns: (transfer none): a #GstTagList
 */
GstTagList *
clapper_harvest_get_tags (ClapperHarvest *self)
{
  g_return_val_if_fail (CLAPPER_IS_HARVEST (self), NULL);

  return self->tags;
}

/**
 * clapper_harvest_get_toc:
 * @harvest: a #ClapperHarvest
 *
 * Get media table of contents.
 *
 * Returns: (transfer none): a #GstToc
 */
GstToc *
clapper_harvest_get_toc (ClapperHarvest *self)
{
  g_return_val_if_fail (CLAPPER_IS_HARVEST (self), NULL);

  return self->toc;
}

/**
 * clapper_harvest_get_request_headers:
 * @harvest: a #ClapperHarvest
 *
 * Get request headers to be used when streaming.
 *
 * Returns: (transfer none): a #GstStructure
 */
GstStructure *
clapper_harvest_get_request_headers (ClapperHarvest *self)
{
  g_return_val_if_fail (CLAPPER_IS_HARVEST (self), NULL);

  return self->request_headers;
}

static void
clapper_harvest_init (ClapperHarvest *self)
{
  self->tags = gst_tag_list_new_empty ();
  gst_tag_list_set_scope (self->tags, GST_TAG_SCOPE_GLOBAL);

  self->toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);

  /* Create request headers with some defaults set 
  self->request_headers = gst_structure_new ("request-headers",
      "User-Agent", "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
      NULL);
*/
}

static void
clapper_harvest_finalize (GObject *object)
{
  ClapperHarvest *self = CLAPPER_HARVEST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_caps (&self->caps);
  gst_clear_buffer (&self->buffer);

  gst_tag_list_unref (self->tags);
  gst_toc_unref (self->toc);
  //gst_structure_free (self->request_headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_harvest_class_init (ClapperHarvestClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperharvest", 0,
      "Clapper Harvest");

  gobject_class->finalize = clapper_harvest_finalize;

  /* NOTE: We are not exposing any props here, because they would either
   * be not writable objects or copies would be created (boxed types)
   * leading to all kinds of mistakes in bindable programming languages */
}
