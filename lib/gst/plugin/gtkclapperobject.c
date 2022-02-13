/*
 * GStreamer
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include <gst/gl/gstglfuncs.h>
#include <gst/allocators/gstdmabuf.h>

#include "gtkclapperobject.h"
#include "gstgtkutils.h"

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
#include <gdk/x11/gdkx.h>
#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif
#endif

#if GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND)
#include <gdk/wayland/gdkwayland.h>
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gsteglimage.h>
#endif

GST_DEBUG_CATEGORY (gst_debug_clapper_object);
#define GST_CAT_DEFAULT gst_debug_clapper_object

static void gtk_clapper_object_paintable_iface_init (GdkPaintableInterface *iface);
static void gtk_clapper_object_finalize (GObject *object);

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

#define gtk_clapper_object_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GtkClapperObject, gtk_clapper_object, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
        gtk_clapper_object_paintable_iface_init)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gtkclapperobject", 0,
        "GTK Clapper Object"));

static void
gtk_clapper_object_class_init (GtkClapperObjectClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gtk_clapper_object_finalize;
}

static void
gtk_clapper_object_init (GtkClapperObject *self)
{
  self->last_pos_x = 0;
  self->last_pos_y = 0;

  self->picture = (GtkPicture *) gtk_picture_new ();

  /* We cannot do textures of 0x0px size */
  gtk_widget_set_size_request (GTK_WIDGET (self->picture), 1, 1);

  /* Center instead of fill to not draw empty space into framebuffer */
  gtk_widget_set_halign (GTK_WIDGET (self->picture), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self->picture), GTK_ALIGN_CENTER);

  gtk_picture_set_paintable (self->picture, GDK_PAINTABLE (self));

  gst_video_info_init (&self->v_info);
  gst_video_info_init (&self->pending_v_info);

  g_weak_ref_init (&self->element, NULL);
  g_mutex_init (&self->lock);

  self->gst_tex_target = GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
  self->gl_tex_target = gst_gl_texture_target_to_gl (self->gst_tex_target);
}

static void
gtk_clapper_object_finalize (GObject *object)
{
  GtkClapperObject *self = GTK_CLAPPER_OBJECT (object);

  if (self->draw_id)
    g_source_remove (self->draw_id);

  gst_buffer_replace (&self->pending_buffer, NULL);
  gst_buffer_replace (&self->buffer, NULL);

  g_mutex_clear (&self->lock);
  g_weak_ref_clear (&self->element);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
_gdk_gl_context_set_active (GtkClapperObject *self, gboolean activate)
{
  /* We wrap around a GDK context, so we need to make
   * both GTK and GStreamer aware of its active state */
  if (activate) {
    gdk_gl_context_make_current (self->gdk_context);
    gst_gl_context_activate (self->wrapped_context, TRUE);
  } else {
    gst_gl_context_activate (self->wrapped_context, FALSE);
    gdk_gl_context_clear_current ();
  }
}

static GdkMemoryFormat
video_format_to_gdk_memory_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_BGR:
      return GDK_MEMORY_B8G8R8;
    case GST_VIDEO_FORMAT_RGB:
      return GDK_MEMORY_R8G8B8;
    case GST_VIDEO_FORMAT_BGRA:
      return GDK_MEMORY_B8G8R8A8;
    case GST_VIDEO_FORMAT_RGBA:
      return GDK_MEMORY_R8G8B8A8;
    case GST_VIDEO_FORMAT_ABGR:
      return GDK_MEMORY_A8B8G8R8;
    case GST_VIDEO_FORMAT_ARGB:
      return GDK_MEMORY_A8R8G8B8;
    case GST_VIDEO_FORMAT_BGRx:
      return GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
    case GST_VIDEO_FORMAT_RGBx:
      return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_RGBA64_BE:
      return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
    default:
      g_assert_not_reached ();
  }

  /* Number not belonging to any format */
  return GDK_MEMORY_N_FORMATS;
}

