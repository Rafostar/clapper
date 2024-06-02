/* ClapperTube Library
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "clapper-tube-src.h"
#include "clapper-tube-client.h"

#define GST_CAT_DEFAULT clapper_tube_src_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_URI,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstURIType
clapper_tube_src_uri_handler_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
clapper_tube_src_uri_handler_get_protocols (GType type)
{
  return clapper_tube_get_supported_schemes ();
}

static gchar *
clapper_tube_src_uri_handler_get_uri (GstURIHandler *handler)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (handler);
  gchar *uri;

  GST_OBJECT_LOCK (self);
  uri = g_strdup (self->uri);
  GST_OBJECT_UNLOCK (self);

  return uri;
}

static gboolean
clapper_tube_src_uri_handler_set_uri (GstURIHandler *handler,
    const gchar *uri, GError **error)
{
  GstGtuberSrc *self = CLAPPER_TUBE_SRC_CAST (handler);
  const gchar *const *protocols;
  gboolean supported = FALSE;
  guint i;

  GST_DEBUG_OBJECT (self, "Changing URI to: %s", uri);

  if (!uri) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "URI property cannot be NULL");
    return FALSE;
  }
  if (GST_STATE (GST_ELEMENT_CAST (self)) >= GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Cannot change URI property while element is running");
    return FALSE;
  }

  protocols = gst_uri_handler_get_protocols (handler);
  for (i = 0; protocols[i]; ++i) {
    if ((supported = gst_uri_has_protocol (location, protocols[i])))
      break;
  }
  if (!supported) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_UNSUPPORTED_PROTOCOL,
        "URI protocol is not supported");
    return FALSE;
  }
  if (!(supported = clapper_tube_has_plugin_for_uri (uri))) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "ClapperTube does not have a plugin for this URI");
    return FALSE;
  }

  GST_OBJECT_LOCK (self);

  g_set_str (&self->uri, uri);
  GST_DEBUG_OBJECT (self, "URI changed to: %s", GST_STR_NULL (self->uri));

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static void
_uri_handler_iface_init (GstUriHandlerInterface *iface)
{
  iface->get_type = clapper_tube_src_uri_handler_get_type;
  iface->get_protocols = clapper_tube_src_uri_handler_get_protocols;
  iface->get_uri = clapper_tube_src_uri_handler_get_uri;
  iface->set_uri = clapper_tube_src_uri_handler_set_uri;
}

#define parent_class clapper_tube_src_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperTubeSrc, clapper_tube_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, _uri_handler_iface_init));
GST_ELEMENT_REGISTER_DEFINE (clappertubesrc, "clappertubesrc",
    GST_RANK_NONE, CLAPPER_TUBE_TYPE_SRC);

static gboolean
clapper_tube_src_start (GstBaseSrc *base_src)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (base_src);
  gboolean can_start;

  GST_DEBUG_OBJECT (self, "Start");

  GST_OBJECT_LOCK (self);
  can_start = (self->uri != NULL);
  GST_OBJECT_UNLOCK (self);

  if (G_UNLIKELY (!can_start)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("%s", "No media URI"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
clapper_tube_src_stop (GstBaseSrc *base_src)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (base_src);

  GST_DEBUG_OBJECT (self, "Stop");

  GST_OBJECT_LOCK (self);
  g_clear_object (&self->info);
  GST_OBJECT_UNLOCK (self);

  self->buf_size = 0;

  return TRUE;
}

static gboolean
clapper_tube_src_get_size (GstBaseSrc *base_src, guint64 *size)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (base_src);

  if (self->buf_size > 0) {
    *size = self->buf_size;
    return TRUE;
  }

  return FALSE;
}

static gboolean
clapper_tube_src_is_seekable (GstBaseSrc *base_src)
{
  return FALSE;
}

static gboolean
clapper_tube_src_unlock (GstBaseSrc *base_src)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (base_src);

  GST_LOG_OBJECT (self, "Cancel triggered");
  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
clapper_tube_src_unlock_stop (GstBaseSrc *base_src)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (base_src);

  GST_LOG_OBJECT (self, "Resetting cancellable");

  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();

  return TRUE;
}

static void
_insert_header_cb (const gchar *name, const gchar *value, GstStructure *structure)
{
  gst_structure_set (structure, name, G_TYPE_STRING, value, NULL);
}

static void
_insert_chapter_cb (guint64 time, const gchar *name, GstTocEntry *entry)
{
  GstTocEntry *subentry;
  GstClockTime clock_time;
  gchar *id;

  clock_time = time * GST_MSECOND;
  GST_DEBUG ("Inserting TOC chapter, time: %" G_GUINT64_FORMAT ", name: %s",
      clock_time, name);

  id = g_strdup_printf ("chap.%" G_GUINT64_FORMAT, time);
  subentry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_CHAPTER, id);
  g_free (id);

  gst_toc_entry_set_tags (subentry,
      gst_tag_list_new (GST_TAG_TITLE, name, NULL));
  gst_toc_entry_set_start_stop_times (subentry, clock_time, GST_CLOCK_TIME_NONE);

  gst_toc_entry_append_sub_entry (entry, subentry);
}

static void
gst_gtuber_src_push_events (GstGtuberSrc *self, GtuberMediaInfo *info)
{
  GHashTable *gtuber_headers, *gtuber_chapters;
  GstTagList *tags;
  const gchar *tag;

  gtuber_headers = gtuber_media_info_get_request_headers (info);

  if (gtuber_headers && g_hash_table_size (gtuber_headers) > 0) {
    GstStructure *config, *req_headers;
    GstEvent *event;

    GST_DEBUG_OBJECT (self, "Creating " GST_GTUBER_HEADERS " event");

    req_headers = gst_structure_new_empty ("request-headers");

    g_hash_table_foreach (gtuber_headers,
        (GHFunc) _insert_header_cb, req_headers);

    config = gst_structure_new (GST_GTUBER_HEADERS,
        GST_GTUBER_REQ_HEADERS, GST_TYPE_STRUCTURE, req_headers,
        NULL);
    gst_structure_free (req_headers);

    event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, config);

    GST_DEBUG_OBJECT (self, "Pushing " GST_GTUBER_HEADERS " event");
    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);
  }

  GST_DEBUG_OBJECT (self, "Creating TAG event");
  tags = gst_tag_list_new_empty ();

  /* GStreamer does not allow empty strings in the tag list */
  if ((tag = gtuber_media_info_get_title (info)) && strlen (tag) > 0)
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_TITLE, tag, NULL);
  if ((tag = gtuber_media_info_get_description (info)) && strlen (tag) > 0)
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_DESCRIPTION, tag, NULL);

  if (gst_tag_list_is_empty (tags)) {
    GST_DEBUG_OBJECT (self, "No tags to push");
    gst_tag_list_unref (tags);
  } else {
    GstEvent *event;

    gst_tag_list_set_scope (tags, GST_TAG_SCOPE_GLOBAL);
    event = gst_event_new_tag (gst_tag_list_copy (tags));

    GST_DEBUG_OBJECT (self, "Pushing TAG event: %p", event);

    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);

    /* FIXME: We should be posting event only to make it reach app
     * after stream start, but currently event is lost that way */
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_tag (NULL, tags));
  }

  gtuber_chapters = gtuber_media_info_get_chapters (info);

  if (gtuber_chapters && g_hash_table_size (gtuber_chapters) > 0) {
    GstToc *toc;
    GstTocEntry *toc_entry;
    GstEvent *event;

    toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);
    toc_entry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_EDITION, "00");

    gst_toc_entry_set_start_stop_times (toc_entry, 0,
        gtuber_media_info_get_duration (info) * GST_SECOND);

    g_hash_table_foreach (gtuber_chapters,
        (GHFunc) _insert_chapter_cb, toc_entry);

    gst_toc_append_entry (toc, toc_entry);
    event = gst_event_new_toc (toc, FALSE);

    GST_DEBUG_OBJECT (self, "Pushing TOC event: %p", event);

    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);

    /* FIXME: We should be posting event only to make it reach app
     * after stream start, but currently event is lost that way */
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_toc (NULL, toc, FALSE));

    gst_toc_unref (toc);
  }

  GST_DEBUG_OBJECT (self, "Pushed all events");
}

