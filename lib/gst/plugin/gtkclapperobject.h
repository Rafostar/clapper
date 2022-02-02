/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2020-2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GTK_TYPE_CLAPPER_OBJECT            (gtk_clapper_object_get_type ())
#define GTK_IS_CLAPPER_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CLAPPER_OBJECT))
#define GTK_IS_CLAPPER_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CLAPPER_OBJECT))
#define GTK_CLAPPER_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CLAPPER_OBJECT, GtkClapperObjectClass))
#define GTK_CLAPPER_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CLAPPER_OBJECT, GtkClapperObjectClass))
#define GTK_CLAPPER_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CLAPPER_OBJECT, GtkClapperObject))
#define GTK_CLAPPER_OBJECT_CAST(obj)       ((GtkClapperObject*)(obj))

#define GTK_CLAPPER_OBJECT_LOCK(w)         g_mutex_lock(&((GtkClapperObject*)(w))->lock)
#define GTK_CLAPPER_OBJECT_UNLOCK(w)       g_mutex_unlock(&((GtkClapperObject*)(w))->lock)

typedef struct _GtkClapperObject GtkClapperObject;
typedef struct _GtkClapperObjectClass GtkClapperObjectClass;

struct _GtkClapperObject
{
  GObject parent;

  GtkPicture *picture;
  GdkPaintable *paintable;

  GstGLDisplay *display;
  GdkGLContext *gdk_context;
  GstGLContext *wrapped_context;

  /* properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;
  gboolean keep_last_frame;

  gint display_width;
  gint display_height;
  gdouble display_aspect_ratio;

  /* Object dimensions */
  gint scaled_width;
  gint scaled_height;

  /* Position coords */
  gdouble last_pos_x;
  gdouble last_pos_y;

  gboolean negotiated;
  gboolean ignore_buffers;

  GstBuffer *pending_buffer;
  GstBuffer *buffer;

  GstVideoInfo pending_v_info;
  GstVideoInfo v_info;

  guint texture_id;

  /* resize */
  gboolean pending_resize;
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

struct _GtkClapperObjectClass
{
  GObjectClass parent_class;
};

GType              gtk_clapper_object_get_type             (void);
GtkClapperObject * gtk_clapper_object_new                  (void);
GtkWidget *        gtk_clapper_object_get_widget           (GtkClapperObject *object);

gboolean           gtk_clapper_object_init_winsys          (GtkClapperObject *object);

gboolean           gtk_clapper_object_set_format           (GtkClapperObject *object, GstVideoInfo *v_info);
void               gtk_clapper_object_set_buffer           (GtkClapperObject *object, GstBuffer *buffer);
void               gtk_clapper_object_set_element          (GtkClapperObject *object, GstElement *element);

G_END_DECLS
