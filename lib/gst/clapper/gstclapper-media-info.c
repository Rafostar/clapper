/* GStreamer
 *
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstclapper-mediainfo
 * @title: GstClapperMediaInfo
 * @short_description: Clapper Media Information
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapper-media-info.h"
#include "gstclapper-media-info-private.h"

/* Per-stream information */
G_DEFINE_ABSTRACT_TYPE (GstClapperStreamInfo, gst_clapper_stream_info,
    G_TYPE_OBJECT);

static void
gst_clapper_stream_info_init (GstClapperStreamInfo * sinfo)
{
  sinfo->stream_index = -1;
}

static void
gst_clapper_stream_info_finalize (GObject * object)
{
  GstClapperStreamInfo *sinfo = GST_CLAPPER_STREAM_INFO (object);

  g_free (sinfo->codec);
  g_free (sinfo->stream_id);

  if (sinfo->caps)
    gst_caps_unref (sinfo->caps);

  if (sinfo->tags)
    gst_tag_list_unref (sinfo->tags);

  G_OBJECT_CLASS (gst_clapper_stream_info_parent_class)->finalize (object);
}

static void
gst_clapper_stream_info_class_init (GstClapperStreamInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_clapper_stream_info_finalize;
}

/**
 * gst_clapper_stream_info_get_index:
 * @info: a #GstClapperStreamInfo
 *
 * Function to get stream index from #GstClapperStreamInfo instance.
 *
 * Returns: the stream index of this stream.
 */
gint
gst_clapper_stream_info_get_index (const GstClapperStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_STREAM_INFO (info), -1);

  return info->stream_index;
}

/**
 * gst_clapper_stream_info_get_stream_type:
 * @info: a #GstClapperStreamInfo
 *
 * Function to return human readable name for the stream type
 * of the given @info (ex: "audio", "video", "subtitle")
 *
 * Returns: a human readable name
 */
const gchar *
gst_clapper_stream_info_get_stream_type (const GstClapperStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_STREAM_INFO (info), NULL);

  if (GST_IS_CLAPPER_VIDEO_INFO (info))
    return "video";
  else if (GST_IS_CLAPPER_AUDIO_INFO (info))
    return "audio";
  else
    return "subtitle";
}

/**
 * gst_clapper_stream_info_get_tags:
 * @info: a #GstClapperStreamInfo
 *
 * Returns: (transfer none): the tags contained in this stream.
 */
GstTagList *
gst_clapper_stream_info_get_tags (const GstClapperStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_STREAM_INFO (info), NULL);

  return info->tags;
}

/**
 * gst_clapper_stream_info_get_codec:
 * @info: a #GstClapperStreamInfo
 *
 * A string describing codec used in #GstClapperStreamInfo.
 *
 * Returns: codec string or NULL on unknown.
 */
const gchar *
gst_clapper_stream_info_get_codec (const GstClapperStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_STREAM_INFO (info), NULL);

  return info->codec;
}

/**
 * gst_clapper_stream_info_get_caps:
 * @info: a #GstClapperStreamInfo
 *
 * Returns: (transfer none): the #GstCaps of the stream.
 */
GstCaps *
gst_clapper_stream_info_get_caps (const GstClapperStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_STREAM_INFO (info), NULL);

  return info->caps;
}

/* Video information */
G_DEFINE_TYPE (GstClapperVideoInfo, gst_clapper_video_info,
    GST_TYPE_CLAPPER_STREAM_INFO);

static void
gst_clapper_video_info_init (GstClapperVideoInfo * info)
{
  info->width = -1;
  info->height = -1;
  info->framerate_num = 0;
  info->framerate_denom = 1;
  info->par_num = 1;
  info->par_denom = 1;
}

static void
gst_clapper_video_info_class_init (G_GNUC_UNUSED GstClapperVideoInfoClass * klass)
{
  /* nothing to do here */
}

/**
 * gst_clapper_video_info_get_width:
 * @info: a #GstClapperVideoInfo
 *
 * Returns: the width of video in #GstClapperVideoInfo.
 */
gint
gst_clapper_video_info_get_width (const GstClapperVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_VIDEO_INFO (info), -1);

  return info->width;
}

