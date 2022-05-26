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

#include "gstclappergluploader.h"
#include "gst/plugin/gstgtkutils.h"

#if GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11
#include <gdk/x11/gdkx.h>
#endif

#define GST_CAT_DEFAULT gst_clapper_gl_uploader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_gl_uploader_parent_class
GST_CLAPPER_IMPORTER_DEFINE (GstClapperGLUploader, gst_clapper_gl_uploader, GST_TYPE_CLAPPER_GL_BASE_IMPORTER);

static void
_update_elements_caps_locked (GstClapperGLUploader *self, GstCaps *upload_sink_caps)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  GstCaps *upload_src_caps, *color_sink_caps, *color_src_caps, *gdk_sink_caps;

  GST_INFO_OBJECT (self, "Input caps: %" GST_PTR_FORMAT, upload_sink_caps);

  upload_src_caps = gst_gl_upload_transform_caps (self->upload, gl_bi->gst_context,
      GST_PAD_SINK, upload_sink_caps, NULL);
  upload_src_caps = gst_caps_fixate (upload_src_caps);

  GST_INFO_OBJECT (self, "GLUpload caps: %" GST_PTR_FORMAT, upload_src_caps);
  gst_gl_upload_set_caps (self->upload, upload_sink_caps, upload_src_caps);

  gdk_sink_caps = gst_clapper_gl_base_importer_make_supported_gdk_gl_caps ();
  color_sink_caps = gst_gl_color_convert_transform_caps (gl_bi->gst_context,
      GST_PAD_SRC, upload_src_caps, gdk_sink_caps);
  gst_caps_unref (gdk_sink_caps);

  /* Second caps arg is transfer-full */
  color_src_caps = gst_gl_color_convert_fixate_caps (gl_bi->gst_context,
      GST_PAD_SINK, upload_src_caps, color_sink_caps);

  GST_INFO_OBJECT (self, "GLColorConvert caps: %" GST_PTR_FORMAT, color_src_caps);
  gst_gl_color_convert_set_caps (self->color_convert, upload_src_caps, color_src_caps);

  self->has_pending_v_info = gst_video_info_from_caps (&self->pending_v_info, color_src_caps);

  gst_caps_unref (upload_src_caps);
  gst_caps_unref (color_src_caps);
}

static void
gst_clapper_gl_uploader_set_caps (GstClapperImporter *importer, GstCaps *caps)
{
  GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (importer);

  GST_CLAPPER_GL_BASE_IMPORTER_LOCK (self);
  _update_elements_caps_locked (self, caps);
  GST_CLAPPER_GL_BASE_IMPORTER_UNLOCK (self);
}

static void
_uploader_reconfigure_locked (GstClapperGLUploader *self)
{
  GstCaps *in_caps = NULL;

  GST_DEBUG_OBJECT (self, "Reconfiguring upload");

  gst_gl_upload_get_caps (self->upload, &in_caps, NULL);

  if (G_LIKELY (in_caps)) {
    _update_elements_caps_locked (self, in_caps);
    gst_caps_unref (in_caps);
  }
}

static gboolean
gst_clapper_gl_uploader_prepare (GstClapperImporter *importer)
{
  gboolean res = GST_CLAPPER_IMPORTER_CLASS (parent_class)->prepare (importer);

  if (res) {
    GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (importer);
    GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (importer);

    GST_CLAPPER_GL_BASE_IMPORTER_LOCK (self);

    if (!self->upload)
      self->upload = gst_gl_upload_new (gl_bi->gst_context);
    if (!self->color_convert)
      self->color_convert = gst_gl_color_convert_new (gl_bi->gst_context);

    GST_CLAPPER_GL_BASE_IMPORTER_UNLOCK (self);
  }

  return res;
}

static GstBuffer *
_upload_perform_locked (GstClapperGLUploader *self, GstBuffer *buffer)
{
  GstBuffer *upload_buf = NULL;
  GstGLUploadReturn ret;

  ret = gst_gl_upload_perform_with_buffer (self->upload, buffer, &upload_buf);

  if (G_UNLIKELY (ret != GST_GL_UPLOAD_DONE)) {
    switch (ret) {
      case GST_GL_UPLOAD_RECONFIGURE:
        _uploader_reconfigure_locked (self);
        /* Retry with the same buffer after reconfiguring */
        return _upload_perform_locked (self, buffer);
      default:
        GST_ERROR_OBJECT (self, "Could not upload input buffer, returned: %i", ret);
        break;
    }
  }

  return upload_buf;
}

static GstBufferPool *
gst_clapper_gl_uploader_create_pool (GstClapperImporter *importer, GstStructure **config)
{
  /* Since GLUpload API provides a ready to use propose_allocation method,
   * we will use it with our query in add_allocation_metas instead of
   * making pool here ourselves */
  return NULL;
}

