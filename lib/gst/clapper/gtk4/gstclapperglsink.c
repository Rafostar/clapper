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

/**
 * SECTION:gstclapperglsink
 * @title: GstClapperGLSink
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglfuncs.h>

#include "gstclapperglsink.h"
#include "gstgtkutils.h"

GST_DEBUG_CATEGORY (gst_debug_clapper_gl_sink);
#define GST_CAT_DEFAULT gst_debug_clapper_gl_sink

#define GST_CLAPPER_GL_SINK_CAPS \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "              \
    "format = (string) RGBA, "                                          \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "                   \
    " ; "                                                               \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY ","                \
    GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION "), "           \
    "format = (string) RGBA, "                                          \
    "width = " GST_VIDEO_SIZE_RANGE ", "                                \
    "height = " GST_VIDEO_SIZE_RANGE ", "                               \
    "framerate = " GST_VIDEO_FPS_RANGE ", "                             \
    "texture-target = (string) { 2D, external-oes } "

static GstStaticPadTemplate gst_clapper_gl_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_CLAPPER_GL_SINK_CAPS));

static void gst_clapper_gl_sink_finalize (GObject * object);
static void gst_clapper_gl_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_clapper_gl_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_clapper_gl_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);
static gboolean gst_clapper_gl_sink_query (GstBaseSink * bsink, GstQuery * query);
static gboolean gst_clapper_gl_sink_start (GstBaseSink * bsink);
static gboolean gst_clapper_gl_sink_stop (GstBaseSink * bsink);
static GstFlowReturn gst_clapper_gl_sink_wait_event (GstBaseSink * bsink, GstEvent * event);

static GstStateChangeReturn
gst_clapper_gl_sink_change_state (GstElement * element,
    GstStateChange transition);

static void gst_clapper_gl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static GstCaps *gst_clapper_gl_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static gboolean gst_clapper_gl_sink_set_caps (GstBaseSink * bsink,
    GstCaps * caps);
static GstFlowReturn gst_clapper_gl_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);

static void
gst_clapper_gl_sink_navigation_interface_init (GstNavigationInterface * iface);

#define gst_clapper_gl_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstClapperGLSink, gst_clapper_gl_sink,
    GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_clapper_gl_sink_navigation_interface_init);
    GST_DEBUG_CATEGORY_INIT (gst_debug_clapper_gl_sink,
        "clapperglsink", 0, "Clapper GL Sink"));

static void
gst_clapper_gl_sink_class_init (GstClapperGLSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;
  GstClapperGLSinkClass *gstclapperglsink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;
  gstclapperglsink_class = (GstClapperGLSinkClass *) klass;

  gobject_class->set_property = gst_clapper_gl_sink_set_property;
  gobject_class->get_property = gst_clapper_gl_sink_get_property;
  gobject_class->finalize = gst_clapper_gl_sink_finalize;

  g_object_class_install_property (gobject_class, PROP_WIDGET,
      g_param_spec_object ("widget", "GTK Widget",
          "The GtkWidget to place in the widget hierarchy "
          "(must only be get from the GTK main thread)",
          GTK_TYPE_WIDGET, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_gtk_install_shared_properties (gobject_class);

  gstelement_class->change_state = gst_clapper_gl_sink_change_state;

  gstbasesink_class->get_caps = gst_clapper_gl_sink_get_caps;
  gstbasesink_class->set_caps = gst_clapper_gl_sink_set_caps;
  gstbasesink_class->get_times = gst_clapper_gl_sink_get_times;
  gstbasesink_class->propose_allocation = gst_clapper_gl_sink_propose_allocation;
  gstbasesink_class->query = gst_clapper_gl_sink_query;
  gstbasesink_class->start = gst_clapper_gl_sink_start;
  gstbasesink_class->stop = gst_clapper_gl_sink_stop;
  gstbasesink_class->wait_event = gst_clapper_gl_sink_wait_event;

  gstvideosink_class->show_frame = gst_clapper_gl_sink_show_frame;

  gstclapperglsink_class->create_widget = gtk_clapper_gl_widget_new;
  gstclapperglsink_class->window_title = "GTK4 GL Renderer";

  gst_element_class_set_metadata (gstelement_class,
      "GTK4 GL Video Sink",
      "Sink/Video", "A video sink that renders to a GtkWidget using OpenGL",
      "Matthew Waters <matthew@centricular.com>, "
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_gl_sink_template);

  gst_type_mark_as_plugin_api (GST_TYPE_CLAPPER_GL_SINK, 0);
}

static void
gst_clapper_gl_sink_init (GstClapperGLSink * clapper_sink)
{
  clapper_sink->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  clapper_sink->par_n = DEFAULT_PAR_N;
  clapper_sink->par_d = DEFAULT_PAR_D;
  clapper_sink->keep_last_frame = DEFAULT_KEEP_LAST_FRAME;

  clapper_sink->had_eos = FALSE;
}

static void
gst_clapper_gl_sink_finalize (GObject * object)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (object);

  GST_DEBUG ("Finalizing Clapper GL sink");

  GST_OBJECT_LOCK (clapper_sink);
  if (clapper_sink->window && clapper_sink->window_destroy_id)
    g_signal_handler_disconnect (clapper_sink->window, clapper_sink->window_destroy_id);
  if (clapper_sink->widget && clapper_sink->widget_destroy_id)
    g_signal_handler_disconnect (clapper_sink->widget, clapper_sink->widget_destroy_id);

  g_clear_object (&clapper_sink->widget);
  GST_OBJECT_UNLOCK (clapper_sink);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
widget_destroy_cb (GtkWidget * widget, GstClapperGLSink * clapper_sink)
{
  GST_OBJECT_LOCK (clapper_sink);
  g_clear_object (&clapper_sink->widget);
  GST_OBJECT_UNLOCK (clapper_sink);
}

static void
window_destroy_cb (GtkWidget * widget, GstClapperGLSink * clapper_sink)
{
  GST_OBJECT_LOCK (clapper_sink);
  if (clapper_sink->widget) {
    if (clapper_sink->widget_destroy_id) {
      g_signal_handler_disconnect (clapper_sink->widget,
          clapper_sink->widget_destroy_id);
      clapper_sink->widget_destroy_id = 0;
    }
    g_clear_object (&clapper_sink->widget);
  }
  clapper_sink->window = NULL;
  GST_OBJECT_UNLOCK (clapper_sink);
}

static GtkClapperGLWidget *
gst_clapper_gl_sink_get_widget (GstClapperGLSink * clapper_sink)
{
  if (clapper_sink->widget != NULL)
    return clapper_sink->widget;

  /* Ensure GTK is initialized, this has no side effect if it was already
   * initialized. Also, we do that lazily, so the application can be first */
  if (!gtk_init_check ()) {
    GST_ERROR_OBJECT (clapper_sink, "Could not ensure GTK initialization.");
    return NULL;
  }

  g_assert (GST_CLAPPER_GL_SINK_GET_CLASS (clapper_sink)->create_widget);
  clapper_sink->widget = (GtkClapperGLWidget *)
      GST_CLAPPER_GL_SINK_GET_CLASS (clapper_sink)->create_widget ();

  g_object_bind_property (clapper_sink, "force-aspect-ratio", clapper_sink->widget,
      "force-aspect-ratio", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property (clapper_sink, "pixel-aspect-ratio", clapper_sink->widget,
      "pixel-aspect-ratio", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property (clapper_sink, "keep-last-frame", clapper_sink->widget,
      "keep-last-frame", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* Take the floating ref, other wise the destruction of the container will
   * make this widget disappear possibly before we are done. */
  gst_object_ref_sink (clapper_sink->widget);

  clapper_sink->widget_destroy_id = g_signal_connect (clapper_sink->widget, "destroy",
      G_CALLBACK (widget_destroy_cb), clapper_sink);

  /* back pointer */
  gtk_clapper_gl_widget_set_element (GTK_CLAPPER_GL_WIDGET (clapper_sink->widget),
      GST_ELEMENT (clapper_sink));

  return clapper_sink->widget;
}

static void
gst_clapper_gl_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (object);

  switch (prop_id) {
    case PROP_WIDGET:
    {
      GObject *widget = NULL;

      GST_OBJECT_LOCK (clapper_sink);
      if (clapper_sink->widget != NULL)
        widget = G_OBJECT (clapper_sink->widget);
      GST_OBJECT_UNLOCK (clapper_sink);

      if (!widget)
        widget =
            gst_gtk_invoke_on_main ((GThreadFunc) gst_clapper_gl_sink_get_widget,
            clapper_sink);

      g_value_set_object (value, widget);
      break;
    }
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, clapper_sink->force_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, clapper_sink->par_n, clapper_sink->par_d);
      break;
    case PROP_KEEP_LAST_FRAME:
      g_value_set_boolean (value, clapper_sink->keep_last_frame);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_gl_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      clapper_sink->force_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      clapper_sink->par_n = gst_value_get_fraction_numerator (value);
      clapper_sink->par_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_KEEP_LAST_FRAME:
      clapper_sink->keep_last_frame = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_gl_sink_navigation_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  GstClapperGLSink *sink = GST_CLAPPER_GL_SINK (navigation);
  GstEvent *event;
  GstPad *pad;

  event = gst_event_new_navigation (structure);
  pad = gst_pad_get_peer (GST_VIDEO_SINK_PAD (sink));

  GST_TRACE_OBJECT (sink, "navigation event %" GST_PTR_FORMAT, structure);

  if (GST_IS_PAD (pad) && GST_IS_EVENT (event)) {
    if (!gst_pad_send_event (pad, gst_event_ref (event))) {
      /* If upstream didn't handle the event we'll post a message with it
       * for the application in case it wants to do something with it */
      gst_element_post_message (GST_ELEMENT_CAST (sink),
          gst_navigation_message_new_event (GST_OBJECT_CAST (sink), event));
    }
    gst_event_unref (event);
    gst_object_unref (pad);
  }
}

static void
gst_clapper_gl_sink_navigation_interface_init (GstNavigationInterface * iface)
{
  iface->send_event = gst_clapper_gl_sink_navigation_send_event;
}

static gboolean
gst_clapper_gl_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;
  GstStructure *allocation_meta = NULL;
  gint display_width, display_height;

  if (!clapper_sink->display || !clapper_sink->context)
    return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GST_DEBUG_OBJECT (clapper_sink, "create new pool");
    pool = gst_gl_buffer_pool_new (clapper_sink->context);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);

    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (pool);
      goto config_failed;
    }
  }

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  GST_OBJECT_LOCK (clapper_sink);
  display_width = clapper_sink->display_width;
  display_height = clapper_sink->display_height;
  GST_OBJECT_UNLOCK (clapper_sink);

  if (display_width != 0 && display_height != 0) {
    GST_DEBUG_OBJECT (clapper_sink, "sending alloc query with size %dx%d",
        display_width, display_height);
    allocation_meta = gst_structure_new ("GstVideoOverlayCompositionMeta",
        "width", G_TYPE_UINT, display_width,
        "height", G_TYPE_UINT, display_height, NULL);
  }

  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, allocation_meta);

  if (allocation_meta)
    gst_structure_free (allocation_meta);

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, 0);

  if (clapper_sink->context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, 0);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    return FALSE;
  }
}

