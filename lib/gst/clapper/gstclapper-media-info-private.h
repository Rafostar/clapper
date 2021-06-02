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

#include "gstclapper-media-info.h"

#ifndef __GST_CLAPPER_MEDIA_INFO_PRIVATE_H__
#define __GST_CLAPPER_MEDIA_INFO_PRIVATE_H__

struct _GstClapperStreamInfo
{
  GObject parent;

  gchar *codec;

  GstCaps *caps;
  gint stream_index;
  GstTagList  *tags;
  gchar *stream_id;
};

struct _GstClapperStreamInfoClass
{
  GObjectClass parent_class;
};

struct _GstClapperSubtitleInfo
{
  GstClapperStreamInfo  parent;

  gchar *title;
  gchar *language;
};

struct _GstClapperSubtitleInfoClass
{
  GstClapperStreamInfoClass parent_class;
};

struct _GstClapperAudioInfo
{
  GstClapperStreamInfo  parent;

  gint channels;
  gint sample_rate;

  guint bitrate;
  guint max_bitrate;

  gchar *language;
};

struct _GstClapperAudioInfoClass
{
  GstClapperStreamInfoClass parent_class;
};

struct _GstClapperVideoInfo
{
  GstClapperStreamInfo  parent;

  gint width;
  gint height;
  gint framerate_num;
  gint framerate_denom;
  gint par_num;
  gint par_denom;

  guint bitrate;
  guint max_bitrate;
};

struct _GstClapperVideoInfoClass
{
  GstClapperStreamInfoClass parent_class;
};

struct _GstClapperMediaInfo
{
  GObject parent;

  gchar *uri;
  gchar *title;
  gchar *container;
  gboolean seekable, is_live;
  GstTagList *tags;
  GstToc *toc;
  GstSample *image_sample;

  GList *stream_list;
  GList *audio_stream_list;
  GList *video_stream_list;
  GList *subtitle_stream_list;

  GstClockTime duration;
};

struct _GstClapperMediaInfoClass
{
  GObjectClass parent_class;
};

G_GNUC_INTERNAL GstClapperMediaInfo *   gst_clapper_media_info_new  (const gchar *uri);
G_GNUC_INTERNAL GstClapperMediaInfo *   gst_clapper_media_info_copy  (GstClapperMediaInfo *ref);
G_GNUC_INTERNAL GstClapperStreamInfo *  gst_clapper_stream_info_new  (gint stream_index, GType type);
G_GNUC_INTERNAL GstClapperStreamInfo *  gst_clapper_stream_info_copy (GstClapperStreamInfo *ref);

#endif /* __GST_CLAPPER_MEDIA_INFO_PRIVATE_H__ */
