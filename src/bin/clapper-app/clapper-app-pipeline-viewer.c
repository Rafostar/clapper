/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <gst/gst.h>
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>
#include <librsvg/rsvg.h>
#include <math.h>

#include "clapper-app-pipeline-viewer.h"

#define GST_CAT_DEFAULT clapper_app_pipeline_viewer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppPipelineViewer
{
  GtkWidget parent;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  GdkTexture *preview_texture;
  GdkTexture *texture;

  RsvgHandle *handle;
  gdouble intrinsic_w;
  gdouble intrinsic_h;

  graphene_rect_t viewport;

  gdouble zoom;
  gboolean zooming;

  gdouble pointer_x;
  gdouble pointer_y;

  gdouble drag_adj_x;
  gdouble drag_adj_y;

  gint allocated_width;
  gint allocated_height;

  ClapperPlayer *player;
  GCancellable *cancellable;

  gboolean running;
  gboolean pending_preview;
  gboolean pending_refresh;

  guint refresh_id;
};

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_LAST
};

static void
_scrollable_iface_init (GtkScrollableInterface *iface)
{
}

#define parent_class clapper_app_pipeline_viewer_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperAppPipelineViewer, clapper_app_pipeline_viewer, GTK_TYPE_WIDGET,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, _scrollable_iface_init));

static void clapper_app_pipeline_viewer_preview (ClapperAppPipelineViewer *self);
static void clapper_app_pipeline_viewer_refresh (ClapperAppPipelineViewer *self);

typedef struct
{
  ClapperPlayer *player;
  RsvgHandle *handle;
  gdouble intrinsic_w;
  gdouble intrinsic_h;
  graphene_rect_t viewport;
  gdouble zoom;
  gint scale_factor;
} ClapperAppPipelineViewerData;

static ClapperAppPipelineViewerData *
_thread_data_create (ClapperAppPipelineViewer *self)
{
  ClapperAppPipelineViewerData *data;

  data = g_new (ClapperAppPipelineViewerData, 1);
  data->player = gst_object_ref (self->player);
  data->handle = (self->handle) ? g_object_ref (self->handle) : NULL;
  data->intrinsic_w = self->intrinsic_w;
  data->intrinsic_h = self->intrinsic_h;
  graphene_rect_init_from_rect (&data->viewport, &self->viewport);
  data->zoom = self->zoom;
  data->scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  GST_TRACE ("Created render data: %p", data);

  return data;
}

static void
_thread_data_free (ClapperAppPipelineViewerData *data)
{
  GST_TRACE ("Freeing render data: %p", data);

  gst_object_unref (data->player);
  g_clear_object (&data->handle);

  g_free (data);
}

static inline void
_set_cancelled_error (GError **error)
{
  g_set_error (error, G_IO_ERROR,
      G_IO_ERROR_CANCELLED, "Cancelled");
}

static inline void
_cancel_cancellable (ClapperAppPipelineViewer *self)
{
  g_cancellable_cancel (self->cancellable);

  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();
}