/* Called with a lock on props */
static gboolean
get_is_stream_allowed (GtuberStream *stream, GstGtuberSrc *self)
{
  if (self->codecs > 0) {
    GtuberCodecFlags flags = gtuber_stream_get_codec_flags (stream);

    if ((self->codecs & flags) != flags)
      return FALSE;
  }

  if (gtuber_stream_get_video_codec (stream) != NULL) {
    if (self->max_height > 0) {
      guint height = gtuber_stream_get_height (stream);

      if (height == 0 || height > self->max_height)
        return FALSE;
    }
    if (self->max_fps > 0) {
      guint fps = gtuber_stream_get_fps (stream);

      if (fps == 0 || fps > self->max_fps)
        return FALSE;
    }
  }

  if (self->itags->len > 0) {
    guint i, itag = gtuber_stream_get_itag (stream);
    gboolean found = FALSE;

    for (i = 0; i < self->itags->len; i++) {
      if ((found = itag == g_array_index (self->itags, guint, i)))
        break;
    }

    if (!found)
      return FALSE;
  }

  return TRUE;
}

static gboolean
astream_filter_func (GtuberAdaptiveStream *astream, GstGtuberSrc *self)
{
  return get_is_stream_allowed ((GtuberStream *) astream, self);
}

static gchar *
gst_gtuber_generate_manifest (GstGtuberSrc *self, GtuberMediaInfo *info,
    GtuberAdaptiveStreamManifest *manifest_type)
{
  GtuberManifestGenerator *gen;
  GtuberAdaptiveStreamManifest type;
  gchar *data;

  gen = gtuber_manifest_generator_new ();
  gtuber_manifest_generator_set_media_info (gen, info);

  gtuber_manifest_generator_set_filter_func (gen,
      (GtuberAdaptiveStreamFilter) astream_filter_func, self, NULL);

  for (type = GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH;
      type <= GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS; type++) {
    gtuber_manifest_generator_set_manifest_type (gen, type);

    /* Props are accessed in filter callback */
    g_mutex_lock (&self->prop_lock);
    data = gtuber_manifest_generator_to_data (gen);
    g_mutex_unlock (&self->prop_lock);

    if (data)
      break;
  }

  g_object_unref (gen);

  if (manifest_type) {
    if (!data)
      type = GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN;

    *manifest_type = type;
  }

  return data;
}

