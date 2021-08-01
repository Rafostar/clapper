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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoaffinetransformationmeta.h>

#include "gtkclapperglwidget.h"
#include "gstgtkutils.h"
#include "gstclapperglutils.h"

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
#include <gdk/x11/gdkx.h>
#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif
#if GST_GL_HAVE_PLATFORM_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#endif

#if GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND)
#include <gdk/wayland/gdkwayland.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

/**
 * SECTION:gtkclapperglwidget
 * @title: GtkClapperGLWidget
 * @short_description: a #GtkGLArea that renders GStreamer video #GstBuffers
 * @see_also: #GtkGLArea, #GstBuffer
 *
 * #GtkClapperGLWidget is a #GtkWidget that renders GStreamer video buffers.
 */

GST_DEBUG_CATEGORY (gst_debug_clapper_gl_widget);
#define GST_CAT_DEFAULT gst_debug_clapper_gl_widget

struct _GtkClapperGLWidgetPrivate
{
  gboolean initiated;

  GstGLDisplay *display;
  GdkGLContext *gdk_context;
  GstGLContext *other_context;
  GstGLContext *context;

  GstGLTextureTarget texture_target;
  guint gl_target;

  GstGLUpload *upload;
  GstGLShader *shader;

  GLuint vao;
  GLuint vertex_buffer;
  GLint attr_position;
  GLint attr_texture;
  GLuint current_tex;

  GstGLOverlayCompositor *overlay_compositor;
};

static const GLfloat vertices[] = {
  1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
  -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
  1.0f, -1.0f, 0.0f, 1.0f, 1.0f
};

static const GLushort indices[] = {
  0, 1, 2, 0, 2, 3
};

G_DEFINE_TYPE_WITH_CODE (GtkClapperGLWidget, gtk_clapper_gl_widget, GTK_TYPE_GL_AREA,
    G_ADD_PRIVATE (GtkClapperGLWidget)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gtkclapperglwidget", 0,
        "GTK Clapper GL Widget"));

static void
gtk_clapper_gl_widget_get_preferred_width (GtkWidget * widget, gint * min,
    gint * natural)
{
  GtkClapperGLWidget *clapper_widget = (GtkClapperGLWidget *) widget;
  gint video_width = clapper_widget->display_width;

  if (!clapper_widget->negotiated)
    video_width = 10;

  if (min)
    *min = 1;
  if (natural)
    *natural = video_width;
}

static void
gtk_clapper_gl_widget_get_preferred_height (GtkWidget * widget, gint * min,
    gint * natural)
{
  GtkClapperGLWidget *clapper_widget = (GtkClapperGLWidget *) widget;
  gint video_height = clapper_widget->display_height;

  if (!clapper_widget->negotiated)
    video_height = 10;

  if (min)
    *min = 1;
  if (natural)
    *natural = video_height;
}

static void
gtk_clapper_gl_widget_measure (GtkWidget * widget, GtkOrientation orientation,
    gint for_size, gint * min, gint * natural,
    gint * minimum_baseline, gint * natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_clapper_gl_widget_get_preferred_width (widget, min, natural);
  else
    gtk_clapper_gl_widget_get_preferred_height (widget, min, natural);

  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
gtk_clapper_gl_widget_size_allocate (GtkWidget * widget,
    gint width, gint height, gint baseline)
{
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (widget);
  gint scale_factor = gtk_widget_get_scale_factor (widget);

  clapper_widget->scaled_width = width * scale_factor;
  clapper_widget->scaled_height = height * scale_factor;

  gtk_gl_area_queue_render (GTK_GL_AREA (widget));
}

static void
gtk_clapper_gl_widget_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      clapper_widget->force_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      clapper_widget->par_n = gst_value_get_fraction_numerator (value);
      clapper_widget->par_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_KEEP_LAST_FRAME:
      clapper_widget->keep_last_frame = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtk_clapper_gl_widget_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, clapper_widget->force_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, clapper_widget->par_n, clapper_widget->par_d);
      break;
    case PROP_KEEP_LAST_FRAME:
      g_value_set_boolean (value, clapper_widget->keep_last_frame);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