static void
_invalidate_viewport (ClapperAppPipelineViewer *self)
{
  g_clear_object (&self->texture);
  _cancel_cancellable (self);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
_refresh_viewport (ClapperAppPipelineViewer *self)
{
  self->viewport.origin.x = gtk_adjustment_get_value (self->hadjustment);
  self->viewport.origin.y = gtk_adjustment_get_value (self->vadjustment);
  self->viewport.size.width = gtk_widget_get_width ((GtkWidget *) self);
  self->viewport.size.height = gtk_widget_get_height ((GtkWidget *) self);

  clapper_app_pipeline_viewer_refresh (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  self->refresh_id = 0;
}

static void
_set_refresh_viewport_timeout (ClapperAppPipelineViewer *self)
{
  /* Wait a bit while adjustment still moves, then refresh */
  g_clear_handle_id (&self->refresh_id, g_source_remove);
  self->refresh_id = g_timeout_add_once (200, (GSourceOnceFunc) _refresh_viewport, self);
}

static RsvgHandle *
_load_pipeline_graph (ClapperAppPipelineViewer *self, ClapperPlayer *player,
    GstDebugGraphDetails details, GCancellable *cancellable, GError **error)
{
  RsvgHandle *handle = NULL;
  gchar *dot_data, *img_data = NULL;
  guint size = 0;

  dot_data = clapper_player_make_pipeline_graph (player, details);

  if (!g_cancellable_is_cancelled (cancellable)) {
    Agraph_t *graph;
    GVC_t *gvc;

    graph = agmemread (dot_data);

    gvc = gvContext ();
    gvLayout (gvc, graph, "dot");
    gvRenderData (gvc, graph, "svg", &img_data, &size);

    agclose (graph);
    gvFreeContext (gvc);
  } else if (*error == NULL) {
    _set_cancelled_error (error);
  }

  g_free (dot_data);

  if (img_data) {
    if (!g_cancellable_is_cancelled (cancellable))
      handle = rsvg_handle_new_from_data ((const guint8 *) img_data, size, error);
    else if (*error == NULL)
      _set_cancelled_error (error);

    g_free (img_data);
  }

  return handle;
}

static inline GdkTexture *
_create_texture_from_surface (cairo_surface_t *surface)
{
  GdkTexture *texture;
  GBytes *bytes;

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
      cairo_image_surface_get_height (surface) * cairo_image_surface_get_stride (surface),
      (GDestroyNotify) cairo_surface_destroy,
      cairo_surface_reference (surface));

  texture = gdk_memory_texture_new (
      cairo_image_surface_get_width (surface),
      cairo_image_surface_get_height (surface),
      GDK_MEMORY_DEFAULT,
      bytes,
      cairo_image_surface_get_stride (surface));

  g_bytes_unref (bytes);

  return texture;
}

static GdkTexture *
_render_texture (RsvgHandle *handle, const graphene_rect_t *viewport, gint render_w, gint render_h,
    gint scale_factor, GCancellable *cancellable, GError **error)
{
  GdkTexture *texture = NULL;
  cairo_surface_t *surface;
  cairo_t *cr;
  gint x, y, w, h;

  x = floor (viewport->origin.x * scale_factor);
  y = floor (viewport->origin.y * scale_factor);
  w = ceil (viewport->size.width * scale_factor);
  h = ceil (viewport->size.height * scale_factor);

  GST_DEBUG ("Creating surface, bb: (%i,%i,%i,%i)", x, y, w, h);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cr = cairo_create (surface);

  if (!g_cancellable_is_cancelled (cancellable)) {
    if (rsvg_handle_render_document (handle, cr, &(RsvgRectangle) {-x, -y, render_w, render_h}, error))
      texture = _create_texture_from_surface (surface);
  } else if (error == NULL) {
    _set_cancelled_error (error);
  }

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return texture;
}

static void
_preview_in_thread (GTask *task, ClapperAppPipelineViewer *self,
    ClapperAppPipelineViewerData *data, GCancellable *cancellable)
{
  GdkTexture *texture = NULL;
  GError *error = NULL;

  g_clear_object (&data->handle);
  if ((data->handle = _load_pipeline_graph (self,
      data->player, GST_DEBUG_GRAPH_SHOW_ALL, cancellable, &error))) {
    rsvg_handle_set_dpi (data->handle, 90);
    rsvg_handle_get_intrinsic_size_in_pixels (data->handle,
        &data->intrinsic_w, &data->intrinsic_h);

    texture = _render_texture (data->handle,
        &GRAPHENE_RECT_INIT (0, 0, data->intrinsic_w, data->intrinsic_h),
        ceil (data->intrinsic_w), ceil (data->intrinsic_h),
        1, cancellable, &error);
  }

  if (texture)
    g_task_return_pointer (task, texture, (GDestroyNotify) g_object_unref);
  else
    g_task_return_error (task, error);
}

static void
_refresh_in_thread (GTask *task, ClapperAppPipelineViewer *self,
    ClapperAppPipelineViewerData *data, GCancellable *cancellable)
{
  GdkTexture *texture;
  GError *error = NULL;
  gint render_w, render_h;

  render_w = ceil (data->zoom * data->intrinsic_w * data->scale_factor);
  render_h = ceil (data->zoom * data->intrinsic_h * data->scale_factor);

  texture = _render_texture (data->handle, &data->viewport, render_w, render_h,
      data->scale_factor, cancellable, &error);

  if (texture)
    g_task_return_pointer (task, texture, (GDestroyNotify) g_object_unref);
  else
    g_task_return_error (task, error);
}

static inline void
_finish_texture_task (ClapperAppPipelineViewer *self, GTask *task, GdkTexture **texture)
{
  GError *error = NULL;
  gboolean cancelled = FALSE;

  g_clear_object (texture);
  *texture = (GdkTexture *) g_task_propagate_pointer (task, &error);

  if (error) {
    if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_CANCELLED) {
      GST_ERROR ("Error: %s", (error->message)
          ? error->message : "Could not render pipeline graph");
    } else {
      GST_DEBUG ("Refresh cancelled");
      cancelled = TRUE;
    }
    g_error_free (error);
  }

  /* Resize will also trigger redraw. We need to always call resize,
   * because regenerated image has slightly different dimensions each time. */
  if (!cancelled)
    gtk_widget_queue_resize (GTK_WIDGET (self));

  self->running = FALSE;

  if (self->pending_preview) {
    clapper_app_pipeline_viewer_preview (self);
    self->pending_preview = FALSE;
  } else if (self->pending_refresh) {
    clapper_app_pipeline_viewer_refresh (self);
    self->pending_refresh = FALSE;
  }
}

