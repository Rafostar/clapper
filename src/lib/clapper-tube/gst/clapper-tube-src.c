/* Clapper Tube Library
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
#include "../clapper-tube-private.h"

#define GST_CAT_DEFAULT clapper_tube_src_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperTubeSrc
{
  GstPushSrc parent;

  GCancellable *cancellable;
  gsize buf_size;

  ClapperTubeClient *client;
  ClapperTubeExtractor *pending_extractor;

  /* props */
  gchar *uri;
};

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
  ClapperTubeExtractor *extractor;
  GUri *guri;
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

  if (!(guri = g_uri_parse (uri, G_URI_FLAGS_ENCODED, NULL))) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "URI is invalid");
    return FALSE;
  }

  if ((extractor = clapper_tube_loader_get_extractor_for_uri (guri)))
    clapper_tube_extractor_set_uri (extractor, guri);

  g_uri_unref (guri);

  if (!extractor) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Clapper Tube does not have an extractor for this URI");
    return FALSE;
  }

  GST_OBJECT_LOCK (self);

  g_set_str (&self->uri, uri);
  GST_INFO_OBJECT (self, "URI changed to: %s", self->uri);

  gst_object_replace ((GstObject **) &self->pending_extractor, GST_OBJECT_CAST (extractor));
  GST_DEBUG_OBJECT (self, "Pending extractor: %" GST_PTR_FORMAT, extractor);

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
  clapper_tube_client_stop (self->client);
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
_push_events (ClapperTubeSrc *self, ClapperTubeHarvest *harvest, gboolean updated)
{
  GstEvent *event;
  GstTagList *tags;
  GstToc *toc;

  event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY,
      gst_structure_copy (clapper_tube_harvest_get_request_headers (harvest));
  GST_DEBUG_OBJECT (self, "Pushing request headers: %", GST_PTR_FORMAT, event);
  gst_pad_push_event (GST_BASE_SRC_PAD (self), event);

  tags = clapper_tube_harvest_get_tags (harvest);
  if (!gst_tag_list_is_empty (tags)) {
    event = gst_event_new_tag (gst_tag_list_copy (tags));
    GST_DEBUG_OBJECT (self, "Pushing tags: %" GST_PTR_FORMAT, event);
    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);

    /* FIXME: We should be posting event only to make it reach app
     * after stream start, but currently event is lost that way */
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_tag (NULL, tags));
  }

  toc = clapper_tube_harvest_get_toc (harvest);
  if (g_list_length (gst_toc_get_entries (toc)) > 0) {
    event = gst_event_new_toc (toc, updated);
    GST_DEBUG_OBJECT (self, "Pushing TOC: %" GST_PTR_FORMAT, event);
    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);

    /* FIXME: We should be posting event only to make it reach app
     * after stream start, but currently event is lost that way */
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_toc (NULL, toc, updated));
  }

  GST_DEBUG_OBJECT (self, "Pushed all events");
}

static GstFlowReturn
gst_gtuber_src_create (GstPushSrc *push_src, GstBuffer **outbuf)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC (push_src);
  ClapperTubeExtractor *extractor = NULL;
  ClapperTubeHarvest *harvest = NULL;
  GstCaps *caps = NULL;
  GError *error = NULL;

  /* When non-zero, we already returned complete data */
  if (self->buf_size > 0)
    return GST_FLOW_EOS;

  /* Ensure client is created. Because it spins up its
   * own thread, create it here since it will be used */
  if (!self->client)
    self->client = clapper_tube_client_new ();

  GST_OBJECT_LOCK (self);

  if (self->pending_extractor)
    extractor = gst_object_ref (self->pending_extractor);

  GST_OBJECT_UNLOCK (self);

  if (!extractor) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("%s", "No pending extractor to use"), (NULL));
    return GST_FLOW_ERROR;
  }

  harvest = clapper_tube_client_run (self->client, extractor, self->cancellable, &error);
  gst_object_unref (extractor);

  if (!harvest) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("%s", error->message), (NULL));
    g_clear_error (&error);

    return GST_FLOW_ERROR;
  }

  if (!clapper_tube_harvest_unpack (harvest, outbuf, &self->buf_size, &caps)) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("%s", "Extraction harvest is empty"), (NULL));
    return GST_FLOW_ERROR;
  }

  if (gst_base_src_set_caps (GST_BASE_SRC (self), caps))
    GST_INFO_OBJECT (self, "Using caps: %" GST_PTR_FORMAT, caps);
  else
    GST_ERROR_OBJECT (self, "Current caps could not be set");

  gst_clear_caps (&caps);

  /* Now push all events before buffer */
  _push_events (self, harvest, FALSE);

  return GST_FLOW_OK;
}

static inline gboolean
_handle_uri_query (GstGtuberSrc *self, GstQuery *query)
{
  /* Since our URI does not actually lead to manifest data, we answer
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
  self->cancellable = g_cancellable_new ();
}

static void
clapper_tube_src_dispose (GObject *object)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (object);

  GST_OBJECT_LOCK (self);
  g_clear_object (&self->client);
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_tube_src_finalize (GObject *object)
{
  ClapperTubeSrc *self = CLAPPER_TUBE_SRC_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_clear_object (&self->cancellable);
  g_free (self->uri);

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
      "Clapper Tube Source");

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

  gst_element_class_set_static_metadata (gstelement_class, "Clapper Tube Source",
      "Source", "Clapper Tube source element",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