_calculate_par (GtkClapperGLWidget * clapper_widget, GstVideoInfo * info)
{
  gboolean ok;
  gint width, height;
  gint par_n, par_d;
  gint display_par_n, display_par_d;

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  par_n = GST_VIDEO_INFO_PAR_N (info);
  par_d = GST_VIDEO_INFO_PAR_D (info);

  if (!par_n)
    par_n = 1;

  /* get display's PAR */
  if (clapper_widget->par_n != 0 && clapper_widget->par_d != 0) {
    display_par_n = clapper_widget->par_n;
    display_par_d = clapper_widget->par_d;
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

  ok = gst_video_calculate_display_ratio (&clapper_widget->display_ratio_num,
      &clapper_widget->display_ratio_den, width, height, par_n, par_d, display_par_n,
      display_par_d);

  if (ok) {
    GST_LOG ("PAR: %u/%u DAR:%u/%u", par_n, par_d, display_par_n,
        display_par_d);
    return TRUE;
  }

  return FALSE;
}

static void
_apply_par (GtkClapperGLWidget * clapper_widget)
{
  guint display_ratio_num, display_ratio_den;
  gint width, height;

  width = GST_VIDEO_INFO_WIDTH (&clapper_widget->v_info);
  height = GST_VIDEO_INFO_HEIGHT (&clapper_widget->v_info);

  display_ratio_num = clapper_widget->display_ratio_num;
  display_ratio_den = clapper_widget->display_ratio_den;

  if (height % display_ratio_den == 0) {
    GST_DEBUG ("keeping video height");
    clapper_widget->display_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    clapper_widget->display_height = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG ("keeping video width");
    clapper_widget->display_width = width;
    clapper_widget->display_height = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG ("approximating while keeping video height");
    clapper_widget->display_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    clapper_widget->display_height = height;
  }

  GST_DEBUG ("scaling to %dx%d", clapper_widget->display_width, clapper_widget->display_height);
}

static gboolean
_queue_draw (GtkClapperGLWidget * clapper_widget)
{
  GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);
  clapper_widget->draw_id = 0;

  if (clapper_widget->pending_resize) {
    clapper_widget->pending_resize = FALSE;

    clapper_widget->v_info = clapper_widget->pending_v_info;
    clapper_widget->negotiated = TRUE;

    _apply_par (clapper_widget);

    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);

    gtk_widget_queue_resize (GTK_WIDGET (clapper_widget));
  } else {
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);

    gtk_gl_area_queue_render (GTK_GL_AREA (clapper_widget));
  }

  return G_SOURCE_REMOVE;
}

static const gchar *
_gdk_key_to_navigation_string (guint keyval)
{
  /* TODO: expand */
  switch (keyval) {
#define KEY(key) case GDK_KEY_ ## key: return G_STRINGIFY(key)
      KEY (Up);
      KEY (Down);
      KEY (Left);
      KEY (Right);
      KEY (Home);
      KEY (End);
#undef KEY
    default:
      return NULL;
  }
}

static gboolean
_get_is_navigation_allowed (GstElement * element, GstState min_state)
{
  if (GST_IS_NAVIGATION (element)) {
    GstState nav_state;

    GST_OBJECT_LOCK (element);
    nav_state = element->current_state;
    GST_OBJECT_UNLOCK (element);

    return nav_state >= min_state;
  }

  return FALSE;
}

static gboolean
gtk_clapper_gl_widget_key_event (GtkEventControllerKey * key_controller,
    guint keyval, guint keycode, GdkModifierType state)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (key_controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (widget);
  GstElement *element;

  if ((element = g_weak_ref_get (&clapper_widget->element))) {
    if (_get_is_navigation_allowed (element, GST_STATE_PAUSED)) {
      GdkEvent *event = gtk_event_controller_get_current_event (controller);
      const gchar *str = _gdk_key_to_navigation_string (keyval);

      if (str) {
        const gchar *key_type =
            gdk_event_get_event_type (event) ==
            GDK_KEY_PRESS ? "key-press" : "key-release";
        gst_navigation_send_key_event (GST_NAVIGATION (element), key_type, str);
      }
    }
    g_object_unref (element);
  }

  return FALSE;
}

static void
_fit_stream_to_allocated_size (GtkClapperGLWidget * clapper_widget, GstVideoRectangle * result)
{
  if (clapper_widget->force_aspect_ratio) {
    GstVideoRectangle src, dst;

    src.x = 0;
    src.y = 0;
    src.w = clapper_widget->display_width;
    src.h = clapper_widget->display_height;

    dst.x = 0;
    dst.y = 0;
    dst.w = clapper_widget->scaled_width;
    dst.h = clapper_widget->scaled_height;

    gst_video_sink_center_rect (src, dst, result, TRUE);
  } else {
    result->x = 0;
    result->y = 0;
    result->w = clapper_widget->scaled_width;
    result->h = clapper_widget->scaled_height;
  }
}

