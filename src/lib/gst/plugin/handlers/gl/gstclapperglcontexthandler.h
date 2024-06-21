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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#include <gtk/gtk.h>

#include "gst/plugin/gstclappercontexthandler.h"
#include "gst/plugin/clapper-gst-visibility.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_GL_CONTEXT_HANDLER (gst_clapper_gl_context_handler_get_type())

CLAPPER_GST_API
G_DECLARE_FINAL_TYPE (GstClapperGLContextHandler, gst_clapper_gl_context_handler, GST, CLAPPER_GL_CONTEXT_HANDLER, GstClapperContextHandler)

#define GST_CLAPPER_GL_CONTEXT_HANDLER_CAST(obj)        ((GstClapperGLContextHandler *)(obj))

#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WAYLAND     (GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND))
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11         (GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11))
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_GLX     (GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11 && GST_GL_HAVE_PLATFORM_GLX)
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11_EGL     (GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_X11 && GST_GL_HAVE_PLATFORM_EGL)
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32       (GST_GL_HAVE_WINDOW_WIN32 && defined (GDK_WINDOWING_WIN32))
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_WGL   (GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32 && GST_GL_HAVE_PLATFORM_WGL)
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32_EGL   (GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_WIN32 && GST_GL_HAVE_PLATFORM_EGL)
#define GST_CLAPPER_GL_CONTEXT_HANDLER_HAVE_MACOS       (GST_GL_HAVE_WINDOW_COCOA && defined (GDK_WINDOWING_MACOS) && GST_GL_HAVE_PLATFORM_CGL)

struct _GstClapperGLContextHandler
{
  GstClapperContextHandler parent;

  GdkGLContext *gdk_context;

  GstGLDisplay *gst_display;
  GstGLContext *wrapped_context;
  GstGLContext *gst_context;
};

CLAPPER_GST_API
void         gst_clapper_gl_context_handler_add_handler            (GPtrArray *context_handlers);

CLAPPER_GST_API
GstCaps *    gst_clapper_gl_context_handler_make_gdk_gl_caps       (const gchar *features, gboolean only_2d);

CLAPPER_GST_API
GdkTexture * gst_clapper_gl_context_handler_make_gl_texture        (GstClapperGLContextHandler *handler, GstBuffer *buffer, GstVideoInfo *v_info);

G_END_DECLS
