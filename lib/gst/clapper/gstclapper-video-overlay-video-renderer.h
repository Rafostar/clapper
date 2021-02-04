/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_H__
#define __GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_H__

#include <gst/clapper/gstclapper-types.h>
#include <gst/clapper/gstclapper-video-renderer.h>

G_BEGIN_DECLS

typedef struct _GstClapperVideoOverlayVideoRenderer
    GstClapperVideoOverlayVideoRenderer;
typedef struct _GstClapperVideoOverlayVideoRendererClass
    GstClapperVideoOverlayVideoRendererClass;

#define GST_TYPE_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER             (gst_clapper_video_overlay_video_renderer_get_type ())
#define GST_IS_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER))
#define GST_IS_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER))
#define GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER, GstClapperVideoOverlayVideoRendererClass))
#define GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER, GstClapperVideoOverlayVideoRenderer))
#define GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER, GstClapperVideoOverlayVideoRendererClass))
#define GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_CAST(obj)        ((GstClapperVideoOverlayVideoRenderer*)(obj))

GST_CLAPPER_API
GType    gst_clapper_video_overlay_video_renderer_get_type             (void);

GST_CLAPPER_API
GstClapperVideoRenderer *
         gst_clapper_video_overlay_video_renderer_new                  (gpointer window_handle);

GST_CLAPPER_API
GstClapperVideoRenderer *
         gst_clapper_video_overlay_video_renderer_new_with_sink        (gpointer window_handle, GstElement *video_sink);

GST_CLAPPER_API
void     gst_clapper_video_overlay_video_renderer_set_window_handle    (GstClapperVideoOverlayVideoRenderer *self, gpointer window_handle);

GST_CLAPPER_API
gpointer gst_clapper_video_overlay_video_renderer_get_window_handle    (GstClapperVideoOverlayVideoRenderer *self);

GST_CLAPPER_API
void     gst_clapper_video_overlay_video_renderer_expose               (GstClapperVideoOverlayVideoRenderer *self);

GST_CLAPPER_API
void     gst_clapper_video_overlay_video_renderer_set_render_rectangle (GstClapperVideoOverlayVideoRenderer *self, gint x, gint y, gint width, gint height);

GST_CLAPPER_API
void     gst_clapper_video_overlay_video_renderer_get_render_rectangle (GstClapperVideoOverlayVideoRenderer *self, gint *x, gint *y, gint *width, gint *height);

G_END_DECLS

#endif /* __GST_CLAPPER_VIDEO_OVERLAY_VIDEO_RENDERER_H__ */