static void
_display_size_to_stream_size (GtkClapperGLWidget * clapper_widget, gdouble x,
    gdouble y, gdouble * stream_x, gdouble * stream_y)
{
  gdouble stream_width, stream_height;
  GstVideoRectangle result;

  _fit_stream_to_allocated_size (clapper_widget, &result);

  stream_width = (gdouble) GST_VIDEO_INFO_WIDTH (&clapper_widget->v_info);
  stream_height = (gdouble) GST_VIDEO_INFO_HEIGHT (&clapper_widget->v_info);

  /* from display coordinates to stream coordinates */
  if (result.w > 0)
    *stream_x = (x - result.x) / result.w * stream_width;
  else
    *stream_x = 0.;

  /* clip to stream size */
  if (*stream_x < 0.)
    *stream_x = 0.;
  if (*stream_x > GST_VIDEO_INFO_WIDTH (&clapper_widget->v_info))
    *stream_x = GST_VIDEO_INFO_WIDTH (&clapper_widget->v_info);

  /* same for y-axis */
  if (result.h > 0)
    *stream_y = (y - result.y) / result.h * stream_height;
  else
    *stream_y = 0.;

  if (*stream_y < 0.)
    *stream_y = 0.;
  if (*stream_y > GST_VIDEO_INFO_HEIGHT (&clapper_widget->v_info))
    *stream_y = GST_VIDEO_INFO_HEIGHT (&clapper_widget->v_info);

  GST_TRACE ("transform %fx%f into %fx%f", x, y, *stream_x, *stream_y);
}

static gboolean
gtk_clapper_gl_widget_button_event (GtkGestureClick * gesture,
    gint n_press, gdouble x, gdouble y)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (gesture);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (widget);
  GstElement *element;

  if (clapper_widget->display_width == 0 || clapper_widget->display_height == 0)
    return FALSE;

  if ((element = g_weak_ref_get (&clapper_widget->element))) {
    if (_get_is_navigation_allowed (element, GST_STATE_PLAYING)) {
      GdkEvent *event = gtk_event_controller_get_current_event (controller);
      const gchar *key_type =
          gdk_event_get_event_type (event) == GDK_BUTTON_PRESS
          ? "mouse-button-press" : "mouse-button-release";
      gdouble stream_x, stream_y;

      _display_size_to_stream_size (clapper_widget, x, y, &stream_x, &stream_y);

      gst_navigation_send_mouse_event (GST_NAVIGATION (element), key_type,
          /* Gesture is set to ignore other buttons so we do not have to check */
          GDK_BUTTON_PRIMARY,
          stream_x, stream_y);
    }
    g_object_unref (element);
  }

  return FALSE;
}

static gboolean
gtk_clapper_gl_widget_motion_event (GtkEventControllerMotion * motion_controller,
    gdouble x, gdouble y)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (motion_controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (widget);
  GstElement *element;

  if ((x == clapper_widget->last_pos_x && y == clapper_widget->last_pos_y) ||
      clapper_widget->display_width == 0 || clapper_widget->display_height == 0)
    return FALSE;

  if ((element = g_weak_ref_get (&clapper_widget->element))) {
    if (_get_is_navigation_allowed (element, GST_STATE_PLAYING)) {
      gdouble stream_x, stream_y;

      clapper_widget->last_pos_x = x;
      clapper_widget->last_pos_y = y;
      _display_size_to_stream_size (clapper_widget, x, y, &stream_x, &stream_y);

      gst_navigation_send_mouse_event (GST_NAVIGATION (element), "mouse-move",
          0, stream_x, stream_y);
    }
    g_object_unref (element);
  }

  return FALSE;
}

static void
gtk_clapper_gl_widget_settings_changed (GtkGLArea * glarea)
{
  GST_DEBUG ("GTK settings changed, queued render");
  gtk_gl_area_queue_render (glarea);
}

