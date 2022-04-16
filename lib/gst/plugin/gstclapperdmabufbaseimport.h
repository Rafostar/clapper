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

#include <gst/gl/gstglfuncs.h>

#include "gstclapperglbaseimport.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT               (gst_clapper_dmabuf_base_import_get_type())
#define GST_IS_CLAPPER_DMABUF_BASE_IMPORT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT))
#define GST_IS_CLAPPER_DMABUF_BASE_IMPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT))
#define GST_CLAPPER_DMABUF_BASE_IMPORT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT, GstClapperDmabufBaseImportClass))
#define GST_CLAPPER_DMABUF_BASE_IMPORT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT, GstClapperDmabufBaseImport))
#define GST_CLAPPER_DMABUF_BASE_IMPORT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT, GstClapperDmabufBaseImportClass))
#define GST_CLAPPER_DMABUF_BASE_IMPORT_CAST(obj)          ((GstClapperDmabufBaseImport *)(obj))

#define GST_CLAPPER_DMABUF_BASE_IMPORT_GET_LOCK(obj)      (&GST_CLAPPER_DMABUF_BASE_IMPORT_CAST(obj)->lock)
#define GST_CLAPPER_DMABUF_BASE_IMPORT_LOCK(obj)          g_mutex_lock (GST_CLAPPER_DMABUF_BASE_IMPORT_GET_LOCK(obj))
#define GST_CLAPPER_DMABUF_BASE_IMPORT_UNLOCK(obj)        g_mutex_unlock (GST_CLAPPER_DMABUF_BASE_IMPORT_GET_LOCK(obj))

typedef struct _GstClapperDmabufBaseImport GstClapperDmabufBaseImport;
typedef struct _GstClapperDmabufBaseImportClass GstClapperDmabufBaseImportClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperDmabufBaseImport, gst_object_unref)
#endif

struct _GstClapperDmabufBaseImport
{
  GstClapperGLBaseImport parent;

  GMutex lock;

  gboolean prepared;

  GstGLTextureTarget gst_tex_target;
  guint gl_tex_target;

  GstGLShader *shader;

  GLuint vao;
  GLuint vertex_buffer;
  GLint attr_position;
  GLint attr_texture;
};

struct _GstClapperDmabufBaseImportClass
{
  GstClapperGLBaseImportClass parent_class;
};

GType gst_clapper_dmabuf_base_import_get_type (void);

GdkTexture * gst_clapper_dmabuf_base_import_fds_into_texture (GstClapperDmabufBaseImport *dmabuf_bi, gint *fds, gsize *offsets);

G_END_DECLS
