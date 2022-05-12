/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "gstgtkutils.h"

#define _IS_FRAME_PREMULTIPLIED(f) (GST_VIDEO_INFO_FLAG_IS_SET (&(f)->info, GST_VIDEO_FLAG_PREMULTIPLIED_ALPHA))

struct invoke_context
{
  GThreadFunc func;
  gpointer data;
  GMutex lock;
  GCond cond;
  gboolean fired;

  gpointer res;
};

static gboolean
gst_gtk_invoke_func (struct invoke_context *info)
{
  g_mutex_lock (&info->lock);
  info->res = info->func (info->data);
  info->fired = TRUE;
  g_cond_signal (&info->cond);
  g_mutex_unlock (&info->lock);

  return G_SOURCE_REMOVE;
}

gpointer
gst_gtk_invoke_on_main (GThreadFunc func, gpointer data)
{
  GMainContext *main_context = g_main_context_default ();
  struct invoke_context info;

  g_mutex_init (&info.lock);
  g_cond_init (&info.cond);
  info.fired = FALSE;
  info.func = func;
  info.data = data;

  g_main_context_invoke (main_context, (GSourceFunc) gst_gtk_invoke_func,
      &info);

  g_mutex_lock (&info.lock);
  while (!info.fired)
    g_cond_wait (&info.cond, &info.lock);
  g_mutex_unlock (&info.lock);

  g_mutex_clear (&info.lock);
  g_cond_clear (&info.cond);

  return info.res;
}

static GdkMemoryFormat
gst_gdk_memory_format_from_frame (GstVideoFrame *frame)
{
  switch (GST_VIDEO_FRAME_FORMAT (frame)) {
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_RGBA64_BE:
      return (_IS_FRAME_PREMULTIPLIED (frame))
          ? GDK_MEMORY_R16G16B16A16_PREMULTIPLIED
          : GDK_MEMORY_R16G16B16A16;
    case GST_VIDEO_FORMAT_RGBA:
      return (_IS_FRAME_PREMULTIPLIED (frame))
          ? GDK_MEMORY_R8G8B8A8_PREMULTIPLIED
          : GDK_MEMORY_R8G8B8A8;
    case GST_VIDEO_FORMAT_BGRA:
      return (_IS_FRAME_PREMULTIPLIED (frame))
          ? GDK_MEMORY_B8G8R8A8_PREMULTIPLIED
          : GDK_MEMORY_B8G8R8A8;
    case GST_VIDEO_FORMAT_ARGB:
      return (_IS_FRAME_PREMULTIPLIED (frame))
          ? GDK_MEMORY_A8R8G8B8_PREMULTIPLIED
          : GDK_MEMORY_A8R8G8B8;
    case GST_VIDEO_FORMAT_ABGR:
      /* GTK is missing premultiplied ABGR support */
      return GDK_MEMORY_A8B8G8R8;
    case GST_VIDEO_FORMAT_RGBx:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GST_VIDEO_FORMAT_BGRx:
      return GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
    case GST_VIDEO_FORMAT_RGB:
      return GDK_MEMORY_R8G8B8;
    case GST_VIDEO_FORMAT_BGR:
      return GDK_MEMORY_B8G8R8;
    default:
      break;
  }

  /* This should never happen as long as above switch statement
   * is updated when new formats are added to caps */
  g_assert_not_reached ();

  /* Fallback format */
  return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
}

GdkTexture *
gst_video_frame_into_gdk_texture (GstVideoFrame *frame)
{
  GdkTexture *texture;
  GBytes *bytes;

  bytes = g_bytes_new_with_free_func (
      GST_VIDEO_FRAME_PLANE_DATA (frame, 0),
      GST_VIDEO_FRAME_HEIGHT (frame) * GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0),
      (GDestroyNotify) gst_buffer_unref,
      gst_buffer_ref (frame->buffer));

  texture = gdk_memory_texture_new (
      GST_VIDEO_FRAME_WIDTH (frame),
      GST_VIDEO_FRAME_HEIGHT (frame),
      gst_gdk_memory_format_from_frame (frame),
      bytes,
      GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0));

  g_bytes_unref (bytes);

  return texture;
}
