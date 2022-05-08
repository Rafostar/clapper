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

#include <gst/gl/egl/gsteglimage.h>
#include <gst/allocators/gstdmabuf.h>

static const GLfloat vertices[] = {
  1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
  -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
  1.0f, -1.0f, 0.0f, 1.0f, 1.0f
};
static const GLushort indices[] = {
  0, 1, 2, 0, 2, 3
};

/* GTK4 renders things upside down ¯\_(ツ)_/¯ */
static const gfloat vertical_flip_matrix[] = {
  1.0f, 0.0f, 0.0f, 0.0f,
  0.0f, -1.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f,
};

typedef struct
{
  GstClapperGLUploader *dmabuf_bi;
  GLuint id;
  guint width;
  guint height;
} GstClapperDmabufTexData;

#define GST_CAT_DEFAULT gst_clapper_gl_uploader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_gl_uploader_parent_class
GST_CLAPPER_IMPORTER_DEFINE (GstClapperGLUploader, gst_clapper_gl_uploader, GST_TYPE_CLAPPER_GL_BASE_IMPORTER);

static void
gst_clapper_gl_uploader_bind_buffer (GstClapperGLUploader *self)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  const GstGLFuncs *gl = gl_bi->gst_context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, self->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (self->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (self->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (self->attr_position);
  gl->EnableVertexAttribArray (self->attr_texture);
}

static void
gst_clapper_gl_uploader_unbind_buffer (GstClapperGLUploader *self)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  const GstGLFuncs *gl = gl_bi->gst_context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  gl->DisableVertexAttribArray (self->attr_position);
  gl->DisableVertexAttribArray (self->attr_texture);
}

