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

#include "gstclapperimporter.h"
#include "gstgtkutils.h"

#define GST_CAT_DEFAULT gst_clapper_importer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_importer_parent_class
G_DEFINE_TYPE (GstClapperImporter, gst_clapper_importer, GST_TYPE_OBJECT);

typedef struct
{
  GdkTexture *texture;
  GstVideoOverlayRectangle *rectangle;

  gint x, y;
  guint width, height;

  gint index;
  gatomicrefcount ref_count;
} GstClapperGdkOverlay;

static GstClapperGdkOverlay *
gst_clapper_gdk_overlay_new (GdkTexture *texture, GstVideoOverlayRectangle *rectangle,
    gint x, gint y, guint width, guint height, guint index)
{
  GstClapperGdkOverlay *overlay = g_slice_new (GstClapperGdkOverlay);

  overlay->texture = g_object_ref (texture);
  overlay->rectangle = gst_video_overlay_rectangle_ref (rectangle);
  overlay->x = x;
  overlay->y = y;
  overlay->width = width;
  overlay->height = height;
  overlay->index = index;

  g_atomic_ref_count_init (&overlay->ref_count);

  return overlay;
}

static GstClapperGdkOverlay *
gst_clapper_gdk_overlay_ref (GstClapperGdkOverlay *overlay)
{
  g_atomic_ref_count_inc (&overlay->ref_count);

  return overlay;
}

static void
gst_clapper_gdk_overlay_unref (GstClapperGdkOverlay *overlay)
{
  if (g_atomic_ref_count_dec (&overlay->ref_count)) {
    GST_TRACE ("Freeing overlay: %" GST_PTR_FORMAT, overlay);

    g_object_unref (overlay->texture);
    gst_video_overlay_rectangle_unref (overlay->rectangle);
    g_slice_free (GstClapperGdkOverlay, overlay);
  }
}

static GstBufferPool *
_default_create_pool (GstClapperImporter *self, GstStructure **config)
{
  GST_FIXME_OBJECT (self, "Need to create buffer pool");

  return NULL;
}

static GdkTexture *
_default_generate_texture (GstClapperImporter *self,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GST_FIXME_OBJECT (self, "GdkTexture generation not implemented");

  return NULL;
}

static void
gst_clapper_importer_init (GstClapperImporter *self)
{
  gst_video_info_init (&self->pending_v_info);
  gst_video_info_init (&self->v_info);

  self->pending_overlays = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_clapper_gdk_overlay_unref);
  self->overlays = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_clapper_gdk_overlay_unref);

  gdk_rgba_parse (&self->bg, "black");
}

