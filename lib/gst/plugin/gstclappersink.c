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

#include "gstclappersink.h"
#include "gstgtkutils.h"

#define DEFAULT_FORCE_ASPECT_RATIO  TRUE
#define DEFAULT_PAR_N               1
#define DEFAULT_PAR_D               1
#define DEFAULT_KEEP_LAST_FRAME     FALSE

#define WINDOW_CSS_CLASS_NAME       "clappersinkwindow"

enum
{
  PROP_0,
  PROP_WIDGET,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_KEEP_LAST_FRAME,
  PROP_LAST
};

#define GST_CAT_DEFAULT gst_clapper_sink_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void gst_clapper_sink_navigation_interface_init (
    GstNavigationInterface *iface);

#define parent_class gst_clapper_sink_parent_class
G_DEFINE_TYPE_WITH_CODE (GstClapperSink, gst_clapper_sink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_clapper_sink_navigation_interface_init));
GST_ELEMENT_REGISTER_DEFINE (clappersink, "clappersink", GST_RANK_NONE,
    GST_TYPE_CLAPPER_SINK);

static void
window_clear_no_lock (GstClapperSink *self)
{
  if (!self->window)
    return;

  GST_TRACE_OBJECT (self, "Window clear");

  if (self->window_destroy_id) {
    g_signal_handler_disconnect (self->window, self->window_destroy_id);
    self->window_destroy_id = 0;
  }
  self->window = NULL;
  self->presented_window = FALSE;
}

static void
widget_clear_no_lock (GstClapperSink *self)
{
  if (!self->widget)
    return;

  GST_TRACE_OBJECT (self, "Widget clear");

  if (self->widget_destroy_id) {
    g_signal_handler_disconnect (self->widget, self->widget_destroy_id);
    self->widget_destroy_id = 0;
  }
  g_clear_object (&self->widget);
}

static void
widget_destroy_cb (GtkWidget *widget, GstClapperSink *self)
{
  GST_CLAPPER_SINK_LOCK (self);
  widget_clear_no_lock (self);
  GST_CLAPPER_SINK_UNLOCK (self);
}

static void
window_destroy_cb (GtkWidget *window, GstClapperSink *self)
{
  GST_DEBUG_OBJECT (self, "Window destroy");

  GST_CLAPPER_SINK_LOCK (self);

  widget_clear_no_lock (self);
  window_clear_no_lock (self);

  GST_CLAPPER_SINK_UNLOCK (self);
}

static void
calculate_stream_coords (GstClapperSink *self, GtkWidget *widget,
    gdouble x, gdouble y, gdouble *stream_x, gdouble *stream_y)
{
  GstVideoRectangle result;
  gint scaled_width, scaled_height, scale_factor;
  gint video_width, video_height;
  gboolean force_aspect_ratio;

  GST_CLAPPER_SINK_LOCK (self);

  video_width = GST_VIDEO_INFO_WIDTH (&self->v_info);
  video_height = GST_VIDEO_INFO_HEIGHT (&self->v_info);
  force_aspect_ratio = self->force_aspect_ratio;

  GST_CLAPPER_SINK_UNLOCK (self);

  scale_factor = gtk_widget_get_scale_factor (widget);
  scaled_width = gtk_widget_get_width (widget) * scale_factor;
  scaled_height = gtk_widget_get_height (widget) * scale_factor;

  if (force_aspect_ratio) {
    GstVideoRectangle src, dst;

    src.x = 0;
    src.y = 0;
    src.w = gdk_paintable_get_intrinsic_width ((GdkPaintable *) self->paintable);
    src.h = gdk_paintable_get_intrinsic_height ((GdkPaintable *) self->paintable);

    dst.x = 0;
    dst.y = 0;
    dst.w = scaled_width;
    dst.h = scaled_height;

    gst_video_center_rect (&src, &dst, &result, TRUE);
  } else {
    result.x = 0;
    result.y = 0;
    result.w = scaled_width;
    result.h = scaled_height;
  }

  /* Display coordinates to stream coordinates */
  *stream_x = (result.w > 0)
      ? (x - result.x) / result.w * video_width
      : 0;
  *stream_y = (result.h > 0)
      ? (y - result.y) / result.h * video_height
      : 0;

  /* Clip to stream size */
  *stream_x = CLAMP (*stream_x, 0, video_width);
  *stream_y = CLAMP (*stream_y, 0, video_height);

  GST_LOG ("Transform coords %fx%f => %fx%f", x, y, *stream_x, *stream_y);
}

