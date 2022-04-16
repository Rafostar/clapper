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
#include <gst/allocators/gstdmabuf.h>

#include "gstclapperdmabufimport.h"
#include "gstclappergdkmemory.h"

#define GST_CAT_DEFAULT gst_clapper_dmabuf_import_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#ifndef GST_CAPS_FEATURE_MEMORY_DMABUF
#define GST_CAPS_FEATURE_MEMORY_DMABUF "memory:DMABuf"
#endif

static GstStaticPadTemplate gst_clapper_dmabuf_import_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_DMABUF,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS ", NV12 }")
        "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_DMABUF ", "
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS ", NV12 }")));

static GstStaticPadTemplate gst_clapper_dmabuf_import_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS ", NV12 }")
        "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY ", "
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS ", NV12 }")));

#define parent_class gst_clapper_dmabuf_import_parent_class
G_DEFINE_TYPE (GstClapperDmabufImport, gst_clapper_dmabuf_import, GST_TYPE_CLAPPER_DMABUF_BASE_IMPORT);
GST_ELEMENT_REGISTER_DEFINE (clapperdmabufimport, "clapperdmabufimport", GST_RANK_NONE,
    GST_TYPE_CLAPPER_DMABUF_IMPORT);

static gboolean
verify_dmabuf_memory (GstBuffer *buffer, GstVideoInfo *info, gint *fds, gsize *offsets)
{
  guint i, n_planes = GST_VIDEO_INFO_N_PLANES (info);

  for (i = 0; i < n_planes; i++) {
    GstMemory *memory;
    gsize plane_size, mem_skip;
    guint mem_idx, length;

    plane_size = gst_gl_get_plane_data_size (info, NULL, i);

    if (!gst_buffer_find_memory (buffer,
        GST_VIDEO_INFO_PLANE_OFFSET (info, i),
        plane_size, &mem_idx, &length, &mem_skip)) {
      GST_DEBUG ("Could not find memory %u", i);
      return FALSE;
    }

    /* We cannot have more then one DMABuf per plane */
    if (length != 1) {
      GST_DEBUG ("Data for plane %u spans %u memories", i, length);
      return FALSE;
    }

    memory = gst_buffer_peek_memory (buffer, mem_idx);

    offsets[i] = memory->offset + mem_skip;
    fds[i] = gst_dmabuf_memory_get_fd (memory);
  }

  return TRUE;
}

static GstBufferPool *
gst_clapper_dmabuf_import_create_upstream_pool (GstClapperBaseImport *bi, GstStructure **config)
{
  return NULL;
}

static GstFlowReturn
gst_clapper_dmabuf_import_transform (GstBaseTransform *bt,
    GstBuffer *in_buf, GstBuffer *out_buf)
{
  GstClapperDmabufBaseImport *dmabuf_bi = GST_CLAPPER_DMABUF_BASE_IMPORT_CAST (bt);
  GstClapperBaseImport *bi = GST_CLAPPER_BASE_IMPORT_CAST (bt);
  GstMapInfo info;
  GstMemory *memory;
  GstVideoMeta *meta;
  GstFlowReturn ret = GST_FLOW_ERROR;

  GST_LOG_OBJECT (bt, "Transforming from %" GST_PTR_FORMAT
      " into %" GST_PTR_FORMAT, in_buf, out_buf);

  if ((meta = gst_buffer_get_video_meta (in_buf))) {
    guint i;

    GST_VIDEO_INFO_WIDTH (&bi->in_info) = meta->width;
    GST_VIDEO_INFO_HEIGHT (&bi->in_info) = meta->height;

    for (i = 0; i < meta->n_planes; i++) {
      GST_VIDEO_INFO_PLANE_OFFSET (&bi->in_info, i) = meta->offset[i];
      GST_VIDEO_INFO_PLANE_STRIDE (&bi->in_info, i) = meta->stride[i];
    }
  }

  memory = gst_buffer_peek_memory (out_buf, 0);

  if (G_LIKELY (gst_memory_map (memory, &info, GST_MAP_WRITE))) {
    gint fds[GST_VIDEO_MAX_PLANES];
    gsize offsets[GST_VIDEO_MAX_PLANES];

    if (verify_dmabuf_memory (in_buf, &bi->in_info, fds, offsets)) {
      GstClapperGdkMemory *clapper_memory = GST_CLAPPER_GDK_MEMORY_CAST (memory);

      if (G_LIKELY ((clapper_memory->texture = gst_clapper_dmabuf_base_import_fds_into_texture (
          dmabuf_bi, fds, offsets))))
        ret = GST_FLOW_OK;
    }

    gst_memory_unmap (memory, &info);
  }

  return ret;
}

static void
gst_clapper_dmabuf_import_init (GstClapperDmabufImport *self)
{
}

static void
gst_clapper_dmabuf_import_class_init (GstClapperDmabufImportClass *klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *gstbasetransform_class = (GstBaseTransformClass *) klass;
  GstClapperBaseImportClass *bi_class = (GstClapperBaseImportClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperdmabufimport", 0,
      "Clapper DMABuf Import");

  gstbasetransform_class->transform = gst_clapper_dmabuf_import_transform;

  bi_class->create_upstream_pool = gst_clapper_dmabuf_import_create_upstream_pool;

  gst_element_class_set_metadata (gstelement_class,
      "Clapper DMABuf import",
      "Filter/Video", "Imports DMABuf into ClapperGdkMemory",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_dmabuf_import_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_dmabuf_import_src_template);
}