static void
gst_clapper_importer_finalize (GObject *object)
{
  GstClapperImporter *self = GST_CLAPPER_IMPORTER_CAST (object);

  GST_TRACE ("Finalize");

  gst_clear_caps (&self->pending_caps);

  gst_clear_buffer (&self->pending_buffer);
  gst_clear_buffer (&self->buffer);

  g_ptr_array_unref (self->pending_overlays);
  g_ptr_array_unref (self->overlays);

  g_clear_object (&self->texture);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_importer_class_init (GstClapperImporterClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstClapperImporterClass *importer_class = (GstClapperImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperimporter", 0,
      "Clapper Importer");

  gobject_class->finalize = gst_clapper_importer_finalize;

  importer_class->create_pool = _default_create_pool;
  importer_class->generate_texture = _default_generate_texture;
}

static GstClapperGdkOverlay *
_get_cached_overlay (GPtrArray *overlays, GstVideoOverlayRectangle *rectangle)
{
  guint i;

  for (i = 0; i < overlays->len; i++) {
    GstClapperGdkOverlay *overlay = g_ptr_array_index (overlays, i);

    if (overlay->rectangle == rectangle)
      return overlay;
  }

  return NULL;
}

static gint
_sort_overlays_cb (gconstpointer a, gconstpointer b)
{
  GstClapperGdkOverlay *overlay_a, *overlay_b;

  overlay_a = *((GstClapperGdkOverlay **) a);
  overlay_b = *((GstClapperGdkOverlay **) b);

  return (overlay_a->index - overlay_b->index);
}

/*
 * Prepares overlays to show with the next rendered buffer.
 *
 * In order for overlays caching to work correctly, this should be called for
 * every received buffer (even if its going to be disgarded), also must be
 * called together with pending buffer replacement within a single importer
 * locking, to make sure prepared overlays always match the pending buffer.
 */
static void
gst_clapper_importer_prepare_overlays_locked (GstClapperImporter *self)
{
  GstVideoOverlayCompositionMeta *comp_meta;
  guint num_overlays, i;

  if (G_UNLIKELY (!self->pending_buffer)
      || !(comp_meta = gst_buffer_get_video_overlay_composition_meta (self->pending_buffer))) {
    guint n_pending = self->pending_overlays->len;

    /* Remove all cached overlays if new buffer does not have any */
    if (n_pending > 0) {
      GST_TRACE ("No overlays in buffer, removing all cached ones");
      g_ptr_array_remove_range (self->pending_overlays, 0, n_pending);
    }

    return;
  }

  GST_LOG_OBJECT (self, "Preparing overlays...");

  /* Mark all old overlays as unused by giving them negative index */
  for (i = 0; i < self->pending_overlays->len; i++) {
    GstClapperGdkOverlay *overlay = g_ptr_array_index (self->pending_overlays, i);
    overlay->index = -1;
  }

  num_overlays = gst_video_overlay_composition_n_rectangles (comp_meta->overlay);

  for (i = 0; i < num_overlays; i++) {
    GdkTexture *texture;
    GstBuffer *comp_buffer;
    GstVideoFrame comp_frame;
    GstVideoMeta *v_meta;
    GstVideoInfo v_info;
    GstVideoOverlayRectangle *rectangle;
    GstClapperGdkOverlay *overlay;
    GstVideoOverlayFormatFlags flags, alpha_flags = 0;
    gint comp_x, comp_y;
    guint comp_width, comp_height;

    rectangle = gst_video_overlay_composition_get_rectangle (comp_meta->overlay, i);

    if ((overlay = _get_cached_overlay (self->pending_overlays, rectangle))) {
      overlay->index = i;

      GST_TRACE ("Reusing cached overlay: %" GST_PTR_FORMAT, overlay);
      continue;
    }

    if (G_UNLIKELY (!gst_video_overlay_rectangle_get_render_rectangle (rectangle,
        &comp_x, &comp_y, &comp_width, &comp_height))) {
      GST_WARNING ("Invalid overlay rectangle dimensions: %" GST_PTR_FORMAT, rectangle);
      continue;
    }

    flags = gst_video_overlay_rectangle_get_flags (rectangle);

    if (flags & GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA)
      alpha_flags |= GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA;

    comp_buffer = gst_video_overlay_rectangle_get_pixels_unscaled_argb (rectangle, alpha_flags);

    /* Update overlay video info from video meta */
    if ((v_meta = gst_buffer_get_video_meta (comp_buffer))) {
      gst_video_info_set_format (&v_info, v_meta->format, v_meta->width, v_meta->height);
      v_info.stride[0] = v_meta->stride[0];

      if (alpha_flags & GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA)
        v_info.flags |= GST_VIDEO_FLAG_PREMULTIPLIED_ALPHA;
    }

    if (G_UNLIKELY (!gst_video_frame_map (&comp_frame, &v_info, comp_buffer, GST_MAP_READ)))
      return;

    if ((texture = gst_video_frame_into_gdk_texture (&comp_frame))) {
      overlay = gst_clapper_gdk_overlay_new (texture, rectangle, comp_x, comp_y,
          comp_width, comp_height, i);
      g_object_unref (texture);

      GST_TRACE_OBJECT (self, "Created overlay: %"
          GST_PTR_FORMAT ", x: %i, y: %i, width: %u, height: %u",
          overlay, overlay->x, overlay->y, overlay->width, overlay->height);

      g_ptr_array_insert (self->pending_overlays, i, overlay);
    }

    gst_video_frame_unmap (&comp_frame);
  }

  /* Remove all overlays that are not going to be used */
  for (i = self->pending_overlays->len; i > 0; i--) {
    GstClapperGdkOverlay *overlay = g_ptr_array_index (self->pending_overlays, i - 1);

    if (overlay->index < 0) {
      GST_TRACE ("Removing unused cached overlay: %" GST_PTR_FORMAT, overlay);
      g_ptr_array_remove (self->pending_overlays, overlay);
    }
  }

  /* Sort remaining overlays */
  if (self->pending_overlays->len > 1) {
    GST_LOG_OBJECT (self, "Sorting overlays");
    g_ptr_array_sort (self->pending_overlays, (GCompareFunc) _sort_overlays_cb);
  }

  if (G_UNLIKELY (num_overlays != self->pending_overlays->len)) {
    GST_WARNING_OBJECT (self, "Some overlays could not be prepared, %u != %u",
        num_overlays, self->pending_overlays->len);
  }

  GST_LOG_OBJECT (self, "Prepared overlays: %u", self->pending_overlays->len);
}

gboolean
gst_clapper_importer_prepare (GstClapperImporter *self)
{
  GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

  if (importer_class->prepare) {
    if (!importer_class->prepare (self))
      return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Importer prepared");

  return TRUE;
}

void
gst_clapper_importer_share_data (GstClapperImporter *self, GstClapperImporter *dest)
{
  GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

  if (importer_class->share_data)
    importer_class->share_data (self, dest);
}

void
gst_clapper_importer_set_caps (GstClapperImporter *self, GstCaps *caps)
{
  GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

  GST_OBJECT_LOCK (self);
  gst_caps_replace (&self->pending_caps, caps);
  GST_OBJECT_UNLOCK (self);

  if (importer_class->set_caps)
    importer_class->set_caps (self, caps);
}

void
gst_clapper_importer_set_buffer (GstClapperImporter *self, GstBuffer *buffer)
{
  GST_OBJECT_LOCK (self);

  /* Pending v_info, buffer and overlays must be
   * set within a single importer locking */
  if (self->pending_caps) {
    self->has_pending_v_info = gst_video_info_from_caps (&self->pending_v_info, self->pending_caps);
    gst_clear_caps (&self->pending_caps);
  }
  gst_buffer_replace (&self->pending_buffer, buffer);
  gst_clapper_importer_prepare_overlays_locked (self);

  GST_OBJECT_UNLOCK (self);
}

GstBufferPool *
gst_clapper_importer_create_pool (GstClapperImporter *self, GstStructure **config)
{
  GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

  return importer_class->create_pool (self, config);
}

void
gst_clapper_importer_add_allocation_metas (GstClapperImporter *self, GstQuery *query)
{
  GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

  if (importer_class->add_allocation_metas)
    importer_class->add_allocation_metas (self, query);
}

gboolean
gst_clapper_importer_handle_context_query (GstClapperImporter *self,
    GstBaseSink *bsink, GstQuery *query)
{
  GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

  if (!importer_class->handle_context_query)
    return FALSE;

  return importer_class->handle_context_query (self, bsink, query);
}

void
gst_clapper_importer_snapshot (GstClapperImporter *self, GdkSnapshot *snapshot,
    gdouble width, gdouble height)
{
  guint i;
  gboolean buffer_changed;

  /* Collect all data that we need to snapshot pending buffer,
   * lock ourselves to make sure everything matches */
  GST_OBJECT_LOCK (self);

  if (self->has_pending_v_info) {
    self->v_info = self->pending_v_info;
    self->has_pending_v_info = FALSE;
  }

  buffer_changed = gst_buffer_replace (&self->buffer, self->pending_buffer);

  /* Ref overlays associated with current buffer */
  for (i = 0; i < self->pending_overlays->len; i++) {
    GstClapperGdkOverlay *overlay = g_ptr_array_index (self->pending_overlays, i);

    g_ptr_array_insert (self->overlays, i, gst_clapper_gdk_overlay_ref (overlay));
  }

  GST_OBJECT_UNLOCK (self);

  /* Draw black BG when no buffer or imported format has alpha */
  if (!self->buffer || GST_VIDEO_INFO_HAS_ALPHA (&self->v_info))
    gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (self->buffer) {
    if (buffer_changed || !self->texture) {
      GstClapperImporterClass *importer_class = GST_CLAPPER_IMPORTER_GET_CLASS (self);

      GST_TRACE_OBJECT (self, "Importing %" GST_PTR_FORMAT, self->buffer);

      g_clear_object (&self->texture);
      self->texture = importer_class->generate_texture (self, self->buffer, &self->v_info);
    } else {
      GST_TRACE_OBJECT (self, "Reusing texture from %" GST_PTR_FORMAT, self->buffer);
    }

    if (G_LIKELY (self->texture)) {
      gtk_snapshot_append_texture (snapshot, self->texture, &GRAPHENE_RECT_INIT (0, 0, width, height));

      if (self->overlays->len > 0) {
        gfloat scale_x, scale_y;

        /* FIXME: GStreamer scales subtitles without considering pixel aspect ratio.
         * See: https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/issues/20 */
        scale_x = (gfloat) width / GST_VIDEO_INFO_WIDTH (&self->v_info);
        scale_y = (gfloat) height / GST_VIDEO_INFO_HEIGHT (&self->v_info);

        for (i = 0; i < self->overlays->len; i++) {
          GstClapperGdkOverlay *overlay = g_ptr_array_index (self->overlays, i);

          gtk_snapshot_append_texture (snapshot, overlay->texture,
              &GRAPHENE_RECT_INIT (overlay->x * scale_x, overlay->y * scale_y,
                  overlay->width * scale_x, overlay->height * scale_y));
        }
      }
    } else {
      GST_ERROR_OBJECT (self, "Failed import of %" GST_PTR_FORMAT, self->buffer);

      /* Draw black instead of texture on failure if not drawn already */
      if (!GST_VIDEO_INFO_HAS_ALPHA (&self->v_info))
        gtk_snapshot_append_color (snapshot, &self->bg, &GRAPHENE_RECT_INIT (0, 0, width, height));
    }
  }

  /* Unref all used overlays */
  if (self->overlays->len > 0)
    g_ptr_array_remove_range (self->overlays, 0, self->overlays->len);
}
