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

#include "gstclapperimport.h"
#include "gstclappergdkmemory.h"
#include "gstgtkutils.h"

#define GST_CAT_DEFAULT gst_clapper_import_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate gst_clapper_import_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE ("{ " GST_CLAPPER_GDK_MEMORY_FORMATS " }")));

static GstStaticPadTemplate gst_clapper_import_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY,
            "{ " GST_CLAPPER_GDK_MEMORY_FORMATS " }")));

#define parent_class gst_clapper_import_parent_class
G_DEFINE_TYPE (GstClapperImport, gst_clapper_import, GST_TYPE_CLAPPER_BASE_IMPORT);
GST_ELEMENT_REGISTER_DEFINE (clapperimport, "clapperimport", GST_RANK_NONE,
    GST_TYPE_CLAPPER_IMPORT);

static GstBufferPool *
gst_clapper_import_create_upstream_pool (GstClapperBaseImport *bi, GstStructure **config)
{
  GstClapperImport *self = GST_CLAPPER_IMPORT_CAST (bi);
  GstBufferPool *pool;

  GST_DEBUG_OBJECT (self, "Creating new upstream pool");

  pool = gst_video_buffer_pool_new ();
  *config = gst_buffer_pool_get_config (pool);

  return pool;
}

static void
video_frame_unmap_and_free (GstVideoFrame *frame)
{
  gst_video_frame_unmap (frame);
  g_slice_free (GstVideoFrame, frame);
}

static GstFlowReturn
gst_clapper_import_transform (GstBaseTransform *bt,
    GstBuffer *in_buf, GstBuffer *out_buf)
{
  GstClapperBaseImport *bi = GST_CLAPPER_BASE_IMPORT_CAST (bt);
  GstVideoFrame *frame;
  GstMapInfo info;
  GstMemory *memory;

  frame = g_slice_new (GstVideoFrame);

  if (!gst_clapper_base_import_map_buffers (bi, in_buf, out_buf,
      GST_MAP_READ, GST_MAP_WRITE, frame, &info, &memory)) {
    g_slice_free (GstVideoFrame, frame);

    return GST_FLOW_ERROR;
  }

  /* Keep frame data alive as long as necessary,
   * unmap only after bytes are destroyed */
  GST_CLAPPER_GDK_MEMORY_CAST (memory)->texture = gst_video_frame_into_gdk_texture (
      frame, (GDestroyNotify) video_frame_unmap_and_free);

  gst_memory_unmap (memory, &info);

  return GST_FLOW_OK;
}

static void
gst_clapper_import_init (GstClapperImport *self)
{
}

static void
gst_clapper_import_class_init (GstClapperImportClass *klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *gstbasetransform_class = (GstBaseTransformClass *) klass;
  GstClapperBaseImportClass *bi_class = (GstClapperBaseImportClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperimport", 0,
      "Clapper Import");

  gstbasetransform_class->transform = gst_clapper_import_transform;

  bi_class->create_upstream_pool = gst_clapper_import_create_upstream_pool;

  gst_element_class_set_metadata (gstelement_class,
      "Clapper import",
      "Filter/Video", "Imports RAW video data into ClapperGdkMemory",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_import_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_import_src_template);
}