static void
gtk_clapper_object_bind_buffer (GtkClapperObject *self)
{
  const GstGLFuncs *gl = self->wrapped_context->gl_vtable;

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
gtk_clapper_object_unbind_buffer (GtkClapperObject *self)
{
  const GstGLFuncs *gl = self->wrapped_context->gl_vtable;

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  gl->DisableVertexAttribArray (self->attr_position);
  gl->DisableVertexAttribArray (self->attr_texture);
}

static void
gtk_clapper_object_init_redisplay (GtkClapperObject *self)
{
  GstGLSLStage *frag_stage, *vert_stage;
  GError *error = NULL;
  gchar *frag_str;
  const GstGLFuncs *gl;

  if (self->gst_tex_target != GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    return;

  if (!((vert_stage = gst_glsl_stage_new_with_string (self->wrapped_context,
      GL_VERTEX_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
      gst_gl_shader_string_vertex_mat4_vertex_transform)))) {
    GST_ERROR ("Failed to retrieve vertex shader for texture target");
    return;
  }

  frag_str = gst_gl_shader_string_fragment_external_oes_get_default (
      self->wrapped_context, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frag_stage = gst_glsl_stage_new_with_string (self->wrapped_context,
      GL_FRAGMENT_SHADER, GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, frag_str);

  g_free (frag_str);

  if (!frag_stage) {
    GST_ERROR ("Failed to retrieve fragment shader for texture target");
    return;
  }

  if (!((self->shader = gst_gl_shader_new_link_with_stages (self->wrapped_context,
      &error, vert_stage, frag_stage, NULL)))) {
    GST_ERROR ("Failed to initialize shader: %s", error->message);

    g_clear_error (&error);
    gst_object_unref (vert_stage);
    gst_object_unref (frag_stage);

    return;
  }

  self->attr_position =
      gst_gl_shader_get_attribute_location (self->shader, "a_position");
  self->attr_texture =
      gst_gl_shader_get_attribute_location (self->shader, "a_texcoord");

  gl = self->wrapped_context->gl_vtable;

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &self->vao);
    gl->BindVertexArray (self->vao);
  }

  gl->GenBuffers (1, &self->vertex_buffer);
  gl->BindBuffer (GL_ARRAY_BUFFER, self->vertex_buffer);
  gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices, GL_STATIC_DRAW);

  if (gl->GenVertexArrays) {
    gtk_clapper_object_bind_buffer (self);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
  self->initiated = TRUE;
}

static gboolean
_dmabuf_into_texture (GtkClapperObject *self, gint *fds, gsize *offsets)
{
  GstEGLImage *image;
  const GstGLFuncs *gl;

  image = gst_egl_image_from_dmabuf_direct_target (self->wrapped_context,
      fds, offsets, &self->v_info, self->gst_tex_target);

  /* If HW colorspace conversion failed and there is only one
   * plane, we can just make it into single EGLImage as is */
  if (!image && GST_VIDEO_INFO_N_PLANES (&self->v_info) == 1)
    image = gst_egl_image_from_dmabuf (self->wrapped_context,
        fds[0], &self->v_info, 0, offsets[0]);

  /* Still no image? Give up then */
  if (!image)
    return FALSE;

  gl = self->wrapped_context->gl_vtable;

  if (!self->texture_id)
    gl->GenTextures (1, &self->texture_id);

  gl->BindTexture (self->gl_tex_target, self->texture_id);

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
_ext_texture_into_2d (GtkClapperObject *self, guint tex_width, guint tex_height)
{
  GLuint framebuffer, new_texture_id;
  GLenum status;
  const GstGLFuncs *gl;

  if (!self->initiated)
    gtk_clapper_object_init_redisplay (self);

  gl = self->wrapped_context->gl_vtable;

  gl->GenFramebuffers (1, &framebuffer);
  gl->BindFramebuffer (GL_FRAMEBUFFER, framebuffer);

  gl->GenTextures (1, &new_texture_id);
  gl->BindTexture (GL_TEXTURE_2D, new_texture_id);

  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  gl->FramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, new_texture_id, 0);

  status = gl->CheckFramebufferStatus (GL_FRAMEBUFFER);
  if (G_UNLIKELY (status != GL_FRAMEBUFFER_COMPLETE)) {
    GST_ERROR ("Invalid framebuffer status: %u", status);

    gl->BindTexture (GL_TEXTURE_2D, 0);
    gl->DeleteTextures (1, &new_texture_id);

    gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
    gl->DeleteFramebuffers (1, &framebuffer);

    return FALSE;
  }

  gl->Viewport (0, 0, tex_width, tex_height);

  gst_gl_shader_use (self->shader);

  if (gl->BindVertexArray)
    gl->BindVertexArray (self->vao);

  gtk_clapper_object_bind_buffer (self);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (self->gl_tex_target, self->texture_id);

  gst_gl_shader_set_uniform_1i (self->shader, "tex", 0);
  gst_gl_shader_set_uniform_matrix_4fv (self->shader,
      "u_transformation", 1, FALSE, vertical_flip_matrix);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  if (gl->BindVertexArray)
    gl->BindVertexArray (0);
  else
    gtk_clapper_object_unbind_buffer (self);

  gl->BindTexture (self->gl_tex_target, 0);

  /* Replace external OES texture with new 2D one */
  gl->DeleteTextures (1, &self->texture_id);
  self->texture_id = new_texture_id;

  gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
  gl->DeleteFramebuffers (1, &framebuffer);

  return TRUE;
}

static GdkTexture *
gtk_clapper_object_import_dmabuf (GtkClapperObject *self, gint *fds, gsize *offsets)
{
  GdkTexture *texture;
  guint tex_width, tex_height;

  _gdk_gl_context_set_active (self, TRUE);

  if (!_dmabuf_into_texture (self, fds, offsets)) {
    _gdk_gl_context_set_active (self, FALSE);
    return NULL;
  }

  switch (self->gst_tex_target) {
    case GST_GL_TEXTURE_TARGET_2D:
      tex_width = GST_VIDEO_INFO_WIDTH (&self->v_info);
      tex_height = GST_VIDEO_INFO_HEIGHT (&self->v_info);
      break;
    case GST_GL_TEXTURE_TARGET_EXTERNAL_OES:{
      GtkWidget *widget = (GtkWidget *) self->picture;
      gint scale;

      scale = gtk_widget_get_scale_factor (widget);
      tex_width = gtk_widget_get_width (widget) * scale;
      tex_height = gtk_widget_get_height (widget) * scale;

      if (G_LIKELY (_ext_texture_into_2d (self, tex_width, tex_height)))
        break;

      return NULL;
    }
    default:
      g_assert_not_reached ();
      return NULL;
  }

  texture = gdk_gl_texture_new (self->gdk_context,
      self->texture_id, tex_width, tex_height, NULL, NULL);

  _gdk_gl_context_set_active (self, FALSE);

  return texture;
}

typedef gboolean (*MemTypeCheckFunc) (gpointer data);

static gboolean
buffer_memory_type_check (GstBuffer *buffer, MemTypeCheckFunc func)
{
  guint i, n_mems;

  n_mems = gst_buffer_n_memory (buffer);

  for (i = 0; i < n_mems; i++) {
    if (!func (gst_buffer_peek_memory (buffer, i)))
      return FALSE;
  }

  return n_mems > 0;
}

static gboolean
verify_dmabuf_memory (GtkClapperObject *self, guint n_planes,
    gint *fds, gsize *offsets)
{
  guint i;

  for (i = 0; i < n_planes; i++) {
    GstMemory *memory;
    gsize plane_size, mem_skip;
    guint mem_idx, length;

    plane_size = gst_gl_get_plane_data_size (&self->v_info, NULL, i);

    if (!gst_buffer_find_memory (self->buffer,
        GST_VIDEO_INFO_PLANE_OFFSET (&self->v_info, i),
        plane_size, &mem_idx, &length, &mem_skip)) {
      GST_DEBUG_OBJECT (self, "Could not find memory %u", i);
      return FALSE;
    }

    /* We can't have more then one DMABuf per plane */
    if (length != 1) {
      GST_DEBUG_OBJECT (self, "Data for plane %u spans %u memories",
          i, length);
      return FALSE;
    }

    memory = gst_buffer_peek_memory (self->buffer, mem_idx);

    offsets[i] = memory->offset + mem_skip;
    fds[i] = gst_dmabuf_memory_get_fd (memory);
  }

  return TRUE;
}

static GdkTexture *
obtain_texture_from_current_buffer (GtkClapperObject *self)
{
  GdkTexture *texture = NULL;
  GstVideoFrame frame;

  /* DMABuf */
  if (buffer_memory_type_check (self->buffer, (MemTypeCheckFunc) gst_is_dmabuf_memory)) {
    gsize offsets[GST_VIDEO_MAX_PLANES];
    gint fds[GST_VIDEO_MAX_PLANES];
    guint n_planes;

    n_planes = GST_VIDEO_INFO_N_PLANES (&self->v_info);

    if (!verify_dmabuf_memory (self, n_planes, fds, offsets)) {
      GST_ERROR ("DMABuf memory is invalid");
      return NULL;
    }

    if (!((texture = gtk_clapper_object_import_dmabuf (self, fds, offsets))))
      GST_ERROR ("Could not create texture from DMABuf");

    return texture;
  }

  /* GL Memory */
  if (buffer_memory_type_check (self->buffer, (MemTypeCheckFunc) gst_is_gl_memory)) {
    if (gst_video_frame_map (&frame, &self->v_info, self->buffer, GST_MAP_READ | GST_MAP_GL)) {

      GST_FIXME_OBJECT (self, "Handle GstGLMemory");

      texture = gdk_gl_texture_new (
          self->gdk_context,
          *(guint *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0),
          GST_VIDEO_FRAME_WIDTH (&frame),
          GST_VIDEO_FRAME_HEIGHT (&frame),
          NULL, NULL);

      gst_video_frame_unmap (&frame);
    }

    return texture;
  }

  /* RAW */
  if (gst_video_frame_map (&frame, &self->v_info, self->buffer, GST_MAP_READ)) {
    GBytes *bytes;

    /* Our ref on a buffer together with 2 buffers pool ensures that
     * current buffer will not be freed while another one is prepared */
    bytes = g_bytes_new_with_free_func (
        GST_VIDEO_FRAME_PLANE_DATA (&frame, 0),
        GST_VIDEO_FRAME_HEIGHT (&frame) * GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0),
        NULL, NULL);

    texture = gdk_memory_texture_new (
        GST_VIDEO_FRAME_WIDTH (&frame),
        GST_VIDEO_FRAME_HEIGHT (&frame),
        video_format_to_gdk_memory_format (GST_VIDEO_FRAME_FORMAT (&frame)),
        bytes,
        GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0));

    g_bytes_unref (bytes);
    gst_video_frame_unmap (&frame);
  }

  return texture;
}

