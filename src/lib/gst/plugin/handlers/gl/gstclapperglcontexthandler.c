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

#include "gstclapperglcontexthandler.h"
#include "gst/plugin/gstgdkformats.h"
#include "gst/plugin/gstgtkutils.h"

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11
#include <gdk/x11/gdkx.h>
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_EGL || GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32
#include <gdk/win32/gdkwin32.h>
#endif

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_MACOS
#include <gdk/macos/gdkmacos.h>
#endif

#define GST_CAT_DEFAULT gst_clapper_gl_context_handler_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_gl_context_handler_parent_class
G_DEFINE_TYPE (GstClapperGLContextHandler, gst_clapper_gl_context_handler, GST_TYPE_OBJECT);

static GstGLContext *
_wrap_current_gl (GstGLDisplay *display, GdkGLAPI gdk_gl_api, GstGLPlatform platform)
{
  GstGLAPI gst_gl_api = GST_GL_API_NONE;

  switch (gdk_gl_api) {
    case GDK_GL_API_GL:
      gst_gl_api = GST_GL_API_OPENGL | GST_GL_API_OPENGL3;
      break;
    case GDK_GL_API_GLES:
      gst_gl_api = GST_GL_API_GLES2;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (gst_gl_api != GST_GL_API_NONE) {
    guintptr gl_handle;

    gst_gl_display_filter_gl_api (display, gst_gl_api);

    if ((gl_handle = gst_gl_context_get_current_gl_context (platform)))
      return gst_gl_context_new_wrapped (display, gl_handle, platform, gst_gl_api);
  }

  return NULL;
}

static gboolean
_realize_gdk_context_with_api (GdkGLContext *gdk_context, GdkGLAPI api, gint maj, gint min)
{
  GError *error = NULL;
  gboolean success;

  gdk_gl_context_set_allowed_apis (gdk_context, api);
  gdk_gl_context_set_required_version (gdk_context, maj, min);

  GST_DEBUG ("Trying to realize %s context, min ver: %i.%i",
      (api & GDK_GL_API_GL) ? "GL" : "GLES", maj, min);

  if (!(success = gdk_gl_context_realize (gdk_context, &error))) {
    GST_DEBUG ("Could not realize Gdk context with %s: %s",
        (api & GDK_GL_API_GL) ? "GL" : "GLES", error->message);
    g_clear_error (&error);
  }

  return success;
}

static gboolean
_gl_context_handler_context_realize (GstClapperGLContextHandler *self, GdkGLContext *gdk_context)
{
  GdkGLAPI preferred_api = GDK_GL_API_GL;
  GdkDisplay *gdk_display;
  const gchar *gl_env;
  gboolean success;

  GST_DEBUG_OBJECT (self, "Realizing GdkGLContext with default implementation");

  /* Use single "GST_GL_API" env to also influence Gdk GL selection */
  if ((gl_env = g_getenv ("GST_GL_API"))) {
    preferred_api = (g_str_has_prefix (gl_env, "gles"))
        ? GDK_GL_API_GLES
        : g_str_has_prefix (gl_env, "opengl")
        ? GDK_GL_API_GL
        : GDK_GL_API_GL | GDK_GL_API_GLES;

    /* With requested by user API, we either use it or give up */
    return _realize_gdk_context_with_api (gdk_context, preferred_api, 0, 0);
  }

  gdk_display = gdk_gl_context_get_display (gdk_context);
  GST_DEBUG_OBJECT (self, "Auto selecting GL API for display: %s",
      gdk_display_get_name (gdk_display));

  /* Apple decoder uses rectangle texture-target, which GLES does not support.
   * For Linux we prefer EGL + GLES in order to get direct HW colorspace conversion.
   * Windows will try EGL + GLES setup first and auto fallback to WGL. */
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display))
    preferred_api = GDK_GL_API_GLES;
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_EGL
  if (GDK_IS_X11_DISPLAY (gdk_display) && gdk_x11_display_get_egl_display (gdk_display))
    preferred_api = GDK_GL_API_GLES;
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_EGL
  if (GDK_IS_WIN32_DISPLAY (gdk_display) && gdk_win32_display_get_egl_display (gdk_display))
    preferred_api = GDK_GL_API_GLES;
#endif

  /* FIXME: Remove once GStreamer can handle DRM modifiers. This tries to avoid
   * "scrambled" image on Linux with Intel GPUs that are mostly used together with
   * x86 CPUs at the expense of using slightly slower non-direct DMABuf import.
   * See: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/1236 */
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WAYLAND || GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_EGL
#if !defined(HAVE_GST_PATCHES) && (defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64))
  {
    guint major = 0, minor = 0, micro = 0, nano = 0;

    gst_version (&major, &minor, &micro, &nano);
    if (major == 1 && minor < 24)
      preferred_api = GDK_GL_API_GL;
  }