static void
clapper_app_pipeline_viewer_preview_cb (ClapperAppPipelineViewer *self,
    GAsyncResult *res, gpointer user_data G_GNUC_UNUSED)
{
  GTask *task = G_TASK (res);
  ClapperAppPipelineViewerData *data;

  data = (ClapperAppPipelineViewerData *) g_task_get_task_data (task);

  g_clear_object (&self->handle);
  self->handle = g_object_ref (data->handle);
  self->intrinsic_w = data->intrinsic_w;
  self->intrinsic_h = data->intrinsic_h;

  _finish_texture_task (self, task, &self->preview_texture);
}

static void
clapper_app_pipeline_viewer_refresh_cb (ClapperAppPipelineViewer *self,
    GAsyncResult *res, gpointer user_data G_GNUC_UNUSED)
{
  _finish_texture_task (self, G_TASK (res), &self->texture);
}

static void
clapper_app_pipeline_viewer_preview (ClapperAppPipelineViewer *self)
{
  GTask *task;

  _cancel_cancellable (self);

  if (self->running) {
    self->pending_preview = TRUE;
    return;
  }

  self->running = TRUE;

  task = g_task_new (self, self->cancellable,
      (GAsyncReadyCallback) clapper_app_pipeline_viewer_preview_cb, NULL);
  g_task_set_task_data (task, _thread_data_create (self),
      (GDestroyNotify) _thread_data_free);
  g_task_run_in_thread (task, (GTaskThreadFunc) _preview_in_thread);

  g_object_unref (task);
}

static void
clapper_app_pipeline_viewer_refresh (ClapperAppPipelineViewer *self)
{
  GTask *task;

  g_clear_object (&self->texture);
  _cancel_cancellable (self);

  if (self->running) {
    self->pending_refresh = TRUE;
    return;
  }

  self->running = TRUE;

  task = g_task_new (self, self->cancellable,
      (GAsyncReadyCallback) clapper_app_pipeline_viewer_refresh_cb, NULL);
  g_task_set_task_data (task, _thread_data_create (self),
      (GDestroyNotify) _thread_data_free);
  g_task_run_in_thread (task, (GTaskThreadFunc) _refresh_in_thread);

  g_object_unref (task);
}