static gboolean
gst_clapper_gl_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      res = gst_gl_handle_context_query ((GstElement *) clapper_sink, query,
          clapper_sink->display, clapper_sink->context, clapper_sink->gtk_context);
      break;
    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }

  return res;
}

static gboolean
gst_clapper_gl_sink_start_on_main (GstBaseSink * bsink)
{
  GstClapperGLSink *gst_sink = GST_CLAPPER_GL_SINK (bsink);
  GstClapperGLSinkClass *klass = GST_CLAPPER_GL_SINK_GET_CLASS (bsink);
  GtkWidget *toplevel;
  GtkRoot *root;

  if (gst_clapper_gl_sink_get_widget (gst_sink) == NULL)
    return FALSE;

  /* After this point, clapper_sink->widget will always be set */

  root = gtk_widget_get_root (GTK_WIDGET (gst_sink->widget));
  if (!GTK_IS_ROOT (root)) {
    GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (gst_sink->widget));
    if (parent) {
      GtkWidget *temp_parent;
      while ((temp_parent = gtk_widget_get_parent (parent)))
        parent = temp_parent;
    }
    toplevel = (parent) ? parent : GTK_WIDGET (gst_sink->widget);

    /* sanity check */
    g_assert (klass->window_title);

    /* User did not add widget its own UI, let's popup a new GtkWindow to
     * make gst-launch-1.0 work. */
    gst_sink->window = gtk_window_new ();
    gtk_window_set_default_size (GTK_WINDOW (gst_sink->window), 640, 480);
    gtk_window_set_title (GTK_WINDOW (gst_sink->window), klass->window_title);
    gtk_window_set_child (GTK_WINDOW (gst_sink->window), toplevel);

    gst_sink->window_destroy_id = g_signal_connect (
        GTK_WINDOW (gst_sink->window),
        "destroy", G_CALLBACK (window_destroy_cb), gst_sink);
  }

  return TRUE;
}

