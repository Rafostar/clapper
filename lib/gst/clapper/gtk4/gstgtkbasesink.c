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
 * SECTION:gtkgstsink
 * @title: GstGtkBaseSink
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglfuncs.h>

#include "gtkconfig.h"
#include "gstgtkbasesink.h"
#include "gstgtkutils.h"

GST_DEBUG_CATEGORY (gst_debug_gtk_base_sink);
#define GST_CAT_DEFAULT gst_debug_gtk_base_sink

#define DEFAULT_FORCE_ASPECT_RATIO  TRUE
#define DEFAULT_PAR_N               0
#define DEFAULT_PAR_D               1
#define DEFAULT_IGNORE_TEXTURES     FALSE

static GstStaticPadTemplate gst_gtk_base_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, "RGBA") "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY ", "
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, "RGBA")));

static void gst_gtk_base_sink_finalize (GObject * object);
static void gst_gtk_base_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_gtk_base_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_gtk_base_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);
static gboolean gst_gtk_base_sink_query (GstBaseSink * bsink, GstQuery * query);
static gboolean gst_gtk_base_sink_start (GstBaseSink * bsink);
static gboolean gst_gtk_base_sink_stop (GstBaseSink * bsink);

static GstStateChangeReturn
gst_gtk_base_sink_change_state (GstElement * element,
    GstStateChange transition);

static void gst_gtk_base_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static GstCaps *gst_gtk_base_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static gboolean gst_gtk_base_sink_set_caps (GstBaseSink * bsink,
    GstCaps * caps);
static GstFlowReturn gst_gtk_base_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);

static void
gst_gtk_base_sink_navigation_interface_init (GstNavigationInterface * iface);

enum
{
  PROP_0,
  PROP_WIDGET,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_IGNORE_TEXTURES,
};

#define gst_gtk_base_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtkBaseSink, gst_gtk_base_sink,
    GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_gtk_base_sink_navigation_interface_init);
    GST_DEBUG_CATEGORY_INIT (gst_debug_gtk_base_sink,
        "gtkbasesink", 0, "GTK Video Sink base class"));