static void
_rate_streams_vals (guint a_num, guint b_num, guint weight,
    guint *a_pts, guint *b_pts)
{
  if (a_num > b_num)
    *a_pts += weight;
  else if (a_num < b_num)
    *b_pts += weight;
}

static gchar *
gst_gtuber_generate_best_uri_data (GstGtuberSrc *self, GtuberMediaInfo *info)
{
  GPtrArray *streams;
  GtuberStream *best_stream = NULL;
  gchar *data = NULL;
  guint i;

  streams = gtuber_media_info_get_streams (info);

  for (i = 0; i < streams->len; i++) {
    GtuberStream *stream;

    stream = g_ptr_array_index (streams, i);

    if (get_is_stream_allowed (stream, self)) {
      guint best_pts = 0, curr_pts = 0;

      if (best_stream) {
        _rate_streams_vals (
            gtuber_stream_get_height (best_stream),
            gtuber_stream_get_height (stream),
            8, &best_pts, &curr_pts);
        _rate_streams_vals (
            gtuber_stream_get_width (best_stream),
            gtuber_stream_get_width (stream),
            4, &best_pts, &curr_pts);
        _rate_streams_vals (
            gtuber_stream_get_bitrate (best_stream),
            gtuber_stream_get_bitrate (stream),
            2, &best_pts, &curr_pts);
        _rate_streams_vals (
            gtuber_stream_get_fps (best_stream),
            gtuber_stream_get_fps (stream),
            1, &best_pts, &curr_pts);
      }

      if (!best_stream || curr_pts > best_pts) {
        best_stream = stream;
        GST_DEBUG ("Current best stream itag: %u",
            gtuber_stream_get_itag (best_stream));
      }
    }
  }

  if (best_stream)
    data = g_strdup (gtuber_stream_get_uri (best_stream));

  return data;
}

