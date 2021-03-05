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

#include "gtkgstbasewidget.h"
#include "gstgtkutils.h"

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
#include <gdk/x11/gdkx.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

#if GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND)
#include <gdk/wayland/gdkwayland.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

/**
 * SECTION:gtkgstbasewidget
 * @title: GtkGstBaseWidget
 * @short_description: a #GtkGLArea that renders GStreamer video #GstBuffers
 * @see_also: #GtkGLArea, #GstBuffer
 *
 * #GtkGstBaseWidget is an #GtkWidget that renders GStreamer video buffers.
 */

GST_DEBUG_CATEGORY (gst_debug_gtk_base_widget);
#define GST_CAT_DEFAULT gst_debug_gtk_base_widget

#define DEFAULT_FORCE_ASPECT_RATIO  TRUE
#define DEFAULT_PAR_N               0
#define DEFAULT_PAR_D               1
#define DEFAULT_IGNORE_TEXTURES     FALSE

struct _GtkGstBaseWidgetPrivate
{
  gboolean initiated;
  GstGLDisplay *display;
  GdkGLContext *gdk_context;
  GstGLContext *other_context;
  GstGLContext *context;
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

G_DEFINE_TYPE_WITH_CODE (GtkGstBaseWidget, gtk_gst_base_widget, GTK_TYPE_GL_AREA,
    G_ADD_PRIVATE (GtkGstBaseWidget)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gtkgstbasewidget", 0,
        "GTK Gst Base Widget"));

enum
{
  PROP_0,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_IGNORE_TEXTURES,
};

static void
gtk_gst_base_widget_get_preferred_width (GtkWidget * widget, gint * min,
    gint * natural)
{
  GtkGstBaseWidget *base_widget = (GtkGstBaseWidget *) widget;
  gint video_width = base_widget->display_width;

  if (!base_widget->negotiated)
    video_width = 10;

  if (min)
    *min = 1;
  if (natural)
    *natural = video_width;
}

static void
gtk_gst_base_widget_get_preferred_height (GtkWidget * widget, gint * min,
    gint * natural)
{
  GtkGstBaseWidget *base_widget = (GtkGstBaseWidget *) widget;
  gint video_height = base_widget->display_height;

  if (!base_widget->negotiated)
    video_height = 10;

  if (min)
    *min = 1;
  if (natural)
    *natural = video_height;
}