#endif
#endif

  /* Continue with GLES only if it should have "GL_EXT_texture_norm16"
   * extension, as we need it to handle P010_10LE, etc. */
  if ((preferred_api == GDK_GL_API_GLES)
      && _realize_gdk_context_with_api (gdk_context, GDK_GL_API_GLES, 3, 1))
    return TRUE;

  /* If not using GLES 3.1, try with core GL 3.2 that GTK4 defaults to */
  if (_realize_gdk_context_with_api (gdk_context, GDK_GL_API_GL, 3, 2))
    return TRUE;

  /* Try with what we normally prefer first, otherwise use fallback */
  if (!(success = _realize_gdk_context_with_api (gdk_context, preferred_api, 0, 0))) {
    GdkGLAPI fallback_api;

    fallback_api = (GDK_GL_API_GL | GDK_GL_API_GLES);
    fallback_api &= ~preferred_api;

    success = _realize_gdk_context_with_api (gdk_context, fallback_api, 0, 0);
  }

  return success;
}

static gboolean
_retrieve_gl_context_on_main (GstClapperGLContextHandler *self)
{
  GdkDisplay *gdk_display;
  GdkGLContext *gdk_context;
  GError *error = NULL;
  GdkGLAPI gdk_gl_api;
  GstGLPlatform platform = GST_GL_PLATFORM_NONE;
  gint gl_major = 0, gl_minor = 0;

  if (!gtk_init_check ()) {
    GST_ERROR_OBJECT (self, "Could not ensure GTK initialization");
    return FALSE;
  }

  gdk_display = gdk_display_get_default ();

  if (G_UNLIKELY (!gdk_display)) {
    GST_ERROR_OBJECT (self, "Could not retrieve Gdk display");
    return FALSE;
  }

  if (!(gdk_context = gdk_display_create_gl_context (gdk_display, &error))) {
    GST_ERROR_OBJECT (self, "Error creating Gdk GL context: %s",
        error ? error->message : "No error set by Gdk");
    g_clear_error (&error);

    return FALSE;
  }

  if (!_gl_context_handler_context_realize (self, gdk_context)) {
    GST_ERROR_OBJECT (self, "Could not realize Gdk context: %" GST_PTR_FORMAT,
        gdk_context);
    g_object_unref (gdk_context);

    return FALSE;
  }
  gdk_gl_api = gdk_gl_context_get_api (gdk_context);

  GST_OBJECT_LOCK (self);

  self->gdk_context = gdk_context;

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display)) {
    struct wl_display *wayland_display =
        gdk_wayland_display_get_wl_display (gdk_display);
    self->gst_display = (GstGLDisplay *)
        gst_gl_display_wayland_new_with_display (wayland_display);
  }
#endif

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display)) {
    gpointer display_ptr;
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_EGL
    display_ptr = gdk_x11_display_get_egl_display (gdk_display);
    if (display_ptr) {
      self->gst_display = (GstGLDisplay *)
          gst_gl_display_egl_new_with_egl_display (display_ptr);
    }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_GLX
    if (!self->gst_display) {
      display_ptr = gdk_x11_display_get_xdisplay (gdk_display);
      self->gst_display = (GstGLDisplay *)
          gst_gl_display_x11_new_with_display (display_ptr);
    }
  }
#endif
#endif

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32
  if (GDK_IS_WIN32_DISPLAY (gdk_display)) {
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_EGL
    gpointer display_ptr = gdk_win32_display_get_egl_display (gdk_display);
    if (display_ptr) {
      self->gst_display = (GstGLDisplay *)
          gst_gl_display_egl_new_with_egl_display (display_ptr);
    }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_WGL
    if (!self->gst_display) {
      self->gst_display =
          gst_gl_display_new_with_type (GST_GL_DISPLAY_TYPE_WIN32);
    }
  }
#endif
#endif

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_MACOS
  if (GDK_IS_MACOS_DISPLAY (gdk_display)) {
    self->gst_display =
        gst_gl_display_new_with_type (GST_GL_DISPLAY_TYPE_COCOA);
  }
#endif

  /* Fallback to generic display */
  if (G_UNLIKELY (!self->gst_display)) {
    GST_WARNING_OBJECT (self, "Unknown Gdk display!");
    self->gst_display = gst_gl_display_new ();
  }