/**
 * gst_clapper_video_info_get_height:
 * @info: a #GstClapperVideoInfo
 *
 * Returns: the height of video in #GstClapperVideoInfo.
 */
gint
gst_clapper_video_info_get_height (const GstClapperVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_VIDEO_INFO (info), -1);

  return info->height;
}

/**
 * gst_clapper_video_info_get_framerate:
 * @info: a #GstClapperVideoInfo
 * @fps_n: (out): Numerator of frame rate
 * @fps_d: (out): Denominator of frame rate
 *
 */
void
gst_clapper_video_info_get_framerate (const GstClapperVideoInfo * info,
    gint * fps_n, gint * fps_d)
{
  g_return_if_fail (GST_IS_CLAPPER_VIDEO_INFO (info));

  *fps_n = info->framerate_num;
  *fps_d = info->framerate_denom;
}

/**
 * gst_clapper_video_info_get_pixel_aspect_ratio:
 * @info: a #GstClapperVideoInfo
 * @par_n: (out): numerator
 * @par_d: (out): denominator
 *
 * Returns the pixel aspect ratio in @par_n and @par_d
 *
 */
void
gst_clapper_video_info_get_pixel_aspect_ratio (const GstClapperVideoInfo * info,
    guint * par_n, guint * par_d)
{
  g_return_if_fail (GST_IS_CLAPPER_VIDEO_INFO (info));

  *par_n = info->par_num;
  *par_d = info->par_denom;
}

/**
 * gst_clapper_video_info_get_bitrate:
 * @info: a #GstClapperVideoInfo
 *
 * Returns: the current bitrate of video in #GstClapperVideoInfo.
 */
gint
gst_clapper_video_info_get_bitrate (const GstClapperVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_VIDEO_INFO (info), -1);

  return info->bitrate;
}

/**
 * gst_clapper_video_info_get_max_bitrate:
 * @info: a #GstClapperVideoInfo
 *
 * Returns: the maximum bitrate of video in #GstClapperVideoInfo.
 */
gint
gst_clapper_video_info_get_max_bitrate (const GstClapperVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_VIDEO_INFO (info), -1);

  return info->max_bitrate;
}

/* Audio information */
G_DEFINE_TYPE (GstClapperAudioInfo, gst_clapper_audio_info,
    GST_TYPE_CLAPPER_STREAM_INFO);

static void
gst_clapper_audio_info_init (GstClapperAudioInfo * info)
{
  info->channels = 0;
  info->sample_rate = 0;
  info->bitrate = -1;
  info->max_bitrate = -1;
}

static void
gst_clapper_audio_info_finalize (GObject * object)
{
  GstClapperAudioInfo *info = GST_CLAPPER_AUDIO_INFO (object);

  g_free (info->language);

  G_OBJECT_CLASS (gst_clapper_audio_info_parent_class)->finalize (object);
}

static void
gst_clapper_audio_info_class_init (GstClapperAudioInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_clapper_audio_info_finalize;
}

/**
 * gst_clapper_audio_info_get_language:
 * @info: a #GstClapperAudioInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
const gchar *
gst_clapper_audio_info_get_language (const GstClapperAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_AUDIO_INFO (info), NULL);

  return info->language;
}

/**
 * gst_clapper_audio_info_get_channels:
 * @info: a #GstClapperAudioInfo
 *
 * Returns: the number of audio channels in #GstClapperAudioInfo.
 */
gint
gst_clapper_audio_info_get_channels (const GstClapperAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_AUDIO_INFO (info), 0);

  return info->channels;
}

/**
 * gst_clapper_audio_info_get_sample_rate:
 * @info: a #GstClapperAudioInfo
 *
 * Returns: the audio sample rate in #GstClapperAudioInfo.
 */
gint
gst_clapper_audio_info_get_sample_rate (const GstClapperAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_AUDIO_INFO (info), 0);

  return info->sample_rate;
}

/**
 * gst_clapper_audio_info_get_bitrate:
 * @info: a #GstClapperAudioInfo
 *
 * Returns: the audio bitrate in #GstClapperAudioInfo.
 */
gint
gst_clapper_audio_info_get_bitrate (const GstClapperAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_AUDIO_INFO (info), -1);

  return info->bitrate;
}

