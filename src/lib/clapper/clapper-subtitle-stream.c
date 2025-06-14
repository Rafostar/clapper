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
 * ClapperSubtitleStream:
 *
 * Represents a subtitle stream within media.
 */

#include <gst/tag/tag.h>

#include "clapper-subtitle-stream-private.h"
#include "clapper-stream-private.h"

#define GST_CAT_DEFAULT clapper_subtitle_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperSubtitleStream
{
  ClapperStream parent;

  gchar *lang_code;
  gchar *lang_name;
};

#define parent_class clapper_subtitle_stream_parent_class
G_DEFINE_TYPE (ClapperSubtitleStream, clapper_subtitle_stream, CLAPPER_TYPE_STREAM);

enum
{
  PROP_0,
  PROP_LANG_CODE,
  PROP_LANG_NAME,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_update_using_tags (ClapperSubtitleStream *self, GstTagList *tags)
{
  ClapperStream *stream = CLAPPER_STREAM_CAST (self);
  gchar *lang_code = NULL, *lang_name = NULL;

  /* Prefer code (and name from it), fallback to extracted name */
  if (!gst_tag_list_get_string_index (tags, GST_TAG_LANGUAGE_CODE, 0, &lang_code))
    gst_tag_list_get_string_index (tags, GST_TAG_LANGUAGE_NAME, 0, &lang_name);

  clapper_stream_take_string_prop (stream, param_specs[PROP_LANG_CODE], &self->lang_code, lang_code);
  clapper_stream_take_string_prop (stream, param_specs[PROP_LANG_NAME], &self->lang_name, lang_name);
}

ClapperStream *
clapper_subtitle_stream_new (GstStream *gst_stream)
{
  ClapperSubtitleStream *subtitle_stream;

  subtitle_stream = g_object_new (CLAPPER_TYPE_SUBTITLE_STREAM,
      "stream-type", CLAPPER_STREAM_TYPE_SUBTITLE, NULL);
  gst_object_ref_sink (subtitle_stream);

  clapper_stream_set_gst_stream (CLAPPER_STREAM_CAST (subtitle_stream), gst_stream);

  return CLAPPER_STREAM_CAST (subtitle_stream);
}

/**
 * clapper_subtitle_stream_get_lang_code:
 * @stream: a #ClapperSubtitleStream
 *
 * Get an ISO-639 language code of the @stream.
 *
 * Returns: (transfer full) (nullable): the language code of subtitle stream.
 */
gchar *
clapper_subtitle_stream_get_lang_code (ClapperSubtitleStream *self)
{
  gchar *lang_code;

  g_return_val_if_fail (CLAPPER_IS_SUBTITLE_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  lang_code = g_strdup (self->lang_code);
  GST_OBJECT_UNLOCK (self);

  return lang_code;
}

/**
 * clapper_subtitle_stream_get_lang_name:
 * @stream: a #ClapperSubtitleStream
 *
 * Get language name of the @stream.
 *
 * This function will try to return a translated string into current
 * locale if possible, with a fallback to a name extracted from tags.
 *
 * Returns: (transfer full) (nullable): the language name of subtitle stream.
 */
gchar *
clapper_subtitle_stream_get_lang_name (ClapperSubtitleStream *self)
{
  gchar *lang_name = NULL;

  g_return_val_if_fail (CLAPPER_IS_SUBTITLE_STREAM (self), NULL);

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
clapper_subtitle_stream_init (ClapperSubtitleStream *self)
{
}

static void
clapper_subtitle_stream_finalize (GObject *object)
{
  ClapperSubtitleStream *self = CLAPPER_SUBTITLE_STREAM_CAST (object);

  g_free (self->lang_code);
  g_free (self->lang_name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_subtitle_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperSubtitleStream *self = CLAPPER_SUBTITLE_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_LANG_CODE:
      g_value_take_string (value, clapper_subtitle_stream_get_lang_code (self));
      break;
    case PROP_LANG_NAME:
      g_value_take_string (value, clapper_subtitle_stream_get_lang_name (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_subtitle_stream_internal_stream_updated (ClapperStream *stream,
    GstCaps *caps, GstTagList *tags)
{
  ClapperSubtitleStream *self = CLAPPER_SUBTITLE_STREAM_CAST (stream);

  CLAPPER_STREAM_CLASS (parent_class)->internal_stream_updated (stream, caps, tags);

  if (tags)
    _update_using_tags (self, tags);
}

static void
clapper_subtitle_stream_class_init (ClapperSubtitleStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperStreamClass *stream_class = (ClapperStreamClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappersubtitlestream", 0,
      "Clapper Subtitle Stream");

  gobject_class->get_property = clapper_subtitle_stream_get_property;
  gobject_class->finalize = clapper_subtitle_stream_finalize;

  stream_class->internal_stream_updated = clapper_subtitle_stream_internal_stream_updated;

  /**
   * ClapperSubtitleStream:lang-code:
   *
   * Stream language code in ISO-639 format.
   */
  param_specs[PROP_LANG_CODE] = g_param_spec_string ("lang-code",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperSubtitleStream:lang-name:
   *
   * Stream language name.
   */
  param_specs[PROP_LANG_NAME] = g_param_spec_string ("lang-name",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