static gboolean
gst_clapper_gl_sink_start (GstBaseSink * bsink)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);
  GtkClapperGLWidget *clapper_widget;

  if (!(! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
      gst_clapper_gl_sink_start_on_main, bsink)))
    return FALSE;

  /* After this point, clapper_sink->widget will always be set */
  clapper_widget = GTK_CLAPPER_GL_WIDGET (clapper_sink->widget);

  if (!gtk_clapper_gl_widget_init_winsys (clapper_widget)) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, NOT_FOUND, ("%s",
            "Failed to initialize OpenGL with GTK"), (NULL));
    return FALSE;
  }

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

  return TRUE;
}

static gboolean
gst_clapper_gl_sink_stop_on_main (GstBaseSink * bsink)
{
  GstClapperGLSink *gst_sink = GST_CLAPPER_GL_SINK (bsink);

  if (gst_sink->window) {
    gtk_window_destroy (GTK_WINDOW (gst_sink->window));
    gst_sink->window = NULL;
    gst_sink->widget = NULL;
  }

  return TRUE;
}

static gboolean
gst_clapper_gl_sink_stop (GstBaseSink * bsink)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);

  if (clapper_sink->display) {
    gst_object_unref (clapper_sink->display);
    clapper_sink->display = NULL;
  }
  if (clapper_sink->context) {
    gst_object_unref (clapper_sink->context);
    clapper_sink->context = NULL;
  }
  if (clapper_sink->gtk_context) {
    gst_object_unref (clapper_sink->gtk_context);
    clapper_sink->gtk_context = NULL;
  }
  if (clapper_sink->window)
    return ! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
        gst_clapper_gl_sink_stop_on_main, bsink);

  return TRUE;
}