static gboolean
calculate_display_par (GtkClapperObject *self, GstVideoInfo *info)
{
  gboolean success;
  gint width, height;
  gint par_n, par_d;
  gint display_par_n, display_par_d;

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  par_n = GST_VIDEO_INFO_PAR_N (info);
  par_d = GST_VIDEO_INFO_PAR_D (info);

  if (!par_n)
    par_n = 1;

  /* User set props */
  if (self->par_n != 0 && self->par_d != 0) {
    display_par_n = self->par_n;
    display_par_d = self->par_d;
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

  if ((success = gst_video_calculate_display_ratio (&self->display_ratio_num,
      &self->display_ratio_den, width, height, par_n, par_d, display_par_n,
      display_par_d))) {
    GST_LOG ("PAR: %u/%u, DAR: %u/%u", par_n, par_d, display_par_n, display_par_d);
  }

  return success;
}

static void
update_display_size (GtkClapperObject *self)
{
  guint display_ratio_num, display_ratio_den;
  gint width, height;

  display_ratio_num = self->display_ratio_num;
  display_ratio_den = self->display_ratio_den;

  width = GST_VIDEO_INFO_WIDTH (&self->v_info);
  height = GST_VIDEO_INFO_HEIGHT (&self->v_info);

  if (height % display_ratio_den == 0) {
    GST_DEBUG ("Keeping video height");

    self->display_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num, display_ratio_den);
    self->display_height = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG ("Keeping video width");

    self->display_width = width;
    self->display_height = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG ("Approximating while keeping video height");

    self->display_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num, display_ratio_den);
    self->display_height = height;
  }

  self->display_aspect_ratio = ((gdouble) self->display_width
      / (gdouble) self->display_height);
  GST_DEBUG ("Scaling to %dx%d", self->display_width, self->display_height);
}

