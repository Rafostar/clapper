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

#ifndef __GTK_GST_BASE_WIDGET_H__
#define __GTK_GST_BASE_WIDGET_H__

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

GType gtk_gst_base_widget_get_type (void);
#define GTK_TYPE_GST_BASE_WIDGET            (gtk_gst_base_widget_get_type())
#define GTK_GST_BASE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GTK_TYPE_GST_BASE_WIDGET,GtkGstBaseWidget))
#define GTK_GST_BASE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GTK_TYPE_GST_BASE_WIDGET,GtkGstBaseWidgetClass))
#define GTK_IS_GST_BASE_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GTK_TYPE_GST_BASE_WIDGET))
#define GTK_IS_GST_BASE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GTK_TYPE_GST_BASE_WIDGET))
#define GTK_GST_BASE_WIDGET_CAST(obj)       ((GtkGstBaseWidget*)(obj))
#define GTK_GST_BASE_WIDGET_LOCK(w)         g_mutex_lock(&((GtkGstBaseWidget*)(w))->lock)
#define GTK_GST_BASE_WIDGET_UNLOCK(w)       g_mutex_unlock(&((GtkGstBaseWidget*)(w))->lock)

typedef struct _GtkGstBaseWidget GtkGstBaseWidget;
typedef struct _GtkGstBaseWidgetClass GtkGstBaseWidgetClass;
typedef struct _GtkGstBaseWidgetPrivate GtkGstBaseWidgetPrivate;

struct _GtkGstBaseWidget
{
  /* <private> */
  GtkGLArea parent;
  GtkGstBaseWidgetPrivate *priv;

  /* properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;
  gboolean ignore_textures;

  gint display_width;
  gint display_height;

  /* Widget dimensions */
  gint scaled_width;
  gint scaled_height;

  gboolean negotiated;
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

struct _GtkGstBaseWidgetClass
{
  GtkGLAreaClass parent_class;
};

/* API */
gboolean        gtk_gst_base_widget_set_format           (GtkGstBaseWidget * widget, GstVideoInfo * v_info);
void            gtk_gst_base_widget_set_buffer           (GtkGstBaseWidget * widget, GstBuffer * buffer);
void            gtk_gst_base_widget_set_element          (GtkGstBaseWidget * widget, GstElement * element);

GtkWidget *     gtk_gst_base_widget_new (void);

gboolean        gtk_gst_base_widget_init_winsys          (GtkGstBaseWidget * widget);
GstGLDisplay *  gtk_gst_base_widget_get_display          (GtkGstBaseWidget * widget);
GstGLContext *  gtk_gst_base_widget_get_context          (GtkGstBaseWidget * widget);
GstGLContext *  gtk_gst_base_widget_get_gtk_context      (GtkGstBaseWidget * widget);

G_END_DECLS

#endif /* __GTK_GST_BASE_WIDGET_H__ */
