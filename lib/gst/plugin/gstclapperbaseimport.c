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

#include "gstclapperbaseimport.h"
#include "gstclappergdkmemory.h"
#include "gstclappergdkbufferpool.h"

#define GST_CAT_DEFAULT gst_clapper_base_import_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_base_import_parent_class
G_DEFINE_TYPE (GstClapperBaseImport, gst_clapper_base_import, GST_TYPE_BASE_TRANSFORM);

static void
gst_clapper_base_import_init (GstClapperBaseImport *self)
{
  g_mutex_init (&self->lock);

  gst_video_info_init (&self->in_info);
  gst_video_info_init (&self->out_info);
}

static void
gst_clapper_base_import_finalize (GObject *object)
{
  GstClapperBaseImport *self = GST_CLAPPER_BASE_IMPORT_CAST (object);

  GST_TRACE ("Finalize");
  g_mutex_clear (&self->lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static GstStateChangeReturn
gst_clapper_base_import_change_state (GstElement *element, GstStateChange transition)
{
  GST_DEBUG_OBJECT (element, "Changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static gboolean
gst_clapper_base_import_start (GstBaseTransform *bt)
{
  GST_INFO_OBJECT (bt, "Start");

  return TRUE;
}

static gboolean
gst_clapper_base_import_stop (GstBaseTransform *bt)
{
  GST_INFO_OBJECT (bt, "Stop");

  return TRUE;
}

static GstCaps *
gst_clapper_base_import_transform_caps (GstBaseTransform *bt,
    GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
  GstCaps *result, *tmp;

  tmp = (direction == GST_PAD_SINK)
      ? gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SRC_PAD (bt))
      : gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SINK_PAD (bt));

  if (filter) {
    GST_DEBUG ("Intersecting with filter caps: %" GST_PTR_FORMAT, filter);
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }
  GST_DEBUG ("Returning %s caps: %" GST_PTR_FORMAT,
      (direction == GST_PAD_SINK) ? "src" : "sink", result);

  return result;
}

static gboolean
_structure_filter_cb (GQuark field_id, GValue *value,
    G_GNUC_UNUSED gpointer user_data)
{
  const gchar *str = g_quark_to_string (field_id);

  if (!strcmp (str, "format")
      || !strcmp (str, "width")
      || !strcmp (str, "height")
      || !strcmp (str, "pixel-aspect-ratio")
      || !strcmp (str, "framerate"))
    return TRUE;

  return FALSE;
}

static GstCaps *
gst_clapper_base_import_fixate_caps (GstBaseTransform *bt,
    GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
  GstCaps *fixated;

  fixated = (!gst_caps_is_any (caps))
      ? gst_caps_fixate (gst_caps_ref (caps))
      : gst_caps_copy (caps);

  if (direction == GST_PAD_SINK) {
    guint i, n = gst_caps_get_size (fixated);

    for (i = 0; i < n; i++) {
      GstCapsFeatures *features;
      GstStructure *structure;
      gboolean had_overlay_comp;

      features = gst_caps_get_features (fixated, i);
      had_overlay_comp = gst_caps_features_contains (features,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

      features = gst_caps_features_new (GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY, NULL);
      if (had_overlay_comp)
        gst_caps_features_add (features, GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

      gst_caps_set_features (fixated, i, features);

      /* Remove fields that do not apply to our memory */
      if ((structure = gst_caps_get_structure (fixated, i))) {
        gst_structure_filter_and_map_in_place (structure,
            (GstStructureFilterMapFunc) _structure_filter_cb, NULL);
      }
    }
  }
  GST_DEBUG ("Fixated %s caps: %" GST_PTR_FORMAT,
      (direction == GST_PAD_SRC) ? "sink" : "src", fixated);

  return fixated;
}

static gboolean
gst_clapper_base_import_set_caps (GstBaseTransform *bt,
    GstCaps *incaps, GstCaps *outcaps)
{
  GstClapperBaseImport *self = GST_CLAPPER_BASE_IMPORT_CAST (bt);
  gboolean has_sink_info, has_src_info;

  if ((has_sink_info = gst_video_info_from_caps (&self->in_info, incaps)))
    GST_INFO_OBJECT (self, "Set sink caps: %" GST_PTR_FORMAT, incaps);
  if ((has_src_info = gst_video_info_from_caps (&self->out_info, outcaps)))
    GST_INFO_OBJECT (self, "Set src caps: %" GST_PTR_FORMAT, outcaps);

  return (has_sink_info && has_src_info);
}

static gboolean
gst_clapper_base_import_import_propose_allocation (GstBaseTransform *bt,
    GstQuery *decide_query, GstQuery *query)
{
  GstClapperBaseImport *self = GST_CLAPPER_BASE_IMPORT_CAST (bt);
  GstClapperBaseImportClass *bi_class = GST_CLAPPER_BASE_IMPORT_GET_CLASS (self);
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  GstVideoInfo info;
  guint size;
  gboolean need_pool;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (bt,
      decide_query, query))
    return FALSE;

  /* Passthrough, nothing to do */
  if (!decide_query)
    return TRUE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (!caps) {
    GST_DEBUG_OBJECT (self, "No caps specified");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_DEBUG_OBJECT (self, "Invalid caps specified");
    return FALSE;
  }

  /* Normal size of a frame */
  size = GST_VIDEO_INFO_SIZE (&info);

  if (need_pool) {
    GstStructure *config = NULL;

    GST_DEBUG_OBJECT (self, "Need to create upstream pool");
    pool = bi_class->create_upstream_pool (self, &config);

    if (pool) {
      /* If we did not get config, use default one */
      if (!config)
        config = gst_buffer_pool_get_config (pool);

      gst_buffer_pool_config_set_params (config, caps, size, 2, 0);

      if (!gst_buffer_pool_set_config (pool, config)) {
        gst_object_unref (pool);

        GST_DEBUG_OBJECT (self, "Failed to set config");
        return FALSE;
      }
    } else if (config) {
      GST_WARNING_OBJECT (self, "Got pool config without a pool to apply it!");
      gst_structure_free (config);
    }
  }

  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    gst_object_unref (pool);

  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
}

static gboolean
gst_clapper_base_import_decide_allocation (GstBaseTransform *bt, GstQuery *query)
{
  GstClapperBaseImport *self = GST_CLAPPER_BASE_IMPORT_CAST (bt);
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  GstVideoInfo info;
  guint size = 0, min = 0, max = 0;
  gboolean update_pool, need_pool = TRUE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps) {
    GST_DEBUG_OBJECT (self, "No caps specified");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_DEBUG_OBJECT (self, "Invalid caps specified");
    return FALSE;
  }

  if ((update_pool = gst_query_get_n_allocation_pools (query) > 0)) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if ((need_pool = !GST_IS_CLAPPER_GDK_BUFFER_POOL (pool)))
        gst_clear_object (&pool);
    }
  } else {
    size = GST_VIDEO_INFO_SIZE (&info);
  }

  if (need_pool) {
    GstStructure *config;

    GST_DEBUG_OBJECT (self, "Creating new downstream pool");

    pool = gst_clapper_gdk_buffer_pool_new ();
    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_set_params (config, caps, size, min, max);

    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (pool);

      GST_DEBUG_OBJECT (self, "Failed to set config");
      return FALSE;
    }
  }

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (bt, query);
}