#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WAYLAND
  if (GST_IS_GL_DISPLAY_WAYLAND (self->gst_display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_INFO_OBJECT (self, "Using EGL on Wayland");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_EGL
  if (GST_IS_GL_DISPLAY_EGL (self->gst_display)
      && GDK_IS_X11_DISPLAY (gdk_display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_INFO_OBJECT (self, "Using EGL on x11");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_GLX
  if (GST_IS_GL_DISPLAY_X11 (self->gst_display)) {
    platform = GST_GL_PLATFORM_GLX;
    GST_INFO_OBJECT (self, "Using GLX on x11");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_EGL
  if (GST_IS_GL_DISPLAY_EGL (self->gst_display)
      && GDK_IS_WIN32_DISPLAY (gdk_display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_INFO_OBJECT (self, "Using EGL on Win32");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_WGL
  if (gst_gl_display_get_handle_type (self->gst_display) == GST_GL_DISPLAY_TYPE_WIN32) {
    platform = GST_GL_PLATFORM_WGL;
    GST_INFO_OBJECT (self, "Using WGL on Win32");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_MACOS
  if (gst_gl_display_get_handle_type (self->gst_display) == GST_GL_DISPLAY_TYPE_COCOA) {
    platform = GST_GL_PLATFORM_CGL;
    GST_INFO_OBJECT (self, "Using CGL on macOS");
    goto have_display;
  }
#endif

  g_clear_object (&self->gdk_context);
  gst_clear_object (&self->gst_display);

  GST_OBJECT_UNLOCK (self);

  GST_ERROR_OBJECT (self, "Unsupported GL platform");
  return FALSE;

have_display:
  gdk_gl_context_make_current (self->gdk_context);

  self->wrapped_context = _wrap_current_gl (self->gst_display, gdk_gl_api, platform);
  if (!self->wrapped_context) {
    GST_ERROR ("Could not retrieve Gdk OpenGL context");
    gdk_gl_context_clear_current ();

    g_clear_object (&self->gdk_context);
    gst_clear_object (&self->gst_display);

    GST_OBJECT_UNLOCK (self);

    return FALSE;
  }

  GST_INFO ("Retrieved Gdk OpenGL context %" GST_PTR_FORMAT, self->wrapped_context);
  gst_gl_context_activate (self->wrapped_context, TRUE);

  if (!gst_gl_context_fill_info (self->wrapped_context, &error)) {
    GST_ERROR ("Failed to fill Gdk context info: %s", error->message);
    g_clear_error (&error);

    gst_gl_context_activate (self->wrapped_context, FALSE);

    gst_clear_object (&self->wrapped_context);
    g_clear_object (&self->gdk_context);
    gst_clear_object (&self->gst_display);

    GST_OBJECT_UNLOCK (self);

    return FALSE;
  }

  gst_gl_context_get_gl_version (self->wrapped_context, &gl_major, &gl_minor);
  GST_INFO ("Using OpenGL%s %i.%i", (gdk_gl_api == GDK_GL_API_GLES) ? " ES" : "",
      gl_major, gl_minor);

  /* Deactivate in both places */
  gst_gl_context_activate (self->wrapped_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
_retrieve_gst_context (GstClapperGLContextHandler *self)
{
  GstGLDisplay *gst_display = NULL;
  GstGLContext *gst_context = NULL;
  GError *error = NULL;

  GST_OBJECT_LOCK (self);

  gst_display = gst_object_ref (self->gst_display);
  GST_TRACE_OBJECT (self, "Creating new GstGLContext");

  /* GstGLDisplay operations require display object lock to be held */
  GST_OBJECT_LOCK (gst_display);

  if (!gst_gl_display_create_context (gst_display, self->wrapped_context,
      &self->gst_context, &error)) {
    GST_WARNING ("Could not create OpenGL context: %s",
        error ? error->message : "Unknown");
    g_clear_error (&error);

    GST_OBJECT_UNLOCK (gst_display);
    GST_OBJECT_UNLOCK (self);

    return FALSE;
  }

  gst_context = gst_object_ref (self->gst_context);

  GST_OBJECT_UNLOCK (self);

  gst_gl_display_add_context (gst_display, gst_context);

  GST_OBJECT_UNLOCK (gst_display);

  gst_object_unref (gst_display);
  gst_object_unref (gst_context);

  return TRUE;
}

static gboolean
gst_clapper_gl_context_handler_handle_context_query (GstClapperContextHandler *handler,
    GstBaseSink *bsink, GstQuery *query)
{
  GstClapperGLContextHandler *self = GST_CLAPPER_GL_CONTEXT_HANDLER_CAST (handler);
  gboolean res;

  GST_OBJECT_LOCK (self);
  res = gst_gl_handle_context_query (GST_ELEMENT_CAST (bsink), query,
      self->gst_display, self->gst_context, self->wrapped_context);
  GST_OBJECT_UNLOCK (self);

  return res;
}

static void
gst_clapper_gl_context_handler_init (GstClapperGLContextHandler *self)
{
}

static void
gst_clapper_gl_context_handler_constructed (GObject *object)
{
  GstClapperGLContextHandler *self = GST_CLAPPER_GL_CONTEXT_HANDLER_CAST (object);

  if (! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
      _retrieve_gl_context_on_main, self)) {
    _retrieve_gst_context (self);
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, constructed, (object));
}

static void
gst_clapper_gl_context_handler_finalize (GObject *object)
{
  GstClapperGLContextHandler *self = GST_CLAPPER_GL_CONTEXT_HANDLER_CAST (object);

  GST_TRACE ("Finalize");

  g_clear_object (&self->gdk_context);

  gst_clear_object (&self->gst_display);
  gst_clear_object (&self->wrapped_context);
  gst_clear_object (&self->gst_context);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_gl_context_handler_class_init (GstClapperGLContextHandlerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstClapperContextHandlerClass *handler_class = (GstClapperContextHandlerClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperglcontexthandler", 0,
      "Clapper GL Context Handler");

  gobject_class->constructed = gst_clapper_gl_context_handler_constructed;
  gobject_class->finalize = gst_clapper_gl_context_handler_finalize;

  handler_class->handle_context_query = gst_clapper_gl_context_handler_handle_context_query;
}

void
gst_clapper_gl_context_handler_add_handler (GPtrArray *context_handlers)
{
  guint i;
  gboolean found = FALSE;

  for (i = 0; i < context_handlers->len; i++) {
    GstClapperContextHandler *handler = g_ptr_array_index (context_handlers, i);

    if ((found = GST_IS_CLAPPER_GL_CONTEXT_HANDLER (handler)))
      break;
  }

  if (!found) {
    GstClapperGLContextHandler *handler = g_object_new (
        GST_TYPE_CLAPPER_GL_CONTEXT_HANDLER, NULL);

    g_ptr_array_add (context_handlers, handler);
    GST_DEBUG ("Added GL context handler to handlers array");
  }
}

GstCaps *
gst_clapper_gl_context_handler_make_gdk_gl_caps (const gchar *features, gboolean only_2d)
{
  GstCaps *caps, *tmp;

  if (only_2d) {
    tmp = gst_caps_from_string (GST_VIDEO_CAPS_MAKE (
        "{ " GST_GDK_GL_TEXTURE_FORMATS " }") ", "
        "texture-target = (string) { " GST_GL_TEXTURE_TARGET_2D_STR " }");
  } else {
    tmp = gst_caps_from_string (GST_VIDEO_CAPS_MAKE (
        "{ " GST_GDK_GL_TEXTURE_FORMATS " }"));
  }

  caps = gst_caps_copy (tmp);

  gst_caps_set_features_simple (tmp, gst_caps_features_new (
      features, NULL));
  gst_caps_set_features_simple (caps, gst_caps_features_new (
      features, GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, NULL));

  gst_caps_append (caps, tmp);

  return caps;
}

GdkTexture *
gst_clapper_gl_context_handler_make_gl_texture (GstClapperGLContextHandler *self,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GdkTexture *texture;
  GstGLSyncMeta *sync_meta;
  GstVideoFrame frame;

  if (G_UNLIKELY (!gst_video_frame_map (&frame, v_info, buffer, GST_MAP_READ | GST_MAP_GL))) {
    GST_ERROR_OBJECT (self, "Could not map input buffer for reading");
    return NULL;
  }

  GST_OBJECT_LOCK (self);

  /* Must have context active here for both sync meta
   * and Gdk texture format auto-detection to work */
  gdk_gl_context_make_current (self->gdk_context);
  gst_gl_context_activate (self->wrapped_context, TRUE);

  sync_meta = gst_buffer_get_gl_sync_meta (buffer);

  /* Wait for all previous OpenGL commands to complete,
   * before we start using the input texture */
  if (sync_meta) {
    gst_gl_sync_meta_set_sync_point (sync_meta, self->gst_context);
    gst_gl_sync_meta_wait (sync_meta, self->wrapped_context);
  }

  texture = gdk_gl_texture_new (
      self->gdk_context,
      *(guint *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0),
      GST_VIDEO_FRAME_WIDTH (&frame),
      GST_VIDEO_FRAME_HEIGHT (&frame),
      (GDestroyNotify) gst_buffer_unref,
      gst_buffer_ref (buffer));

  gst_gl_context_activate (self->wrapped_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_OBJECT_UNLOCK (self);

  gst_video_frame_unmap (&frame);

  return texture;
}
