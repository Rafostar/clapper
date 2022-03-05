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

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_BASE_IMPORT               (gst_clapper_base_import_get_type())
#define GST_IS_CLAPPER_BASE_IMPORT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_BASE_IMPORT))
#define GST_IS_CLAPPER_BASE_IMPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_BASE_IMPORT))
#define GST_CLAPPER_BASE_IMPORT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_BASE_IMPORT, GstClapperBaseImportClass))
#define GST_CLAPPER_BASE_IMPORT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_BASE_IMPORT, GstClapperBaseImport))
#define GST_CLAPPER_BASE_IMPORT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_BASE_IMPORT, GstClapperBaseImportClass))
#define GST_CLAPPER_BASE_IMPORT_CAST(obj)          ((GstClapperBaseImport *)(obj))

#define GST_CLAPPER_BASE_IMPORT_GET_LOCK(obj)      (&GST_CLAPPER_BASE_IMPORT_CAST(obj)->lock)
#define GST_CLAPPER_BASE_IMPORT_LOCK(obj)          g_mutex_lock (GST_CLAPPER_BASE_IMPORT_GET_LOCK(obj))
#define GST_CLAPPER_BASE_IMPORT_UNLOCK(obj)        g_mutex_unlock (GST_CLAPPER_BASE_IMPORT_GET_LOCK(obj))

typedef struct _GstClapperBaseImport GstClapperBaseImport;
typedef struct _GstClapperBaseImportClass GstClapperBaseImportClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperBaseImport, gst_object_unref)
#endif

struct _GstClapperBaseImport
{
  GstBaseTransform parent;

  GMutex lock;

  GstVideoInfo in_info, out_info;
};

struct _GstClapperBaseImportClass
{
  GstBaseTransformClass parent_class;

  GstBufferPool * (* create_upstream_pool) (GstClapperBaseImport *bi,
                                            GstStructure        **config);
};

GType gst_clapper_base_import_get_type (void);

gboolean gst_clapper_base_import_map_buffers (GstClapperBaseImport *bi,
    GstBuffer *in_buf, GstBuffer *out_buf, GstMapFlags in_flags, GstMapFlags out_flags,
    GstVideoFrame *frame, GstMapInfo *info, GstMemory **mem);

G_END_DECLS