static GstBufferPool *
gst_clapper_base_import_create_upstream_pool (GstClapperBaseImport *self, GstStructure **config)
{
  GST_FIXME_OBJECT (self, "Need to create upstream buffer pool");

  return NULL;
}

static void
gst_clapper_base_import_class_init (GstClapperBaseImportClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *gstbasetransform_class = (GstBaseTransformClass *) klass;
  GstClapperBaseImportClass *bi_class = (GstClapperBaseImportClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperbaseimport", 0,
      "Clapper Base Import");

  gobject_class->finalize = gst_clapper_base_import_finalize;

  gstelement_class->change_state = gst_clapper_base_import_change_state;

  gstbasetransform_class->passthrough_on_same_caps = TRUE;
  gstbasetransform_class->transform_ip_on_passthrough = FALSE;
  gstbasetransform_class->start = gst_clapper_base_import_start;
  gstbasetransform_class->stop = gst_clapper_base_import_stop;
  gstbasetransform_class->transform_caps = gst_clapper_base_import_transform_caps;
  gstbasetransform_class->fixate_caps = gst_clapper_base_import_fixate_caps;
  gstbasetransform_class->set_caps = gst_clapper_base_import_set_caps;
  gstbasetransform_class->propose_allocation = gst_clapper_base_import_import_propose_allocation;
  gstbasetransform_class->decide_allocation = gst_clapper_base_import_decide_allocation;

  bi_class->create_upstream_pool = gst_clapper_base_import_create_upstream_pool;
}

/*
 * Maps input video frame and output memory from in/out buffers
 * using flags passed to this method.
 *
 * Remember to unmap both using `gst_video_frame_unmap` and
 * `gst_memory_unmap` when done with the data.
 */
gboolean
gst_clapper_base_import_map_buffers (GstClapperBaseImport *self,
    GstBuffer *in_buf, GstBuffer *out_buf, GstMapFlags in_flags, GstMapFlags out_flags,
    GstVideoFrame *frame, GstMapInfo *info, GstMemory **mem)
{
  GST_LOG_OBJECT (self, "Transforming from %" GST_PTR_FORMAT
      " into %" GST_PTR_FORMAT, in_buf, out_buf);

  if (G_UNLIKELY (!gst_video_frame_map (frame, &self->in_info, in_buf, in_flags))) {
    GST_ERROR_OBJECT (self, "Could not map input buffer for reading");
    return FALSE;
  }

  *mem = gst_buffer_peek_memory (out_buf, 0);

  if (G_UNLIKELY (!gst_memory_map (*mem, info, out_flags))) {
    GST_ERROR_OBJECT (self, "Could not map output memory for writing");
    gst_video_frame_unmap (frame);

    return FALSE;
  }

  return TRUE;
}