static void
motion_cb (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, ClapperAppPipelineViewer *self)
{
  self->pointer_x = x;
  self->pointer_y = y;
}

static gboolean
scroll_cb (GtkEventControllerScroll *scroll,
    gdouble dx, gdouble dy, ClapperAppPipelineViewer *self)
{
  gdouble scale_factor, calc_scale, multiplier;
  gdouble event_x, event_y, x = 0, y = 0;

  scale_factor = (dy > 0) ? 0.9 : (dy < 0) ? 1.1 : 0;
  if (scale_factor == 0)
    return TRUE;

  calc_scale = CLAMP (self->zoom * scale_factor, 0.1, 10.0);

  if (G_APPROX_VALUE (calc_scale, self->zoom, FLT_EPSILON))
    return TRUE;

  GST_LOG_OBJECT (self, "Zoom to: %.2lf", calc_scale);

  multiplier = calc_scale / self->zoom;

  event_x = self->pointer_x - gtk_adjustment_get_value (self->hadjustment) * multiplier;
  event_y = self->pointer_y - gtk_adjustment_get_value (self->vadjustment) * multiplier;

  x = self->pointer_x * multiplier - event_x;
  y = self->pointer_y * multiplier - event_y;

  /* Do not act on adjustment changes here */
  self->zooming = TRUE;

  self->zoom = calc_scale;

  _invalidate_viewport (self);

  gtk_adjustment_set_upper (self->hadjustment, G_MAXDOUBLE);
  gtk_adjustment_set_upper (self->vadjustment, G_MAXDOUBLE);

  gtk_adjustment_set_value (self->hadjustment, x);
  gtk_adjustment_set_value (self->vadjustment, y);

  _set_refresh_viewport_timeout (self);

  self->zooming = FALSE;

  return TRUE;
}

static void
drag_begin_cb (GtkGestureDrag *drag, gdouble start_x, gdouble start_y,
    ClapperAppPipelineViewer *self)
{
  GdkCursor *cursor;

  GST_DEBUG_OBJECT (self, "Drag begin");

  cursor = gdk_cursor_new_from_name ("all-scroll", NULL);
  gtk_widget_set_cursor (GTK_WIDGET (self), cursor);
  g_object_unref (cursor);

  self->drag_adj_x = gtk_adjustment_get_value (self->hadjustment);
  self->drag_adj_y = gtk_adjustment_get_value (self->vadjustment);
}

static void
drag_update_cb (GtkGestureDrag *drag, gdouble offset_x, gdouble offset_y,
    ClapperAppPipelineViewer *self)
{
  gtk_adjustment_set_value (self->hadjustment, self->drag_adj_x - offset_x);
  gtk_adjustment_set_value (self->vadjustment, self->drag_adj_y - offset_y);
}

static void
drag_end_cb (GtkGestureDrag *drag, gdouble offset_x, gdouble offset_y,
    ClapperAppPipelineViewer *self)
{
  GdkCursor *cursor;

  GST_DEBUG_OBJECT (self, "Drag end");

  cursor = gdk_cursor_new_from_name ("default", NULL);
  gtk_widget_set_cursor (GTK_WIDGET (self), cursor);
  g_object_unref (cursor);
}

static void
_adjustment_value_changed_cb (GtkAdjustment *adjustment, ClapperAppPipelineViewer *self)
{
  if (self->zooming)
    return;

  _invalidate_viewport (self);
  _set_refresh_viewport_timeout (self);

  gtk_widget_queue_allocate ((GtkWidget *) self);
}

static void
_on_widget_size_changed (ClapperAppPipelineViewer *self)
{
  if (!self->preview_texture)
    return;

  _invalidate_viewport (self);
  _set_refresh_viewport_timeout (self);
}

