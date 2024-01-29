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
 * ClapperExternalSubtitleStream:
 *
 * Represents an external subtitle stream.
 */

#include "clapper-external-subtitle-stream.h"
#include "clapper-stream-private.h"
#include "clapper-utils-private.h"

#define GST_CAT_DEFAULT clapper_external_subtitle_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperExternalSubtitleStream
{
  ClapperSubtitleStream parent;

  gchar *uri;
};

#define parent_class clapper_external_subtitle_stream_parent_class
G_DEFINE_TYPE (ClapperExternalSubtitleStream, clapper_external_subtitle_stream, CLAPPER_TYPE_SUBTITLE_STREAM);

enum
{
  PROP_0,
  PROP_URI,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * clapper_external_subtitle_stream_new:
 * @uri: a subtitles URI
 *
 * Creates new #ClapperExternalSubtitleStream from URI.
 *
 * Use one of the URI protocols supported by plugins in #GStreamer
 * installation. For local files you can use either "file" protocol
 * or [method@Clapper.ExternalSubtitleStream.new_from_file] method.
 *
 * It is considered a programmer error trying to create new stream from
 * invalid URI. If URI is valid, but unsupported by installed plugins on user
 * system, [class@Clapper.Player] will emit a [signal@Clapper.Player::missing-plugin]
 * signal upon playback.
 *
 * Returns: (transfer full): a new #ClapperExternalSubtitleStream.
 */
ClapperExternalSubtitleStream *
clapper_external_subtitle_stream_new (const gchar *uri)
{
  ClapperExternalSubtitleStream *stream;

  g_return_val_if_fail (uri != NULL, NULL);

  stream = g_object_new (CLAPPER_TYPE_EXTERNAL_SUBTITLE_STREAM, "uri", uri, NULL);
  gst_object_ref_sink (stream);

  GST_TRACE_OBJECT (stream, "New external subtitle stream, URI: %s",
      stream->uri);

  return stream;
}

/**
 * clapper_external_subtitle_stream_new_from_file:
 * @file: (transfer none): a #GFile
 *
 * Creates new #ClapperExternalSubtitleStream from #GFile.
 *
 * Same as [method@Clapper.ExternalSubtitleStream.new], but uses a
 * [class@GLib.File] for convenience in some situations instead of an URI.
 *
 * Returns: (transfer full): a new #ClapperExternalSubtitleStream.
 */
ClapperExternalSubtitleStream *
clapper_external_subtitle_stream_new_from_file (GFile *file)
{
  ClapperExternalSubtitleStream *stream;
  gchar *uri;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uri = clapper_utils_uri_from_file (file);
  stream = clapper_external_subtitle_stream_new (uri);

  g_free (uri);

  return stream;
}

/**
 * clapper_external_subtitle_stream_get_uri:
 * @stream: a #ClapperExternalSubtitleStream
 *
 * Get the URI of #ClapperExternalSubtitleStream.
 *
 * Returns: an URI of #ClapperExternalSubtitleStream.
 */
const gchar *
clapper_external_subtitle_stream_get_uri (ClapperExternalSubtitleStream *self)
{
  g_return_val_if_fail (CLAPPER_IS_EXTERNAL_SUBTITLE_STREAM (self), NULL);

  return self->uri;
}

static void
clapper_external_subtitle_stream_init (ClapperExternalSubtitleStream *self)
{
}

static void
clapper_external_subtitle_stream_constructed (GObject *object)
{
  ClapperExternalSubtitleStream *self = CLAPPER_EXTERNAL_SUBTITLE_STREAM_CAST (object);
  GstStream *gst_stream;
  GstTagList *tags;
  gchar *title;

  /* Be safe when someone incorrectly constructs stream without URI */
  if (G_UNLIKELY (self->uri == NULL))
    self->uri = g_strdup ("file://");

  gst_stream = gst_stream_new (GST_OBJECT_NAME (self),
      NULL, GST_STREAM_TYPE_UNKNOWN, GST_STREAM_FLAG_NONE);

  title = clapper_utils_title_from_uri (self->uri);
  tags = gst_tag_list_new (GST_TAG_TITLE, title, NULL);
  g_free (title);

  gst_stream_set_tags (gst_stream, tags);
  gst_tag_list_unref (tags);

  clapper_stream_set_gst_stream (CLAPPER_STREAM_CAST (self), gst_stream);
  gst_object_unref (gst_stream);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_external_subtitle_stream_finalize (GObject *object)
{
  ClapperExternalSubtitleStream *self = CLAPPER_EXTERNAL_SUBTITLE_STREAM_CAST (object);

  g_free (self->uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_external_subtitle_stream_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperExternalSubtitleStream *self = CLAPPER_EXTERNAL_SUBTITLE_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_external_subtitle_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperExternalSubtitleStream *self = CLAPPER_EXTERNAL_SUBTITLE_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, clapper_external_subtitle_stream_get_uri (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_external_subtitle_stream_class_init (ClapperExternalSubtitleStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperexternalsubtitlestream", 0,
      "Clapper External Subtitle Stream");

  gobject_class->constructed = clapper_external_subtitle_stream_constructed;
  gobject_class->set_property = clapper_external_subtitle_stream_set_property;
  gobject_class->get_property = clapper_external_subtitle_stream_get_property;
  gobject_class->finalize = clapper_external_subtitle_stream_finalize;

  /**
   * ClapperExternalSubtitleStream:uri:
   *
   * Stream URI.
   */
  param_specs[PROP_URI] = g_param_spec_string ("uri",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
