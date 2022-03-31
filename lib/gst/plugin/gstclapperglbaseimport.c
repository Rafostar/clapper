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

#include <gtk/gtk.h>
#include <gst/gl/gstglfuncs.h>

#include "gstclapperglbaseimport.h"
#include "gstgtkutils.h"

#if GST_CLAPPER_GL_HAVE_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

#if GST_CLAPPER_GL_HAVE_X11
#include <gdk/x11/gdkx.h>
#endif

#if GST_CLAPPER_GL_HAVE_X11_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif
#if GST_CLAPPER_GL_HAVE_X11_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif

#define GST_CAT_DEFAULT gst_clapper_gl_base_import_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_gl_base_import_parent_class
G_DEFINE_TYPE (GstClapperGLBaseImport, gst_clapper_gl_base_import, GST_TYPE_CLAPPER_BASE_IMPORT);

static void
gst_clapper_gl_base_import_init (GstClapperGLBaseImport *self)
{
  g_mutex_init (&self->lock);
}

static void
gst_clapper_gl_base_import_finalize (GObject *object)
{
  GstClapperGLBaseImport *self = GST_CLAPPER_GL_BASE_IMPORT_CAST (object);

  g_clear_object (&self->gdk_context);
  gst_clear_object (&self->gst_context);
  gst_clear_object (&self->wrapped_context);
  gst_clear_object (&self->gst_display);

  g_mutex_clear (&self->lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static GstGLContext *
wrap_current_gl (GstGLDisplay *display, GdkGLAPI gdk_gl_api, GstGLPlatform platform)
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
retrieve_gl_context_on_main (GstClapperGLBaseImport *self)
{
  GstClapperGLBaseImportClass *gl_bi_class = GST_CLAPPER_GL_BASE_IMPORT_GET_CLASS (self);
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

  if (!(gdk_context = gdk_display_create_gl_context (gdk_display, &error))) {
    GST_ERROR_OBJECT (self, "Error creating Gdk GL context: %s",
        error ? error->message : "No error set by Gdk");
    g_clear_error (&error);

    return FALSE;
  }

  if (!gl_bi_class->gdk_context_realize (self, gdk_context)) {
    GST_ERROR_OBJECT (self, "Could not realize Gdk context: %" GST_PTR_FORMAT,
        gdk_context);
    g_object_unref (gdk_context);

    return FALSE;
  }
  gdk_gl_api = gdk_gl_context_get_api (gdk_context);

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (self);

  self->gdk_context = gdk_context;

#if GST_CLAPPER_GL_HAVE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display)) {
    struct wl_display *wayland_display =
        gdk_wayland_display_get_wl_display (gdk_display);
    self->gst_display = (GstGLDisplay *)
        gst_gl_display_wayland_new_with_display (wayland_display);
  }
#endif

#if GST_CLAPPER_GL_HAVE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display)) {
    gpointer display_ptr;
#if GST_CLAPPER_GL_HAVE_X11_EGL
    display_ptr = gdk_x11_display_get_egl_display (gdk_display);
    if (display_ptr) {
      self->gst_display = (GstGLDisplay *)
          gst_gl_display_egl_new_with_egl_display (display_ptr);
    }
#endif
#if GST_CLAPPER_GL_HAVE_X11_GLX
    if (!self->gst_display) {
      display_ptr = gdk_x11_display_get_xdisplay (gdk_display);
      self->gst_display = (GstGLDisplay *)
          gst_gl_display_x11_new_with_display (display_ptr);
    }
  }
#endif
#endif

  /* Fallback to generic display */
  if (G_UNLIKELY (!self->gst_display)) {
    GST_WARNING_OBJECT (self, "Unknown Gdk display!");
    self->gst_display = gst_gl_display_new ();
  }

#if GST_CLAPPER_GL_HAVE_WAYLAND
  if (GST_IS_GL_DISPLAY_WAYLAND (self->gst_display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_INFO_OBJECT (self, "Using EGL on Wayland");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_HAVE_X11_EGL
  if (GST_IS_GL_DISPLAY_EGL (self->gst_display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_INFO_OBJECT (self, "Using EGL on x11");
    goto have_display;
  }
#endif
#if GST_CLAPPER_GL_HAVE_X11_GLX
  if (GST_IS_GL_DISPLAY_X11 (self->gst_display)) {
    platform = GST_GL_PLATFORM_GLX;
    GST_INFO_OBJECT (self, "Using GLX on x11");
    goto have_display;
  }
#endif

  g_clear_object (&self->gdk_context);
  gst_clear_object (&self->gst_display);

  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

  GST_ERROR_OBJECT (self, "Unsupported GL platform");
  return FALSE;

have_display:
  gdk_gl_context_make_current (self->gdk_context);

  self->wrapped_context = wrap_current_gl (self->gst_display, gdk_gl_api, platform);
  if (!self->wrapped_context) {
    GST_ERROR ("Could not retrieve Gdk OpenGL context");
    gdk_gl_context_clear_current ();

    g_clear_object (&self->gdk_context);
    gst_clear_object (&self->gst_display);

    GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

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

    GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

    return FALSE;
  }

  gst_gl_context_get_gl_version (self->wrapped_context, &gl_major, &gl_minor);
  GST_INFO ("Using OpenGL%s %i.%i", (gdk_gl_api == GDK_GL_API_GLES) ? " ES" : "",
      gl_major, gl_minor);

  /* Deactivate in both places */
  gst_gl_context_activate (self->wrapped_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

  return TRUE;
}

static gboolean
ensure_gl_context (GstClapperGLBaseImport *self)
{
  GstGLDisplay *gst_display = NULL;
  GstGLContext *gst_context = NULL;
  GError *error = NULL;
  gboolean has_gdk_contexts;

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (self);
  has_gdk_contexts = (self->gdk_context && self->wrapped_context);
  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

  if (!has_gdk_contexts) {
    if (!(! !gst_gtk_invoke_on_main (
        (GThreadFunc) (GCallback) retrieve_gl_context_on_main, self))) {
      GST_ERROR_OBJECT (self, "Could not retrieve Gdk GL context");

      return FALSE;
    }
  }

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (self);

  gst_display = gst_object_ref (self->gst_display);

  /* GstGLDisplay operations require object lock to be held */
  GST_OBJECT_LOCK (gst_display);

  if (!self->gst_context) {
    GST_TRACE_OBJECT (self, "Creating new GstGLContext");

    if (!gst_gl_display_create_context (gst_display, self->wrapped_context,
        &self->gst_context, &error)) {
      GST_WARNING ("Could not create OpenGL context: %s",
          error ? error->message : "Unknown");
      g_clear_error (&error);

      GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

      return FALSE;
    }
  }

  gst_context = gst_object_ref (self->gst_context);

  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

  /* Calls `set_context` internally, so we cannot be locked here */
  gst_gl_display_add_context (gst_display, gst_context);
  gst_gl_element_propagate_display_context (GST_ELEMENT_CAST (self), gst_display);

  GST_OBJECT_UNLOCK (gst_display);

  gst_object_unref (gst_display);
  gst_object_unref (gst_context);

  return TRUE;
}

static void
gst_clapper_gl_base_import_set_context (GstElement *element, GstContext *context)
{
  GstClapperGLBaseImport *self = GST_CLAPPER_GL_BASE_IMPORT_CAST (element);

  GST_DEBUG_OBJECT (self, "Set context");

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (self);
  gst_gl_handle_set_context (element, context, &self->gst_display,
      &self->wrapped_context);
  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static GstStateChangeReturn
gst_clapper_gl_base_import_change_state (GstElement *element, GstStateChange transition)
{
  GstClapperGLBaseImport *self = GST_CLAPPER_GL_BASE_IMPORT_CAST (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!ensure_gl_context (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_clapper_gl_base_import_query (GstBaseTransform *bt,
    GstPadDirection direction, GstQuery *query)
{
  GstClapperGLBaseImport *self = GST_CLAPPER_GL_BASE_IMPORT_CAST (bt);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      GST_CLAPPER_GL_BASE_IMPORT_LOCK (self);
      res = gst_gl_handle_context_query (GST_ELEMENT_CAST (self), query,
          self->gst_display, self->gst_context, self->wrapped_context);
      GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (self);
      break;
    default:
      res = GST_BASE_TRANSFORM_CLASS (parent_class)->query (bt, direction, query);
      break;
  }

  return res;
}

static gboolean
gst_clapper_gl_base_import_gdk_context_realize (GstClapperGLBaseImport *self, GdkGLContext *gdk_context)
{
  GError *error = NULL;
  gboolean success;

  GST_DEBUG_OBJECT (self, "Realizing GdkGLContext with default implementation");

  gdk_gl_context_set_allowed_apis (gdk_context, GDK_GL_API_GLES);
  if (!(success = gdk_gl_context_realize (gdk_context, &error))) {
    GST_WARNING_OBJECT (self, "Could not realize Gdk context with GLES: %s", error->message);
    g_clear_error (&error);
  }
  if (!success) {
    gdk_gl_context_set_allowed_apis (gdk_context, GDK_GL_API_GL);
    if (!(success = gdk_gl_context_realize (gdk_context, &error))) {
      GST_WARNING_OBJECT (self, "Could not realize Gdk context with GL: %s", error->message);
      g_clear_error (&error);
    }
  }

  return success;
}

static void
gst_clapper_gl_base_import_class_init (GstClapperGLBaseImportClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *gstbasetransform_class = (GstBaseTransformClass *) klass;
  GstClapperGLBaseImportClass *gl_bi_class = (GstClapperGLBaseImportClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperglbaseimport", 0,
      "Clapper GL Base Import");

  gobject_class->finalize = gst_clapper_gl_base_import_finalize;

  gstelement_class->set_context = gst_clapper_gl_base_import_set_context;
  gstelement_class->change_state = gst_clapper_gl_base_import_change_state;

  gstbasetransform_class->query = gst_clapper_gl_base_import_query;

  gl_bi_class->gdk_context_realize = gst_clapper_gl_base_import_gdk_context_realize;
}