static void
gst_clapper_gl_uploader_add_allocation_metas (GstClapperImporter *importer, GstQuery *query)
{
  GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (importer);
  GstGLUpload *upload;

  GST_CLAPPER_GL_BASE_IMPORTER_LOCK (self);
  upload = gst_object_ref (self->upload);
  GST_CLAPPER_GL_BASE_IMPORTER_UNLOCK (self);

  gst_gl_upload_propose_allocation (upload, NULL, query);
  gst_object_unref (upload);

  GST_CLAPPER_IMPORTER_CLASS (parent_class)->add_allocation_metas (importer, query);
}

static GdkTexture *
gst_clapper_gl_uploader_generate_texture (GstClapperImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (importer);
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (importer);
  GstBuffer *upload_buf, *color_buf;
  GdkTexture *texture;

  /* XXX: We both upload and perform color conversion here, thus we skip
   * upload for buffers that are not going to be shown and gain more free
   * CPU time to prepare the next one. Improves performance on weak HW. */

  GST_LOG_OBJECT (self, "Uploading %" GST_PTR_FORMAT, buffer);

  GST_CLAPPER_GL_BASE_IMPORTER_LOCK (self);

  upload_buf = _upload_perform_locked (self, buffer);

  if (G_UNLIKELY (!upload_buf)) {
    GST_ERROR_OBJECT (self, "Could not perform upload on input buffer");
    GST_CLAPPER_GL_BASE_IMPORTER_UNLOCK (self);

    return NULL;
  }
  GST_LOG_OBJECT (self, "Uploaded into %" GST_PTR_FORMAT, upload_buf);

  color_buf = gst_gl_color_convert_perform (self->color_convert, upload_buf);
  gst_buffer_unref (upload_buf);

  /* Use video info associated with converted buffer */
  if (self->has_pending_v_info) {
    self->v_info = self->pending_v_info;
    self->has_pending_v_info = FALSE;
  }

  GST_CLAPPER_GL_BASE_IMPORTER_UNLOCK (self);

  if (G_UNLIKELY (!color_buf)) {
    GST_ERROR_OBJECT (self, "Could not perform color conversion on input buffer");
    return NULL;
  }
  GST_LOG_OBJECT (self, "Color converted into %" GST_PTR_FORMAT, color_buf);

  texture = gst_clapper_gl_base_importer_make_gl_texture (gl_bi, color_buf, &self->v_info);
  gst_buffer_unref (color_buf);

  return texture;
}

static void
gst_clapper_gl_uploader_init (GstClapperGLUploader *self)
{
  gst_video_info_init (&self->pending_v_info);
  gst_video_info_init (&self->v_info);
}

static void
gst_clapper_gl_uploader_finalize (GObject *object)
{
  GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (object);

  gst_clear_object (&self->upload);
  gst_clear_object (&self->color_convert);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_gl_uploader_class_init (GstClapperGLUploaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstClapperImporterClass *importer_class = (GstClapperImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergluploader", 0,
      "Clapper GL Uploader");

  gobject_class->finalize = gst_clapper_gl_uploader_finalize;

  importer_class->prepare = gst_clapper_gl_uploader_prepare;
  importer_class->set_caps = gst_clapper_gl_uploader_set_caps;
  importer_class->create_pool = gst_clapper_gl_uploader_create_pool;
  importer_class->add_allocation_metas = gst_clapper_gl_uploader_add_allocation_metas;
  importer_class->generate_texture = gst_clapper_gl_uploader_generate_texture;
}

GstClapperImporter *
make_importer (void)
{
  return g_object_new (GST_TYPE_CLAPPER_GL_UPLOADER, NULL);
}

#if GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11_GLX
static gboolean
_filter_glx_caps_cb (GstCapsFeatures *features,
    GstStructure *structure, gpointer user_data)
{
  return !gst_caps_features_contains (features, "memory:DMABuf");
}

static gboolean
_update_glx_caps_on_main (GstCaps *caps)
{
  GdkDisplay *gdk_display;

  if (!gtk_init_check ())
    return FALSE;

  gdk_display = gdk_display_get_default ();
  if (G_UNLIKELY (!gdk_display))
    return FALSE;

  if (GDK_IS_X11_DISPLAY (gdk_display)) {
    gboolean using_glx = TRUE;

#if GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11_EGL
    using_glx = (gdk_x11_display_get_egl_display (gdk_display) == NULL);
#endif

    if (using_glx) {
      gst_caps_filter_and_map_in_place (caps,
          (GstCapsFilterMapFunc) _filter_glx_caps_cb, NULL);
    }
  }

  return TRUE;
}
#endif

GstCaps *
make_caps (gboolean is_template, GstRank *rank, GStrv *context_types)
{
  GstCaps *caps = gst_gl_upload_get_input_template_caps ();

#if GST_CLAPPER_GL_BASE_IMPORTER_HAVE_X11_GLX
  if (!is_template && !(! !gst_gtk_invoke_on_main ((GThreadFunc) (GCallback)
      _update_glx_caps_on_main, caps)))
    gst_clear_caps (&caps);
#endif

  if (G_UNLIKELY (!caps))
    return NULL;

  *rank = GST_RANK_MARGINAL + 1;
  *context_types = gst_clapper_gl_base_importer_make_gl_context_types ();

  return caps;
}