static inline void
_set_adjustment (ClapperAppPipelineViewer *self,
    GtkAdjustment **dest_adjustment, GtkAdjustment *adjustment)
{
  if (*dest_adjustment)
    g_signal_handlers_disconnect_by_func (*dest_adjustment, _adjustment_value_changed_cb, self);

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_clear_object (dest_adjustment);
  *dest_adjustment = g_object_ref_sink (adjustment);

  g_signal_connect (*dest_adjustment, "value-changed", G_CALLBACK (_adjustment_value_changed_cb), self);
}

static inline void
_set_adjustment_values (ClapperAppPipelineViewer *self, GtkAdjustment *adjustment,
    gboolean is_rtl, gint viewport_size, gint upper)
{
  gdouble value = gtk_adjustment_get_value (adjustment);

  /* We clamp to the left in RTL mode */
  if (adjustment == self->hadjustment && is_rtl) {
    gdouble dist = gtk_adjustment_get_upper (adjustment) - value - gtk_adjustment_get_page_size (adjustment);
    value = upper - dist - viewport_size;
  }

  gtk_adjustment_configure (adjustment, value, 0, upper,
      viewport_size * 0.1, viewport_size * 0.9, viewport_size);
}

void
clapper_app_pipeline_viewer_set_player (ClapperAppPipelineViewer *self, ClapperPlayer *player)
{
  gst_object_replace ((GstObject **) &self->player, GST_OBJECT_CAST (player));
  clapper_app_pipeline_viewer_preview (self);
}

static void
clapper_app_pipeline_viewer_size_allocate (GtkWidget *widget,
    gint width, gint height, gint baseline)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (widget);
  gint sizes[2] = { width, height };
  gboolean visible, is_rtl;

  visible = gtk_widget_get_visible (widget);
  is_rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  /* Update both at once, then notify */
  g_object_freeze_notify (G_OBJECT (self->hadjustment));
  g_object_freeze_notify (G_OBJECT (self->vadjustment));

  if (visible) {
    gint min = 0, nat = 0;

    if (width != self->allocated_width || height != self->allocated_height)
      _on_widget_size_changed (self);

    gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
    sizes[1] = MAX (sizes[1], nat);

    gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, sizes[1], &min, &nat, NULL, NULL);
    sizes[0] = MAX (sizes[0], nat);
  }

  _set_adjustment_values (self, self->hadjustment, is_rtl, width, sizes[0]);
  _set_adjustment_values (self, self->vadjustment, is_rtl, height, sizes[1]);

  self->allocated_width = width;
  self->allocated_height = height;

  g_object_thaw_notify (G_OBJECT (self->hadjustment));
  g_object_thaw_notify (G_OBJECT (self->vadjustment));
}

static void
clapper_app_pipeline_viewer_measure (GtkWidget *widget, GtkOrientation orientation,
    gint for_size, gint *minimum, gint *natural,
    gint *minimum_baseline, gint *natural_baseline)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (widget);

  if (self->preview_texture) {
    gdouble size;

    size = (orientation == GTK_ORIENTATION_HORIZONTAL)
        ? self->intrinsic_w
        : self->intrinsic_h;

    *minimum = *natural = ceil (self->zoom * size);
  } else {
    GTK_WIDGET_CLASS (parent_class)->measure (widget, orientation,
        for_size, minimum, natural, minimum_baseline, natural_baseline);
  }
}

