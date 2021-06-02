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

#ifndef __GST_CLAPPER_MEDIA_INFO_H__
#define __GST_CLAPPER_MEDIA_INFO_H__

#include <gst/gst.h>
#include <gst/clapper/clapper-prelude.h>

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_STREAM_INFO \
  (gst_clapper_stream_info_get_type ())
#define GST_CLAPPER_STREAM_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLAPPER_STREAM_INFO,GstClapperStreamInfo))
#define GST_CLAPPER_STREAM_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLAPPER_STREAM_INFO,GstClapperStreamInfo))
#define GST_IS_CLAPPER_STREAM_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLAPPER_STREAM_INFO))
#define GST_IS_CLAPPER_STREAM_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLAPPER_STREAM_INFO))

/**
 * GstClapperStreamInfo:
 *
 * Base structure for information concerning a media stream. Depending on
 * the stream type, one can find more media-specific information in
 * #GstClapperVideoInfo, #GstClapperAudioInfo, #GstClapperSubtitleInfo.
 */
typedef struct _GstClapperStreamInfo GstClapperStreamInfo;
typedef struct _GstClapperStreamInfoClass GstClapperStreamInfoClass;

GST_CLAPPER_API
GType         gst_clapper_stream_info_get_type (void);

GST_CLAPPER_API
gint          gst_clapper_stream_info_get_index (const GstClapperStreamInfo *info);

GST_CLAPPER_API
const gchar*  gst_clapper_stream_info_get_stream_type (const GstClapperStreamInfo *info);

GST_CLAPPER_API
GstTagList*   gst_clapper_stream_info_get_tags  (const GstClapperStreamInfo *info);

GST_CLAPPER_API
GstCaps*      gst_clapper_stream_info_get_caps  (const GstClapperStreamInfo *info);

GST_CLAPPER_API
const gchar*  gst_clapper_stream_info_get_codec (const GstClapperStreamInfo *info);

#define GST_TYPE_CLAPPER_VIDEO_INFO \
  (gst_clapper_video_info_get_type ())
#define GST_CLAPPER_VIDEO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLAPPER_VIDEO_INFO, GstClapperVideoInfo))
#define GST_CLAPPER_VIDEO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((obj),GST_TYPE_CLAPPER_VIDEO_INFO, GstClapperVideoInfoClass))
#define GST_IS_CLAPPER_VIDEO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLAPPER_VIDEO_INFO))
#define GST_IS_CLAPPER_VIDEO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((obj),GST_TYPE_CLAPPER_VIDEO_INFO))

/**
 * GstClapperVideoInfo:
 *
 * #GstClapperStreamInfo specific to video streams.
 */
typedef struct _GstClapperVideoInfo GstClapperVideoInfo;
typedef struct _GstClapperVideoInfoClass GstClapperVideoInfoClass;

GST_CLAPPER_API
GType         gst_clapper_video_info_get_type (void);

GST_CLAPPER_API
gint          gst_clapper_video_info_get_bitrate     (const GstClapperVideoInfo * info);

GST_CLAPPER_API
gint          gst_clapper_video_info_get_max_bitrate (const GstClapperVideoInfo * info);

GST_CLAPPER_API
gint          gst_clapper_video_info_get_width       (const GstClapperVideoInfo * info);

GST_CLAPPER_API
gint          gst_clapper_video_info_get_height      (const GstClapperVideoInfo * info);

GST_CLAPPER_API
void          gst_clapper_video_info_get_framerate   (const GstClapperVideoInfo * info,
                                                     gint * fps_n,
                                                     gint * fps_d);

GST_CLAPPER_API
void          gst_clapper_video_info_get_pixel_aspect_ratio (const GstClapperVideoInfo * info,
                                                            guint * par_n,
                                                            guint * par_d);

#define GST_TYPE_CLAPPER_AUDIO_INFO \
  (gst_clapper_audio_info_get_type ())
#define GST_CLAPPER_AUDIO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLAPPER_AUDIO_INFO, GstClapperAudioInfo))
#define GST_CLAPPER_AUDIO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLAPPER_AUDIO_INFO, GstClapperAudioInfoClass))
#define GST_IS_CLAPPER_AUDIO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLAPPER_AUDIO_INFO))
#define GST_IS_CLAPPER_AUDIO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLAPPER_AUDIO_INFO))

/**
 * GstClapperAudioInfo:
 *
 * #GstClapperStreamInfo specific to audio streams.
 */
typedef struct _GstClapperAudioInfo GstClapperAudioInfo;
typedef struct _GstClapperAudioInfoClass GstClapperAudioInfoClass;

GST_CLAPPER_API
GType         gst_clapper_audio_info_get_type (void);

