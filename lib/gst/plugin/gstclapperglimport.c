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

#include "gstclapperglimport.h"
#include "gstclappergdkmemory.h"

#define GST_CAT_DEFAULT gst_clapper_gl_import_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate gst_clapper_gl_import_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS " }") ", "
            "texture-target = (string) { " GST_GL_TEXTURE_TARGET_2D_STR " }"
        "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY ", "
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS " }") ", "
            "texture-target = (string) { " GST_GL_TEXTURE_TARGET_2D_STR " }"));

static GstStaticPadTemplate gst_clapper_gl_import_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS " }")
        "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY ", "
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            "{ " GST_CLAPPER_GDK_GL_TEXTURE_FORMATS " }")));

#define parent_class gst_clapper_gl_import_parent_class
G_DEFINE_TYPE (GstClapperGLImport, gst_clapper_gl_import, GST_TYPE_CLAPPER_GL_BASE_IMPORT);
GST_ELEMENT_REGISTER_DEFINE (clapperglimport, "clapperglimport", GST_RANK_NONE,
    GST_TYPE_CLAPPER_GL_IMPORT);

static GstBufferPool *
gst_clapper_gl_import_create_upstream_pool (GstClapperBaseImport *bi, GstStructure **config)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (bi);
  GstBufferPool *pool;
  GstGLContext *context;

  GST_DEBUG_OBJECT (bi, "Creating new pool");

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (gl_bi);
  context = gst_object_ref (gl_bi->gst_context);
  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);

  pool = gst_gl_buffer_pool_new (context);
  gst_object_unref (context);

  *config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_add_option (*config, GST_BUFFER_POOL_OPTION_GL_SYNC_META);

  return pool;
}

static void
video_frame_unmap_and_free (GstVideoFrame *frame)
{
  gst_video_frame_unmap (frame);
  g_slice_free (GstVideoFrame, frame);
}

static GstFlowReturn
gst_clapper_gl_import_transform (GstBaseTransform *bt,
    GstBuffer *in_buf, GstBuffer *out_buf)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (bt);
  GstClapperBaseImport *bi = GST_CLAPPER_BASE_IMPORT_CAST (bt);
  GstVideoFrame *frame;
  GstMapInfo info;
  GstMemory *memory;
  GstClapperGdkMemory *clapper_memory;
  GstGLSyncMeta *sync_meta;

  frame = g_slice_new (GstVideoFrame);

  if (!gst_clapper_base_import_map_buffers (bi, in_buf, out_buf,
      GST_MAP_READ | GST_MAP_GL, GST_MAP_WRITE, frame, &info, &memory)) {
    g_slice_free (GstVideoFrame, frame);

    return GST_FLOW_ERROR;
  }

  clapper_memory = GST_CLAPPER_GDK_MEMORY_CAST (memory);

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (gl_bi);

  /* Must have context active here for both sync meta
   * and Gdk texture format auto-detection to work */
  gdk_gl_context_make_current (gl_bi->gdk_context);
  gst_gl_context_activate (gl_bi->wrapped_context, TRUE);

  sync_meta = gst_buffer_get_gl_sync_meta (in_buf);

  /* Wait for all previous OpenGL commands to complete,
   * before we start using the input texture */
  if (sync_meta) {
    gst_gl_sync_meta_set_sync_point (sync_meta, gl_bi->gst_context);
    gst_gl_sync_meta_wait (sync_meta, gl_bi->wrapped_context);
  }

  /* Keep input data alive as long as necessary,
   * unmap only after texture is destroyed */
  clapper_memory->texture = gdk_gl_texture_new (
      gl_bi->gdk_context,
      *(guint *) GST_VIDEO_FRAME_PLANE_DATA (frame, 0),
      GST_VIDEO_FRAME_WIDTH (frame),
      GST_VIDEO_FRAME_HEIGHT (frame),
      (GDestroyNotify) video_frame_unmap_and_free,
      frame);

  gst_gl_context_activate (gl_bi->wrapped_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);

  gst_memory_unmap (memory, &info);

  return GST_FLOW_OK;
}

static void
gst_clapper_gl_import_init (GstClapperGLImport *self)
{
}

static void
gst_clapper_gl_import_class_init (GstClapperGLImportClass *klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *gstbasetransform_class = (GstBaseTransformClass *) klass;
  GstClapperBaseImportClass *bi_class = (GstClapperBaseImportClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperglimport", 0,
      "Clapper GL Import");

  gstbasetransform_class->transform = gst_clapper_gl_import_transform;

  bi_class->create_upstream_pool = gst_clapper_gl_import_create_upstream_pool;

  gst_element_class_set_metadata (gstelement_class,
      "Clapper GL import",
      "Filter/Video", "Imports GL memory into ClapperGdkMemory",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_gl_import_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_clapper_gl_import_src_template);
}