static void
gst_clapper_sink_widget_motion_event (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, GstClapperSink *self)
{
  GtkWidget *widget;
  gdouble stream_x, stream_y;
  gboolean is_inactive;

  if (x == self->last_pos_x && y == self->last_pos_y)
    return;

  GST_OBJECT_LOCK (self);
  is_inactive = (GST_STATE (self) < GST_STATE_PLAYING);
  GST_OBJECT_UNLOCK (self);

  if (is_inactive)
    return;

  self->last_pos_x = x;
  self->last_pos_y = y;

  widget = gtk_event_controller_get_widget ((GtkEventController *) motion);
  calculate_stream_coords (self, widget, x, y, &stream_x, &stream_y);
  GST_LOG ("Event \"mouse-move\", x: %f, y: %f", stream_x, stream_y);

  gst_navigation_send_mouse_event ((GstNavigation *) self, "mouse-move",
      0, stream_x, stream_y);
}

static void
gst_clapper_sink_widget_button_event (GtkGestureClick *click,
    gint n_press, gdouble x, gdouble y, GstClapperSink *self)
{
  GtkWidget *widget;
  GdkEvent *event;
  GdkEventType event_type;
  const gchar *event_name;
  gdouble stream_x, stream_y;
  gboolean is_inactive;

  GST_OBJECT_LOCK (self);
  is_inactive = (GST_STATE (self) < GST_STATE_PLAYING);
  GST_OBJECT_UNLOCK (self);

  if (is_inactive)
    return;

  event = gtk_event_controller_get_current_event ((GtkEventController *) click);
  event_type = gdk_event_get_event_type (event);

  /* FIXME: Touchscreen handling should probably use new touch events from GStreamer 1.22 */
  event_name = (event_type == GDK_BUTTON_PRESS || event_type == GDK_TOUCH_BEGIN)
      ? "mouse-button-press"
      : (event_type == GDK_BUTTON_RELEASE || event_type == GDK_TOUCH_END)
      ? "mouse-button-release"
      : NULL;

  /* Can be NULL on touch */
  if (!event_name)
    return;

  widget = gtk_event_controller_get_widget ((GtkEventController *) click);
  calculate_stream_coords (self, widget, x, y, &stream_x, &stream_y);
  GST_LOG ("Event \"%s\", x: %f, y: %f", event_name, stream_x, stream_y);

  /* Gesture is set to handle only primary button, so we do not have to check */
  gst_navigation_send_mouse_event ((GstNavigation *) self, event_name,
      1, stream_x, stream_y);
}

/* Must call from main thread only with a lock */
static GtkWidget *
gst_clapper_sink_get_widget (GstClapperSink *self)
{
  if (G_UNLIKELY (!self->widget)) {
    GtkEventController *controller;
    GtkGesture *gesture;

    /* Make sure GTK is initialized */
    if (!gtk_init_check ()) {
      GST_ERROR_OBJECT (self, "Could not ensure GTK initialization");
      return NULL;
    }

    self->widget = gtk_picture_new ();

    /* Otherwise widget in grid will appear as a 1x1px
     * video which might be misleading for users */
    gtk_widget_set_hexpand (self->widget, TRUE);
    gtk_widget_set_vexpand (self->widget, TRUE);

    gtk_widget_set_focusable (self->widget, TRUE);
    gtk_widget_set_can_focus (self->widget, TRUE);

    controller = gtk_event_controller_motion_new ();
    g_signal_connect (controller, "motion",
        G_CALLBACK (gst_clapper_sink_widget_motion_event), self);
    gtk_widget_add_controller (self->widget, controller);

    gesture = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 1);
    g_signal_connect (gesture, "pressed",
        G_CALLBACK (gst_clapper_sink_widget_button_event), self);
    g_signal_connect (gesture, "released",
        G_CALLBACK (gst_clapper_sink_widget_button_event), self);
    gtk_widget_add_controller (self->widget, GTK_EVENT_CONTROLLER (gesture));

    /* TODO: Implement touch events once we depend on GStreamer 1.22 */

    /* Take floating ref */
    g_object_ref_sink (self->widget);

    /* Set widget back pointer */
    gst_clapper_paintable_set_widget (self->paintable, self->widget);

    /* Set earlier remembered property */
    gtk_picture_set_keep_aspect_ratio (GTK_PICTURE (self->widget),
        self->force_aspect_ratio);

    gtk_picture_set_paintable (GTK_PICTURE (self->widget), GDK_PAINTABLE (self->paintable));

    self->widget_destroy_id = g_signal_connect (self->widget,
        "destroy", G_CALLBACK (widget_destroy_cb), self);
  }

  return self->widget;
}