static void
update_paintable (GtkClapperObject *self, GdkPaintable *paintable)
{
  /* No change, so discard the new one */
  if (self->paintable == paintable) {
    if (paintable)
      g_object_unref (paintable);

    return;
  }

  if (self->paintable)
    g_object_unref (self->paintable);

  self->paintable = paintable;

  if (self->pending_resize) {
    update_display_size (self);
    gdk_paintable_invalidate_size ((GdkPaintable *) self);

    self->pending_resize = FALSE;
  }

  gdk_paintable_invalidate_contents ((GdkPaintable *) self);
}

static gboolean
draw_on_main_cb (GtkClapperObject *self)
{
  GdkTexture *texture;

  GTK_CLAPPER_OBJECT_LOCK (self);

  /* Replace used buffer and set matching v_info */
  gst_buffer_replace (&self->buffer, self->pending_buffer);
  self->v_info = self->pending_v_info;

  texture = obtain_texture_from_current_buffer (self);
  if (texture)
    update_paintable (self, (GdkPaintable *) texture);

  self->draw_id = 0;
  GTK_CLAPPER_OBJECT_UNLOCK (self);

  return G_SOURCE_REMOVE;
}

void
gtk_clapper_object_set_element (GtkClapperObject *self, GstElement *element)
{
  g_weak_ref_set (&self->element, element);
}