static GstBuffer *
gst_gtuber_media_info_to_buffer (GstGtuberSrc *self, GtuberMediaInfo *info,
    GError **error)
{
  GtuberAdaptiveStreamManifest manifest_type;
  GstBuffer *buffer;
  GstCaps *caps = NULL;
  gchar *data;

  if ((data = gst_gtuber_generate_manifest (self, info, &manifest_type))) {
    GST_INFO ("Using adaptive streaming");

    switch (manifest_type) {
      case GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH:
        caps = gst_caps_new_empty_simple ("application/dash+xml");
        break;
      case GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS:
        caps = gst_caps_new_empty_simple ("application/x-hls");
        break;
      default:
        GST_WARNING_OBJECT (self, "Unsupported gtuber manifest type");
        break;
    }
  } else if ((data = gst_gtuber_generate_best_uri_data (self, info))) {
    GST_INFO ("Using direct stream");
    caps = gst_caps_new_empty_simple ("text/uri-list");
  } else {
    g_set_error (error, GTUBER_MANIFEST_GENERATOR_ERROR,
        GTUBER_MANIFEST_GENERATOR_ERROR_NO_DATA,
        "No manifest data was generated");
    return FALSE;
  }

  if (caps) {
    gst_caps_set_simple (caps,
        "source", G_TYPE_STRING, "gtuber",
        NULL);
    if (gst_base_src_set_caps (GST_BASE_SRC (self), caps))
      GST_INFO_OBJECT (self, "Using caps: %" GST_PTR_FORMAT, caps);

    gst_caps_unref (caps);
  }

  self->buf_size = strlen (data);
  buffer = gst_buffer_new_wrapped (data, self->buf_size);

  return buffer;
}

typedef struct
{
  GstGtuberSrc *src;
  GtuberMediaInfo *info;
  GError *error;
  gboolean fired;
} GstGtuberThreadData;

static GstGtuberThreadData *
gst_gtuber_thread_data_new (GstGtuberSrc *self)
{
  GstGtuberThreadData *data;

  data = g_new (GstGtuberThreadData, 1);
  data->src = gst_object_ref (self);
  data->info = NULL;
  data->error = NULL;
  data->fired = FALSE;

  return data;
}

static void
gst_gtuber_thread_data_free (GstGtuberThreadData *data)
{
  gst_object_unref (data->src);
  g_clear_object (&data->info);
  g_clear_error (&data->error);

  g_free (data);
}

static gpointer
client_thread_func (GstGtuberThreadData *data)
{
  GstGtuberSrc *self = data->src;
  GtuberClient *client;
  GMainContext *ctx;
  gchar *uri;

  g_mutex_lock (&self->client_lock);
  GST_DEBUG ("Entered new GtuberClient thread");

  g_mutex_lock (&self->prop_lock);
  uri = location_to_uri (self->location);
  g_mutex_unlock (&self->prop_lock);

  ctx = g_main_context_new ();
  g_main_context_push_thread_default (ctx);

  GST_INFO ("Fetching media info for URI: %s", uri);

  client = gtuber_client_new ();
  data->info = gtuber_client_fetch_media_info (client, uri,
      self->cancellable, &data->error);
  g_object_unref (client);

  g_main_context_pop_thread_default (ctx);
  g_main_context_unref (ctx);

  g_free (uri);

  GST_DEBUG ("Leaving GtuberClient thread");

  data->fired = TRUE;
  g_cond_signal (&self->client_finished);
  g_mutex_unlock (&self->client_lock);

  return NULL;
}

