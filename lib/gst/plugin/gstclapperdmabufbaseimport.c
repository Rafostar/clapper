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
#include <gst/gl/egl/gsteglimage.h>

#include "gstclapperdmabufbaseimport.h"
#include "gstgtkutils.h"

#define GST_CAT_DEFAULT gst_clapper_dmabuf_base_import_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

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
  GstClapperDmabufBaseImport *dmabuf_bi;
  GLuint id;
  guint width;
  guint height;
} GstClapperDmabufTexData;

#define parent_class gst_clapper_dmabuf_base_import_parent_class
G_DEFINE_TYPE (GstClapperDmabufBaseImport, gst_clapper_dmabuf_base_import, GST_TYPE_CLAPPER_GL_BASE_IMPORT);

static void
gst_clapper_dmabuf_base_import_init (GstClapperDmabufBaseImport *self)
{
  g_mutex_init (&self->lock);

  self->gst_tex_target = GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
  self->gl_tex_target = gst_gl_texture_target_to_gl (self->gst_tex_target);
}

static void
gst_clapper_dmabuf_base_import_finalize (GObject *object)
{
  GstClapperDmabufBaseImport *self = GST_CLAPPER_DMABUF_BASE_IMPORT_CAST (object);

  gst_clear_object (&self->shader);

  g_mutex_clear (&self->lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_dmabuf_base_import_bind_buffer (GstClapperDmabufBaseImport *self)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (self);
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
gst_clapper_dmabuf_base_import_unbind_buffer (GstClapperDmabufBaseImport *self)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (self);
  const GstGLFuncs *gl = gl_bi->gst_context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  gl->DisableVertexAttribArray (self->attr_position);
  gl->DisableVertexAttribArray (self->attr_texture);
}

static gboolean
prepare_on_main (GstClapperDmabufBaseImport *self)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (self);
  GstGLSLStage *frag_stage, *vert_stage;
  GError *error = NULL;
  gchar *frag_str;
  const GstGLFuncs *gl;

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (gl_bi);

  gdk_gl_context_make_current (gl_bi->gdk_context);
  gst_gl_context_activate (gl_bi->gst_context, TRUE);

  if (!((vert_stage = gst_glsl_stage_new_with_string (gl_bi->gst_context,
      GL_VERTEX_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
      gst_gl_shader_string_vertex_mat4_vertex_transform)))) {
    gdk_gl_context_make_current (gl_bi->gdk_context);
    gst_gl_context_activate (gl_bi->gst_context, TRUE);

    GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);
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

    GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);
    GST_ERROR ("Failed to retrieve fragment shader for texture target");

    return FALSE;
  }

  GST_CLAPPER_DMABUF_BASE_IMPORT_LOCK (self);

  if (!((self->shader = gst_gl_shader_new_link_with_stages (gl_bi->gst_context,
      &error, vert_stage, frag_stage, NULL)))) {
    GST_CLAPPER_DMABUF_BASE_IMPORT_UNLOCK (self);

    gst_gl_context_activate (gl_bi->gst_context, FALSE);
    gdk_gl_context_clear_current ();

    GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);

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
    gst_clapper_dmabuf_base_import_bind_buffer (self);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  self->prepared = TRUE;

  GST_CLAPPER_DMABUF_BASE_IMPORT_UNLOCK (self);

  gst_gl_context_activate (gl_bi->gst_context, FALSE);
  gdk_gl_context_clear_current ();

  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);

  return TRUE;
}

static gboolean
ensure_prepared (GstClapperDmabufBaseImport *self)
{
  GST_CLAPPER_DMABUF_BASE_IMPORT_LOCK (self);

  if (self->prepared) {
    GST_CLAPPER_DMABUF_BASE_IMPORT_UNLOCK (self);

    return TRUE;
  }

  if (self->gst_tex_target != GST_GL_TEXTURE_TARGET_EXTERNAL_OES) {
    /* We do not need shaders if texture target is 2D */
    self->prepared = TRUE;
    GST_CLAPPER_DMABUF_BASE_IMPORT_UNLOCK (self);

    return TRUE;
  }

  GST_CLAPPER_DMABUF_BASE_IMPORT_UNLOCK (self);

  if (!(! !gst_gtk_invoke_on_main (
      (GThreadFunc) (GCallback) prepare_on_main, self))) {
    GST_ERROR_OBJECT (self, "Could not ensure prepared");

    return FALSE;
  }

  return TRUE;
}

