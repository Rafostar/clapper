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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclappersink.h"
#include "gstgtkutils.h"

#define DEFAULT_FORCE_ASPECT_RATIO  TRUE
#define DEFAULT_PAR_N               0
#define DEFAULT_PAR_D               1

#define SINK_FORMATS \
  "{ BGR, RGB, BGRA, RGBA, ABGR, ARGB, RGBx, BGRx, RGBA64_LE, RGBA64_BE }"

#define GST_CLAPPER_GL_SINK_CAPS \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "              \
    "format = (string)" SINK_FORMATS ", "                               \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "                   \
    " ; "                                                               \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY ","                \
    GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION "), "           \
    "format = (string)" SINK_FORMATS ", "                               \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "

enum
{
  PROP_0,
  PROP_WIDGET,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_LAST
};

GST_DEBUG_CATEGORY (gst_debug_clapper_sink);
#define GST_CAT_DEFAULT gst_debug_clapper_sink

static GstStaticPadTemplate gst_clapper_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf", SINK_FORMATS) ";"
        GST_CLAPPER_GL_SINK_CAPS ";"
        GST_VIDEO_CAPS_MAKE (SINK_FORMATS)));

static void gst_clapper_sink_navigation_interface_init (
    GstNavigationInterface *iface);

#define gst_clapper_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstClapperSink, gst_clapper_sink,
    GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_clapper_sink_navigation_interface_init);
    GST_DEBUG_CATEGORY_INIT (gst_debug_clapper_sink,
        "clappersink", 0, "Clapper Sink"));
GST_ELEMENT_REGISTER_DEFINE (clappersink, "clappersink", GST_RANK_NONE,
    GST_TYPE_CLAPPER_SINK);

static void gst_clapper_sink_finalize (GObject *object);
static void gst_clapper_sink_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *param_spec);
static void gst_clapper_sink_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *param_spec);

static gboolean gst_clapper_sink_propose_allocation (GstBaseSink *bsink,
    GstQuery *query);
static gboolean gst_clapper_sink_start (GstBaseSink *bsink);
static gboolean gst_clapper_sink_stop (GstBaseSink *bsink);

static GstStateChangeReturn
gst_clapper_sink_change_state (GstElement *element, GstStateChange transition);

static void gst_clapper_sink_get_times (GstBaseSink *bsink, GstBuffer *buffer,
    GstClockTime *start, GstClockTime *end);
static GstCaps * gst_clapper_sink_get_caps (GstBaseSink *bsink,
    GstCaps *filter);
static gboolean gst_clapper_sink_set_caps (GstBaseSink *bsink,
    GstCaps *caps);
static GstFlowReturn gst_clapper_sink_show_frame (GstVideoSink *bsink,
    GstBuffer *buffer);

static void
gst_clapper_sink_class_init (GstClapperSinkClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_clapper_sink_set_property;
  gobject_class->get_property = gst_clapper_sink_get_property;
  gobject_class->finalize = gst_clapper_sink_finalize;

  //gst_gtk_install_shared_properties (gobject_class);

  gstelement_class->change_state = gst_clapper_sink_change_state;

  gstbasesink_class->get_caps = gst_clapper_sink_get_caps;
  gstbasesink_class->set_caps = gst_clapper_sink_set_caps;
  gstbasesink_class->get_times = gst_clapper_sink_get_times;
  gstbasesink_class->propose_allocation = gst_clapper_sink_propose_allocation;
  gstbasesink_class->start = gst_clapper_sink_start;
  gstbasesink_class->stop = gst_clapper_sink_stop;

  gstvideosink_class->show_frame = gst_clapper_sink_show_frame;

  gst_element_class_set_metadata (gstelement_class,
      "Clapper Video Sink",
      "Sink/Video", "A GTK4 video sink used by Clapper media player",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_sink_template);
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
            "The GtkWidget to place in the widget hierarchy "
            "(must only be get from the GTK main thread)",
            GTK_TYPE_WIDGET, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  }

  self->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  self->par_n = DEFAULT_PAR_N;
  self->par_d = DEFAULT_PAR_D;
}