static GtuberMediaInfo *
gst_gtuber_fetch_media_info (GstGtuberSrc *self, GError **error)
{
  GtuberMediaInfo *info;
  GstGtuberThreadData *data;

  GST_DEBUG_OBJECT (self, "Fetching media info");

  g_mutex_lock (&self->client_lock);

  data = gst_gtuber_thread_data_new (self);
  self->client_thread = g_thread_new ("GstGtuberClientThread",
      (GThreadFunc) client_thread_func, data);

  while (!data->fired)
    g_cond_wait (&self->client_finished, &self->client_lock);

  g_thread_unref (self->client_thread);
  self->client_thread = NULL;

  g_mutex_unlock (&self->client_lock);

  if (!data->info) {
    *error = g_error_copy (data->error);
    gst_gtuber_thread_data_free (data);

    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Fetched media info");
  info = g_object_ref (data->info);

  gst_gtuber_thread_data_free (data);

  return info;
}

static GstFlowReturn
gst_gtuber_src_create (GstPushSrc *push_src, GstBuffer **outbuf)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (push_src);
  GtuberMediaInfo *info = NULL;
  GError *error = NULL;

  /* When non-zero, we already returned complete data */
  if (self->buf_size > 0)
    return GST_FLOW_EOS;

  g_mutex_lock (&self->prop_lock);
  if (self->info) {
    GST_DEBUG_OBJECT (self, "Using media info set by user");
    info = g_object_ref (self->info);
  }
  g_mutex_unlock (&self->prop_lock);

  if (!info) {
    if (!(info = gst_gtuber_fetch_media_info (self, &error))) {
      GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
          ("%s", error->message), (NULL));
      g_clear_error (&error);

      return GST_FLOW_ERROR;
    }
  }

  if ((*outbuf = gst_gtuber_media_info_to_buffer (self, info, &error)))
    gst_gtuber_src_push_events (self, info);

  /* Hold media info in order for data in it to stay valid */
  g_mutex_lock (&self->prop_lock);
  g_clear_object (&self->info);
  self->info = info;
  g_mutex_unlock (&self->prop_lock);

  return GST_FLOW_OK;
}

static inline gboolean
_handle_uri_query (GstGtuberSrc *self, GstQuery *query)
{
  /* Since our URI doesn't actually lead to manifest data, we answer
   * with "nodata" equivalent, so upstream will not try to fetch it */
  gst_query_set_uri (query, "data:,");

  return TRUE;
}

static gboolean
gst_gtuber_src_query (GstBaseSrc *base_src, GstQuery *query)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      ret = _handle_uri_query (self, query);
      break;
    default:
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (base_src, query);

  return ret;
}

static void
gst_gtuber_src_init (GstGtuberSrc *self)
{
  g_mutex_init (&self->client_lock);
  g_cond_init (&self->client_cond);

  self->cancellable = g_cancellable_new ();
}

static void
clapper_tube_src_dispose (GObject *object)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (object);

  g_mutex_lock (&self->client_lock);

  if (self->client_thread) {
    if (G_LIKELY (self->client_thread != g_thread_self ()))
      g_clear_pointer (&self->client_thread, g_thread_join);
    else
      g_clear_pointer (&self->client_thread, g_thread_unref);
  }

  g_mutex_unlock (&self->client_lock);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_tube_src_finalize (GObject *object)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->uri);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->info);

  g_mutex_clear (&self->client_lock);
  g_cond_clear (&self->client_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gtuber_src_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (object);

  switch (prop_id) {
    case PROP_URI:{
      GError *error = NULL;
      if (!gst_uri_handler_set_uri (self, g_value_get_string (value), &error)) {
        GST_ERROR_OBJECT (self, "%s", error->message);
        g_error_free (error);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gtuber_src_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_take_string (value, gst_uri_handler_get_uri (GST_URI_HANDLER (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gtuber_src_class_init (GstGtuberSrcClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (clapper_tube_src_debug, "clappertubesrc", 0,
      "ClapperTube Source");

  gobject_class->finalize = gst_gtuber_src_finalize;
  gobject_class->set_property = gst_gtuber_src_set_property;
  gobject_class->get_property = gst_gtuber_src_get_property;

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_gtuber_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_gtuber_src_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_gtuber_src_get_size);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_gtuber_src_is_seekable);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_gtuber_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_gtuber_src_unlock_stop);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_gtuber_src_query);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_gtuber_src_create);

  param_specs[PROP_URI] = g_param_spec_string ("uri",
      "URI", "Media URI", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class, "ClapperTube Source",
      "Source", "Source plugin for playing media from various websites",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