static void
gst_gtk_window_show_all_and_unref (GtkWidget * window)
{
  gtk_window_present (GTK_WINDOW (window));
  g_object_unref (window);
}

static GstStateChangeReturn
gst_clapper_gl_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (element, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_OBJECT_LOCK (clapper_sink);
      clapper_sink->had_eos = FALSE;
      if (clapper_sink->widget) {
        GTK_CLAPPER_GL_WIDGET_LOCK (clapper_sink->widget);
        clapper_sink->widget->ignore_buffers = FALSE;
        GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_sink->widget);
      }
      GST_OBJECT_UNLOCK (clapper_sink);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      GtkWindow *window = NULL;

      GST_OBJECT_LOCK (clapper_sink);
      if (clapper_sink->window)
        window = g_object_ref (GTK_WINDOW (clapper_sink->window));
      GST_OBJECT_UNLOCK (clapper_sink);

      if (window) {
        gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
            gst_gtk_window_show_all_and_unref, window);
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_OBJECT_LOCK (clapper_sink);
      if (clapper_sink->widget) {
        GTK_CLAPPER_GL_WIDGET_LOCK (clapper_sink->widget);
        clapper_sink->widget->ignore_buffers =
            !clapper_sink->had_eos || !clapper_sink->keep_last_frame;
        GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_sink->widget);
      }
      GST_OBJECT_UNLOCK (clapper_sink);
      /* Fall through to render black bg */
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (clapper_sink);
      if (clapper_sink->widget)
        gtk_clapper_gl_widget_set_buffer (clapper_sink->widget, NULL);
      GST_OBJECT_UNLOCK (clapper_sink);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_clapper_gl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&clapper_sink->v_info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&clapper_sink->v_info),
            GST_VIDEO_INFO_FPS_N (&clapper_sink->v_info));
      }
    }
  }
}