static void
gst_clapper_sink_finalize (GObject *object)
{
  GstClapperSink *self = GST_CLAPPER_SINK (object);

  GST_TRACE ("Finalize");
  GST_OBJECT_LOCK (self);

  if (self->window && self->window_destroy_id)
    g_signal_handler_disconnect (self->window, self->window_destroy_id);
  //if (self->widget && self->widget_destroy_id)
  //  g_signal_handler_disconnect (self->widget, self->widget_destroy_id);

  g_clear_object (&self->obj);
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
widget_destroy_cb (GtkWidget *widget, GstClapperSink *self)
{
  GST_OBJECT_LOCK (self);
  g_clear_object (&self->obj);
  GST_OBJECT_UNLOCK (self);
}

static void
window_destroy_cb (GtkWidget *window, GstClapperSink *self)
{
  GST_OBJECT_LOCK (self);

  if (self->obj) {
    if (self->widget_destroy_id) {
      GtkWidget *widget;

      widget = gtk_clapper_object_get_widget (self->obj);

      g_signal_handler_disconnect (widget, self->widget_destroy_id);
      self->widget_destroy_id = 0;
    }
    g_clear_object (&self->obj);
  }
  self->window = NULL;

  GST_OBJECT_UNLOCK (self);
}

static GtkWidget *
gst_clapper_sink_get_widget (GstClapperSink *self)
{
  if (G_UNLIKELY (self->obj == NULL)) {
    /* Ensure GTK is initialized */
    if (!gtk_init_check ()) {
      GST_ERROR_OBJECT (self, "Could not ensure GTK initialization");
      return NULL;
    }

    self->obj = gtk_clapper_object_new ();

  /* Take the floating ref, otherwise the destruction of the container will
   * make this widget disappear possibly before we are done. */
  //g_object_ref_sink (self->obj);

  //self->widget_destroy_id = g_signal_connect (widget,
  //    "destroy", G_CALLBACK (widget_destroy_cb), self);

    /* Back pointer */
    gtk_clapper_object_set_element (
        GTK_CLAPPER_OBJECT (self->obj), GST_ELEMENT (self));
  }

  return gtk_clapper_object_get_widget (self->obj);
}

static void
gst_clapper_sink_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GstClapperSink *self = GST_CLAPPER_SINK (object);

  switch (prop_id) {
    case PROP_WIDGET:{
      GObject *widget = NULL;

      GST_OBJECT_LOCK (self);
      if (G_LIKELY (self->obj != NULL))
        widget = G_OBJECT (gtk_clapper_object_get_widget (self->obj));
      GST_OBJECT_UNLOCK (self);

      if (G_UNLIKELY (widget == NULL)) {
        widget = gst_gtk_invoke_on_main (
            (GThreadFunc) gst_clapper_sink_get_widget, self);
      }

      g_value_set_object (value, widget);
      break;
    }
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, self->force_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, self->par_n, self->par_d);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_sink_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstClapperSink *self = GST_CLAPPER_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      self->force_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      self->par_n = gst_value_get_fraction_numerator (value);
      self->par_d = gst_value_get_fraction_denominator (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_sink_navigation_send_event (GstNavigation *navigation,
    GstStructure *structure)
{
  GstClapperSink *sink = GST_CLAPPER_SINK_CAST (navigation);
  GstEvent *event;

  GST_TRACE_OBJECT (sink, "Navigation event: %" GST_PTR_FORMAT, structure);
  event = gst_event_new_navigation (structure);

  if (G_LIKELY (GST_IS_EVENT (event))) {
    GstPad *pad;

    pad = gst_pad_get_peer (GST_VIDEO_SINK_PAD (sink));

    if (G_LIKELY (GST_IS_PAD (pad))) {
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

static void
gst_clapper_sink_navigation_interface_init (GstNavigationInterface *iface)
{
  iface->send_event = gst_clapper_sink_navigation_send_event;
}

static gboolean
gst_clapper_sink_propose_allocation (GstBaseSink *bsink, GstQuery *query)
{
  GstClapperSink *self = GST_CLAPPER_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;
  GstStructure *allocation_meta = NULL;
  gint display_width, display_height;

  //if (!self->display || !self->context)
  //  return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (!caps)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* Normal size of a frame */
  size = GST_VIDEO_INFO_SIZE (&info);

  if (need_pool) {
    GST_DEBUG_OBJECT (self, "Creating new pool");

    pool = gst_buffer_pool_new ();
    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (pool);
      goto config_failed;
    }
  }

  /* We need at least 3 buffers because we keep around the current one
   * for memory to stay valid during resizing and hold on to the pending one */
  gst_query_add_allocation_pool (query, pool, size, 3, 0);
  if (pool)
    gst_object_unref (pool);

  /* FIXME: Read calculated display sizes from widget */
  display_width = GST_VIDEO_INFO_WIDTH (&info);
  display_height = GST_VIDEO_INFO_HEIGHT (&info);

  if (display_width != 0 && display_height != 0) {
    GST_DEBUG_OBJECT (self, "Sending alloc query with size %dx%d",
        display_width, display_height);
    allocation_meta = gst_structure_new ("GstVideoOverlayCompositionMeta",
        "width", G_TYPE_UINT, display_width,
        "height", G_TYPE_UINT, display_height, NULL);
  }

  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, allocation_meta);

  if (allocation_meta)
    gst_structure_free (allocation_meta);

  /* We also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
no_caps:
  GST_DEBUG_OBJECT (bsink, "No caps specified");
  return FALSE;

invalid_caps:
  GST_DEBUG_OBJECT (bsink, "Invalid caps specified");
  return FALSE;

config_failed:
  GST_DEBUG_OBJECT (bsink, "Failed to set config");
  return FALSE;
}

static gboolean
gst_clapper_sink_start_on_main (GstClapperSink *self)
{
  GtkWidget *widget;

  /* Make sure widget is created */
  if (!(widget = gst_clapper_sink_get_widget (self)))
    return FALSE;

  /* After this point, self->obj will always be set */

  if (!GTK_IS_ROOT (gtk_widget_get_root (widget))) {
    GtkWidget *toplevel, *parent;
    gchar *win_title;

    if ((parent = gtk_widget_get_parent (widget))) {
      GtkWidget *temp_parent;

      while ((temp_parent = gtk_widget_get_parent (parent)))
        parent = temp_parent;
    }
    toplevel = (parent) ? parent : widget;

    /* User did not add widget its own UI, let's popup a new GtkWindow to
     * make "gst-launch-1.0" work. */
    self->window = (GtkWindow *) gtk_window_new ();

    win_title = g_strdup_printf ("Clapper Sink - GTK %u.%u.%u Window",
        gtk_get_major_version (),
        gtk_get_minor_version (),
        gtk_get_micro_version ());

    gtk_window_set_default_size (self->window, 640, 480);
    gtk_window_set_title (self->window, win_title);
    gtk_window_set_child (self->window, toplevel);

    g_free (win_title);

    self->window_destroy_id = g_signal_connect (self->window,
        "destroy", G_CALLBACK (window_destroy_cb), self);
  }

  return TRUE;
}

static gboolean
gst_clapper_sink_start (GstBaseSink *bsink)
{
  GstClapperSink *self = GST_CLAPPER_SINK (bsink);
  GtkClapperObject *obj = NULL;

  if (!(! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
      gst_clapper_sink_start_on_main, self)))
    return FALSE;

  //widget = GTK_CLAPPER_WIDGET (self->widget);

  GST_OBJECT_LOCK (self);
  if (self->obj)
    obj = g_object_ref (self->obj);
  GST_OBJECT_UNLOCK (self);

  if (G_UNLIKELY (obj == NULL)) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, NOT_FOUND, ("%s",
        "Clapper widget does not exist"), (NULL));
    return FALSE;
  }

  if (!gtk_clapper_object_init_winsys (obj)) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, NOT_FOUND, ("%s",
        "Failed to initialize OpenGL with GTK"), (NULL));
    return FALSE;
  }
