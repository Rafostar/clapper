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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "gst/plugin/gstclapperimporter.h"
#include "gst/plugin/handlers/gl/gstclapperglcontexthandler.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_GL_UPLOADER (gst_clapper_gl_uploader_get_type())
G_DECLARE_FINAL_TYPE (GstClapperGLUploader, gst_clapper_gl_uploader, GST, CLAPPER_GL_UPLOADER, GstClapperImporter)

#define GST_CLAPPER_GL_UPLOADER_CAST(obj)        ((GstClapperGLUploader *)(obj))

#define GST_CLAPPER_GL_UPLOADER_GET_LOCK(obj)    (&GST_CLAPPER_GL_UPLOADER_CAST(obj)->lock)
#define GST_CLAPPER_GL_UPLOADER_LOCK(obj)        g_mutex_lock (GST_CLAPPER_GL_UPLOADER_GET_LOCK(obj))
#define GST_CLAPPER_GL_UPLOADER_UNLOCK(obj)      g_mutex_unlock (GST_CLAPPER_GL_UPLOADER_GET_LOCK(obj))

struct _GstClapperGLUploader
{
  GstClapperImporter parent;

  GMutex lock;

  GstClapperGLContextHandler *gl_handler;

  GstGLUpload *upload;
  GstGLColorConvert *color_convert;

  GstVideoInfo pending_v_info, v_info;
  gboolean has_pending_v_info;
};

G_END_DECLS