gboolean
gtk_clapper_object_set_format (GtkClapperObject *self, GstVideoInfo *v_info)
{
  GTK_CLAPPER_OBJECT_LOCK (self);

  if (gst_video_info_is_equal (&self->pending_v_info, v_info)) {
    GTK_CLAPPER_OBJECT_UNLOCK (self);
    return TRUE;
  }

  if (!calculate_display_par (self, v_info)) {
    GTK_CLAPPER_OBJECT_UNLOCK (self);
    return FALSE;
  }

  self->pending_resize = TRUE;
  self->pending_v_info = *v_info;

  GTK_CLAPPER_OBJECT_UNLOCK (self);

  return TRUE;
}

void
gtk_clapper_object_set_buffer (GtkClapperObject *self, GstBuffer *buffer)
{
  GstVideoMeta *meta = NULL;

  GTK_CLAPPER_OBJECT_LOCK (self);

  gst_buffer_replace (&self->pending_buffer, buffer);

  if (self->draw_id) {
    GTK_CLAPPER_OBJECT_UNLOCK (self);
    return;
  }

  if (self->pending_buffer)
    meta = gst_buffer_get_video_meta (self->pending_buffer);

  /* Update pending info from video meta */
  if (meta) {
    guint i;

    GST_VIDEO_INFO_WIDTH (&self->pending_v_info) = meta->width;
    GST_VIDEO_INFO_HEIGHT (&self->pending_v_info) = meta->height;

    for (i = 0; i < meta->n_planes; i++) {
      GST_VIDEO_INFO_PLANE_OFFSET (&self->pending_v_info, i) = meta->offset[i];
      GST_VIDEO_INFO_PLANE_STRIDE (&self->pending_v_info, i) = meta->stride[i];
    }
  }

  self->draw_id = g_idle_add_full (G_PRIORITY_DEFAULT,
      (GSourceFunc) draw_on_main_cb, self, NULL);

  GTK_CLAPPER_OBJECT_UNLOCK (self);
}

