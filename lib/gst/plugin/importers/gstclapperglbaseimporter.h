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

#include <gst/gl/gl.h>

#include "gst/plugin/gstclapperimporter.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_GL_BASE_IMPORTER               (gst_clapper_gl_base_importer_get_type())
#define GST_IS_CLAPPER_GL_BASE_IMPORTER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_GL_BASE_IMPORTER))
#define GST_IS_CLAPPER_GL_BASE_IMPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_GL_BASE_IMPORTER))
#define GST_CLAPPER_GL_BASE_IMPORTER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_GL_BASE_IMPORTER, GstClapperGLBaseImporterClass))
#define GST_CLAPPER_GL_BASE_IMPORTER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_GL_BASE_IMPORTER, GstClapperGLBaseImporter))
#define GST_CLAPPER_GL_BASE_IMPORTER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_GL_BASE_IMPORTER, GstClapperGLBaseImporterClass))
#define GST_CLAPPER_GL_BASE_IMPORTER_CAST(obj)          ((GstClapperGLBaseImporter *)(obj))

#define GST_CLAPPER_GL_BASE_IMPORTER_GET_LOCK(obj)      (&GST_CLAPPER_GL_BASE_IMPORTER_CAST(obj)->lock)
#define GST_CLAPPER_GL_BASE_IMPORTER_LOCK(obj)          g_mutex_lock (GST_CLAPPER_GL_BASE_IMPORTER_GET_LOCK(obj))
#define GST_CLAPPER_GL_BASE_IMPORTER_UNLOCK(obj)        g_mutex_unlock (GST_CLAPPER_GL_BASE_IMPORTER_GET_LOCK(obj))

#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_WAYLAND       (GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND))
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11           (GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11))
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11_GLX       (GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11 && GST_GL_HAVE_PLATFORM_GLX)
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11_EGL       (GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11 && GST_GL_HAVE_PLATFORM_EGL)
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_WIN32         (GST_GL_HAVE_WINDOW_WIN32 && defined (GDK_WINDOWING_WIN32))
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_WIN32_WGL     (GST_CLAPPER_GL_BASE_IMPORTER_HAVE_WIN32 && GST_GL_HAVE_PLATFORM_WGL)
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_WIN32_EGL     (GST_CLAPPER_GL_BASE_IMPORTER_HAVE_WIN32 && GST_GL_HAVE_PLATFORM_EGL)
#define GST_CLAPPER_GL_BASE_IMPORTER_HAVE_MACOS         (GST_GL_HAVE_WINDOW_COCOA && defined (GDK_WINDOWING_MACOS) && GST_GL_HAVE_PLATFORM_CGL)

typedef struct _GstClapperGLBaseImporter GstClapperGLBaseImporter;
typedef struct _GstClapperGLBaseImporterClass GstClapperGLBaseImporterClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperGLBaseImporter, gst_object_unref)
#endif

struct _GstClapperGLBaseImporter
{
  GstClapperImporter parent;

  GMutex lock;

  GdkGLContext *gdk_context;

  GstGLDisplay *gst_display;
  GstGLContext *wrapped_context;
  GstGLContext *gst_context;
};

struct _GstClapperGLBaseImporterClass
{
  GstClapperImporterClass parent_class;

  gboolean (* gdk_context_realize) (GstClapperGLBaseImporter *gl_bi,
                                    GdkGLContext             *gdk_context);
};

GType        gst_clapper_gl_base_importer_get_type                   (void);

GstCaps *    gst_clapper_gl_base_importer_make_supported_gdk_gl_caps (void);

GStrv        gst_clapper_gl_base_importer_make_gl_context_types      (void);

GdkTexture * gst_clapper_gl_base_importer_make_gl_texture            (GstClapperGLBaseImporter *self, GstBuffer *buffer, GstVideoInfo *v_info);

G_END_DECLS