static void
gtk_clapper_gl_widget_bind_buffer (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;
  const GstGLFuncs *gl = priv->context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, priv->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (priv->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (priv->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (priv->attr_position);
  gl->EnableVertexAttribArray (priv->attr_texture);
}

static void
gtk_clapper_gl_widget_unbind_buffer (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;
  const GstGLFuncs *gl = priv->context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (priv->attr_position);
  gl->DisableVertexAttribArray (priv->attr_texture);
}

static void
gtk_clapper_gl_widget_init_redisplay (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;
  const GstGLFuncs *gl = priv->context->gl_vtable;
  GError *error = NULL;
  GstGLSLStage *frag_stage, *vert_stage;

  gst_gl_insert_debug_marker (priv->other_context, "initializing redisplay");

  vert_stage = gst_glsl_stage_new_with_string (priv->context,
      GL_VERTEX_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
      gst_gl_shader_string_vertex_mat4_vertex_transform);
  if (priv->texture_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    gchar *frag_str;

    frag_str =
        gst_gl_shader_string_fragment_external_oes_get_default
        (priv->context, GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
    frag_stage = gst_glsl_stage_new_with_string (priv->context,
        GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, frag_str);

    g_free (frag_str);
  } else {
    frag_stage = gst_glsl_stage_new_default_fragment (priv->context);
  }

  if (!vert_stage || !frag_stage) {
    GST_ERROR ("Failed to retrieve fragment shader for texture target");
    if (vert_stage)
      gst_object_unref (vert_stage);
    if (frag_stage)
      gst_object_unref (frag_stage);
    return;
  }

  if (!(priv->shader =
      gst_gl_shader_new_link_with_stages (priv->context, &error,
          vert_stage, frag_stage, NULL))) {
    GST_ERROR ("Failed to initialize shader: %s", error->message);
    return;
  }

  priv->attr_position =
      gst_gl_shader_get_attribute_location (priv->shader, "a_position");
  priv->attr_texture =
      gst_gl_shader_get_attribute_location (priv->shader, "a_texcoord");

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &priv->vao);
    gl->BindVertexArray (priv->vao);
  }

  gl->GenBuffers (1, &priv->vertex_buffer);
  gl->BindBuffer (GL_ARRAY_BUFFER, priv->vertex_buffer);
  gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
      GL_STATIC_DRAW);

  if (gl->GenVertexArrays) {
    gtk_clapper_gl_widget_bind_buffer (clapper_widget);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  if (!priv->overlay_compositor)
    priv->overlay_compositor =
        gst_gl_overlay_compositor_new (priv->other_context);

  priv->initiated = TRUE;
}

static inline void
_draw_black (GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;

  gst_gl_insert_debug_marker (context, "rendering black");
  gl->ClearColor (0.0, 0.0, 0.0, 1.0);
  gl->Clear (GL_COLOR_BUFFER_BIT);
}

static inline void
_draw_black_with_gdk (GdkGLContext * gdk_context)
{
  GST_DEBUG ("rendering empty frame with gdk context %p", gdk_context);
  glClearColor (0.0, 0.0, 0.0, 1.0);
  glClear (GL_COLOR_BUFFER_BIT);
}

static gboolean
gtk_clapper_gl_widget_render (GtkGLArea * widget, GdkGLContext * context)
{
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (widget);

  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;
  const GstGLFuncs *gl;

  GstVideoAffineTransformationMeta *af_meta;
  gfloat matrix[16];

  GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);

  /* Draw black with GDK context when priv is not available yet.
     GTK calls render with GDK context already active. */
  if (!priv->context || !priv->other_context || clapper_widget->ignore_buffers) {
    _draw_black_with_gdk (context);
    goto done;
  }

  gst_gl_context_activate (priv->other_context, TRUE);

  if (!priv->initiated || !clapper_widget->negotiated) {
    if (!priv->initiated)
      gtk_clapper_gl_widget_init_redisplay (clapper_widget);

    _draw_black (priv->other_context);
    goto done;
  }

  /* Upload latest buffer */
  if (clapper_widget->pending_buffer) {
    GstBuffer *buffer = clapper_widget->pending_buffer;
    GstVideoFrame gl_frame;
    GstGLSyncMeta *sync_meta;

    if (!gst_video_frame_map (&gl_frame, &clapper_widget->v_info, buffer,
            GST_MAP_READ | GST_MAP_GL)) {
      _draw_black (priv->other_context);
      goto done;
    }

    priv->current_tex = *(guint *) gl_frame.data[0];
    gst_gl_overlay_compositor_upload_overlays (priv->overlay_compositor,
        buffer);

    sync_meta = gst_buffer_get_gl_sync_meta (buffer);
    if (sync_meta) {
      /* XXX: the set_sync() seems to be needed for resizing */
      gst_gl_sync_meta_set_sync_point (sync_meta, priv->context);
      gst_gl_sync_meta_wait (sync_meta, priv->other_context);
    }

    gst_video_frame_unmap (&gl_frame);

    if (clapper_widget->buffer)
      gst_buffer_unref (clapper_widget->buffer);

    /* Keep the buffer to ensure current_tex stay valid */
    clapper_widget->buffer = buffer;
    clapper_widget->pending_buffer = NULL;
  }

  GST_DEBUG ("rendering buffer %p with gdk context %p",
      clapper_widget->buffer, context);

  /* Draw texture */
  gl = priv->context->gl_vtable;

  if (clapper_widget->force_aspect_ratio) {
    GstVideoRectangle src, dst, result;

    gl->ClearColor (0.0, 0.0, 0.0, 1.0);
    gl->Clear (GL_COLOR_BUFFER_BIT);

    src.x = 0;
    src.y = 0;
    src.w = clapper_widget->display_width;
    src.h = clapper_widget->display_height;

    dst.x = 0;
    dst.y = 0;
    dst.w = clapper_widget->scaled_width;
    dst.h = clapper_widget->scaled_height;

    gst_video_sink_center_rect (src, dst, &result, TRUE);

    gl->Viewport (result.x, result.y, result.w, result.h);
  }

  gst_gl_shader_use (priv->shader);

  if (gl->BindVertexArray)
    gl->BindVertexArray (priv->vao);

  gtk_clapper_gl_widget_bind_buffer (clapper_widget);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (priv->gl_target, priv->current_tex);
  gst_gl_shader_set_uniform_1i (priv->shader, "tex", 0);

  af_meta = gst_buffer_get_video_affine_transformation_meta (
      clapper_widget->buffer);
  gst_clapper_gl_get_affine_transformation_meta_as_ndc (af_meta, matrix);
  gst_gl_shader_set_uniform_matrix_4fv (priv->shader,
      "u_transformation", 1, FALSE, matrix);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  if (gl->BindVertexArray)
    gl->BindVertexArray (0);
  else
    gtk_clapper_gl_widget_unbind_buffer (clapper_widget);

  gl->BindTexture (priv->gl_target, 0);

  /* Draw subtitles */
  gst_gl_overlay_compositor_draw_overlays (priv->overlay_compositor);

done:
  if (priv->other_context)
    gst_gl_context_activate (priv->other_context, FALSE);

  GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
  return FALSE;
}