GST_CLAPPER_API
gint          gst_clapper_audio_info_get_channels    (const GstClapperAudioInfo* info);

GST_CLAPPER_API
gint          gst_clapper_audio_info_get_sample_rate (const GstClapperAudioInfo* info);

GST_CLAPPER_API
gint          gst_clapper_audio_info_get_bitrate     (const GstClapperAudioInfo* info);

GST_CLAPPER_API
gint          gst_clapper_audio_info_get_max_bitrate (const GstClapperAudioInfo* info);

GST_CLAPPER_API
const gchar*  gst_clapper_audio_info_get_language    (const GstClapperAudioInfo* info);

#define GST_TYPE_CLAPPER_SUBTITLE_INFO \
  (gst_clapper_subtitle_info_get_type ())
#define GST_CLAPPER_SUBTITLE_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLAPPER_SUBTITLE_INFO, GstClapperSubtitleInfo))
#define GST_CLAPPER_SUBTITLE_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLAPPER_SUBTITLE_INFO,GstClapperSubtitleInfoClass))
#define GST_IS_CLAPPER_SUBTITLE_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLAPPER_SUBTITLE_INFO))
#define GST_IS_CLAPPER_SUBTITLE_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLAPPER_SUBTITLE_INFO))

/**
 * GstClapperSubtitleInfo:
 *
 * #GstClapperStreamInfo specific to subtitle streams.
 */
typedef struct _GstClapperSubtitleInfo GstClapperSubtitleInfo;
typedef struct _GstClapperSubtitleInfoClass GstClapperSubtitleInfoClass;

GST_CLAPPER_API
GType         gst_clapper_subtitle_info_get_type (void);

GST_CLAPPER_API
const gchar * gst_clapper_subtitle_info_get_title    (const GstClapperSubtitleInfo *info);

GST_CLAPPER_API
const gchar * gst_clapper_subtitle_info_get_language (const GstClapperSubtitleInfo *info);

#define GST_TYPE_CLAPPER_MEDIA_INFO \
  (gst_clapper_media_info_get_type())
#define GST_CLAPPER_MEDIA_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLAPPER_MEDIA_INFO,GstClapperMediaInfo))
#define GST_CLAPPER_MEDIA_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLAPPER_MEDIA_INFO,GstClapperMediaInfoClass))
#define GST_IS_CLAPPER_MEDIA_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLAPPER_MEDIA_INFO))
#define GST_IS_CLAPPER_MEDIA_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLAPPER_MEDIA_INFO))

/**
 * GstClapperMediaInfo:
 *
 * Structure containing the media information of a URI.
 */
typedef struct _GstClapperMediaInfo GstClapperMediaInfo;
typedef struct _GstClapperMediaInfoClass GstClapperMediaInfoClass;

GST_CLAPPER_API
GType         gst_clapper_media_info_get_type                       (void);

GST_CLAPPER_API
const gchar * gst_clapper_media_info_get_uri                        (const GstClapperMediaInfo *info);

GST_CLAPPER_API
gboolean      gst_clapper_media_info_is_seekable                    (const GstClapperMediaInfo *info);

GST_CLAPPER_API
gboolean      gst_clapper_media_info_is_live                        (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GstClockTime  gst_clapper_media_info_get_duration                   (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GList *       gst_clapper_media_info_get_stream_list                (const GstClapperMediaInfo *info);

GST_CLAPPER_API
guint         gst_clapper_media_info_get_number_of_streams          (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GList *       gst_clapper_media_info_get_video_streams              (const GstClapperMediaInfo *info);

GST_CLAPPER_API
guint         gst_clapper_media_info_get_number_of_video_streams    (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GList *       gst_clapper_media_info_get_audio_streams              (const GstClapperMediaInfo *info);

GST_CLAPPER_API
guint         gst_clapper_media_info_get_number_of_audio_streams    (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GList *       gst_clapper_media_info_get_subtitle_streams           (const GstClapperMediaInfo *info);

GST_CLAPPER_API
guint         gst_clapper_media_info_get_number_of_subtitle_streams (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GstTagList *  gst_clapper_media_info_get_tags                       (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GstToc *      gst_clapper_media_info_get_toc                        (const GstClapperMediaInfo *info);

GST_CLAPPER_API
const gchar * gst_clapper_media_info_get_title                      (const GstClapperMediaInfo *info);

GST_CLAPPER_API
const gchar * gst_clapper_media_info_get_container_format           (const GstClapperMediaInfo *info);

GST_CLAPPER_API
GstSample *   gst_clapper_media_info_get_image_sample               (const GstClapperMediaInfo *info);

G_END_DECLS

#endif /* __GST_CLAPPER_MEDIA_INFO_H */