static GtkSizeRequestMode
clapper_app_pipeline_viewer_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
clapper_app_pipeline_viewer_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (widget);
  gint w, h, offset_x, offset_y;
  gint widget_w, widget_h;

  if (!self->preview_texture)
    return;

  GST_ERROR ("SNAPSHOT");

  w = ceil (self->zoom * self->intrinsic_w);
  h = ceil (self->zoom * self->intrinsic_h);

  widget_w = gtk_widget_get_width (widget);
  widget_h = gtk_widget_get_height (widget);

  if (widget_w > w)
    offset_x = floor ((gdouble) (widget_w - w) / 2);
  else
    offset_x = -floor (gtk_adjustment_get_value (self->hadjustment));

  if (widget_h > h)
    offset_y = floor ((gdouble) (widget_h - h) / 2);
  else
    offset_y = -floor (gtk_adjustment_get_value (self->vadjustment));

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, widget_w, widget_h));
  gtk_snapshot_save (snapshot);

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (offset_x, offset_y));

  if (self->texture)
    gtk_snapshot_append_texture (snapshot, self->texture, &self->viewport);
  else
    gtk_snapshot_append_texture (snapshot, self->preview_texture, &GRAPHENE_RECT_INIT (0, 0, w, h));

  gtk_snapshot_restore (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
clapper_app_pipeline_viewer_unrealize (GtkWidget *widget)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (widget);

  GST_TRACE_OBJECT (self, "Unrealize");

  g_clear_handle_id (&self->refresh_id, g_source_remove);

  _cancel_cancellable (self);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clapper_app_pipeline_viewer_init (ClapperAppPipelineViewer *self)
{
  GtkEventController *controller;

  _set_adjustment (self, &self->hadjustment, NULL);
  _set_adjustment (self, &self->vadjustment, NULL);

  graphene_rect_init (&self->viewport, 0, 0, 1, 1);
  self->zoom = 0.5;
  self->cancellable = g_cancellable_new ();

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller, "scroll", G_CALLBACK (scroll_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_drag_new ());
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), FALSE);
  g_signal_connect (controller, "drag-begin", G_CALLBACK (drag_begin_cb), self);
  g_signal_connect (controller, "drag-update", G_CALLBACK (drag_update_cb), self);
  g_signal_connect (controller, "drag-end", G_CALLBACK (drag_end_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
clapper_app_pipeline_viewer_dispose (GObject *object)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (object);

  if (self->hadjustment) {
    g_signal_handlers_disconnect_by_func (self->hadjustment, _adjustment_value_changed_cb, self);
    g_clear_object (&self->hadjustment);
  }
  if (self->vadjustment) {
    g_signal_handlers_disconnect_by_func (self->vadjustment, _adjustment_value_changed_cb, self);
    g_clear_object (&self->vadjustment);
  }

  g_clear_object (&self->preview_texture);
  g_clear_object (&self->texture);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_pipeline_viewer_finalize (GObject *object)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_pipeline_viewer_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (object);

  switch (prop_id) {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, self->vadjustment);
      break;
    case PROP_HSCROLL_POLICY:
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, GTK_SCROLL_NATURAL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_pipeline_viewer_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperAppPipelineViewer *self = CLAPPER_APP_PIPELINE_VIEWER_CAST (object);

  switch (prop_id) {
    case PROP_HADJUSTMENT:
      _set_adjustment (self, &self->hadjustment, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      _set_adjustment (self, &self->vadjustment, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
    case PROP_VSCROLL_POLICY:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_pipeline_viewer_class_init (ClapperAppPipelineViewerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperapppipelineviewer", 0,
      "Clapper App Pipeline Viewer");

  gobject_class->get_property = clapper_app_pipeline_viewer_get_property;
  gobject_class->set_property = clapper_app_pipeline_viewer_set_property;
  gobject_class->dispose = clapper_app_pipeline_viewer_dispose;
  gobject_class->finalize = clapper_app_pipeline_viewer_finalize;

  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  widget_class->size_allocate = clapper_app_pipeline_viewer_size_allocate;
  widget_class->measure = clapper_app_pipeline_viewer_measure;
  widget_class->get_request_mode = clapper_app_pipeline_viewer_get_request_mode;
  widget_class->snapshot = clapper_app_pipeline_viewer_snapshot;
  widget_class->unrealize = clapper_app_pipeline_viewer_unrealize;

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}