static void
gtk_gst_base_widget_measure (GtkWidget * widget, GtkOrientation orientation,
    gint for_size, gint * min, gint * natural,
    gint * minimum_baseline, gint * natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_gst_base_widget_get_preferred_width (widget, min, natural);
  else
    gtk_gst_base_widget_get_preferred_height (widget, min, natural);

  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
gtk_gst_base_widget_size_allocate (GtkWidget * widget,
    gint width, gint height, gint baseline)
{
  GtkGstBaseWidget *base_widget = GTK_GST_BASE_WIDGET (widget);
  gint scale_factor = gtk_widget_get_scale_factor (widget);

  GTK_GST_BASE_WIDGET_LOCK (base_widget);

  base_widget->scaled_width = width * scale_factor;
  base_widget->scaled_height = height * scale_factor;

  GTK_GST_BASE_WIDGET_UNLOCK (base_widget);

  gtk_gl_area_queue_render (GTK_GL_AREA (widget));
}

static void
gtk_gst_base_widget_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GtkGstBaseWidget *gtk_widget = GTK_GST_BASE_WIDGET (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      gtk_widget->force_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gtk_widget->par_n = gst_value_get_fraction_numerator (value);
      gtk_widget->par_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_IGNORE_TEXTURES:
      gtk_widget->ignore_textures = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtk_gst_base_widget_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GtkGstBaseWidget *gtk_widget = GTK_GST_BASE_WIDGET (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, gtk_widget->force_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      gst_value_set_fraction (value, gtk_widget->par_n, gtk_widget->par_d);
      break;
    case PROP_IGNORE_TEXTURES:
      g_value_set_boolean (value, gtk_widget->ignore_textures);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
_calculate_par (GtkGstBaseWidget * widget, GstVideoInfo * info)
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
  if (widget->par_n != 0 && widget->par_d != 0) {
    display_par_n = widget->par_n;
    display_par_d = widget->par_d;
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

  ok = gst_video_calculate_display_ratio (&widget->display_ratio_num,
      &widget->display_ratio_den, width, height, par_n, par_d, display_par_n,
      display_par_d);

  if (ok) {
    GST_LOG ("PAR: %u/%u DAR:%u/%u", par_n, par_d, display_par_n,
        display_par_d);
    return TRUE;
  }

  return FALSE;
}

static void
_apply_par (GtkGstBaseWidget * widget)
{
  guint display_ratio_num, display_ratio_den;
  gint width, height;

  width = GST_VIDEO_INFO_WIDTH (&widget->v_info);
  height = GST_VIDEO_INFO_HEIGHT (&widget->v_info);

  display_ratio_num = widget->display_ratio_num;
  display_ratio_den = widget->display_ratio_den;

  if (height % display_ratio_den == 0) {
    GST_DEBUG ("keeping video height");
    widget->display_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    widget->display_height = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG ("keeping video width");
    widget->display_width = width;
    widget->display_height = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG ("approximating while keeping video height");
    widget->display_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    widget->display_height = height;
  }

  GST_DEBUG ("scaling to %dx%d", widget->display_width, widget->display_height);
}

static gboolean
_queue_draw (GtkGstBaseWidget * widget)
{
  GTK_GST_BASE_WIDGET_LOCK (widget);
  widget->draw_id = 0;

  if (widget->pending_resize) {
    widget->pending_resize = FALSE;

    widget->v_info = widget->pending_v_info;
    widget->negotiated = TRUE;

    _apply_par (widget);

    gtk_widget_queue_resize (GTK_WIDGET (widget));
  } else {
    gtk_gl_area_queue_render (GTK_GL_AREA (widget));
  }

  GTK_GST_BASE_WIDGET_UNLOCK (widget);

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
gtk_gst_base_widget_key_event (GtkEventControllerKey * key_controller,
    guint keyval, guint keycode, GdkModifierType state)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (key_controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkGstBaseWidget *base_widget = GTK_GST_BASE_WIDGET (widget);
  GstElement *element;

  if ((element = g_weak_ref_get (&base_widget->element))) {
    if (GST_IS_NAVIGATION (element)) {
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
_fit_stream_to_allocated_size (GtkGstBaseWidget * base_widget, GstVideoRectangle * result)
{
  if (base_widget->force_aspect_ratio) {
    GstVideoRectangle src, dst;

    src.x = 0;
    src.y = 0;
    src.w = base_widget->display_width;
    src.h = base_widget->display_height;

    dst.x = 0;
    dst.y = 0;
    dst.w = base_widget->scaled_width;
    dst.h = base_widget->scaled_height;

    gst_video_sink_center_rect (src, dst, result, TRUE);
  } else {
    result->x = 0;
    result->y = 0;
    result->w = base_widget->scaled_width;
    result->h = base_widget->scaled_height;
  }
}

static void
_display_size_to_stream_size (GtkGstBaseWidget * base_widget, gdouble x,
    gdouble y, gdouble * stream_x, gdouble * stream_y)
{
  gdouble stream_width, stream_height;
  GstVideoRectangle result;

  _fit_stream_to_allocated_size (base_widget, &result);

  stream_width = (gdouble) GST_VIDEO_INFO_WIDTH (&base_widget->v_info);
  stream_height = (gdouble) GST_VIDEO_INFO_HEIGHT (&base_widget->v_info);

  /* from display coordinates to stream coordinates */
  if (result.w > 0)
    *stream_x = (x - result.x) / result.w * stream_width;
  else
    *stream_x = 0.;

  /* clip to stream size */
  if (*stream_x < 0.)
    *stream_x = 0.;
  if (*stream_x > GST_VIDEO_INFO_WIDTH (&base_widget->v_info))
    *stream_x = GST_VIDEO_INFO_WIDTH (&base_widget->v_info);

  /* same for y-axis */
  if (result.h > 0)
    *stream_y = (y - result.y) / result.h * stream_height;
  else
    *stream_y = 0.;

  if (*stream_y < 0.)
    *stream_y = 0.;
  if (*stream_y > GST_VIDEO_INFO_HEIGHT (&base_widget->v_info))
    *stream_y = GST_VIDEO_INFO_HEIGHT (&base_widget->v_info);

  GST_TRACE ("transform %fx%f into %fx%f", x, y, *stream_x, *stream_y);
}

static gboolean
gtk_gst_base_widget_button_event (GtkGestureClick * gesture,
    gint n_press, gdouble x, gdouble y)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (gesture);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkGstBaseWidget *base_widget = GTK_GST_BASE_WIDGET (widget);
  GstElement *element;

  if ((element = g_weak_ref_get (&base_widget->element))) {
    if (GST_IS_NAVIGATION (element)) {
      GdkEvent *event = gtk_event_controller_get_current_event (controller);
      const gchar *key_type =
          gdk_event_get_event_type (event) == GDK_BUTTON_PRESS
          ? "mouse-button-press" : "mouse-button-release";
      gdouble stream_x, stream_y;

      _display_size_to_stream_size (base_widget, x, y, &stream_x, &stream_y);

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
gtk_gst_base_widget_motion_event (GtkEventControllerMotion * motion_controller,
    gdouble x, gdouble y)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (motion_controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkGstBaseWidget *base_widget = GTK_GST_BASE_WIDGET (widget);
  GstElement *element;

  if ((element = g_weak_ref_get (&base_widget->element))) {
    if (GST_IS_NAVIGATION (element)) {
      gdouble stream_x, stream_y;

      _display_size_to_stream_size (base_widget, x, y, &stream_x, &stream_y);

      gst_navigation_send_mouse_event (GST_NAVIGATION (element), "mouse-move",
          0, stream_x, stream_y);
    }
    g_object_unref (element);
  }

  return FALSE;
}

static void
gtk_gst_base_widget_bind_buffer (GtkGstBaseWidget * base_widget)
{
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
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
gtk_gst_base_widget_unbind_buffer (GtkGstBaseWidget * base_widget)
{
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
  const GstGLFuncs *gl = priv->context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (priv->attr_position);
  gl->DisableVertexAttribArray (priv->attr_texture);
}

static void
gtk_gst_base_widget_init_redisplay (GtkGstBaseWidget * base_widget)
{
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
  const GstGLFuncs *gl = priv->context->gl_vtable;
  GError *error = NULL;

  gst_gl_insert_debug_marker (priv->other_context, "initializing redisplay");
  if (!(priv->shader = gst_gl_shader_new_default (priv->context, &error))) {
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
    gtk_gst_base_widget_bind_buffer (base_widget);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

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
gtk_gst_base_widget_render (GtkGLArea * widget, GdkGLContext * context)
{
  GtkGstBaseWidget *base_widget = GTK_GST_BASE_WIDGET (widget);

  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
  const GstGLFuncs *gl;

  GTK_GST_BASE_WIDGET_LOCK (widget);

  /* Draw black with GDK context when priv is not available yet.
     GTK calls render with GDK context already active. */
  if (!priv->context || !priv->other_context || base_widget->ignore_textures) {
    _draw_black_with_gdk (context);
    goto done;
  }

  gst_gl_context_activate (priv->other_context, TRUE);

  if (!priv->initiated || !base_widget->negotiated) {
    if (!priv->initiated)
      gtk_gst_base_widget_init_redisplay (base_widget);

    _draw_black (priv->other_context);
    goto done;
  }

  /* Upload latest buffer */
  if (base_widget->pending_buffer) {
    GstBuffer *buffer = base_widget->pending_buffer;
    GstVideoFrame gl_frame;
    GstGLSyncMeta *sync_meta;

    if (!gst_video_frame_map (&gl_frame, &base_widget->v_info, buffer,
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

    if (base_widget->buffer)
      gst_buffer_unref (base_widget->buffer);

    /* Keep the buffer to ensure current_tex stay valid */
    base_widget->buffer = buffer;
    base_widget->pending_buffer = NULL;
  }

  GST_DEBUG ("rendering buffer %p with gdk context %p",
      base_widget->buffer, context);

  /* Draw texture */
  gl = priv->context->gl_vtable;

  if (base_widget->force_aspect_ratio) {
    GstVideoRectangle src, dst, result;

    gl->ClearColor (0.0, 0.0, 0.0, 1.0);
    gl->Clear (GL_COLOR_BUFFER_BIT);

    src.x = 0;
    src.y = 0;
    src.w = base_widget->display_width;
    src.h = base_widget->display_height;

    dst.x = 0;
    dst.y = 0;
    dst.w = base_widget->scaled_width;
    dst.h = base_widget->scaled_height;

    gst_video_sink_center_rect (src, dst, &result, TRUE);

    gl->Viewport (result.x, result.y, result.w, result.h);
  }

  gst_gl_shader_use (priv->shader);

  if (gl->BindVertexArray)
    gl->BindVertexArray (priv->vao);

  gtk_gst_base_widget_bind_buffer (base_widget);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, priv->current_tex);
  gst_gl_shader_set_uniform_1i (priv->shader, "tex", 0);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  if (gl->BindVertexArray)
    gl->BindVertexArray (0);
  else
    gtk_gst_base_widget_unbind_buffer (base_widget);

  gl->BindTexture (GL_TEXTURE_2D, 0);

  /* Draw subtitles */
  gst_gl_overlay_compositor_draw_overlays (priv->overlay_compositor);

done:
  if (priv->other_context)
    gst_gl_context_activate (priv->other_context, FALSE);

  GTK_GST_BASE_WIDGET_UNLOCK (widget);
  return FALSE;
}

static void
_reset_gl (GtkGstBaseWidget * base_widget)
{
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
  const GstGLFuncs *gl = priv->other_context->gl_vtable;

  if (!priv->gdk_context)
    priv->gdk_context = gtk_gl_area_get_context (GTK_GL_AREA (base_widget));

  if (priv->gdk_context == NULL)
    return;

  gdk_gl_context_make_current (priv->gdk_context);
  gst_gl_context_activate (priv->other_context, TRUE);

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
    gst_object_unref (priv->overlay_compositor);

  gst_gl_context_activate (priv->other_context, FALSE);

  gst_object_unref (priv->other_context);
  priv->other_context = NULL;

  gdk_gl_context_clear_current ();

  g_object_unref (priv->gdk_context);
  priv->gdk_context = NULL;
}

static void
gtk_gst_base_widget_finalize (GObject * object)
{
  GtkGstBaseWidget *base_widget = GTK_GST_BASE_WIDGET (object);
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;

  if (priv->other_context)
    gst_gtk_invoke_on_main ((GThreadFunc) (GCallback) _reset_gl, base_widget);

  if (priv->context)
    gst_object_unref (priv->context);

  if (priv->display)
    gst_object_unref (priv->display);

  if (base_widget->draw_id)
    g_source_remove (base_widget->draw_id);

  gst_buffer_replace (&base_widget->pending_buffer, NULL);
  gst_buffer_replace (&base_widget->buffer, NULL);
  g_mutex_clear (&base_widget->lock);
  g_weak_ref_clear (&base_widget->element);

  G_OBJECT_CLASS (gtk_gst_base_widget_parent_class)->finalize (object);
}

void
gtk_gst_base_widget_set_element (GtkGstBaseWidget * widget,
    GstElement * element)
{
  g_weak_ref_set (&widget->element, element);
}

gboolean
gtk_gst_base_widget_set_format (GtkGstBaseWidget * widget,
    GstVideoInfo * v_info)
{
  GTK_GST_BASE_WIDGET_LOCK (widget);

  if (gst_video_info_is_equal (&widget->pending_v_info, v_info)) {
    GTK_GST_BASE_WIDGET_UNLOCK (widget);
    return TRUE;
  }

  if (!_calculate_par (widget, v_info)) {
    GTK_GST_BASE_WIDGET_UNLOCK (widget);
    return FALSE;
  }

  widget->pending_resize = TRUE;
  widget->pending_v_info = *v_info;

  GTK_GST_BASE_WIDGET_UNLOCK (widget);

  return TRUE;
}

void
gtk_gst_base_widget_set_buffer (GtkGstBaseWidget * widget, GstBuffer * buffer)
{
  /* As we have no type, this is better then no check */
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GTK_GST_BASE_WIDGET_LOCK (widget);

  gst_buffer_replace (&widget->pending_buffer, buffer);

  if (!widget->draw_id) {
    widget->draw_id = g_idle_add_full (G_PRIORITY_DEFAULT,
        (GSourceFunc) _queue_draw, widget, NULL);
  }

  GTK_GST_BASE_WIDGET_UNLOCK (widget);
}

static void
_get_gl_context (GtkGstBaseWidget * base_widget)
{
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
  GstGLPlatform platform = GST_GL_PLATFORM_NONE;
  GstGLAPI gl_api = GST_GL_API_NONE;
  guintptr gl_handle = 0;

  gtk_widget_realize (GTK_WIDGET (base_widget));

  if (priv->other_context)
    gst_object_unref (priv->other_context);
  priv->other_context = NULL;

  if (priv->gdk_context)
    g_object_unref (priv->gdk_context);

  priv->gdk_context = gtk_gl_area_get_context (GTK_GL_AREA (base_widget));
  if (priv->gdk_context == NULL) {
    GError *error = gtk_gl_area_get_error (GTK_GL_AREA (base_widget));

    GST_ERROR_OBJECT (base_widget, "Error creating GdkGLContext : %s",
        error ? error->message : "No error set by Gdk");
    g_clear_error (&error);
    return;
  }

  g_object_ref (priv->gdk_context);

  gdk_gl_context_make_current (priv->gdk_context);

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
  if (GST_IS_GL_DISPLAY_X11 (priv->display)) {
#if GST_GL_HAVE_PLATFORM_GLX
    if (!gl_handle) {
      platform = GST_GL_PLATFORM_GLX;
      gl_handle = gst_gl_context_get_current_gl_context (platform);
    }
#endif

#if GST_GL_HAVE_PLATFORM_EGL
    if (!gl_handle) {
      platform = GST_GL_PLATFORM_EGL;
      gl_handle = gst_gl_context_get_current_gl_context (platform);
    }
#endif

    if (gl_handle) {
      gl_api = gst_gl_context_get_current_gl_api (platform, NULL, NULL);
      priv->other_context =
          gst_gl_context_new_wrapped (priv->display, gl_handle,
          platform, gl_api);
    }
  }
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND && GST_GL_HAVE_PLATFORM_EGL && defined (GDK_WINDOWING_WAYLAND)
  if (GST_IS_GL_DISPLAY_WAYLAND (priv->display)) {
    platform = GST_GL_PLATFORM_EGL;
    gl_api = gst_gl_context_get_current_gl_api (platform, NULL, NULL);
    gl_handle = gst_gl_context_get_current_gl_context (platform);
    if (gl_handle)
      priv->other_context =
          gst_gl_context_new_wrapped (priv->display, gl_handle,
          platform, gl_api);
  }
#endif

  (void) platform;
  (void) gl_api;
  (void) gl_handle;

  if (priv->other_context) {
    GError *error = NULL;

    GST_INFO ("Retrieved Gdk OpenGL context %" GST_PTR_FORMAT,
        priv->other_context);
    gst_gl_context_activate (priv->other_context, TRUE);
    if (!gst_gl_context_fill_info (priv->other_context, &error)) {
      GST_ERROR ("failed to retrieve gdk context info: %s", error->message);
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
gtk_gst_base_widget_class_init (GtkGstBaseWidgetClass * klass)
{
  GObjectClass *gobject_klass = (GObjectClass *) klass;
  GtkWidgetClass *widget_klass = (GtkWidgetClass *) klass;
  GtkGLAreaClass *gl_area_klass = (GtkGLAreaClass *) klass;

  gobject_klass->set_property = gtk_gst_base_widget_set_property;
  gobject_klass->get_property = gtk_gst_base_widget_get_property;
  gobject_klass->finalize = gtk_gst_base_widget_finalize;

  g_object_class_install_property (gobject_klass, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", DEFAULT_PAR_N, DEFAULT_PAR_D,
          G_MAXINT, 1, 1, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_IGNORE_TEXTURES,
      g_param_spec_boolean ("ignore-textures", "Ignore Textures",
          "When enabled, textures will be ignored and not drawn",
          DEFAULT_IGNORE_TEXTURES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  widget_klass->measure = gtk_gst_base_widget_measure;
  widget_klass->size_allocate = gtk_gst_base_widget_size_allocate;

  gl_area_klass->render = gtk_gst_base_widget_render;

  GST_DEBUG_CATEGORY_INIT (gst_debug_gtk_base_widget, "gtkbasewidget", 0,
      "GTK Video Base Widget");
}

static void
gtk_gst_base_widget_init (GtkGstBaseWidget * widget)
{
  GdkDisplay *display;
  GtkGstBaseWidgetPrivate *priv;

  widget->force_aspect_ratio = DEFAULT_FORCE_ASPECT_RATIO;
  widget->par_n = DEFAULT_PAR_N;
  widget->par_d = DEFAULT_PAR_D;
  widget->ignore_textures = DEFAULT_IGNORE_TEXTURES;

  gst_video_info_init (&widget->v_info);
  gst_video_info_init (&widget->pending_v_info);

  g_weak_ref_init (&widget->element, NULL);
  g_mutex_init (&widget->lock);

  widget->key_controller = gtk_event_controller_key_new ();
  g_signal_connect (widget->key_controller, "key-pressed",
      G_CALLBACK (gtk_gst_base_widget_key_event), NULL);
  g_signal_connect (widget->key_controller, "key-released",
      G_CALLBACK (gtk_gst_base_widget_key_event), NULL);

  widget->motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect (widget->motion_controller, "motion",
      G_CALLBACK (gtk_gst_base_widget_motion_event), NULL);

  widget->click_gesture = gtk_gesture_click_new ();
  g_signal_connect (widget->click_gesture, "pressed",
      G_CALLBACK (gtk_gst_base_widget_button_event), NULL);
  g_signal_connect (widget->click_gesture, "released",
      G_CALLBACK (gtk_gst_base_widget_button_event), NULL);

  /* Otherwise widget in grid will appear as a 1x1px
   * video which might be misleading for users */
  gtk_widget_set_hexpand (GTK_WIDGET (widget), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (widget), TRUE);

  gtk_widget_set_focusable (GTK_WIDGET (widget), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (widget->click_gesture),
      GDK_BUTTON_PRIMARY);

  gtk_widget_add_controller (GTK_WIDGET (widget), widget->key_controller);
  gtk_widget_add_controller (GTK_WIDGET (widget), widget->motion_controller);
  gtk_widget_add_controller (GTK_WIDGET (widget),
      GTK_EVENT_CONTROLLER (widget->click_gesture));

  gtk_widget_set_can_focus (GTK_WIDGET (widget), TRUE);

  widget->priv = priv = gtk_gst_base_widget_get_instance_private (widget);

  display = gdk_display_get_default ();

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display)) {
    priv->display = (GstGLDisplay *)
        gst_gl_display_x11_new_with_display (gdk_x11_display_get_xdisplay
        (display));
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

  gtk_gl_area_set_auto_render (GTK_GL_AREA (widget), FALSE);
}

GtkWidget *
gtk_gst_base_widget_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_GST_BASE_WIDGET, NULL);
}

gboolean
gtk_gst_base_widget_init_winsys (GtkGstBaseWidget * base_widget)
{
  GtkGstBaseWidgetPrivate *priv = base_widget->priv;
  GError *error = NULL;

  g_return_val_if_fail (GTK_IS_GST_BASE_WIDGET (base_widget), FALSE);
  g_return_val_if_fail (priv->display != NULL, FALSE);

  GTK_GST_BASE_WIDGET_LOCK (base_widget);

  if (priv->display && priv->gdk_context && priv->other_context) {
    GST_TRACE ("have already initialized contexts");
    GTK_GST_BASE_WIDGET_UNLOCK (base_widget);
    return TRUE;
  }

  if (!priv->other_context) {
    GTK_GST_BASE_WIDGET_UNLOCK (base_widget);
    gst_gtk_invoke_on_main ((GThreadFunc) (GCallback) _get_gl_context, base_widget);
    GTK_GST_BASE_WIDGET_LOCK (base_widget);
  }

  if (!GST_IS_GL_CONTEXT (priv->other_context)) {
    GST_FIXME ("Could not retrieve Gdk OpenGL context");
    GTK_GST_BASE_WIDGET_UNLOCK (base_widget);
    return FALSE;
  }

  GST_OBJECT_LOCK (priv->display);
  if (!gst_gl_display_create_context (priv->display, priv->other_context,
          &priv->context, &error)) {
    GST_WARNING ("Could not create OpenGL context: %s",
        error ? error->message : "Unknown");
    g_clear_error (&error);
    GST_OBJECT_UNLOCK (priv->display);
    GTK_GST_BASE_WIDGET_UNLOCK (base_widget);
    return FALSE;
  }
  gst_gl_display_add_context (priv->display, priv->context);
  GST_OBJECT_UNLOCK (priv->display);

  GTK_GST_BASE_WIDGET_UNLOCK (base_widget);
  return TRUE;
}

GstGLContext *
gtk_gst_base_widget_get_gtk_context (GtkGstBaseWidget * base_widget)
{
  if (!base_widget->priv->other_context)
    return NULL;

  return gst_object_ref (base_widget->priv->other_context);
}

GstGLContext *
gtk_gst_base_widget_get_context (GtkGstBaseWidget * base_widget)
{
  if (!base_widget->priv->context)
    return NULL;

  return gst_object_ref (base_widget->priv->context);
}

GstGLDisplay *
gtk_gst_base_widget_get_display (GtkGstBaseWidget * base_widget)
{
  if (!base_widget->priv->display)
    return NULL;

  return gst_object_ref (base_widget->priv->display);
}
