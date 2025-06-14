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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * ClapperVideoStream:
 *
 * Represents a video stream within media.
 */

#include "clapper-video-stream-private.h"
#include "clapper-stream-private.h"

#define GST_CAT_DEFAULT clapper_video_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperVideoStream
{
  ClapperStream parent;

  gchar *codec;
  gint width;
  gint height;
  gdouble fps;
  guint bitrate;
  gchar *pixel_format;
};

#define parent_class clapper_video_stream_parent_class
G_DEFINE_TYPE (ClapperVideoStream, clapper_video_stream, CLAPPER_TYPE_STREAM);

enum
{
  PROP_0,
  PROP_CODEC,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FPS,
  PROP_BITRATE,
  PROP_PIXEL_FORMAT,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_update_using_caps (ClapperVideoStream *self, GstCaps *caps)
{
  ClapperStream *stream = CLAPPER_STREAM_CAST (self);
  GstStructure *structure;
  gint width = 0, height = 0, fps_n = 0, fps_d = 0;

  if (gst_caps_get_size (caps) == 0)
    return;

  structure = gst_caps_get_structure (caps, 0);

  /* NOTE: We cannot use gst_structure_get() here,
   * as it stops iterating on first not found key */

  gst_structure_get_int (structure, "width", &width);
  clapper_stream_set_int_prop (stream, param_specs[PROP_WIDTH], &self->width, width);

  gst_structure_get_int (structure, "height", &height);
  clapper_stream_set_int_prop (stream, param_specs[PROP_HEIGHT], &self->height, height);

  gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d);

  if (G_UNLIKELY (fps_d == 0))
    fps_d = 1;

  clapper_stream_set_double_prop (stream, param_specs[PROP_FPS], &self->fps, (gdouble) fps_n / fps_d);

  clapper_stream_set_string_prop (stream, param_specs[PROP_PIXEL_FORMAT], &self->pixel_format,
      gst_structure_get_string (structure, "format"));
}

static void
_update_using_tags (ClapperVideoStream *self, GstTagList *tags)
{
  ClapperStream *stream = CLAPPER_STREAM_CAST (self);
  gchar *codec = NULL;
  guint bitrate = 0;

  gst_tag_list_get_string_index (tags, GST_TAG_VIDEO_CODEC, 0, &codec);
  clapper_stream_take_string_prop (stream, param_specs[PROP_CODEC], &self->codec, codec);

  gst_tag_list_get_uint_index (tags, GST_TAG_BITRATE, 0, &bitrate);
  clapper_stream_set_uint_prop (stream, param_specs[PROP_BITRATE], &self->bitrate, bitrate);
}

ClapperStream *
clapper_video_stream_new (GstStream *gst_stream)
{
  ClapperVideoStream *video_stream;

  video_stream = g_object_new (CLAPPER_TYPE_VIDEO_STREAM,
      "stream-type", CLAPPER_STREAM_TYPE_VIDEO, NULL);
  gst_object_ref_sink (video_stream);

  clapper_stream_set_gst_stream (CLAPPER_STREAM_CAST (video_stream), gst_stream);

  return CLAPPER_STREAM_CAST (video_stream);
}

/**
 * clapper_video_stream_get_codec:
 * @stream: a #ClapperVideoStream
 *
 * Get codec used to encode @stream.
 *
 * Returns: (transfer full) (nullable): the video codec of stream
 *   or %NULL if undetermined.
 */