static void
gst_gtk_base_sink_class_init (GstGtkBaseSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;
  GstGtkBaseSinkClass *gstgtkbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;
  gstgtkbasesink_class = (GstGtkBaseSinkClass *) klass;

  gobject_class->set_property = gst_gtk_base_sink_set_property;
  gobject_class->get_property = gst_gtk_base_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_WIDGET,
      g_param_spec_object ("widget", "GTK Widget",
          "The GtkWidget to place in the widget hierarchy "
          "(must only be get from the GTK main thread)",
          GTK_TYPE_WIDGET, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", DEFAULT_PAR_N, DEFAULT_PAR_D,
          G_MAXINT, 1, 1, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_IGNORE_TEXTURES,
      g_param_spec_boolean ("ignore-textures", "Ignore Textures",
          "When enabled, textures will be ignored and not drawn",
          DEFAULT_IGNORE_TEXTURES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_gtk_base_sink_finalize;

  gstelement_class->change_state = gst_gtk_base_sink_change_state;

  gstbasesink_class->get_caps = gst_gtk_base_sink_get_caps;
  gstbasesink_class->set_caps = gst_gtk_base_sink_set_caps;
  gstbasesink_class->get_times = gst_gtk_base_sink_get_times;
  gstbasesink_class->propose_allocation = gst_gtk_base_sink_propose_allocation;
  gstbasesink_class->query = gst_gtk_base_sink_query;
  gstbasesink_class->start = gst_gtk_base_sink_start;
  gstbasesink_class->stop = gst_gtk_base_sink_stop;

  gstvideosink_class->show_frame = gst_gtk_base_sink_show_frame;

  gstgtkbasesink_class->create_widget = gtk_gst_base_widget_new;
  gstgtkbasesink_class->window_title = GTKCONFIG_NAME " GL Renderer";

  gst_element_class_set_metadata (gstelement_class,
      GTKCONFIG_NAME " GL Video Sink",
      "Sink/Video", "A video sink that renders to a GtkWidget using OpenGL",
      "Matthew Waters <matthew@centricular.com>, "
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_gtk_base_sink_template);

  gst_type_mark_as_plugin_api (GST_TYPE_GTK_BASE_SINK, 0);
}

static void
gst_gtk_base_sink_init (GstGtkBaseSink * gtk_sink)
{
  gtk_sink->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  gtk_sink->par_n = DEFAULT_PAR_N;
  gtk_sink->par_d = DEFAULT_PAR_D;
  gtk_sink->ignore_textures = DEFAULT_IGNORE_TEXTURES;
}

static void
gst_gtk_base_sink_finalize (GObject * object)
{
  GstGtkBaseSink *base_sink = GST_GTK_BASE_SINK (object);

  GST_DEBUG ("finalizing base sink");

  GST_OBJECT_LOCK (base_sink);
  if (base_sink->window && base_sink->window_destroy_id)
    g_signal_handler_disconnect (base_sink->window, base_sink->window_destroy_id);
  if (base_sink->widget && base_sink->widget_destroy_id)
    g_signal_handler_disconnect (base_sink->widget, base_sink->widget_destroy_id);

  g_clear_object (&base_sink->widget);
  GST_OBJECT_UNLOCK (base_sink);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
widget_destroy_cb (GtkWidget * widget, GstGtkBaseSink * gtk_sink)
{
  GST_OBJECT_LOCK (gtk_sink);
  g_clear_object (&gtk_sink->widget);
  GST_OBJECT_UNLOCK (gtk_sink);
}

static void
window_destroy_cb (GtkWidget * widget, GstGtkBaseSink * gtk_sink)
{
  GST_OBJECT_LOCK (gtk_sink);
  if (gtk_sink->widget) {
    if (gtk_sink->widget_destroy_id) {
      g_signal_handler_disconnect (gtk_sink->widget,
          gtk_sink->widget_destroy_id);
      gtk_sink->widget_destroy_id = 0;
    }
    g_clear_object (&gtk_sink->widget);
  }
  gtk_sink->window = NULL;
  GST_OBJECT_UNLOCK (gtk_sink);
}

static GtkGstBaseWidget *
gst_gtk_base_sink_get_widget (GstGtkBaseSink * gtk_sink)
{
  if (gtk_sink->widget != NULL)
    return gtk_sink->widget;

  /* Ensure GTK is initialized, this has no side effect if it was already
   * initialized. Also, we do that lazily, so the application can be first */
  if (!gtk_init_check ()) {
    GST_ERROR_OBJECT (gtk_sink, "Could not ensure GTK initialization.");
    return NULL;
  }

  g_assert (GST_GTK_BASE_SINK_GET_CLASS (gtk_sink)->create_widget);
  gtk_sink->widget = (GtkGstBaseWidget *)
      GST_GTK_BASE_SINK_GET_CLASS (gtk_sink)->create_widget ();

  gtk_sink->bind_aspect_ratio =
      g_object_bind_property (gtk_sink, "force-aspect-ratio", gtk_sink->widget,
      "force-aspect-ratio", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_sink->bind_pixel_aspect_ratio =
      g_object_bind_property (gtk_sink, "pixel-aspect-ratio", gtk_sink->widget,
      "pixel-aspect-ratio", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_sink->bind_ignore_textures =
      g_object_bind_property (gtk_sink, "ignore-textures", gtk_sink->widget,
      "ignore-textures", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* Take the floating ref, other wise the destruction of the container will
   * make this widget disappear possibly before we are done. */
  gst_object_ref_sink (gtk_sink->widget);

  gtk_sink->widget_destroy_id = g_signal_connect (gtk_sink->widget, "destroy",
      G_CALLBACK (widget_destroy_cb), gtk_sink);

  /* back pointer */
  gtk_gst_base_widget_set_element (GTK_GST_BASE_WIDGET (gtk_sink->widget),
      GST_ELEMENT (gtk_sink));

  return gtk_sink->widget;
}

static void
gst_gtk_base_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGtkBaseSink *gtk_sink = GST_GTK_BASE_SINK (object);

  switch (prop_id) {
    case PROP_WIDGET:
    {
      GObject *widget = NULL;

      GST_OBJECT_LOCK (gtk_sink);
      if (gtk_sink->widget != NULL)
        widget = G_OBJECT (gtk_sink->widget);
      GST_OBJECT_UNLOCK (gtk_sink);

      if (!widget)
        widget =
            gst_gtk_invoke_on_main ((GThreadFunc) gst_gtk_base_sink_get_widget,
            gtk_sink);

      g_value_set_object (value, widget);
      break;
    }
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, gtk_sink->force_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, gtk_sink->par_n, gtk_sink->par_d);
      break;
    case PROP_IGNORE_TEXTURES:
      g_value_set_boolean (value, gtk_sink->ignore_textures);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gtk_base_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGtkBaseSink *gtk_sink = GST_GTK_BASE_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      gtk_sink->force_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gtk_sink->par_n = gst_value_get_fraction_numerator (value);
      gtk_sink->par_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_IGNORE_TEXTURES:
      gtk_sink->ignore_textures = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gtk_base_sink_navigation_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  GstGtkBaseSink *sink = GST_GTK_BASE_SINK (navigation);
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
gst_gtk_base_sink_navigation_interface_init (GstNavigationInterface * iface)
{
  iface->send_event = gst_gtk_base_sink_navigation_send_event;
}

static gboolean
gst_gtk_base_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstGtkBaseSink *base_sink = GST_GTK_BASE_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;
  GstStructure *allocation_meta = NULL;
  gint display_width, display_height;

  if (!base_sink->display || !base_sink->context)
    return FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  /* the normal size of a frame */
  size = info.size;

  if (need_pool) {
    GST_DEBUG_OBJECT (base_sink, "create new pool");
    pool = gst_gl_buffer_pool_new (base_sink->context);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);

    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }

  /* we need at least 2 buffer because we hold on to the last one */
  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  GST_OBJECT_LOCK (base_sink);
  display_width = base_sink->display_width;
  display_height = base_sink->display_height;
  GST_OBJECT_UNLOCK (base_sink);

  if (display_width != 0 && display_height != 0) {
    GST_DEBUG_OBJECT (base_sink, "sending alloc query with size %dx%d",
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

  if (base_sink->context->gl_vtable->FenceSync)
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
gst_gtk_base_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstGtkBaseSink *base_sink = GST_GTK_BASE_SINK (bsink);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      if (gst_gl_handle_context_query ((GstElement *) base_sink, query,
              base_sink->display, base_sink->context, base_sink->gtk_context))
        return TRUE;
      break;
    }
    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }

  return res;
}

static gboolean
gst_gtk_base_sink_start_on_main (GstBaseSink * bsink)
{
  GstGtkBaseSink *gst_sink = GST_GTK_BASE_SINK (bsink);
  GstGtkBaseSinkClass *klass = GST_GTK_BASE_SINK_GET_CLASS (bsink);
  GtkWidget *toplevel;
  GtkRoot *root;

  if (gst_gtk_base_sink_get_widget (gst_sink) == NULL)
    return FALSE;

  /* After this point, base_sink->widget will always be set */

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
gst_gtk_base_sink_start (GstBaseSink * bsink)
{
  GstGtkBaseSink *base_sink = GST_GTK_BASE_SINK (bsink);
  GtkGstBaseWidget *base_widget;

  if (!(! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
      gst_gtk_base_sink_start_on_main, bsink)))
    return FALSE;

  /* After this point, base_sink->widget will always be set */
  base_widget = GTK_GST_BASE_WIDGET (base_sink->widget);

  if (!gtk_gst_base_widget_init_winsys (base_widget)) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, NOT_FOUND, ("%s",
            "Failed to initialize OpenGL with GTK"), (NULL));
    return FALSE;
  }

  if (!base_sink->display)
    base_sink->display = gtk_gst_base_widget_get_display (base_widget);
  if (!base_sink->context)
    base_sink->context = gtk_gst_base_widget_get_context (base_widget);
  if (!base_sink->gtk_context)
    base_sink->gtk_context = gtk_gst_base_widget_get_gtk_context (base_widget);

  if (!base_sink->display || !base_sink->context || !base_sink->gtk_context) {
    GST_ELEMENT_ERROR (bsink, RESOURCE, NOT_FOUND, ("%s",
            "Failed to retrieve OpenGL context from GTK"), (NULL));
    return FALSE;
  }

  gst_gl_element_propagate_display_context (GST_ELEMENT (bsink),
      base_sink->display);

  return TRUE;
}

static gboolean
gst_gtk_base_sink_stop_on_main (GstBaseSink * bsink)
{
  GstGtkBaseSink *gst_sink = GST_GTK_BASE_SINK (bsink);

  if (gst_sink->window) {
    gtk_window_destroy (GTK_WINDOW (gst_sink->window));
    gst_sink->window = NULL;
    gst_sink->widget = NULL;
  }

  return TRUE;
}

static gboolean
gst_gtk_base_sink_stop (GstBaseSink * bsink)
{
  GstGtkBaseSink *base_sink = GST_GTK_BASE_SINK (bsink);

  if (base_sink->display) {
    gst_object_unref (base_sink->display);
    base_sink->display = NULL;
  }
  if (base_sink->context) {
    gst_object_unref (base_sink->context);
    base_sink->context = NULL;
  }
  if (base_sink->gtk_context) {
    gst_object_unref (base_sink->gtk_context);
    base_sink->gtk_context = NULL;
  }
  if (base_sink->window)
    return ! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
        gst_gtk_base_sink_stop_on_main, bsink);

  return TRUE;
}

static void
gst_gtk_window_show_all_and_unref (GtkWidget * window)
{
  gtk_window_present (GTK_WINDOW (window));
  g_object_unref (window);
}

static GstStateChangeReturn
gst_gtk_base_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstGtkBaseSink *gtk_sink = GST_GTK_BASE_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (element, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      GtkWindow *window = NULL;

      GST_OBJECT_LOCK (gtk_sink);
      if (gtk_sink->window)
        window = g_object_ref (GTK_WINDOW (gtk_sink->window));
      GST_OBJECT_UNLOCK (gtk_sink);

      if (window) {
        gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
            gst_gtk_window_show_all_and_unref, window);
      }
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (gtk_sink);
      if (gtk_sink->widget)
        gtk_gst_base_widget_set_buffer (gtk_sink->widget, NULL);
      GST_OBJECT_UNLOCK (gtk_sink);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_gtk_base_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstGtkBaseSink *gtk_sink;

  gtk_sink = GST_GTK_BASE_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&gtk_sink->v_info),
            GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info));
      }
    }
  }
}