static GtkWidget *
gst_clapper_sink_obtain_widget (GstClapperSink *self)
{
  GtkWidget *widget;

  GST_CLAPPER_SINK_LOCK (self);
  widget = gst_clapper_sink_get_widget (self);
  if (widget)
    g_object_ref (widget);
  GST_CLAPPER_SINK_UNLOCK (self);

  return widget;
}

static void
gst_clapper_sink_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (object);

  GST_CLAPPER_SINK_LOCK (self);

  switch (prop_id) {
    case PROP_WIDGET:
      if (self->widget) {
        g_value_set_object (value, self->widget);
      } else {
        GtkWidget *widget;

        GST_CLAPPER_SINK_UNLOCK (self);
        widget = gst_gtk_invoke_on_main ((GThreadFunc) gst_clapper_sink_obtain_widget, self);
        GST_CLAPPER_SINK_LOCK (self);

        g_value_set_object (value, widget);
        g_object_unref (widget);
      }
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, self->force_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, self->par_n, self->par_d);
      break;
    case PROP_KEEP_LAST_FRAME:
      g_value_set_boolean (value, self->keep_last_frame);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_CLAPPER_SINK_UNLOCK (self);
}

static void
gst_clapper_sink_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (object);

  GST_CLAPPER_SINK_LOCK (self);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      self->force_aspect_ratio = g_value_get_boolean (value);

      if (self->widget) {
        gtk_picture_set_keep_aspect_ratio (GTK_PICTURE (self->widget),
            self->force_aspect_ratio);
      }
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      self->par_n = gst_value_get_fraction_numerator (value);
      self->par_d = gst_value_get_fraction_denominator (value);

      gst_clapper_paintable_set_pixel_aspect_ratio (self->paintable,
          self->par_n, self->par_d);
      break;
    case PROP_KEEP_LAST_FRAME:
      self->keep_last_frame = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_CLAPPER_SINK_UNLOCK (self);
}

static void
gst_clapper_sink_navigation_send_event (GstNavigation *navigation,
    GstStructure *structure)
{
  GstClapperSink *sink = GST_CLAPPER_SINK_CAST (navigation);
  GstEvent *event;

  GST_TRACE_OBJECT (sink, "Navigation event: %" GST_PTR_FORMAT, structure);
  event = gst_event_new_navigation (structure);

  if (G_LIKELY (event)) {
    GstPad *pad;

    pad = gst_pad_get_peer (GST_VIDEO_SINK_PAD (sink));

    if (G_LIKELY (pad)) {
      if (!gst_pad_send_event (pad, gst_event_ref (event))) {
        /* If upstream didn't handle the event we'll post a message with it
         * for the application in case it wants to do something with it */
        gst_element_post_message (GST_ELEMENT_CAST (sink),
            gst_navigation_message_new_event (GST_OBJECT_CAST (sink), event));
      }
      gst_object_unref (pad);
    }
    gst_event_unref (event);
  }
}