GtkClapperObject *
gtk_clapper_object_new (void)
{
  return g_object_new (GTK_TYPE_CLAPPER_OBJECT, NULL);
}

GtkWidget *
gtk_clapper_object_get_widget (GtkClapperObject *self)
{
  return (GtkWidget *) self->picture;
}

static gboolean
wrap_current_gl (GstGLDisplay *display, GstGLPlatform platform, GstGLContext **context)
{
  GstGLAPI gl_api = GST_GL_API_NONE;
  guint gl_major = 0, gl_minor = 0;

  gl_api = gst_gl_context_get_current_gl_api (platform, &gl_major, &gl_minor);

  if (gl_api) {
    const gboolean is_es = gl_api & (GST_GL_API_GLES1 | GST_GL_API_GLES2);
    gchar *gl_api_str = gst_gl_api_to_string (gl_api);
    guintptr gl_handle = 0;

    GST_INFO ("Using GL API: %s, ver: %d.%d", gl_api_str, gl_major, gl_minor);
    g_free (gl_api_str);

    if (is_es && platform == GST_GL_PLATFORM_EGL && !g_getenv ("GST_GL_API")) {
      GST_DEBUG ("No GST_GL_API env and GTK is using EGL GLES2, enforcing it");
      gst_gl_display_filter_gl_api (display, GST_GL_API_GLES2);
    }

    gl_handle = gst_gl_context_get_current_gl_context (platform);
    if (gl_handle) {
      if ((*context = gst_gl_context_new_wrapped (display,
          gl_handle, platform, gl_api)))
        return TRUE;
    }
  }

  return FALSE;
}

static void
retrieve_gl_context_on_main (GtkClapperObject *self)
{
  GdkDisplay *gdk_display;
  GstGLPlatform platform = GST_GL_PLATFORM_NONE;
  GError *error = NULL;

  gst_clear_object (&self->wrapped_context);
  g_clear_object (&self->gdk_context);

  gtk_widget_realize (GTK_WIDGET (self->picture));
  if (!((self->gdk_context = gdk_surface_create_gl_context (gtk_native_get_surface (
      gtk_widget_get_native (GTK_WIDGET (self->picture))), &error)))) {
    GST_ERROR_OBJECT (self, "Error creating Gdk GL context: %s",
        error ? error->message : "No error set by Gdk");
    g_clear_error (&error);
    return;
  }

  //gdk_gl_context_set_use_es (self->gdk_context, TRUE);
  //gdk_gl_context_realize (self->gdk_context, &error);

  gdk_display = gdk_gl_context_get_display (self->gdk_context);

#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (gdk_display)) {
    gpointer display_ptr;
#if GST_GL_HAVE_PLATFORM_EGL && GTK_CHECK_VERSION(4,3,1)
    display_ptr = gdk_x11_display_get_egl_display (gdk_display);
    if (display_ptr)
      self->display = (GstGLDisplay *)
          gst_gl_display_egl_new_with_egl_display (display_ptr);
#endif
  }
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display)) {
    struct wl_display *wayland_display =
        gdk_wayland_display_get_wl_display (gdk_display);
    self->display = (GstGLDisplay *)
        gst_gl_display_wayland_new_with_display (wayland_display);
  }
#endif
  if (G_UNLIKELY (!self->display)) {
    GST_WARNING_OBJECT (self, "Unknown Gdk display!");
    self->display = gst_gl_display_new ();
  }

