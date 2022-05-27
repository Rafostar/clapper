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
#include <gst/video/video.h>

#include "gstclapperimporter.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_PAINTABLE (gst_clapper_paintable_get_type())
G_DECLARE_FINAL_TYPE (GstClapperPaintable, gst_clapper_paintable, GST, CLAPPER_PAINTABLE, GObject)

#define GST_CLAPPER_PAINTABLE_CAST(obj)          ((GstClapperPaintable *)(obj))

#define GST_CLAPPER_PAINTABLE_GET_LOCK(obj)      (&GST_CLAPPER_PAINTABLE_CAST(obj)->lock)
#define GST_CLAPPER_PAINTABLE_LOCK(obj)          g_mutex_lock (GST_CLAPPER_PAINTABLE_GET_LOCK(obj))
#define GST_CLAPPER_PAINTABLE_UNLOCK(obj)        g_mutex_unlock (GST_CLAPPER_PAINTABLE_GET_LOCK(obj))

#define GST_CLAPPER_PAINTABLE_IMPORTER_GET_LOCK(obj)      (&GST_CLAPPER_PAINTABLE_CAST(obj)->importer_lock)
#define GST_CLAPPER_PAINTABLE_IMPORTER_LOCK(obj)          g_mutex_lock (GST_CLAPPER_PAINTABLE_IMPORTER_GET_LOCK(obj))
#define GST_CLAPPER_PAINTABLE_IMPORTER_UNLOCK(obj)        g_mutex_unlock (GST_CLAPPER_PAINTABLE_IMPORTER_GET_LOCK(obj))

struct _GstClapperPaintable
{
  GObject parent;

  GMutex lock;
  GMutex importer_lock;

  GstVideoInfo v_info;

  GdkRGBA bg;

  GWeakRef widget;
  GstClapperImporter *importer;

  /* Sink properties */
  gint par_n, par_d;

  /* Resize */
  gboolean pending_resize;
  guint display_ratio_num;
  guint display_ratio_den;

  /* GdkPaintableInterface */
  gint display_width;
  gint display_height;
  gdouble display_aspect_ratio;

  /* Pending draw signal id */
  guint draw_id;
};

GstClapperPaintable * gst_clapper_paintable_new                    (void);
void                  gst_clapper_paintable_queue_draw             (GstClapperPaintable *paintable);
void                  gst_clapper_paintable_set_widget             (GstClapperPaintable *paintable, GtkWidget *widget);
void                  gst_clapper_paintable_set_importer           (GstClapperPaintable *paintable, GstClapperImporter *importer);
gboolean              gst_clapper_paintable_set_video_info         (GstClapperPaintable *paintable, const GstVideoInfo *v_info);
void                  gst_clapper_paintable_set_pixel_aspect_ratio (GstClapperPaintable *paintable, gint par_n, gint par_d);

G_END_DECLS