static gboolean
gst_clapper_sink_propose_allocation (GstBaseSink *bsink, GstQuery *query)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);
  GstCaps *caps;
  GstVideoInfo info;
  guint size, min_buffers;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (!caps) {
    GST_DEBUG_OBJECT (self, "No caps specified");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_DEBUG_OBJECT (self, "Invalid caps specified");
    return FALSE;
  }

  /* Normal size of a frame */
  size = GST_VIDEO_INFO_SIZE (&info);

  /* We keep around current buffer and a pending one */
  min_buffers = 3;

  if (need_pool) {
    GstBufferPool *pool;
    GstStructure *config = NULL;

    GST_DEBUG_OBJECT (self, "Need to create buffer pool");

    GST_CLAPPER_SINK_LOCK (self);
    pool = gst_clapper_importer_create_pool (self->importer, &config);
    GST_CLAPPER_SINK_UNLOCK (self);

    if (pool) {
      /* If we did not get config, use default one */
      if (!config)
        config = gst_buffer_pool_get_config (pool);

      gst_buffer_pool_config_set_params (config, caps, size, min_buffers, 0);

      if (!gst_buffer_pool_set_config (pool, config)) {
        gst_object_unref (pool);

        GST_ERROR_OBJECT (self, "Failed to set config");
        return FALSE;
      }

      gst_query_add_allocation_pool (query, pool, size, min_buffers, 0);
      gst_object_unref (pool);
    } else if (config) {
      GST_WARNING_OBJECT (self, "Got config without a pool to apply it");
      gst_structure_free (config);
    }
  }

  GST_CLAPPER_SINK_LOCK (self);
  gst_clapper_importer_add_allocation_metas (self->importer, query);
  GST_CLAPPER_SINK_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_clapper_sink_query (GstBaseSink *bsink, GstQuery *query)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);
  gboolean res = FALSE;

  GST_CLAPPER_SINK_LOCK (self);

  if (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT) {
    gboolean is_inactive;

    GST_OBJECT_LOCK (self);
    is_inactive = (GST_STATE (self) < GST_STATE_PAUSED);
    GST_OBJECT_UNLOCK (self);

    /* Some random context query in the middle of playback
     * should not trigger importer replacement */
    if (is_inactive)
      gst_clapper_importer_loader_find_importer_for_context_query (self->loader, query, &self->importer);
    if (self->importer)
      res = gst_clapper_importer_handle_context_query (self->importer, bsink, query);
  }

  GST_CLAPPER_SINK_UNLOCK (self);

  if (res)
    return TRUE;

  return GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
}

static gboolean
gst_clapper_sink_start_on_main (GstClapperSink *self)
{
  GtkWidget *widget;

  GST_CLAPPER_SINK_LOCK (self);

  /* Make sure widget is created */
  if (!(widget = gst_clapper_sink_get_widget (self))) {
    GST_CLAPPER_SINK_UNLOCK (self);

    return FALSE;
  }

  /* When no toplevel window, make our own */
  if (G_UNLIKELY (!gtk_widget_get_root (widget) && !self->window)) {
    GtkWidget *toplevel, *parent;
    GtkCssProvider *provider;
    gchar *win_title;

    if ((parent = gtk_widget_get_parent (widget))) {
      GtkWidget *temp_parent;

      while ((temp_parent = gtk_widget_get_parent (parent)))
        parent = temp_parent;
    }
    toplevel = (parent) ? parent : widget;

    self->window = (GtkWindow *) gtk_window_new ();
    gtk_widget_add_css_class (GTK_WIDGET (self->window), WINDOW_CSS_CLASS_NAME);

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider,
        "." WINDOW_CSS_CLASS_NAME " { background: none; }", -1);
    gtk_style_context_add_provider_for_display (
        gdk_display_get_default (), GTK_STYLE_PROVIDER (provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);

    win_title = g_strdup_printf ("Clapper Sink - GTK %u.%u.%u Window",
        gtk_get_major_version (),
        gtk_get_minor_version (),
        gtk_get_micro_version ());

    /* Set some common default size, adding stock headerbar height
     * to it in order to display 4:3 aspect video widget */
    gtk_window_set_default_size (self->window, 640, 480 + 37);
    gtk_window_set_title (self->window, win_title);
    gtk_window_set_child (self->window, toplevel);

    g_free (win_title);

    self->window_destroy_id = g_signal_connect (self->window,
        "destroy", G_CALLBACK (window_destroy_cb), self);
  }

  GST_CLAPPER_SINK_UNLOCK (self);

  return TRUE;
}

static gboolean
window_present_on_main_idle (GtkWindow *window)
{
  GST_INFO ("Presenting window");
  gtk_window_present (window);

  return G_SOURCE_REMOVE;
}

static gboolean
gst_clapper_sink_start (GstBaseSink *bsink)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);

  GST_INFO_OBJECT (self, "Start");

  if (G_UNLIKELY (!(! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
      gst_clapper_sink_start_on_main, self)))) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("GtkWidget could not be created"), (NULL));

    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_clapper_sink_stop_on_main (GstClapperSink *self)
{
  GtkWindow *window = NULL;

  GST_CLAPPER_SINK_LOCK (self);
  if (self->window)
    window = g_object_ref (self->window);
  GST_CLAPPER_SINK_UNLOCK (self);

  if (window) {
    gtk_window_destroy (window);
    g_object_unref (window);
  }

  return TRUE;
}