#if GST_GL_HAVE_PLATFORM_EGL
#if GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND)
  if (GST_IS_GL_DISPLAY_WAYLAND (self->display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_DEBUG ("Using EGL on Wayland");
    goto have_platform;
  }
#endif
#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
  if (GST_IS_GL_DISPLAY_EGL (self->display)) {
    platform = GST_GL_PLATFORM_EGL;
    GST_DEBUG ("Using EGL on x11");
    goto have_platform;
  }
#endif
#endif /* GST_GL_HAVE_PLATFORM_EGL */

  GST_ERROR ("Unsupported GL platform");
  return;

have_platform:
  g_object_ref (self->gdk_context);
  gdk_gl_context_make_current (self->gdk_context);

  if (!wrap_current_gl (self->display, platform, &self->wrapped_context)) {
    GST_WARNING ("Could not retrieve Gdk OpenGL context");
    return;
  }

  GST_INFO ("Retrieved Gdk OpenGL context %" GST_PTR_FORMAT, self->wrapped_context);
  gst_gl_context_activate (self->wrapped_context, TRUE);

  if (!gst_gl_context_fill_info (self->wrapped_context, &error)) {
    GST_ERROR ("Failed to retrieve Gdk context info: %s", error->message);
    g_clear_error (&error);
    g_clear_object (&self->wrapped_context);
  }

  /* Deactivate in both places */
  _gdk_gl_context_set_active (self, FALSE);
}

gboolean
gtk_clapper_object_init_winsys (GtkClapperObject *self)
{
  GError *error = NULL;

  GTK_CLAPPER_OBJECT_LOCK (self);

  if (self->display && self->gdk_context && self->wrapped_context) {
    GST_TRACE ("Have already initialized contexts");
    GTK_CLAPPER_OBJECT_UNLOCK (self);
    return TRUE;
  }

  if (!self->wrapped_context) {
    GTK_CLAPPER_OBJECT_UNLOCK (self);
    gst_gtk_invoke_on_main ((GThreadFunc) (GCallback) retrieve_gl_context_on_main, self);
    GTK_CLAPPER_OBJECT_LOCK (self);
  }

  if (!GST_IS_GL_CONTEXT (self->wrapped_context)) {
    GST_FIXME ("Could not retrieve Gdk GL context");
    GTK_CLAPPER_OBJECT_UNLOCK (self);
    return FALSE;
  }

  GTK_CLAPPER_OBJECT_UNLOCK (self);

  return TRUE;
}

/*
 * GdkPaintableInterface
 */
static void
gtk_clapper_object_paintable_snapshot (GdkPaintable *paintable,
    GdkSnapshot  *snapshot, gdouble width, gdouble height)
{
  GtkClapperObject *self = GTK_CLAPPER_OBJECT_CAST (paintable);

  if (self->paintable)
    gdk_paintable_snapshot (self->paintable, snapshot, width, height);
}

static GdkPaintable *
gtk_clapper_object_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkClapperObject *self = GTK_CLAPPER_OBJECT_CAST (paintable);

  return (self->paintable)
      ? g_object_ref (self->paintable)
      : gdk_paintable_new_empty (0, 0);
}

static gint
gtk_clapper_object_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkClapperObject *self = GTK_CLAPPER_OBJECT_CAST (paintable);

  return self->display_width;
}

static gint
gtk_clapper_object_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkClapperObject *self = GTK_CLAPPER_OBJECT_CAST (paintable);

  return self->display_height;
}

static gdouble
gtk_clapper_object_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkClapperObject *self = GTK_CLAPPER_OBJECT_CAST (paintable);

  return self->display_aspect_ratio;
}

static void
gtk_clapper_object_paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_clapper_object_paintable_snapshot;
  iface->get_current_image = gtk_clapper_object_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_clapper_object_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_clapper_object_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_clapper_object_paintable_get_intrinsic_aspect_ratio;
}
