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

#ifndef __GTK_CLAPPER_GL_WIDGET_H__
#define __GTK_CLAPPER_GL_WIDGET_H__

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

GType gtk_clapper_gl_widget_get_type (void);
#define GTK_TYPE_CLAPPER_GL_WIDGET            (gtk_clapper_gl_widget_get_type())
#define GTK_CLAPPER_GL_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GTK_TYPE_CLAPPER_GL_WIDGET,GtkClapperGLWidget))
#define GTK_CLAPPER_GL_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GTK_TYPE_CLAPPER_GL_WIDGET,GtkClapperGLWidgetClass))
#define GTK_IS_CLAPPER_GL_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GTK_TYPE_CLAPPER_GL_WIDGET))
#define GTK_IS_CLAPPER_GL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GTK_TYPE_CLAPPER_GL_WIDGET))
#define GTK_CLAPPER_GL_WIDGET_CAST(obj)       ((GtkClapperGLWidget*)(obj))
#define GTK_CLAPPER_GL_WIDGET_LOCK(w)         g_mutex_lock(&((GtkClapperGLWidget*)(w))->lock)
#define GTK_CLAPPER_GL_WIDGET_UNLOCK(w)       g_mutex_unlock(&((GtkClapperGLWidget*)(w))->lock)

typedef struct _GtkClapperGLWidget GtkClapperGLWidget;
typedef struct _GtkClapperGLWidgetClass GtkClapperGLWidgetClass;
typedef struct _GtkClapperGLWidgetPrivate GtkClapperGLWidgetPrivate;

struct _GtkClapperGLWidget
{
  /* <private> */
  GtkGLArea parent;
  GtkClapperGLWidgetPrivate *priv;

  /* properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;
  gboolean keep_last_frame;

  gint display_width;
  gint display_height;

  /* Widget dimensions */
  gint scaled_width;
  gint scaled_height;

  /* Position coords */
  gdouble last_pos_x;
  gdouble last_pos_y;

  gboolean negotiated;
  gboolean ignore_buffers;
  GstBuffer *pending_buffer;
  GstBuffer *buffer;
  GstVideoInfo v_info;

  /* resize */
  gboolean pending_resize;
  GstVideoInfo pending_v_info;
  guint display_ratio_num;
  guint display_ratio_den;

  /*< private >*/
  GMutex lock;
  GWeakRef element;

  /* event controllers */
  GtkEventController *key_controller;
  GtkEventController *motion_controller;
  GtkGesture *click_gesture;

  /* Pending draw idles callback */
  guint draw_id;
};

struct _GtkClapperGLWidgetClass
{
  GtkGLAreaClass parent_class;
};

/* API */
gboolean        gtk_clapper_gl_widget_set_format           (GtkClapperGLWidget * widget, GstVideoInfo * v_info);
void            gtk_clapper_gl_widget_set_buffer           (GtkClapperGLWidget * widget, GstBuffer * buffer);
void            gtk_clapper_gl_widget_set_element          (GtkClapperGLWidget * widget, GstElement * element);

GtkWidget *     gtk_clapper_gl_widget_new                  (void);

gboolean        gtk_clapper_gl_widget_init_winsys          (GtkClapperGLWidget * widget);
GstGLDisplay *  gtk_clapper_gl_widget_get_display          (GtkClapperGLWidget * widget);
GstGLContext *  gtk_clapper_gl_widget_get_context          (GtkClapperGLWidget * widget);
GstGLContext *  gtk_clapper_gl_widget_get_gtk_context      (GtkClapperGLWidget * widget);
gboolean        gtk_clapper_gl_widget_update_output_format (GtkClapperGLWidget * widget, GstCaps * caps);

G_END_DECLS

#endif /* __GTK_CLAPPER_GL_WIDGET_H__ */