/*
  if (!clapper_sink->display)
    clapper_sink->display = gtk_clapper_gl_widget_get_display (clapper_widget);
  if (!clapper_sink->context)
    clapper_sink->context = gtk_clapper_gl_widget_get_context (clapper_widget);
  if (!clapper_sink->gtk_context)
    clapper_sink->gtk_context = gtk_clapper_gl_widget_get_gtk_context (clapper_widget);

  if (!clapper_sink->display || !clapper_sink->context || !clapper_sink->gtk_context) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, NOT_FOUND, ("%s",
            "Failed to retrieve OpenGL context from GTK"), (NULL));
    return FALSE;
  }

  gst_gl_element_propagate_display_context (GST_ELEMENT (bsink),
      clapper_sink->display);
*/

  return TRUE;
}

static gboolean
gst_clapper_sink_stop_on_main (GstClapperSink *self)
{
  if (self->window) {
    gtk_window_destroy (self->window);
    self->window = NULL;
    //self->widget = NULL;
  }

  return TRUE;
}

static gboolean
gst_clapper_sink_stop (GstBaseSink *bsink)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);

  if (G_UNLIKELY (self->window != NULL)) {
    return ! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
        gst_clapper_sink_stop_on_main, self);
  }

  return TRUE;
}

static void
gst_gtk_window_show_all_and_unref (GtkWindow *window)
{
  gtk_window_present (window);
  g_object_unref (window);
}