/**
 * gst_clapper_audio_info_get_max_bitrate:
 * @info: a #GstClapperAudioInfo
 *
 * Returns: the audio maximum bitrate in #GstClapperAudioInfo.
 */
gint
gst_clapper_audio_info_get_max_bitrate (const GstClapperAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_AUDIO_INFO (info), -1);

  return info->max_bitrate;
}

/* Subtitle information */
G_DEFINE_TYPE (GstClapperSubtitleInfo, gst_clapper_subtitle_info,
    GST_TYPE_CLAPPER_STREAM_INFO);

static void
gst_clapper_subtitle_info_init (G_GNUC_UNUSED GstClapperSubtitleInfo * info)
{
  /* nothing to do */
}

static void
gst_clapper_subtitle_info_finalize (GObject * object)
{
  GstClapperSubtitleInfo *info = GST_CLAPPER_SUBTITLE_INFO (object);

  g_free (info->title);
  g_free (info->language);

  G_OBJECT_CLASS (gst_clapper_subtitle_info_parent_class)->finalize (object);
}

static void
gst_clapper_subtitle_info_class_init (GstClapperSubtitleInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_clapper_subtitle_info_finalize;
}

/**
 * gst_clapper_subtitle_info_get_title:
 * @info: a #GstClapperSubtitleInfo
 *
 * Returns: the title of the stream, or NULL if unknown.
 */
const gchar *
gst_clapper_subtitle_info_get_title (const GstClapperSubtitleInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_SUBTITLE_INFO (info), NULL);

  return info->title;
}

/**
 * gst_clapper_subtitle_info_get_language:
 * @info: a #GstClapperSubtitleInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
const gchar *
gst_clapper_subtitle_info_get_language (const GstClapperSubtitleInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_SUBTITLE_INFO (info), NULL);

  return info->language;
}

/* Global media information */
G_DEFINE_TYPE (GstClapperMediaInfo, gst_clapper_media_info, G_TYPE_OBJECT);

static void
gst_clapper_media_info_init (GstClapperMediaInfo * info)
{
  info->duration = -1;
  info->is_live = FALSE;
  info->seekable = FALSE;
}

static void
gst_clapper_media_info_finalize (GObject * object)
{
  GstClapperMediaInfo *info = GST_CLAPPER_MEDIA_INFO (object);

  g_free (info->uri);
  g_free (info->title);
  g_free (info->container);

  if (info->tags)
    gst_tag_list_unref (info->tags);
  if (info->toc)
    gst_toc_unref (info->toc);
  if (info->image_sample)
    gst_sample_unref (info->image_sample);
  if (info->audio_stream_list)
    g_list_free (info->audio_stream_list);
  if (info->video_stream_list)
    g_list_free (info->video_stream_list);
  if (info->subtitle_stream_list)
    g_list_free (info->subtitle_stream_list);
  if (info->stream_list)
    g_list_free_full (info->stream_list, g_object_unref);

  G_OBJECT_CLASS (gst_clapper_media_info_parent_class)->finalize (object);
}

static void
gst_clapper_media_info_class_init (GstClapperMediaInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_clapper_media_info_finalize;
}

static GstClapperVideoInfo *
gst_clapper_video_info_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_VIDEO_INFO, NULL);
}

static GstClapperAudioInfo *
gst_clapper_audio_info_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_AUDIO_INFO, NULL);
}

static GstClapperSubtitleInfo *
gst_clapper_subtitle_info_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_SUBTITLE_INFO, NULL);
}

static GstClapperStreamInfo *
gst_clapper_video_info_copy (GstClapperVideoInfo * ref)
{
  GstClapperVideoInfo *ret;

  ret = gst_clapper_video_info_new ();

  ret->width = ref->width;
  ret->height = ref->height;
  ret->framerate_num = ref->framerate_num;
  ret->framerate_denom = ref->framerate_denom;
  ret->par_num = ref->par_num;
  ret->par_denom = ref->par_denom;
  ret->bitrate = ref->bitrate;
  ret->max_bitrate = ref->max_bitrate;

  return (GstClapperStreamInfo *) ret;
}

