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
 * An object storing data from enhancers that implement [iface@Clapper.Extractable] interface.
 *
 * Since: 0.8
 */

/*
 * NOTE: We cannot simply expose GstMiniObjects for
 * implementations to assemble TagList/Toc themselves, see:
 * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/2867
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

  GstTagList *tags;
  GstToc *toc;
  GstStructure *headers;

  guint16 n_chapters;
  guint16 n_tracks;
};

#define parent_class clapper_harvest_parent_class
G_DEFINE_TYPE (ClapperHarvest, clapper_harvest, GST_TYPE_OBJECT);

static inline void
_ensure_tags (ClapperHarvest *self)
{
  if (!self->tags) {
    self->tags = gst_tag_list_new_empty ();
    gst_tag_list_set_scope (self->tags, GST_TAG_SCOPE_GLOBAL);
  }
}

static inline void
_ensure_toc (ClapperHarvest *self)
{
  if (!self->toc)
    self->toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);
}

static inline void
_ensure_headers (ClapperHarvest *self)
{
  if (!self->headers)
    self->headers = gst_structure_new_empty ("request-headers");
}

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
    GstBuffer **buffer, gsize *buf_size, GstCaps **caps,
    GstTagList **tags, GstToc **toc, GstStructure **headers)
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

  *tags = self->tags;
  self->tags = NULL;

  *toc = self->toc;
  self->toc = NULL;

  *headers = self->headers;
  self->headers = NULL;

  return TRUE;
}

/**
 * clapper_harvest_fill:
 * @harvest: a #ClapperHarvest
 * @media_type: media mime type
 * @data: (array length=size) (element-type guint8) (transfer full): data to fill @harvest
 * @size: allocated size of @data
 *
 * Fill harvest with extracted data. It can be anything that GStreamer
 * can parse and play such as single URI or a streaming manifest.
 *
 * Calling again this function will replace previously filled content.
 *
 * Commonly used media types are:
 *
 *   * `application/dash+xml`
 *
 *   * `application/x-hls`
 *
 *   * `text/uri-list`
 *
 * Returns: %TRUE when filled successfully, %FALSE if taken data was empty.
 *
 * Since: 0.8
 */
gboolean
clapper_harvest_fill (ClapperHarvest *self, const gchar *media_type, gpointer data, gsize size)
{
  g_return_val_if_fail (CLAPPER_IS_HARVEST (self), FALSE);
  g_return_val_if_fail (media_type != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (!data || size == 0) {
    g_free (data);
    return FALSE;
  }

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gboolean is_printable = (strcmp (media_type, "application/dash+xml") == 0)
        || (strcmp (media_type, "application/x-hls") == 0)
        || (strcmp (media_type, "text/uri-list") == 0);

    if (is_printable) {
      gchar *data_str;

      data_str = g_new0 (gchar, size + 1);
      memcpy (data_str, data, size);

      GST_DEBUG_OBJECT (self, "Filled with data:\n%s", data_str);

      g_free (data_str);
    }
  }

  gst_clear_buffer (&self->buffer);
  gst_clear_caps (&self->caps);

  self->buffer = gst_buffer_new_wrapped (data, size);
  self->buf_size = size;
  self->caps = gst_caps_new_simple (media_type,
      "source", G_TYPE_STRING, "clapper-harvest", NULL);

  return TRUE;
}

/**
 * clapper_harvest_fill_with_text:
 * @harvest: a #ClapperHarvest
 * @media_type: media mime type
 * @text: (transfer full): data to fill @harvest as %NULL terminated string
 *
 * A convenience method to fill @harvest using a %NULL terminated string.
 *
 * For more info, see [method@Clapper.Harvest.fill] documentation.
 *
 * Returns: %TRUE when filled successfully, %FALSE if taken data was empty.
 *
 * Since: 0.8
 */
gboolean
clapper_harvest_fill_with_text (ClapperHarvest *self, const gchar *media_type, gchar *text)
{
  g_return_val_if_fail (text != NULL, FALSE);

  return clapper_harvest_fill (self, media_type, text, strlen (text));
}

/**
 * clapper_harvest_fill_with_bytes:
 * @harvest: a #ClapperHarvest
 * @media_type: media mime type
 * @bytes: (transfer full): a #GBytes to fill @harvest
 *
 * A convenience method to fill @harvest with data from #GBytes.
 *
 * For more info, see [method@Clapper.Harvest.fill] documentation.
 *
 * Returns: %TRUE when filled successfully, %FALSE if taken data was empty.
 *
 * Since: 0.8
 */
gboolean
clapper_harvest_fill_with_bytes (ClapperHarvest *self, const gchar *media_type, GBytes *bytes)
{
  gpointer data;
  gsize size = 0;

  g_return_val_if_fail (bytes != NULL, FALSE);

  data = g_bytes_unref_to_data (bytes, &size);

  return clapper_harvest_fill (self, media_type, data, size);
}

/**
 * clapper_harvest_tags_add:
 * @harvest: a #ClapperHarvest
 * @tag: a name of tag to set
 * @...: %NULL-terminated list of arguments
 *
 * Append one or more tags into the tag list.
 *
 * Variable arguments should be in the form of tag and value pairs.
 *
 * Since: 0.8
 */
void
clapper_harvest_tags_add (ClapperHarvest *self, const gchar *tag, ...)
{
  va_list args;

  g_return_if_fail (CLAPPER_IS_HARVEST (self));
  g_return_if_fail (tag != NULL);

  _ensure_tags (self);

  va_start (args, tag);
  gst_tag_list_add_valist (self->tags, GST_TAG_MERGE_APPEND, tag, args);
  va_end (args);
}