static void
_cleanup_gl_private (GtkClapperGLWidgetPrivate * priv)
{
  const GstGLFuncs *gl = priv->other_context->gl_vtable;

  if (priv->vao) {
    gl->DeleteVertexArrays (1, &priv->vao);
    priv->vao = 0;
  }
  if (priv->vertex_buffer) {
    gl->DeleteBuffers (1, &priv->vertex_buffer);
    priv->vertex_buffer = 0;
  }
  if (priv->upload) {
    gst_object_unref (priv->upload);
    priv->upload = NULL;
  }
  if (priv->shader) {
    gst_object_unref (priv->shader);
    priv->shader = NULL;
  }
  if (priv->overlay_compositor)
    gst_gl_overlay_compositor_free_overlays (priv->overlay_compositor);
}

static void
_cleanup_gl_thread (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;

  if (!priv->gdk_context)
    priv->gdk_context = gtk_gl_area_get_context (GTK_GL_AREA (clapper_widget));

  if (priv->gdk_context == NULL)
    return;

  gdk_gl_context_make_current (priv->gdk_context);
  gst_gl_context_activate (priv->other_context, TRUE);

  _cleanup_gl_private (priv);

  gst_gl_context_activate (priv->other_context, FALSE);
  gdk_gl_context_clear_current ();

  priv->initiated = FALSE;
}

static void
_reset_gl (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;

  if (!priv->gdk_context)
    priv->gdk_context = gtk_gl_area_get_context (GTK_GL_AREA (clapper_widget));

  if (priv->gdk_context == NULL)
    return;

  gdk_gl_context_make_current (priv->gdk_context);
  gst_gl_context_activate (priv->other_context, TRUE);

  _cleanup_gl_private (priv);

  if (priv->overlay_compositor)
    gst_object_unref (priv->overlay_compositor);

  gst_gl_context_activate (priv->other_context, FALSE);

  gst_object_unref (priv->other_context);
  priv->other_context = NULL;

  gdk_gl_context_clear_current ();

  g_object_unref (priv->gdk_context);
  priv->gdk_context = NULL;
}

static void
gtk_clapper_gl_widget_finalize (GObject * object)
{
  GtkClapperGLWidget *clapper_widget = GTK_CLAPPER_GL_WIDGET (object);
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;

  if (priv->other_context)
    gst_gtk_invoke_on_main ((GThreadFunc) (GCallback) _reset_gl, clapper_widget);

  if (priv->context)
    gst_object_unref (priv->context);

  if (priv->display)
    gst_object_unref (priv->display);

  if (clapper_widget->draw_id)
    g_source_remove (clapper_widget->draw_id);

  gst_buffer_replace (&clapper_widget->pending_buffer, NULL);
  gst_buffer_replace (&clapper_widget->buffer, NULL);
  g_mutex_clear (&clapper_widget->lock);
  g_weak_ref_clear (&clapper_widget->element);

  G_OBJECT_CLASS (gtk_clapper_gl_widget_parent_class)->finalize (object);
}