static GstClapperStreamInfo *
gst_clapper_audio_info_copy (GstClapperAudioInfo * ref)
{
  GstClapperAudioInfo *ret;

  ret = gst_clapper_audio_info_new ();

  ret->sample_rate = ref->sample_rate;
  ret->channels = ref->channels;
  ret->bitrate = ref->bitrate;
  ret->max_bitrate = ref->max_bitrate;

  if (ref->language)
    ret->language = g_strdup (ref->language);

  return (GstClapperStreamInfo *) ret;
}

static GstClapperStreamInfo *
gst_clapper_subtitle_info_copy (GstClapperSubtitleInfo * ref)
{
  GstClapperSubtitleInfo *ret;

  ret = gst_clapper_subtitle_info_new ();
  if (ref->title)
    ret->title = g_strdup (ref->title);
  if (ref->language)
    ret->language = g_strdup (ref->language);

  return (GstClapperStreamInfo *) ret;
}

GstClapperStreamInfo *
gst_clapper_stream_info_copy (GstClapperStreamInfo * ref)
{
  GstClapperStreamInfo *info = NULL;

  if (!ref)
    return NULL;

  if (GST_IS_CLAPPER_VIDEO_INFO (ref))
    info = gst_clapper_video_info_copy ((GstClapperVideoInfo *) ref);
  else if (GST_IS_CLAPPER_AUDIO_INFO (ref))
    info = gst_clapper_audio_info_copy ((GstClapperAudioInfo *) ref);
  else
    info = gst_clapper_subtitle_info_copy ((GstClapperSubtitleInfo *) ref);

  info->stream_index = ref->stream_index;
  if (ref->tags)
    info->tags = gst_tag_list_ref (ref->tags);
  if (ref->caps)
    info->caps = gst_caps_copy (ref->caps);
  if (ref->codec)
    info->codec = g_strdup (ref->codec);
  if (ref->stream_id)
    info->stream_id = g_strdup (ref->stream_id);

  return info;
}

GstClapperMediaInfo *
gst_clapper_media_info_copy (GstClapperMediaInfo * ref)
{
  GList *l;
  GstClapperMediaInfo *info;

  if (!ref)
    return NULL;

  info = gst_clapper_media_info_new (ref->uri);
  info->duration = ref->duration;
  info->seekable = ref->seekable;
  info->is_live = ref->is_live;
  if (ref->tags)
    info->tags = gst_tag_list_ref (ref->tags);
  if (ref->toc)
    info->toc = gst_toc_ref (ref->toc);
  if (ref->title)
    info->title = g_strdup (ref->title);
  if (ref->container)
    info->container = g_strdup (ref->container);
  if (ref->image_sample)
    info->image_sample = gst_sample_ref (ref->image_sample);

  for (l = ref->stream_list; l != NULL; l = l->next) {
    GstClapperStreamInfo *s;

    s = gst_clapper_stream_info_copy ((GstClapperStreamInfo *) l->data);
    info->stream_list = g_list_append (info->stream_list, s);

    if (GST_IS_CLAPPER_AUDIO_INFO (s))
      info->audio_stream_list = g_list_append (info->audio_stream_list, s);
    else if (GST_IS_CLAPPER_VIDEO_INFO (s))
      info->video_stream_list = g_list_append (info->video_stream_list, s);
    else
      info->subtitle_stream_list =
          g_list_append (info->subtitle_stream_list, s);
  }

  return info;
}

GstClapperStreamInfo *
gst_clapper_stream_info_new (gint stream_index, GType type)
{
  GstClapperStreamInfo *info = NULL;

  if (type == GST_TYPE_CLAPPER_AUDIO_INFO)
    info = (GstClapperStreamInfo *) gst_clapper_audio_info_new ();
  else if (type == GST_TYPE_CLAPPER_VIDEO_INFO)
    info = (GstClapperStreamInfo *) gst_clapper_video_info_new ();
  else
    info = (GstClapperStreamInfo *) gst_clapper_subtitle_info_new ();

  info->stream_index = stream_index;

  return info;
}

GstClapperMediaInfo *
gst_clapper_media_info_new (const gchar * uri)
{
  GstClapperMediaInfo *info;

  g_return_val_if_fail (uri != NULL, NULL);

  info = g_object_new (GST_TYPE_CLAPPER_MEDIA_INFO, NULL);
  info->uri = g_strdup (uri);

  return info;
}