gchar *
clapper_video_stream_get_codec (ClapperVideoStream *self)
{
  gchar *codec;

  g_return_val_if_fail (CLAPPER_IS_VIDEO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  codec = g_strdup (self->codec);
  GST_OBJECT_UNLOCK (self);

  return codec;
}

/**
 * clapper_video_stream_get_width:
 * @stream: a #ClapperVideoStream
 *
 * Get width of video @stream.
 *
 * Returns: the width of video stream.
 */
gint
clapper_video_stream_get_width (ClapperVideoStream *self)
{
  gint width;

  g_return_val_if_fail (CLAPPER_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  width = self->width;
  GST_OBJECT_UNLOCK (self);

  return width;
}

/**
 * clapper_video_stream_get_height:
 * @stream: a #ClapperVideoStream
 *
 * Get height of video @stream.
 *
 * Returns: the height of video stream.
 */
gint
clapper_video_stream_get_height (ClapperVideoStream *self)
{
  gint height;

  g_return_val_if_fail (CLAPPER_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  height = self->height;
  GST_OBJECT_UNLOCK (self);

  return height;
}

/**
 * clapper_video_stream_get_fps:
 * @stream: a #ClapperVideoStream
 *
 * Get number of frames per second in video @stream.
 *
 * Returns: the FPS of video stream.
 */
gdouble
clapper_video_stream_get_fps (ClapperVideoStream *self)
{
  gdouble fps;

  g_return_val_if_fail (CLAPPER_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  fps = self->fps;
  GST_OBJECT_UNLOCK (self);

  return fps;
}

/**
 * clapper_video_stream_get_bitrate:
 * @stream: a #ClapperVideoStream
 *
 * Get bitrate of video @stream.
 *
 * Returns: the bitrate of video stream.
 */
guint
clapper_video_stream_get_bitrate (ClapperVideoStream *self)
{
  guint bitrate;

  g_return_val_if_fail (CLAPPER_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  bitrate = self->bitrate;
  GST_OBJECT_UNLOCK (self);

  return bitrate;
}

/**
 * clapper_video_stream_get_pixel_format:
 * @stream: a #ClapperVideoStream
 *
 * Get pixel format of video @stream.
 *
 * Returns: (transfer full) (nullable): the pixel format of stream
 *   or %NULL if undetermined.
 */
gchar *
clapper_video_stream_get_pixel_format (ClapperVideoStream *self)
{
  gchar *pixel_format;

  g_return_val_if_fail (CLAPPER_IS_VIDEO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  pixel_format = g_strdup (self->pixel_format);
  GST_OBJECT_UNLOCK (self);

  return pixel_format;
}

static void
clapper_video_stream_init (ClapperVideoStream *self)
{
}

static void
clapper_video_stream_finalize (GObject *object)
{
  ClapperVideoStream *self = CLAPPER_VIDEO_STREAM_CAST (object);

  g_free (self->codec);
  g_free (self->pixel_format);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_video_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperVideoStream *self = CLAPPER_VIDEO_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_CODEC:
      g_value_take_string (value, clapper_video_stream_get_codec (self));
      break;
    case PROP_WIDTH:
      g_value_set_int (value, clapper_video_stream_get_width (self));
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, clapper_video_stream_get_height (self));
      break;
    case PROP_FPS:
      g_value_set_double (value, clapper_video_stream_get_fps (self));
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, clapper_video_stream_get_bitrate (self));
      break;
    case PROP_PIXEL_FORMAT:
      g_value_take_string (value, clapper_video_stream_get_pixel_format (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_video_stream_internal_stream_updated (ClapperStream *stream,
    GstCaps *caps, GstTagList *tags)
{
  ClapperVideoStream *self = CLAPPER_VIDEO_STREAM_CAST (stream);

  CLAPPER_STREAM_CLASS (parent_class)->internal_stream_updated (stream, caps, tags);

  if (caps)
    _update_using_caps (self, caps);
  if (tags)
    _update_using_tags (self, tags);
}

static void
clapper_video_stream_class_init (ClapperVideoStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperStreamClass *stream_class = (ClapperStreamClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappervideostream", 0,
      "Clapper Video Stream");

  gobject_class->get_property = clapper_video_stream_get_property;
  gobject_class->finalize = clapper_video_stream_finalize;

  stream_class->internal_stream_updated = clapper_video_stream_internal_stream_updated;

  /**
   * ClapperVideoStream:codec:
   *
   * Stream codec.
   */
  param_specs[PROP_CODEC] = g_param_spec_string ("codec",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperVideoStream:width:
   *
   * Stream width.
   */
  param_specs[PROP_WIDTH] = g_param_spec_int ("width",
      NULL, NULL, 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperVideoStream:height:
   *
   * Stream height.
   */
  param_specs[PROP_HEIGHT] = g_param_spec_int ("height",
      NULL, NULL, 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperVideoStream:fps:
   *
   * Stream FPS.
   */
  param_specs[PROP_FPS] = g_param_spec_double ("fps",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperVideoStream:bitrate:
   *
   * Stream bitrate.
   */
  param_specs[PROP_BITRATE] = g_param_spec_uint ("bitrate",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperVideoStream:pixel-format:
   *
   * Stream pixel format.
   */
  param_specs[PROP_PIXEL_FORMAT] = g_param_spec_string ("pixel-format",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
