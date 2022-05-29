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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapperpaintable.h"

#define DEFAULT_PAR_N               1
#define DEFAULT_PAR_D               1

#define GST_CAT_DEFAULT gst_clapper_paintable_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void gst_clapper_paintable_iface_init (GdkPaintableInterface *iface);
static void gst_clapper_paintable_dispose (GObject *object);
static void gst_clapper_paintable_finalize (GObject *object);

#define parent_class gst_clapper_paintable_parent_class
G_DEFINE_TYPE_WITH_CODE (GstClapperPaintable, gst_clapper_paintable, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
        gst_clapper_paintable_iface_init));

static void
gst_clapper_paintable_class_init (GstClapperPaintableClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperpaintable", 0,
      "Clapper Paintable");

  gobject_class->dispose = gst_clapper_paintable_dispose;
  gobject_class->finalize = gst_clapper_paintable_finalize;
}

static void
gst_clapper_paintable_init (GstClapperPaintable *self)
{
  self->display_width = 1;
  self->display_height = 1;
  self->display_aspect_ratio = 1.0;

  self->par_n = DEFAULT_PAR_N;
  self->par_d = DEFAULT_PAR_D;

  g_mutex_init (&self->lock);
  g_mutex_init (&self->importer_lock);

  gst_video_info_init (&self->v_info);
  g_weak_ref_init (&self->widget, NULL);

  gdk_rgba_parse (&self->bg, "black");
}

static void
gst_clapper_paintable_dispose (GObject *object)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE (object);

  GST_CLAPPER_PAINTABLE_LOCK (self);

  if (self->draw_id > 0) {
    g_source_remove (self->draw_id);
    self->draw_id = 0;
  }

  GST_CLAPPER_PAINTABLE_UNLOCK (self);

  GST_CLAPPER_PAINTABLE_IMPORTER_LOCK (self);
  gst_clear_object (&self->importer);
  GST_CLAPPER_PAINTABLE_IMPORTER_UNLOCK (self);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_clapper_paintable_finalize (GObject *object)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE (object);

  GST_TRACE ("Finalize");

  g_weak_ref_clear (&self->widget);

  g_mutex_clear (&self->lock);
  g_mutex_clear (&self->importer_lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static gboolean
calculate_display_par (GstClapperPaintable *self, const GstVideoInfo *info)
{
  gint width, height, par_n, par_d, req_par_n, req_par_d;
  gboolean success;

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  /* Cannot apply aspect ratio if there is no video */
  if (width == 0 || height == 0)
    return FALSE;

  par_n = GST_VIDEO_INFO_PAR_N (info);
  par_d = GST_VIDEO_INFO_PAR_D (info);

  req_par_n = self->par_n;
  req_par_d = self->par_d;

  if (par_n == 0)
    par_n = 1;

  /* Use defaults if user set zero */
  if (req_par_n == 0 || req_par_d == 0) {
    req_par_n = DEFAULT_PAR_N;
    req_par_d = DEFAULT_PAR_D;
  }

  GST_LOG_OBJECT (self, "PAR: %u/%u, DAR: %u/%u", par_n, par_d, req_par_n, req_par_d);

  if (!(success = gst_video_calculate_display_ratio (&self->display_ratio_num,
      &self->display_ratio_den, width, height, par_n, par_d,
      req_par_n, req_par_d))) {
    GST_ERROR_OBJECT (self, "Could not calculate display ratio values");
  }

  return success;
}

static void
invalidate_paintable_size_internal (GstClapperPaintable *self)
{
  gint video_width, video_height;
  guint display_ratio_num, display_ratio_den;

  GST_CLAPPER_PAINTABLE_LOCK (self);

  video_width = GST_VIDEO_INFO_WIDTH (&self->v_info);
  video_height = GST_VIDEO_INFO_HEIGHT (&self->v_info);

  display_ratio_num = self->display_ratio_num;
  display_ratio_den = self->display_ratio_den;

  GST_CLAPPER_PAINTABLE_UNLOCK (self);

  if (video_height % display_ratio_den == 0) {
    GST_LOG ("Keeping video height");

    self->display_width = (guint)
        gst_util_uint64_scale_int (video_height, display_ratio_num, display_ratio_den);
    self->display_height = video_height;
  } else if (video_width % display_ratio_num == 0) {
    GST_LOG ("Keeping video width");

    self->display_width = video_width;
    self->display_height = (guint)
        gst_util_uint64_scale_int (video_width, display_ratio_den, display_ratio_num);
  } else {
    GST_LOG ("Approximating while keeping video height");

    self->display_width = (guint)
        gst_util_uint64_scale_int (video_height, display_ratio_num, display_ratio_den);
    self->display_height = video_height;
  }

  self->display_aspect_ratio = ((gdouble) self->display_width
      / (gdouble) self->display_height);

  GST_DEBUG_OBJECT (self, "Invalidate paintable size, display: %dx%d",
      self->display_width, self->display_height);
  gdk_paintable_invalidate_size ((GdkPaintable *) self);
}

static gboolean
invalidate_paintable_size_on_main_cb (GstClapperPaintable *self)
{
  GST_CLAPPER_PAINTABLE_LOCK (self);
  self->draw_id = 0;
  GST_CLAPPER_PAINTABLE_UNLOCK (self);

  invalidate_paintable_size_internal (self);

  return G_SOURCE_REMOVE;
}

static gboolean
update_paintable_on_main_cb (GstClapperPaintable *self)
{
  gboolean size_changed;

  GST_CLAPPER_PAINTABLE_LOCK (self);

  /* Check if we will need to invalidate size */
  if ((size_changed = self->pending_resize))
    self->pending_resize = FALSE;

  self->draw_id = 0;

  GST_CLAPPER_PAINTABLE_UNLOCK (self);

  if (size_changed)
    invalidate_paintable_size_internal (self);

  GST_LOG_OBJECT (self, "Invalidate paintable contents");
  gdk_paintable_invalidate_contents ((GdkPaintable *) self);

  return G_SOURCE_REMOVE;
}

GstClapperPaintable *
gst_clapper_paintable_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_PAINTABLE, NULL);
}