/**
 * gst_clapper_media_info_get_uri:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: the URI associated with #GstClapperMediaInfo.
 */
const gchar *
gst_clapper_media_info_get_uri (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->uri;
}

/**
 * gst_clapper_media_info_is_seekable:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: %TRUE if the media is seekable.
 */
gboolean
gst_clapper_media_info_is_seekable (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), FALSE);

  return info->seekable;
}

/**
 * gst_clapper_media_info_is_live:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: %TRUE if the media is live.
 */
gboolean
gst_clapper_media_info_is_live (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), FALSE);

  return info->is_live;
}

/**
 * gst_clapper_media_info_get_stream_list:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: (transfer none) (element-type GstClapperStreamInfo): A #GList of
 * matching #GstClapperStreamInfo.
 */
GList *
gst_clapper_media_info_get_stream_list (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->stream_list;
}

/**
 * gst_clapper_media_info_get_video_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: (transfer none) (element-type GstClapperVideoInfo): A #GList of
 * matching #GstClapperVideoInfo.
 */
GList *
gst_clapper_media_info_get_video_streams (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->video_stream_list;
}

/**
 * gst_clapper_media_info_get_subtitle_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: (transfer none) (element-type GstClapperSubtitleInfo): A #GList of
 * matching #GstClapperSubtitleInfo.
 */
GList *
gst_clapper_media_info_get_subtitle_streams (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->subtitle_stream_list;
}

/**
 * gst_clapper_media_info_get_audio_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: (transfer none) (element-type GstClapperAudioInfo): A #GList of
 * matching #GstClapperAudioInfo.
 */
GList *
gst_clapper_media_info_get_audio_streams (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->audio_stream_list;
}

/**
 * gst_clapper_media_info_get_duration:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: duration of the media.
 */
GstClockTime
gst_clapper_media_info_get_duration (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), -1);

  return info->duration;
}

/**
 * gst_clapper_media_info_get_tags:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: (transfer none): the tags contained in media info.
 */
GstTagList *
gst_clapper_media_info_get_tags (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->tags;
}

/**
 * gst_clapper_media_info_get_toc:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: (transfer none): the toc contained in media info.
 */
GstToc *
gst_clapper_media_info_get_toc (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->toc;
}

/**
 * gst_clapper_media_info_get_title:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: the media title. When metadata does not contain title,
 * returns title parsed from URI.
 */
const gchar *
gst_clapper_media_info_get_title (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->title;
}

/**
 * gst_clapper_media_info_get_container_format:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: the container format.
 */
const gchar *
gst_clapper_media_info_get_container_format (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->container;
}

/**
 * gst_clapper_media_info_get_image_sample:
 * @info: a #GstClapperMediaInfo
 *
 * Function to get the image (or preview-image) stored in taglist.
 * Application can use `gst_sample_*_()` API's to get caps, buffer etc.
 *
 * Returns: (transfer none): GstSample or NULL.
 */
GstSample *
gst_clapper_media_info_get_image_sample (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), NULL);

  return info->image_sample;
}

/**
 * gst_clapper_media_info_get_number_of_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: number of total streams.
 */
guint
gst_clapper_media_info_get_number_of_streams (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), 0);

  return g_list_length (info->stream_list);
}

/**
 * gst_clapper_media_info_get_number_of_video_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: number of video streams.
 */
guint
gst_clapper_media_info_get_number_of_video_streams (const GstClapperMediaInfo *
    info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), 0);

  return g_list_length (info->video_stream_list);
}

/**
 * gst_clapper_media_info_get_number_of_audio_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: number of audio streams.
 */
guint
gst_clapper_media_info_get_number_of_audio_streams (const GstClapperMediaInfo *
    info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), 0);

  return g_list_length (info->audio_stream_list);
}

/**
 * gst_clapper_media_info_get_number_of_subtitle_streams:
 * @info: a #GstClapperMediaInfo
 *
 * Returns: number of subtitle streams.
 */
guint gst_clapper_media_info_get_number_of_subtitle_streams
    (const GstClapperMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_CLAPPER_MEDIA_INFO (info), 0);

  return g_list_length (info->subtitle_stream_list);
}
