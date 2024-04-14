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
 * ClapperAudioStream:
 *
 * Represents an audio stream within media.
 */

#include <gst/tag/tag.h>

#include "clapper-audio-stream-private.h"
#include "clapper-stream-private.h"

#define GST_CAT_DEFAULT clapper_audio_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAudioStream
{
  ClapperStream parent;

  gchar *codec;
  guint bitrate;
  gchar *sample_format;
  gint sample_rate;
  gint channels;
  gchar *lang_code;
  gchar *lang_name;
};

#define parent_class clapper_audio_stream_parent_class
G_DEFINE_TYPE (ClapperAudioStream, clapper_audio_stream, CLAPPER_TYPE_STREAM);

enum
{
  PROP_0,
  PROP_CODEC,
  PROP_BITRATE,
  PROP_SAMPLE_FORMAT,
  PROP_SAMPLE_RATE,
  PROP_CHANNELS,
  PROP_LANG_CODE,
  PROP_LANG_NAME,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_update_using_caps (ClapperAudioStream *self, GstCaps *caps)
{
  ClapperStream *stream = CLAPPER_STREAM_CAST (self);
  GstStructure *structure;
  gint sample_rate = 0, channels = 0;

  if (gst_caps_get_size (caps) == 0)
    return;

  structure = gst_caps_get_structure (caps, 0);

  clapper_stream_set_string_prop (stream, param_specs[PROP_SAMPLE_FORMAT], &self->sample_format,
      gst_structure_get_string (structure, "format"));

  gst_structure_get_int (structure, "rate", &sample_rate);
  clapper_stream_set_int_prop (stream, param_specs[PROP_SAMPLE_RATE], &self->sample_rate, sample_rate);

  gst_structure_get_int (structure, "channels", &channels);
  clapper_stream_set_int_prop (stream, param_specs[PROP_CHANNELS], &self->channels, channels);
}

static void
_update_using_tags (ClapperAudioStream *self, GstTagList *tags)
{
  ClapperStream *stream = CLAPPER_STREAM_CAST (self);
  gchar *codec = NULL, *lang_code = NULL, *lang_name = NULL;
  guint bitrate = 0;

  gst_tag_list_get_string_index (tags, GST_TAG_AUDIO_CODEC, 0, &codec);
  clapper_stream_take_string_prop (stream, param_specs[PROP_CODEC], &self->codec, codec);

  gst_tag_list_get_uint_index (tags, GST_TAG_BITRATE, 0, &bitrate);
  clapper_stream_set_uint_prop (stream, param_specs[PROP_BITRATE], &self->bitrate, bitrate);

  /* Prefer code (and name from it), fallback to extracted name */
  if (!gst_tag_list_get_string_index (tags, GST_TAG_LANGUAGE_CODE, 0, &lang_code))
    gst_tag_list_get_string_index (tags, GST_TAG_LANGUAGE_NAME, 0, &lang_name);

  clapper_stream_take_string_prop (stream, param_specs[PROP_LANG_CODE], &self->lang_code, lang_code);
  clapper_stream_take_string_prop (stream, param_specs[PROP_LANG_NAME], &self->lang_name, lang_name);
}

ClapperStream *
clapper_audio_stream_new (GstStream *gst_stream)
{
  ClapperAudioStream *audio_stream;

  audio_stream = g_object_new (CLAPPER_TYPE_AUDIO_STREAM,
      "stream-type", CLAPPER_STREAM_TYPE_AUDIO, NULL);
  gst_object_ref_sink (audio_stream);

  clapper_stream_set_gst_stream (CLAPPER_STREAM_CAST (audio_stream), gst_stream);

  return CLAPPER_STREAM_CAST (audio_stream);
}

/**
 * clapper_audio_stream_get_codec:
 * @stream: a #ClapperAudioStream
 *
 * Get codec used to encode @stream.
 *
 * Returns: (transfer full) (nullable): the audio codec of stream
 *   or %NULL if undetermined.
 */
gchar *
clapper_audio_stream_get_codec (ClapperAudioStream *self)
{
  gchar *codec;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  codec = g_strdup (self->codec);
  GST_OBJECT_UNLOCK (self);

  return codec;
}

/**
 * clapper_audio_stream_get_bitrate:
 * @stream: a #ClapperAudioStream
 *
 * Get bitrate of audio @stream.
 *
 * Returns: the bitrate of audio stream.
 */
guint
clapper_audio_stream_get_bitrate (ClapperAudioStream *self)
{
  guint bitrate;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  bitrate = self->bitrate;
  GST_OBJECT_UNLOCK (self);

  return bitrate;
}

/**
 * clapper_audio_stream_get_sample_format:
 * @stream: a #ClapperAudioStream
 *
 * Get sample format of audio @stream.
 *
 * Returns: (transfer full) (nullable): the sample format of stream
 *   or %NULL if undetermined.
 */
gchar *
clapper_audio_stream_get_sample_format (ClapperAudioStream *self)
{
  gchar *sample_format;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  sample_format = g_strdup (self->sample_format);
  GST_OBJECT_UNLOCK (self);

  return sample_format;
}

/**
 * clapper_audio_stream_get_sample_rate:
 * @stream: a #ClapperAudioStream
 *
 * Get sample rate of audio @stream (in Hz).
 *
 * Returns: the sample rate of audio stream.
 */
gint
clapper_audio_stream_get_sample_rate (ClapperAudioStream *self)
{
  gint sample_rate;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  sample_rate = self->sample_rate;
  GST_OBJECT_UNLOCK (self);

  return sample_rate;
}

/**
 * clapper_audio_stream_get_channels:
 * @stream: a #ClapperAudioStream
 *
 * Get number of audio channels in @stream.
 *
 * Returns: the number of audio channels.
 */
gint
clapper_audio_stream_get_channels (ClapperAudioStream *self)
{
  gint channels;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  channels = self->channels;
  GST_OBJECT_UNLOCK (self);

  return channels;
}

/**
 * clapper_audio_stream_get_lang_code:
 * @stream: a #ClapperAudioStream
 *
 * Get an ISO-639 language code of the @stream.
 *
 * Returns: (transfer full) (nullable): the language code of audio stream.
 */
gchar *
clapper_audio_stream_get_lang_code (ClapperAudioStream *self)
{
  gchar *lang_code;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  lang_code = g_strdup (self->lang_code);
  GST_OBJECT_UNLOCK (self);

  return lang_code;
}

/**
 * clapper_audio_stream_get_lang_name:
 * @stream: a #ClapperAudioStream
 *
 * Get language name of the @stream.
 *
 * This function will try to return a translated string into current
 * locale if possible, with a fallback to a name extracted from tags.
 *
 * Returns: (transfer full) (nullable): the language name of audio stream.
 */
gchar *
clapper_audio_stream_get_lang_name (ClapperAudioStream *self)
{
  gchar *lang_name = NULL;

  g_return_val_if_fail (CLAPPER_IS_AUDIO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);

  /* Prefer from code as its translated to user locale,
   * otherwise try to fallback to the one sent in tags */
  if (self->lang_code)
    lang_name = g_strdup (gst_tag_get_language_name (self->lang_code));
  if (!lang_name)
    lang_name = g_strdup (self->lang_name);

  GST_OBJECT_UNLOCK (self);

  return lang_name;
}

static void
clapper_audio_stream_init (ClapperAudioStream *self)
{
}

static void
clapper_audio_stream_finalize (GObject *object)
{
  ClapperAudioStream *self = CLAPPER_AUDIO_STREAM_CAST (object);

  g_free (self->codec);
  g_free (self->sample_format);
  g_free (self->lang_code);
  g_free (self->lang_name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_audio_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAudioStream *self = CLAPPER_AUDIO_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_CODEC:
      g_value_take_string (value, clapper_audio_stream_get_codec (self));
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, clapper_audio_stream_get_bitrate (self));
      break;
    case PROP_SAMPLE_FORMAT:
      g_value_take_string (value, clapper_audio_stream_get_sample_format (self));
      break;
    case PROP_SAMPLE_RATE:
      g_value_set_int (value, clapper_audio_stream_get_sample_rate (self));
      break;
    case PROP_CHANNELS:
      g_value_set_int (value, clapper_audio_stream_get_channels (self));
      break;
    case PROP_LANG_CODE:
      g_value_take_string (value, clapper_audio_stream_get_lang_code (self));
      break;
    case PROP_LANG_NAME:
      g_value_take_string (value, clapper_audio_stream_get_lang_name (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_audio_stream_internal_stream_updated (ClapperStream *stream,
    GstCaps *caps, GstTagList *tags)
{
  ClapperAudioStream *self = CLAPPER_AUDIO_STREAM_CAST (stream);

  CLAPPER_STREAM_CLASS (parent_class)->internal_stream_updated (stream, caps, tags);

  if (caps)
    _update_using_caps (self, caps);
  if (tags)
    _update_using_tags (self, tags);
}

static void
clapper_audio_stream_class_init (ClapperAudioStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperStreamClass *stream_class = (ClapperStreamClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperaudiostream", 0,
      "Clapper Audio Stream");

  gobject_class->get_property = clapper_audio_stream_get_property;
  gobject_class->finalize = clapper_audio_stream_finalize;

  stream_class->internal_stream_updated = clapper_audio_stream_internal_stream_updated;

  /**
   * ClapperAudioStream:codec:
   *
   * Stream codec.
   */
  param_specs[PROP_CODEC] = g_param_spec_string ("codec",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperAudioStream:bitrate:
   *
   * Stream bitrate.
   */
  param_specs[PROP_BITRATE] = g_param_spec_uint ("bitrate",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperAudioStream:sample-format:
   *
   * Stream sample format.
   */
  param_specs[PROP_SAMPLE_FORMAT] = g_param_spec_string ("sample-format",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperAudioStream:sample-rate:
   *
   * Stream sample rate (in Hz).
   */
  param_specs[PROP_SAMPLE_RATE] = g_param_spec_int ("sample-rate",
      NULL, NULL, 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperAudioStream:channels:
   *
   * Stream number of audio channels.
   */
  param_specs[PROP_CHANNELS] = g_param_spec_int ("channels",
      NULL, NULL, 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperAudioStream:lang-code:
   *
   * Stream language code in ISO-639 format.
   */
  param_specs[PROP_LANG_CODE] = g_param_spec_string ("lang-code",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperAudioStream:lang-name:
   *
   * Stream language name.
   */
  param_specs[PROP_LANG_NAME] = g_param_spec_string ("lang-name",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