void
gst_clapper_paintable_set_widget (GstClapperPaintable *self, GtkWidget *widget)
{
  g_weak_ref_set (&self->widget, widget);
}

void
gst_clapper_paintable_set_importer (GstClapperPaintable *self, GstClapperImporter *importer)
{
  GST_CLAPPER_PAINTABLE_IMPORTER_LOCK (self);
  gst_object_replace ((GstObject **) &self->importer, GST_OBJECT_CAST (importer));
  GST_CLAPPER_PAINTABLE_IMPORTER_UNLOCK (self);
}

void
gst_clapper_paintable_queue_draw (GstClapperPaintable *self)
{
  GST_CLAPPER_PAINTABLE_LOCK (self);

  if (self->draw_id > 0) {
    GST_CLAPPER_PAINTABLE_UNLOCK (self);
    GST_TRACE ("Already have pending draw");

    return;
  }

  self->draw_id = g_idle_add_full (G_PRIORITY_DEFAULT,
      (GSourceFunc) update_paintable_on_main_cb, self, NULL);

  GST_CLAPPER_PAINTABLE_UNLOCK (self);
}

gboolean
gst_clapper_paintable_set_video_info (GstClapperPaintable *self, const GstVideoInfo *v_info)
{
  GST_CLAPPER_PAINTABLE_LOCK (self);

  if (gst_video_info_is_equal (&self->v_info, v_info)) {
    GST_CLAPPER_PAINTABLE_UNLOCK (self);
    return TRUE;
  }

  /* Reject info if values would cause integer overflow */
  if (G_UNLIKELY (!calculate_display_par (self, v_info))) {
    GST_CLAPPER_PAINTABLE_UNLOCK (self);
    return FALSE;
  }

  self->pending_resize = TRUE;
  self->v_info = *v_info;

  GST_CLAPPER_PAINTABLE_UNLOCK (self);

  return TRUE;
}