static gboolean
prepare_dmabuf_support_on_main (GstClapperGLUploader *self)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  GstGLSLStage *frag_stage, *vert_stage;
  GError *error = NULL;
  gchar *frag_str;
  const GstGLFuncs *gl;

  GST_OBJECT_LOCK (self);

  /* FIXME: Return if already prepared */

  gdk_gl_context_make_current (gl_bi->gdk_context);
  gst_gl_context_activate (gl_bi->gst_context, TRUE);

  if (!((vert_stage = gst_glsl_stage_new_with_string (gl_bi->gst_context,
      GL_VERTEX_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
      gst_gl_shader_string_vertex_mat4_vertex_transform)))) {
    gdk_gl_context_make_current (gl_bi->gdk_context);
    gst_gl_context_activate (gl_bi->gst_context, TRUE);

    GST_OBJECT_UNLOCK (self);
    GST_ERROR ("Failed to retrieve vertex shader for texture target");

    return FALSE;
  }

  frag_str = gst_gl_shader_string_fragment_external_oes_get_default (
      gl_bi->gst_context, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frag_stage = gst_glsl_stage_new_with_string (gl_bi->gst_context,
      GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, frag_str);

  g_free (frag_str);

  if (!frag_stage) {
    gst_gl_context_activate (gl_bi->gst_context, FALSE);
    gdk_gl_context_clear_current ();

    GST_OBJECT_UNLOCK (self);
    GST_ERROR ("Failed to retrieve fragment shader for texture target");

    return FALSE;
  }

  if (!((self->shader = gst_gl_shader_new_link_with_stages (gl_bi->gst_context,
      &error, vert_stage, frag_stage, NULL)))) {
    gst_gl_context_activate (gl_bi->gst_context, FALSE);
    gdk_gl_context_clear_current ();

    GST_OBJECT_UNLOCK (self);

    GST_ERROR ("Failed to initialize shader: %s", error->message);
    g_clear_error (&error);

    return FALSE;
  }

  self->attr_position =
      gst_gl_shader_get_attribute_location (self->shader, "a_position");
  self->attr_texture =
      gst_gl_shader_get_attribute_location (self->shader, "a_texcoord");

  gl = gl_bi->gst_context->gl_vtable;

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &self->vao);
    gl->BindVertexArray (self->vao);
  }

  gl->GenBuffers (1, &self->vertex_buffer);
  gl->BindBuffer (GL_ARRAY_BUFFER, self->vertex_buffer);
  gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices, GL_STATIC_DRAW);

  if (gl->GenVertexArrays) {
    gst_clapper_gl_uploader_bind_buffer (self);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  //self->prepared = TRUE;

  gst_gl_context_activate (gl_bi->gst_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static void
_tex_data_free (GstClapperDmabufTexData *tex_data)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (tex_data->dmabuf_bi);

  if (G_LIKELY (tex_data->id > 0)) {
    const GstGLFuncs *gl;

    GST_OBJECT_LOCK (gl_bi);

    gl = gl_bi->gst_context->gl_vtable;

    gdk_gl_context_make_current (gl_bi->gdk_context);
    gst_gl_context_activate (gl_bi->gst_context, TRUE);

    gl->DeleteTextures (1, &tex_data->id);

    gst_gl_context_activate (gl_bi->gst_context, FALSE);
    gdk_gl_context_clear_current ();

    GST_OBJECT_UNLOCK (gl_bi);
  }

  gst_object_unref (tex_data->dmabuf_bi);
  g_slice_free (GstClapperDmabufTexData, tex_data);
}

static gboolean
_dmabuf_into_texture (GstClapperGLUploader *self, gint *fds, GstVideoInfo *v_info,
    gsize *offsets, GstClapperDmabufTexData *tex_data)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  GstEGLImage *image;
  const GstGLFuncs *gl;

  image = gst_egl_image_from_dmabuf_direct_target (gl_bi->gst_context,
      fds, offsets, v_info, self->gst_tex_target);

  /* If HW colorspace conversion failed and there is only one
   * plane, we can just make it into single EGLImage as is */
  if (!image && GST_VIDEO_INFO_N_PLANES (v_info) == 1)
    image = gst_egl_image_from_dmabuf (gl_bi->gst_context,
        fds[0], v_info, 0, offsets[0]);

  if (!image)
    return FALSE;

  gl = gl_bi->gst_context->gl_vtable;

  gl->GenTextures (1, &tex_data->id);
  tex_data->width = GST_VIDEO_INFO_WIDTH (v_info);
  tex_data->height = GST_VIDEO_INFO_HEIGHT (v_info);

  gl->BindTexture (self->gl_tex_target, tex_data->id);

  gl->TexParameteri (self->gl_tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri (self->gl_tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  gl->TexParameteri (self->gl_tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri (self->gl_tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->EGLImageTargetTexture2D (self->gl_tex_target, gst_egl_image_get_image (image));

  gl->BindTexture (GL_TEXTURE_2D, 0);
  gst_egl_image_unref (image);

  return TRUE;
}

static gboolean
_oes_texture_into_2d (GstClapperGLUploader *self, GstClapperDmabufTexData *tex_data)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  GLuint framebuffer, tex_id;
  GLenum status;
  const GstGLFuncs *gl;

  gl = gl_bi->gst_context->gl_vtable;

  gl->GenFramebuffers (1, &framebuffer);
  gl->BindFramebuffer (GL_FRAMEBUFFER, framebuffer);

  gl->GenTextures (1, &tex_id);
  gl->BindTexture (GL_TEXTURE_2D, tex_id);

  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, tex_data->width, tex_data->height, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  gl->FramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, tex_id, 0);

  status = gl->CheckFramebufferStatus (GL_FRAMEBUFFER);
  if (G_UNLIKELY (status != GL_FRAMEBUFFER_COMPLETE)) {
    GST_ERROR ("Invalid framebuffer status: %u", status);

    gl->BindTexture (GL_TEXTURE_2D, 0);
    gl->DeleteTextures (1, &tex_id);

    gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
    gl->DeleteFramebuffers (1, &framebuffer);

    return FALSE;
  }

  gl->Viewport (0, 0, tex_data->width, tex_data->height);

  gst_gl_shader_use (self->shader);

  if (gl->BindVertexArray)
    gl->BindVertexArray (self->vao);

  gst_clapper_gl_uploader_bind_buffer (self);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (self->gl_tex_target, tex_data->id);

  gst_gl_shader_set_uniform_1i (self->shader, "tex", 0);
  gst_gl_shader_set_uniform_matrix_4fv (self->shader,
      "u_transformation", 1, FALSE, vertical_flip_matrix);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  if (gl->BindVertexArray)
    gl->BindVertexArray (0);
  else
    gst_clapper_gl_uploader_unbind_buffer (self);

  gl->BindTexture (self->gl_tex_target, 0);

  /* Replace External OES texture with newly created 2D */
  gl->DeleteTextures (1, &tex_data->id);
  tex_data->id = tex_id;

  gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
  gl->DeleteFramebuffers (1, &framebuffer);

  return TRUE;
}

static GdkTexture *
dmabuf_into_gdk_texture (GstClapperGLUploader *self, GstVideoInfo *v_info, gint *fds, gsize *offsets)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (self);
  GdkTexture *texture = NULL;
  GstClapperDmabufTexData *tex_data;

  tex_data = g_slice_new (GstClapperDmabufTexData);
  tex_data->dmabuf_bi = gst_object_ref (self);

  GST_OBJECT_LOCK (self);

  gdk_gl_context_make_current (gl_bi->gdk_context);
  gst_gl_context_activate (gl_bi->gst_context, TRUE);

  if (!_dmabuf_into_texture (self, fds, v_info, offsets, tex_data))
    goto finish;

  /* GTK4 does not support External OES textures.
   * Make it into 2D using framebuffer + shader */
  if (self->gst_tex_target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    if (G_UNLIKELY (!_oes_texture_into_2d (self, tex_data)))
      goto finish;
  }

  texture = gdk_gl_texture_new (gl_bi->gdk_context,
      tex_data->id,
      tex_data->width,
      tex_data->height,
      (GDestroyNotify) _tex_data_free,
      tex_data);

finish:
  gst_gl_context_activate (gl_bi->gst_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_OBJECT_UNLOCK (self);

  return texture;
}

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

  GST_OBJECT_LOCK (self);
  _update_elements_caps_locked (self, caps);
  GST_OBJECT_UNLOCK (self);
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

    GST_OBJECT_LOCK (self);

    if (!self->upload)
      self->upload = gst_gl_upload_new (gl_bi->gst_context);
    if (!self->color_convert)
      self->color_convert = gst_gl_color_convert_new (gl_bi->gst_context);

    GST_OBJECT_UNLOCK (self);

    if (!(! !gst_gtk_invoke_on_main (
        (GThreadFunc) (GCallback) prepare_dmabuf_support_on_main, self))) {
      GST_WARNING_OBJECT (self, "Could not prepare DMABuf support");

      /* FIXME: Continue to allow using glupload/cc as fallback */
      return FALSE;
    }
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

static GdkTexture *
gst_clapper_gl_uploader_generate_texture (GstClapperImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (importer);
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (importer);
  GstBuffer *upload_buf, *color_buf;
  GstVideoMeta *meta;
  GdkTexture *texture;

  /* XXX: We both upload and perform color conversion here, thus we skip
   * upload for buffers that are not going to be shown and gain more free
   * CPU time to prepare the next one. Improves performance on weak HW. */

  if ((meta = gst_buffer_get_video_meta (buffer))) {
    guint i;

    GST_VIDEO_INFO_WIDTH (v_info) = meta->width;
    GST_VIDEO_INFO_HEIGHT (v_info) = meta->height;

    for (i = 0; i < meta->n_planes; i++) {
      GST_VIDEO_INFO_PLANE_OFFSET (v_info, i) = meta->offset[i];
      GST_VIDEO_INFO_PLANE_STRIDE (v_info, i) = meta->stride[i];
    }
  }

  /* FIXME: if can do dmabuf and seems like we have dmabuf here */
  {
    gint fds[GST_VIDEO_MAX_PLANES];
    gsize offsets[GST_VIDEO_MAX_PLANES];

    if (verify_dmabuf_memory (buffer, v_info, fds, offsets)) {
      if ((texture = dmabuf_into_gdk_texture (self, v_info, fds, offsets))) {
        GST_TRACE_OBJECT (self, "Got texture from DMABuf, skipping upload of %" GST_PTR_FORMAT, buffer);
        goto done;
      }
    }
  }

  GST_LOG_OBJECT (self, "Uploading %" GST_PTR_FORMAT, buffer);

  GST_OBJECT_LOCK (self);

  upload_buf = _upload_perform_locked (self, buffer);

  if (G_UNLIKELY (!upload_buf)) {
    GST_ERROR_OBJECT (self, "Could not perform upload on input buffer");
    GST_OBJECT_UNLOCK (self);

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

  GST_OBJECT_UNLOCK (self);

  if (G_UNLIKELY (!color_buf)) {
    GST_ERROR_OBJECT (self, "Could not perform color conversion on input buffer");
    return NULL;
  }
  GST_LOG_OBJECT (self, "Color converted into %" GST_PTR_FORMAT, color_buf);

  texture = gst_clapper_gl_base_importer_make_gl_texture (gl_bi, color_buf, &self->v_info);
  gst_buffer_unref (color_buf);

done:
  return texture;
}

static void
gst_clapper_gl_uploader_init (GstClapperGLUploader *self)
{
  gst_video_info_init (&self->pending_v_info);
  gst_video_info_init (&self->v_info);

  self->gst_tex_target = GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
  self->gl_tex_target = gst_gl_texture_target_to_gl (self->gst_tex_target);
}

static void
gst_clapper_gl_uploader_finalize (GObject *object)
{
  GstClapperGLUploader *self = GST_CLAPPER_GL_UPLOADER_CAST (object);

  gst_clear_object (&self->upload);
  gst_clear_object (&self->color_convert);

  gst_clear_object (&self->shader);

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
  importer_class->generate_texture = gst_clapper_gl_uploader_generate_texture;
}

GstClapperImporter *
make_importer (void)
{
  return g_object_new (GST_TYPE_CLAPPER_GL_UPLOADER, NULL);
}

GstCaps *
make_caps (GstRank *rank, GStrv *context_types)
{
  *rank = GST_RANK_MARGINAL + 1;
  *context_types = gst_clapper_gl_base_importer_make_gl_context_types ();

  return gst_gl_upload_get_input_template_caps ();
}