static GstStateChangeReturn
gst_clapper_sink_change_state (GstElement *element, GstStateChange transition)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (self, "Changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (G_UNLIKELY (ret == GST_STATE_CHANGE_FAILURE))
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      GtkWindow *window = NULL;

      GST_OBJECT_LOCK (self);
      if (self->window)
        window = g_object_ref (self->window);
      GST_OBJECT_UNLOCK (self);

      if (window) {
        gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
            gst_gtk_window_show_all_and_unref, window);
      }
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (self);
      if (G_LIKELY (self->obj != NULL))
        gtk_clapper_object_set_buffer (self->obj, NULL);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_clapper_sink_get_times (GstBaseSink *bsink, GstBuffer *buffer,
    GstClockTime *start, GstClockTime *end)
{
  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    *start = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
      *end = *start + GST_BUFFER_DURATION (buffer);
    } else {
      GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);

      if (GST_VIDEO_INFO_FPS_N (&self->v_info) > 0) {
        *end = *start + gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&self->v_info),
            GST_VIDEO_INFO_FPS_N (&self->v_info));
      }
    }
  }
}

static GstCaps *
gst_clapper_sink_get_caps (GstBaseSink *bsink, GstCaps *filter)
{
  GstCaps *tmp = NULL;
  GstCaps *result = NULL;

  tmp = gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD (bsink));

  if (filter) {
    GST_DEBUG_OBJECT (bsink, "Intersecting with filter caps %" GST_PTR_FORMAT,
        filter);

    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  //result = gst_gl_overlay_compositor_add_caps (result);

  GST_DEBUG_OBJECT (bsink, "Returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_clapper_sink_set_caps (GstBaseSink *bsink, GstCaps *caps)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (bsink);

  GST_DEBUG ("Set caps: %" GST_PTR_FORMAT, caps);
  GST_OBJECT_LOCK (self);

  if (!gst_video_info_from_caps (&self->v_info, caps)) {
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }

  if (G_UNLIKELY (self->obj == NULL)) {
    GST_OBJECT_UNLOCK (self);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("%s", "Output widget was destroyed"), (NULL));
    return FALSE;
  }

  if (!gtk_clapper_object_set_format (self->obj, &self->v_info)) {
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static GstFlowReturn
gst_clapper_sink_show_frame (GstVideoSink *vsink, GstBuffer *buffer)
{
  GstClapperSink *self = GST_CLAPPER_SINK_CAST (vsink);

  GST_TRACE ("Rendering buffer: %p", buffer);
  GST_OBJECT_LOCK (self);

  if (G_UNLIKELY (self->obj == NULL)) {
    GST_OBJECT_UNLOCK (self);
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("%s", "Output widget was destroyed"), (NULL));
    return GST_FLOW_ERROR;
  }

  gtk_clapper_object_set_buffer (self->obj, buffer);
  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;
}