void
gtk_clapper_gl_widget_set_element (GtkClapperGLWidget * clapper_widget,
    GstElement * element)
{
  g_weak_ref_set (&clapper_widget->element, element);
}

gboolean
gtk_clapper_gl_widget_set_format (GtkClapperGLWidget * clapper_widget,
    GstVideoInfo * v_info)
{
  GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);

  if (gst_video_info_is_equal (&clapper_widget->pending_v_info, v_info)) {
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
    return TRUE;
  }

  if (!_calculate_par (clapper_widget, v_info)) {
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
    return FALSE;
  }

  clapper_widget->pending_resize = TRUE;
  clapper_widget->pending_v_info = *v_info;

  GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);

  return TRUE;
}

void
gtk_clapper_gl_widget_set_buffer (GtkClapperGLWidget * clapper_widget,
    GstBuffer * buffer)
{
  g_return_if_fail (GTK_IS_CLAPPER_GL_WIDGET (clapper_widget));

  GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);

  gst_buffer_replace (&clapper_widget->pending_buffer, buffer);

  if (!clapper_widget->draw_id) {
    clapper_widget->draw_id = g_idle_add_full (G_PRIORITY_DEFAULT,
        (GSourceFunc) _queue_draw, clapper_widget, NULL);
  }

  GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
}

static gboolean
_wrap_current_gl (GstGLDisplay * display, GstGLPlatform platform,
    GstGLContext ** other_context)
{
  GstGLAPI gl_api = GST_GL_API_NONE;
  guint gl_major = 0, gl_minor = 0;

  gl_api = gst_gl_context_get_current_gl_api (platform, &gl_major, &gl_minor);

  if (gl_api) {
    const gboolean is_es = gl_api & (GST_GL_API_GLES1 | GST_GL_API_GLES2);
    gchar *gl_api_str = gst_gl_api_to_string (gl_api);
    guintptr gl_handle = 0;

    GST_INFO ("Using GL API: %s, ver: %d.%d", gl_api_str, gl_major, gl_minor);
    g_free (gl_api_str);

    if (is_es && platform == GST_GL_PLATFORM_EGL && !g_getenv ("GST_GL_API")) {
      GST_DEBUG ("No GST_GL_API env and GTK is using EGL GLES2, enforcing it");
      gst_gl_display_filter_gl_api (display, GST_GL_API_GLES2);
    }

    gl_handle = gst_gl_context_get_current_gl_context (platform);
    if (gl_handle) {
      if ((*other_context = gst_gl_context_new_wrapped (display,
          gl_handle, platform, gl_api)))
        return TRUE;
    }
  }

  return FALSE;
}