static GstCaps *
gst_clapper_gl_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstCaps *tmp = NULL;
  GstCaps *result = NULL;

  tmp = gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD (bsink));

  if (filter) {
    GST_DEBUG_OBJECT (bsink, "intersecting with filter caps %" GST_PTR_FORMAT,
        filter);

    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  result = gst_gl_overlay_compositor_add_caps (result);

  GST_DEBUG_OBJECT (bsink, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_clapper_gl_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);
  gboolean res = FALSE;

  GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&clapper_sink->v_info, caps))
    return FALSE;

  GST_OBJECT_LOCK (clapper_sink);

  if (clapper_sink->widget == NULL) {
    GST_OBJECT_UNLOCK (clapper_sink);
    GST_ELEMENT_ERROR (clapper_sink, RESOURCE, NOT_FOUND,
        ("%s", "Output widget was destroyed"), (NULL));
    return FALSE;
  }

  if (!gtk_clapper_gl_widget_set_format (clapper_sink->widget, &clapper_sink->v_info)) {
    GST_OBJECT_UNLOCK (clapper_sink);
    return FALSE;
  }

  res = gtk_clapper_gl_widget_update_output_format (clapper_sink->widget, caps);
  GST_OBJECT_UNLOCK (clapper_sink);

  return res;
}

static GstFlowReturn
gst_clapper_gl_sink_wait_event (GstBaseSink * bsink, GstEvent * event)
{
  GstClapperGLSink *clapper_sink = GST_CLAPPER_GL_SINK (bsink);
  GstFlowReturn ret;

  ret = GST_BASE_SINK_CLASS (parent_class)->wait_event (bsink, event);

  switch (event->type) {
    case GST_EVENT_EOS:
      if (ret == GST_FLOW_OK) {
        GST_OBJECT_LOCK (clapper_sink);
        clapper_sink->had_eos = TRUE;
        GST_OBJECT_UNLOCK (clapper_sink);
      }
      break;
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_clapper_gl_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstClapperGLSink *clapper_sink;

  GST_TRACE ("rendering buffer:%p", buf);

  clapper_sink = GST_CLAPPER_GL_SINK (vsink);

  GST_OBJECT_LOCK (clapper_sink);

  if (clapper_sink->widget == NULL) {
    GST_OBJECT_UNLOCK (clapper_sink);
    GST_ELEMENT_ERROR (clapper_sink, RESOURCE, NOT_FOUND,
        ("%s", "Output widget was destroyed"), (NULL));
    return GST_FLOW_ERROR;
  }

  gtk_clapper_gl_widget_set_buffer (clapper_sink->widget, buf);

  GST_OBJECT_UNLOCK (clapper_sink);

  return GST_FLOW_OK;
}
