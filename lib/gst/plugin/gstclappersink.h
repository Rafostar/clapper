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
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include "gtkclapperobject.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_SINK            (gst_clapper_sink_get_type ())
#define GST_IS_CLAPPER_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_SINK))
#define GST_IS_CLAPPER_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_SINK))
#define GST_CLAPPER_SINK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_SINK, GstClapperSinkClass))
#define GST_CLAPPER_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_SINK, GstClapperSinkClass))
#define GST_CLAPPER_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_SINK, GstClapperSink))
#define GST_CLAPPER_SINK_CAST(obj)       ((GstClapperSink*)(obj))

typedef struct _GstClapperSink GstClapperSink;
typedef struct _GstClapperSinkClass GstClapperSinkClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperSink, gst_object_unref)
#endif

struct _GstClapperSink
{
  GstVideoSink parent;

  GstVideoInfo v_info;

  GtkClapperObject *obj;
  GtkWindow *window;

  /* properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;

  gint display_width, display_height;
  gulong widget_destroy_id, window_destroy_id;
};

struct _GstClapperSinkClass
{
  GstVideoSinkClass parent_class;
};

GST_ELEMENT_REGISTER_DECLARE (clappersink);

GType gst_clapper_sink_get_type (void);

G_END_DECLS