static GstStateChangeReturn
gst_clapper_dmabuf_base_import_change_state (GstElement *element, GstStateChange transition)
{
  GstClapperDmabufBaseImport *self = GST_CLAPPER_DMABUF_BASE_IMPORT_CAST (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!ensure_prepared (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
_dmabuf_into_texture (GstClapperDmabufBaseImport *self, gint *fds, GstVideoInfo *v_info,
    gsize *offsets, GstClapperDmabufTexData *tex_data)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (self);
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
_oes_texture_into_2d (GstClapperDmabufBaseImport *self, GstClapperDmabufTexData *tex_data)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (self);
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

  gst_clapper_dmabuf_base_import_bind_buffer (self);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (self->gl_tex_target, tex_data->id);

  gst_gl_shader_set_uniform_1i (self->shader, "tex", 0);
  gst_gl_shader_set_uniform_matrix_4fv (self->shader,
      "u_transformation", 1, FALSE, vertical_flip_matrix);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  if (gl->BindVertexArray)
    gl->BindVertexArray (0);
  else
    gst_clapper_dmabuf_base_import_unbind_buffer (self);

  gl->BindTexture (self->gl_tex_target, 0);

  /* Replace External OES texture with newly created 2D */
  gl->DeleteTextures (1, &tex_data->id);
  tex_data->id = tex_id;

  gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
  gl->DeleteFramebuffers (1, &framebuffer);

  return TRUE;
}

static void
_tex_data_free (GstClapperDmabufTexData *tex_data)
{
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (tex_data->dmabuf_bi);

  if (G_LIKELY (tex_data->id > 0)) {
    const GstGLFuncs *gl;

    GST_CLAPPER_GL_BASE_IMPORT_LOCK (gl_bi);

    gl = gl_bi->gst_context->gl_vtable;

    gdk_gl_context_make_current (gl_bi->gdk_context);
    gst_gl_context_activate (gl_bi->gst_context, TRUE);

    gl->DeleteTextures (1, &tex_data->id);

    gst_gl_context_activate (gl_bi->gst_context, FALSE);
    gdk_gl_context_clear_current ();

    GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);
  }

  gst_object_unref (tex_data->dmabuf_bi);
  g_slice_free (GstClapperDmabufTexData, tex_data);
}

GdkTexture *
gst_clapper_dmabuf_base_import_fds_into_texture (GstClapperDmabufBaseImport *self, gint *fds, gsize *offsets)
{
  GstClapperBaseImport *bi = GST_CLAPPER_BASE_IMPORT_CAST (self);
  GstClapperGLBaseImport *gl_bi = GST_CLAPPER_GL_BASE_IMPORT_CAST (self);
  GdkTexture *texture = NULL;
  GstClapperDmabufTexData *tex_data;

  tex_data = g_slice_new (GstClapperDmabufTexData);
  tex_data->dmabuf_bi = gst_object_ref (self);

  GST_CLAPPER_GL_BASE_IMPORT_LOCK (gl_bi);

  gdk_gl_context_make_current (gl_bi->gdk_context);
  gst_gl_context_activate (gl_bi->gst_context, TRUE);

  if (!_dmabuf_into_texture (self, fds, &bi->in_info, offsets, tex_data))
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

  GST_CLAPPER_GL_BASE_IMPORT_UNLOCK (gl_bi);

  return texture;
}

static void
gst_clapper_dmabuf_base_import_class_init (GstClapperDmabufBaseImportClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperdmabufbaseimport", 0,
      "Clapper DMABuf Base Import");

  gobject_class->finalize = gst_clapper_dmabuf_base_import_finalize;

  gstelement_class->change_state = gst_clapper_dmabuf_base_import_change_state;
}