static GstCaps *
gst_gtk_base_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
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
gst_gtk_base_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstGtkBaseSink *gtk_sink = GST_GTK_BASE_SINK (bsink);

  GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&gtk_sink->v_info, caps))
    return FALSE;

  GST_OBJECT_LOCK (gtk_sink);

  if (gtk_sink->widget == NULL) {
    GST_OBJECT_UNLOCK (gtk_sink);
    GST_ELEMENT_ERROR (gtk_sink, RESOURCE, NOT_FOUND,
        ("%s", "Output widget was destroyed"), (NULL));
    return FALSE;
  }

  if (!gtk_gst_base_widget_set_format (gtk_sink->widget, &gtk_sink->v_info)) {
    GST_OBJECT_UNLOCK (gtk_sink);
    return FALSE;
  }
  GST_OBJECT_UNLOCK (gtk_sink);

  return TRUE;
}

static GstFlowReturn
gst_gtk_base_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstGtkBaseSink *gtk_sink;

  GST_TRACE ("rendering buffer:%p", buf);

  gtk_sink = GST_GTK_BASE_SINK (vsink);

  GST_OBJECT_LOCK (gtk_sink);

  if (gtk_sink->widget == NULL) {
    GST_OBJECT_UNLOCK (gtk_sink);
    GST_ELEMENT_ERROR (gtk_sink, RESOURCE, NOT_FOUND,
        ("%s", "Output widget was destroyed"), (NULL));
    return GST_FLOW_ERROR;
  }

  gtk_gst_base_widget_set_buffer (gtk_sink->widget, buf);

  GST_OBJECT_UNLOCK (gtk_sink);

  return GST_FLOW_OK;
}