/**
 * clapper_harvest_tags_add_value: (rename-to clapper_harvest_tags_add)
 * @harvest: a #ClapperHarvest
 * @tag: a name of tag to set
 * @value: a #GValue of tag
 *
 * Append another tag into the tag list using #GValue.
 *
 * Since: 0.8
 */
void
clapper_harvest_tags_add_value (ClapperHarvest *self, const gchar *tag, const GValue *value)
{
  g_return_if_fail (CLAPPER_IS_HARVEST (self));
  g_return_if_fail (tag != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  _ensure_tags (self);
  gst_tag_list_add_value (self->tags, GST_TAG_MERGE_APPEND, tag, value);
}

/**
 * clapper_harvest_toc_add:
 * @harvest: a #ClapperHarvest
 * @type: a #GstTocEntryType
 * @title: an entry title
 * @start: entry start time in seconds
 * @end: entry end time in seconds or -1 if none
 *
 * Append a chapter or track name into table of contents.
 *
 * Since: 0.8
 */
void
clapper_harvest_toc_add (ClapperHarvest *self, GstTocEntryType type,
    const gchar *title, gdouble start, gdouble end)
{
  GstTocEntry *entry, *subentry;
  GstClockTime start_time, end_time;
  gchar edition[3]; // 2 + 1
  gchar id[14]; // 7 + 1 + 5 + 1
  const gchar *id_prefix;
  guint16 nth_entry;

  g_return_if_fail (CLAPPER_IS_HARVEST (self));
  g_return_if_fail (type == GST_TOC_ENTRY_TYPE_CHAPTER || type == GST_TOC_ENTRY_TYPE_TRACK);
  g_return_if_fail (title != NULL);
  g_return_if_fail (start >= 0 && end >= start);

  switch (type) {
    case GST_TOC_ENTRY_TYPE_CHAPTER:
      id_prefix = "chapter";
      nth_entry = ++(self->n_chapters);
      break;
    case GST_TOC_ENTRY_TYPE_TRACK:
      id_prefix = "track";
      nth_entry = ++(self->n_tracks);
      break;
    default:
      g_assert_not_reached ();
      return;
  }

  start_time = start * GST_SECOND;
  end_time = (end >= 0) ? end * GST_SECOND : GST_CLOCK_TIME_NONE;

  g_snprintf (edition, sizeof (edition), "0%i", type);
  g_snprintf (id, sizeof (id), "%s.%" G_GUINT16_FORMAT, id_prefix, nth_entry);

  GST_DEBUG_OBJECT (self, "Inserting TOC %s: \"%s\""
      " (%" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT ")",
      id, title, start_time, end_time);

  subentry = gst_toc_entry_new (type, id);
  gst_toc_entry_set_tags (subentry, gst_tag_list_new (GST_TAG_TITLE, title, NULL));
  gst_toc_entry_set_start_stop_times (subentry, start_time, end_time);

  _ensure_toc (self);

find_entry:
  if (!(entry = gst_toc_find_entry (self->toc, edition))) {
    GstTocEntry *toc_entry;

    toc_entry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_EDITION, edition);
    gst_toc_entry_set_start_stop_times (toc_entry, 0, GST_CLOCK_TIME_NONE);
    gst_toc_append_entry (self->toc, toc_entry); // transfer full and must be writable

    goto find_entry;
  }

  gst_toc_entry_append_sub_entry (entry, subentry);
}

/**
 * clapper_harvest_headers_set:
 * @harvest: a #ClapperHarvest
 * @key: a header name
 * @...: %NULL-terminated list of arguments
 *
 * Set one or more request headers named with @key to specified `value`.
 *
 * Arguments should be %NULL terminated list of `key+value` string pairs.
 *
 * Setting again the same key will update its value to the new one.
 *
 * Since: 0.8
 */
void
clapper_harvest_headers_set (ClapperHarvest *self, const gchar *key, ...)
{
  va_list args;

  g_return_if_fail (CLAPPER_IS_HARVEST (self));
  g_return_if_fail (key != NULL);

  _ensure_headers (self);

  va_start (args, key);

  while (key != NULL) {
    const gchar *val = va_arg (args, const gchar *);
    GST_DEBUG_OBJECT (self, "Set header, \"%s\": \"%s\"", key, val);
    gst_structure_set (self->headers, key, G_TYPE_STRING, val, NULL);
    key = va_arg (args, const gchar *);
  }

  va_end (args);
}

/**
 * clapper_harvest_headers_set_value: (rename-to clapper_harvest_headers_set)
 * @harvest: a #ClapperHarvest
 * @key: a header name
 * @value: a string #GValue of header
 *
 * Set another header in the request headers list using #GValue.
 *
 * Setting again the same key will update its value to the new one.
 *
 * Since: 0.8
 */
void
clapper_harvest_headers_set_value (ClapperHarvest *self, const gchar *key, const GValue *value)
{
  g_return_if_fail (CLAPPER_IS_HARVEST (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_VALUE (value) && G_VALUE_HOLDS_STRING (value));

  _ensure_headers (self);

  GST_DEBUG_OBJECT (self, "Set header, \"%s\": \"%s\"", key, g_value_get_string (value));
  gst_structure_set_value (self->headers, key, value);
}

static void
clapper_harvest_init (ClapperHarvest *self)
{
}

static void
clapper_harvest_finalize (GObject *object)
{
  ClapperHarvest *self = CLAPPER_HARVEST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_caps (&self->caps);
  gst_clear_buffer (&self->buffer);

  if (self->tags)
    gst_tag_list_unref (self->tags);
  if (self->toc)
    gst_toc_unref (self->toc);
  if (self->headers)
    gst_structure_free (self->headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_harvest_class_init (ClapperHarvestClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperharvest", 0,
      "Clapper Harvest");

  gobject_class->finalize = clapper_harvest_finalize;
}