static void
_get_gl_context (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;
  GstGLPlatform platform = GST_GL_PLATFORM_NONE;

  gtk_widget_realize (GTK_WIDGET (clapper_widget));

  if (priv->other_context)
    gst_object_unref (priv->other_context);
  priv->other_context = NULL;

  if (priv->gdk_context)
    g_object_unref (priv->gdk_context);

  priv->gdk_context = gtk_gl_area_get_context (GTK_GL_AREA (clapper_widget));
  if (priv->gdk_context == NULL) {
    GError *error = gtk_gl_area_get_error (GTK_GL_AREA (clapper_widget));

    GST_ERROR_OBJECT (clapper_widget, "Error creating GdkGLContext : %s",
        error ? error->message : "No error set by Gdk");
    g_clear_error (&error);
    return;
  }

  g_object_ref (priv->gdk_context);
  gdk_gl_context_make_current (priv->gdk_context);

#if GST_GL_HAVE_WINDOW_WAYLAND && GST_GL_HAVE_PLATFORM_EGL && defined (GDK_WINDOWING_WAYLAND)
  if (GST_IS_GL_DISPLAY_WAYLAND (priv->display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_DEBUG ("Using EGL on Wayland");
    goto have_platform;
  }
#endif
#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
#if GST_GL_HAVE_PLATFORM_EGL
  if (GST_IS_GL_DISPLAY_EGL (priv->display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_DEBUG ("Using EGL on x11");
    goto have_platform;
  }
#endif
#if GST_GL_HAVE_PLATFORM_GLX
  if (GST_IS_GL_DISPLAY_X11 (priv->display)) {
    platform = GST_GL_PLATFORM_GLX;
    GST_DEBUG ("Using GLX on x11");
    goto have_platform;
  }
#endif
#endif

  GST_ERROR ("Unknown GL platform");
  return;

have_platform:

  if (_wrap_current_gl (priv->display, platform, &priv->other_context)) {
    GError *error = NULL;

    GST_INFO ("Retrieved Gdk OpenGL context %" GST_PTR_FORMAT,
        priv->other_context);
    gst_gl_context_activate (priv->other_context, TRUE);
    if (!gst_gl_context_fill_info (priv->other_context, &error)) {
      GST_ERROR ("Failed to retrieve gdk context info: %s", error->message);
      g_clear_error (&error);
      g_object_unref (priv->other_context);
      priv->other_context = NULL;
    } else {
      gst_gl_context_activate (priv->other_context, FALSE);
    }
  } else {
    GST_WARNING ("Could not retrieve Gdk OpenGL context");
  }
}

static void
gtk_clapper_gl_widget_class_init (GtkClapperGLWidgetClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GtkGLAreaClass *gl_area_class = (GtkGLAreaClass *) klass;

  gobject_class->set_property = gtk_clapper_gl_widget_set_property;
  gobject_class->get_property = gtk_clapper_gl_widget_get_property;
  gobject_class->finalize = gtk_clapper_gl_widget_finalize;

  gst_gtk_install_shared_properties (gobject_class);

  widget_class->measure = gtk_clapper_gl_widget_measure;
  widget_class->size_allocate = gtk_clapper_gl_widget_size_allocate;

  gl_area_class->render = gtk_clapper_gl_widget_render;
}

static void
gtk_clapper_gl_widget_init (GtkClapperGLWidget * clapper_widget)
{
  GdkDisplay *display;
  GtkClapperGLWidgetPrivate *priv;
  GtkWidget *widget = GTK_WIDGET (clapper_widget);

  clapper_widget->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  clapper_widget->par_n = DEFAULT_PAR_N;
  clapper_widget->par_d = DEFAULT_PAR_D;
  clapper_widget->keep_last_frame = DEFAULT_KEEP_LAST_FRAME;
  clapper_widget->ignore_buffers = FALSE;
  clapper_widget->last_pos_x = 0;
  clapper_widget->last_pos_y = 0;

  gst_video_info_init (&clapper_widget->v_info);
  gst_video_info_init (&clapper_widget->pending_v_info);

  g_weak_ref_init (&clapper_widget->element, NULL);
  g_mutex_init (&clapper_widget->lock);

  clapper_widget->key_controller = gtk_event_controller_key_new ();
  g_signal_connect (clapper_widget->key_controller, "key-pressed",
      G_CALLBACK (gtk_clapper_gl_widget_key_event), NULL);
  g_signal_connect (clapper_widget->key_controller, "key-released",
      G_CALLBACK (gtk_clapper_gl_widget_key_event), NULL);

  clapper_widget->motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect (clapper_widget->motion_controller, "motion",
      G_CALLBACK (gtk_clapper_gl_widget_motion_event), NULL);

  clapper_widget->click_gesture = gtk_gesture_click_new ();
  g_signal_connect (clapper_widget->click_gesture, "pressed",
      G_CALLBACK (gtk_clapper_gl_widget_button_event), NULL);
  g_signal_connect (clapper_widget->click_gesture, "released",
      G_CALLBACK (gtk_clapper_gl_widget_button_event), NULL);

  /* Otherwise widget in grid will appear as a 1x1px
   * video which might be misleading for users */
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);

  gtk_widget_set_focusable (widget, TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (clapper_widget->click_gesture),
      GDK_BUTTON_PRIMARY);

  gtk_widget_add_controller (widget, clapper_widget->key_controller);
  gtk_widget_add_controller (widget, clapper_widget->motion_controller);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (clapper_widget->click_gesture));

  gtk_widget_set_can_focus (widget, TRUE);

  clapper_widget->priv = priv = gtk_clapper_gl_widget_get_instance_private (clapper_widget);

  display = gdk_display_get_default ();

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display)) {
    gpointer display_ptr;
#if GST_GL_HAVE_PLATFORM_EGL && GTK_CHECK_VERSION(4,3,1)
    display_ptr = gdk_x11_display_get_egl_display (display);
    if (display_ptr)
      priv->display = (GstGLDisplay *)
          gst_gl_display_egl_new_with_egl_display (display_ptr);
#endif
#if GST_GL_HAVE_PLATFORM_GLX
    if (!priv->display) {
      display_ptr = gdk_x11_display_get_xdisplay (display);
      priv->display = (GstGLDisplay *)
          gst_gl_display_x11_new_with_display (display_ptr);
    }
#endif
  }
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display)) {
    struct wl_display *wayland_display =
        gdk_wayland_display_get_wl_display (display);
    priv->display = (GstGLDisplay *)
        gst_gl_display_wayland_new_with_display (wayland_display);
  }