static gboolean
gst_clapper_sink_stop (GstBaseSink *bsink)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);
  gboolean has_window;

  GST_INFO_OBJECT (self, "Stop");

  GST_CLAPPER_SINK_LOCK (self);
  has_window = (self->window != NULL);
  GST_CLAPPER_SINK_UNLOCK (self);

  if (G_UNLIKELY (has_window)) {
    return (! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
        gst_clapper_sink_stop_on_main, self));
  }

  return TRUE;
}

static GstStateChangeReturn
gst_clapper_sink_change_state (GstElement *element, GstStateChange transition)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (element);

  GST_DEBUG_OBJECT (self, "Changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_CLAPPER_SINK_LOCK (self);
      if (!self->keep_last_frame && self->importer) {
        gst_clapper_importer_set_buffer (self->importer, NULL);
        gst_clapper_paintable_queue_draw (self->paintable);
      }
      GST_CLAPPER_SINK_UNLOCK (self);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_CLAPPER_SINK_LOCK (self);
      if (G_UNLIKELY (self->window && !self->presented_window)) {
        g_idle_add_full (G_PRIORITY_DEFAULT,
            (GSourceFunc) window_present_on_main_idle,
            g_object_ref (self->window), (GDestroyNotify) g_object_unref);
        self->presented_window = TRUE;
      }
      GST_CLAPPER_SINK_UNLOCK (self);
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_clapper_sink_get_times (GstBaseSink *bsink, GstBuffer *buffer,
    GstClockTime *start, GstClockTime *end)
{
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    return;

  *start = GST_BUFFER_TIMESTAMP (buffer);

  if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
    *end = *start + GST_BUFFER_DURATION (buffer);
  } else {
    GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);
    gint fps_n, fps_d;

    GST_CLAPPER_SINK_LOCK (self);
    fps_n = GST_VIDEO_INFO_FPS_N (&self->v_info);
    fps_d = GST_VIDEO_INFO_FPS_D (&self->v_info);
    GST_CLAPPER_SINK_UNLOCK (self);

    if (fps_n > 0)
      *end = *start + gst_util_uint64_scale_int (GST_SECOND, fps_d, fps_n);
  }
}

static GstCaps *
gst_clapper_sink_get_caps (GstBaseSink *bsink, GstCaps *filter)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);
  GstCaps *result, *tmp;

  tmp = gst_clapper_importer_loader_make_actual_caps (self->loader);

  if (filter) {
    GST_DEBUG ("Intersecting with filter caps: %" GST_PTR_FORMAT, filter);
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }
  GST_DEBUG ("Returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_clapper_sink_set_caps (GstBaseSink *bsink, GstCaps *caps)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);

  GST_INFO_OBJECT (self, "Set caps: %" GST_PTR_FORMAT, caps);
  GST_CLAPPER_SINK_LOCK (self);

  if (G_UNLIKELY (!self->widget)) {
    GST_CLAPPER_SINK_UNLOCK (self);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Output widget was destroyed"), (NULL));

    return FALSE;
  }

  if (!gst_clapper_importer_loader_find_importer_for_caps (self->loader, caps, &self->importer)) {
    GST_CLAPPER_SINK_UNLOCK (self);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("No importer for given caps found"), (NULL));

    return FALSE;
  }
  gst_clapper_paintable_set_importer (self->paintable, self->importer);

  GST_CLAPPER_SINK_UNLOCK (self);

  return GST_BASE_SINK_CLASS (parent_class)->set_caps (bsink, caps);
}

static gboolean
gst_clapper_sink_set_info (GstVideoSink *vsink, GstCaps *caps, const GstVideoInfo *info)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (vsink);
  gboolean res;

  GST_CLAPPER_SINK_LOCK (self);

  self->v_info = *info;
  GST_DEBUG_OBJECT (self, "Video info changed");

  res = gst_clapper_paintable_set_video_info (self->paintable, info);
  GST_CLAPPER_SINK_UNLOCK (self);

  return res;
}