void
gst_clapper_paintable_set_pixel_aspect_ratio (GstClapperPaintable *self,
    gint par_n, gint par_d)
{
  gboolean success;

  GST_CLAPPER_PAINTABLE_LOCK (self);

  /* No change */
  if (self->par_n == par_n && self->par_d == par_d) {
    GST_CLAPPER_PAINTABLE_UNLOCK (self);
    return;
  }

  self->par_n = par_n;
  self->par_d = par_d;

  /* Check if we can accept new values. This will update
   * display `ratio_num` and `ratio_den` only when successful */
  success = calculate_display_par (self, &self->v_info);

  /* If paintable update is queued, wait for it, otherwise invalidate
   * size only for change to be applied even when paused */
  if (!success || self->draw_id > 0) {
    self->pending_resize = success;
    GST_CLAPPER_PAINTABLE_UNLOCK (self);

    return;
  }

  self->draw_id = g_idle_add_full (G_PRIORITY_DEFAULT,
      (GSourceFunc) invalidate_paintable_size_on_main_cb, self, NULL);

  GST_CLAPPER_PAINTABLE_UNLOCK (self);
}

/*
 * GdkPaintableInterface
 */
static void
gst_clapper_paintable_snapshot_internal (GstClapperPaintable *self,
    GdkSnapshot *snapshot, gdouble width, gdouble height,
    gint widget_width, gint widget_height)
{
  gfloat scale_x, scale_y;

  GST_LOG_OBJECT (self, "Snapshot");

  scale_x = (gfloat) width / self->display_width;
  scale_y = (gfloat) height / self->display_height;

  /* Apply black borders when keeping aspect ratio */
  if (scale_x == scale_y || abs (scale_x - scale_y) <= FLT_EPSILON) {
    if (widget_height - height > 0) {
      /* XXX: Top uses integer to work with GTK rounding (not going offscreen) */
      gint top_bar_height = (widget_height - height) / 2;
      gdouble bottom_bar_height = (widget_height - top_bar_height - height);

      gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (0, 0, width, -top_bar_height));
      gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (0, height, width, bottom_bar_height));
    } else if (widget_width - width > 0) {
      gint left_bar_width = (widget_width - width) / 2;
      gdouble right_bar_width = (widget_width - left_bar_width - width);

      gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (0, 0, -left_bar_width, height));
      gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (width, 0, right_bar_width, height));
    }
  }

  GST_CLAPPER_PAINTABLE_IMPORTER_LOCK (self);

  if (self->importer) {
    gst_clapper_importer_snapshot (self->importer, snapshot, width, height);
  } else {
    GST_LOG_OBJECT (self, "No texture importer, drawing black");
    gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (0, 0, width, height));
  }

  GST_CLAPPER_PAINTABLE_IMPORTER_UNLOCK (self);
}

static void
gst_clapper_paintable_snapshot (GdkPaintable *paintable,
    GdkSnapshot *snapshot, gdouble width, gdouble height)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE_CAST (paintable);
  GtkWidget *widget;
  gint widget_width = 0, widget_height = 0;

  if ((widget = g_weak_ref_get (&self->widget))) {
    widget_width = gtk_widget_get_width (widget);
    widget_height = gtk_widget_get_height (widget);

    g_object_unref (widget);
  }

  gst_clapper_paintable_snapshot_internal (self, snapshot,
      width, height, widget_width, widget_height);
}

static GdkPaintable *
gst_clapper_paintable_get_current_image (GdkPaintable *paintable)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE_CAST (paintable);
  GtkSnapshot *snapshot = gtk_snapshot_new ();

  /* Snapshot without widget size in order to get
   * paintable without black borders */
  gst_clapper_paintable_snapshot_internal (self, snapshot,
      self->display_width, self->display_height, 0, 0);

  return gtk_snapshot_free_to_paintable (snapshot, NULL);
}

static gint
gst_clapper_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE_CAST (paintable);

  return self->display_width;
}

static gint
gst_clapper_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE_CAST (paintable);

  return self->display_height;
}

static gdouble
gst_clapper_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GstClapperPaintable *self = GST_CLAPPER_PAINTABLE_CAST (paintable);

  return self->display_aspect_ratio;
}

static void
gst_clapper_paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gst_clapper_paintable_snapshot;
  iface->get_current_image = gst_clapper_paintable_get_current_image;
  iface->get_intrinsic_width = gst_clapper_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gst_clapper_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gst_clapper_paintable_get_intrinsic_aspect_ratio;
}
