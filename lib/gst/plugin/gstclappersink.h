/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
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

#include "gstclapperpaintable.h"
#include "gstclapperimporterloader.h"
#include "gstclapperimporter.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_SINK (gst_clapper_sink_get_type())
G_DECLARE_FINAL_TYPE (GstClapperSink, gst_clapper_sink, GST, CLAPPER_SINK, GstVideoSink)

#define GST_CLAPPER_SINK_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_SINK, GstClapperSinkClass))
#define GST_CLAPPER_SINK_CAST(obj)          ((GstClapperSink *)(obj))

#define GST_CLAPPER_SINK_GET_LOCK(obj)      (&GST_CLAPPER_SINK_CAST(obj)->lock)
#define GST_CLAPPER_SINK_LOCK(obj)          g_mutex_lock (GST_CLAPPER_SINK_GET_LOCK(obj))
#define GST_CLAPPER_SINK_UNLOCK(obj)        g_mutex_unlock (GST_CLAPPER_SINK_GET_LOCK(obj))

struct _GstClapperSink
{
  GstVideoSink parent;

  GMutex lock;

  GstClapperPaintable *paintable;
  GstClapperImporterLoader *loader;
  GstClapperImporter *importer;
  GstVideoInfo v_info;

  GtkWidget *widget;
  GtkWindow *window;

  gboolean presented_window;

  /* Properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;
  gboolean keep_last_frame;

  /* Position coords */
  gdouble last_pos_x;
  gdouble last_pos_y;

  gulong widget_destroy_id;
  gulong window_destroy_id;
};

GST_ELEMENT_REGISTER_DECLARE (clappersink);

G_END_DECLS