#endif

  (void) display;

  if (!priv->display)
    priv->display = gst_gl_display_new ();

  GST_INFO ("Created %" GST_PTR_FORMAT, priv->display);

  priv->texture_target = GST_GL_TEXTURE_TARGET_NONE;
  priv->gl_target = 0;

  gtk_gl_area_set_auto_render (GTK_GL_AREA (widget), FALSE);

  g_signal_connect_swapped (gtk_widget_get_settings (widget), "notify",
      G_CALLBACK (gtk_clapper_gl_widget_settings_changed), GTK_GL_AREA (widget));
}

GtkWidget *
gtk_clapper_gl_widget_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_CLAPPER_GL_WIDGET, NULL);
}

gboolean
gtk_clapper_gl_widget_init_winsys (GtkClapperGLWidget * clapper_widget)
{
  GtkClapperGLWidgetPrivate *priv = clapper_widget->priv;
  GError *error = NULL;

  g_return_val_if_fail (GTK_IS_CLAPPER_GL_WIDGET (clapper_widget), FALSE);
  g_return_val_if_fail (priv->display != NULL, FALSE);

  GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);

  if (priv->display && priv->gdk_context && priv->other_context) {
    GST_TRACE ("have already initialized contexts");
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
    return TRUE;
  }

  if (!priv->other_context) {
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
    gst_gtk_invoke_on_main ((GThreadFunc) (GCallback) _get_gl_context, clapper_widget);
    GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);
  }

  if (!GST_IS_GL_CONTEXT (priv->other_context)) {
    GST_FIXME ("Could not retrieve Gdk OpenGL context");
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
    return FALSE;
  }

  GST_OBJECT_LOCK (priv->display);
  if (!gst_gl_display_create_context (priv->display, priv->other_context,
          &priv->context, &error)) {
    GST_WARNING ("Could not create OpenGL context: %s",
        error ? error->message : "Unknown");
    g_clear_error (&error);
    GST_OBJECT_UNLOCK (priv->display);
    GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
    return FALSE;
  }
  gst_gl_display_add_context (priv->display, priv->context);
  GST_OBJECT_UNLOCK (priv->display);

  GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
  return TRUE;
}

GstGLContext *
gtk_clapper_gl_widget_get_gtk_context (GtkClapperGLWidget * clapper_widget)
{
  if (!clapper_widget->priv->other_context)
    return NULL;

  return gst_object_ref (clapper_widget->priv->other_context);
}

GstGLContext *
gtk_clapper_gl_widget_get_context (GtkClapperGLWidget * clapper_widget)
{
  if (!clapper_widget->priv->context)
    return NULL;

  return gst_object_ref (clapper_widget->priv->context);
}

GstGLDisplay *
gtk_clapper_gl_widget_get_display (GtkClapperGLWidget * clapper_widget)
{
  if (!clapper_widget->priv->display)
    return NULL;

  return gst_object_ref (clapper_widget->priv->display);
}

gboolean
gtk_clapper_gl_widget_update_output_format (GtkClapperGLWidget * clapper_widget,
    GstCaps * caps)
{
  GtkClapperGLWidgetPrivate *priv;
  GstGLTextureTarget previous_target;
  GstStructure *structure;
  const gchar *target_str;
  gboolean cleanup_gl;

  GTK_CLAPPER_GL_WIDGET_LOCK (clapper_widget);
  priv = clapper_widget->priv;

  previous_target = priv->texture_target;
  structure = gst_caps_get_structure (caps, 0);
  target_str = gst_structure_get_string (structure, "texture-target");

  if (!target_str)
    target_str = GST_GL_TEXTURE_TARGET_2D_STR;

  priv->texture_target = gst_gl_texture_target_from_string (target_str);
  if (!priv->texture_target)
    goto fail;

  GST_DEBUG_OBJECT (clapper_widget, "Using texture-target: %s", target_str);
  priv->gl_target = gst_gl_texture_target_to_gl (priv->texture_target);

  cleanup_gl = (previous_target != GST_GL_TEXTURE_TARGET_NONE &&
      priv->texture_target != previous_target);

  GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
  if (cleanup_gl)
    gst_gtk_invoke_on_main ((GThreadFunc) (GCallback) _cleanup_gl_thread, clapper_widget);

  return TRUE;

fail:
  GTK_CLAPPER_GL_WIDGET_UNLOCK (clapper_widget);
  return FALSE;
}
