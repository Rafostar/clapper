/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2020 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#ifndef __GST_CLAPPER_GL_SINK_H__
#define __GST_CLAPPER_GL_SINK_H__

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

#include "gtkclapperglwidget.h"

#define GST_TYPE_CLAPPER_GL_SINK            (gst_clapper_gl_sink_get_type())
#define GST_CLAPPER_GL_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CLAPPER_GL_SINK,GstClapperGLSink))
#define GST_CLAPPER_GL_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CLAPPER_GL_SINK,GstClapperGLClass))
#define GST_CLAPPER_GL_SINK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_GL_SINK, GstClapperGLSinkClass))
#define GST_IS_CLAPPER_GL_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CLAPPER_GL_SINK))
#define GST_IS_CLAPPER_GL_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CLAPPER_GL_SINK))
#define GST_CLAPPER_GL_SINK_CAST(obj)       ((GstClapperGLSink*)(obj))

G_BEGIN_DECLS
typedef struct _GstClapperGLSink GstClapperGLSink;
typedef struct _GstClapperGLSinkClass GstClapperGLSinkClass;

GType gst_clapper_gl_sink_get_type (void);

/**
 * GstClapperGLSink:
 *
 * Opaque #GstClapperGLSink object
 */
struct _GstClapperGLSink
{
  /* <private> */
  GstVideoSink parent;

  GstVideoInfo v_info;

  GtkClapperGLWidget *widget;

  gboolean had_eos;

  /* properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;
  gboolean keep_last_frame;

  gboolean ignore_textures;

  GtkWidget *window;
  gulong widget_destroy_id;
  gulong window_destroy_id;

  GstGLDisplay *display;
  GstGLContext *context;
  GstGLContext *gtk_context;

  GstGLUpload *upload;
  GstBuffer *uploaded_buffer;

  /* read/write with object lock */
  gint display_width, display_height;
};

/**
 * GstClapperGLSinkClass:
 *
 * The #GstClapperGLSinkClass struct only contains private data
 */
struct _GstClapperGLSinkClass
{
  GstVideoSinkClass object_class;

  /* metadata */
  const gchar *window_title;

  /* virtuals */
  GtkWidget* (*create_widget) (void);
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperGLSink, gst_object_unref)

G_END_DECLS

#endif /* __GST_CLAPPER_GL_SINK_H__ */