static GstFlowReturn
gst_clapper_sink_show_frame (GstVideoSink *vsink, GstBuffer *buffer)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (vsink);

  GST_TRACE ("Got %" GST_PTR_FORMAT, buffer);
  GST_CLAPPER_SINK_LOCK (self);

  if (G_UNLIKELY (!self->widget)) {
    GST_CLAPPER_SINK_UNLOCK (self);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Output widget was destroyed"), (NULL));

    return GST_FLOW_ERROR;
  }

  gst_clapper_importer_set_buffer (self->importer, buffer);
  gst_clapper_paintable_queue_draw (self->paintable);

  GST_CLAPPER_SINK_UNLOCK (self);

  return GST_FLOW_OK;
}

static void
gst_clapper_sink_init (GstClapperSink *self)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) GST_CLAPPER_SINK_GET_CLASS (self);

  /* HACK: install here instead of class init to avoid GStreamer
   * plugin scanner GObject type conflicts with older GTK versions */
  if (!g_object_class_find_property (gobject_class, "widget")) {
    g_object_class_install_property (gobject_class, PROP_WIDGET,
        g_param_spec_object ("widget", "GTK Widget",
            "The GtkWidget to place in the widget hierarchy",
            GTK_TYPE_WIDGET, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  }

  self->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  self->par_n = DEFAULT_PAR_N;
  self->par_d = DEFAULT_PAR_D;
  self->keep_last_frame = DEFAULT_KEEP_LAST_FRAME;

  g_mutex_init (&self->lock);
  gst_video_info_init (&self->v_info);

  self->paintable = gst_clapper_paintable_new ();
  self->loader = gst_clapper_importer_loader_new ();
}

static void
gst_clapper_sink_dispose (GObject *object)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (object);

  GST_CLAPPER_SINK_LOCK (self);

  window_clear_no_lock (self);
  widget_clear_no_lock (self);

  g_clear_object (&self->paintable);
  gst_clear_object (&self->importer);

  GST_CLAPPER_SINK_UNLOCK (self);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_clapper_sink_finalize (GObject *object)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (object);

  GST_TRACE ("Finalize");

  gst_clear_object (&self->loader);
  g_mutex_clear (&self->lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_sink_class_init (GstClapperSinkClass *klass)
{
  GstPadTemplate *sink_pad_templ;

  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;
  GstVideoSinkClass *gstvideosink_class = (GstVideoSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappersink", 0,
      "Clapper Sink");

  gobject_class->get_property = gst_clapper_sink_get_property;
  gobject_class->set_property = gst_clapper_sink_set_property;
  gobject_class->dispose = gst_clapper_sink_dispose;
  gobject_class->finalize = gst_clapper_sink_finalize;

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device",
          DEFAULT_PAR_N, DEFAULT_PAR_D,
          G_MAXINT, 1, 1, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KEEP_LAST_FRAME,
      g_param_spec_boolean ("keep-last-frame", "Keep last frame",
          "Keep showing last video frame after playback instead of black screen",
          DEFAULT_KEEP_LAST_FRAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_clapper_sink_change_state;

  gstbasesink_class->get_caps = gst_clapper_sink_get_caps;
  gstbasesink_class->set_caps = gst_clapper_sink_set_caps;
  gstbasesink_class->get_times = gst_clapper_sink_get_times;
  gstbasesink_class->propose_allocation = gst_clapper_sink_propose_allocation;
  gstbasesink_class->query = gst_clapper_sink_query;
  gstbasesink_class->start = gst_clapper_sink_start;
  gstbasesink_class->stop = gst_clapper_sink_stop;

  gstvideosink_class->set_info = gst_clapper_sink_set_info;
  gstvideosink_class->show_frame = gst_clapper_sink_show_frame;

  gst_element_class_set_static_metadata (gstelement_class,
      "Clapper video sink",
      "Sink/Video", "A GTK4 video sink used by Clapper media player",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  sink_pad_templ = gst_clapper_importer_loader_make_sink_pad_template ();
  gst_element_class_add_pad_template (gstelement_class, sink_pad_templ);
}

/*
 * GstNavigationInterface
 */
static void
gst_clapper_sink_navigation_interface_init (GstNavigationInterface *iface)
{
  /* TODO: Port to "send_event_simple" once we depend on GStreamer 1.22 */
  iface->send_event = gst_clapper_sink_navigation_send_event;
}
