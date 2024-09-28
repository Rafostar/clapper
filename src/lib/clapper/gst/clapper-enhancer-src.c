/* Clapper Playback Library
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

#include "clapper-enhancer-src-private.h"
#include "clapper-enhancer-director-private.h"

#include "../clapper-extractable-private.h"
#include "../clapper-harvest-private.h"
#include "../clapper-enhancers-loader-private.h"

#define GST_CAT_DEFAULT clapper_enhancer_src_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperEnhancerSrc
{
  GstPushSrc parent;

  GCancellable *cancellable;
  gsize buf_size;

  ClapperEnhancerDirector *director;

  gchar *uri;
  GUri *guri;
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
clapper_enhancer_src_uri_handler_get_type (GType type)
{
  return GST_URI_SRC;
}

static gpointer
_get_schemes_once (gpointer user_data G_GNUC_UNUSED)
{
  return clapper_enhancers_loader_get_schemes (CLAPPER_TYPE_EXTRACTABLE);
}

static const gchar *const *
clapper_enhancer_src_uri_handler_get_protocols (GType type)
{
  static GOnce schemes_once = G_ONCE_INIT;

  g_once (&schemes_once, _get_schemes_once, NULL);
  return (const gchar *const *) schemes_once.retval;
}

static gchar *
clapper_enhancer_src_uri_handler_get_uri (GstURIHandler *handler)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (handler);
  gchar *uri;

  GST_OBJECT_LOCK (self);
  uri = g_strdup (self->uri);
  GST_OBJECT_UNLOCK (self);

  return uri;
}

static gboolean
clapper_enhancer_src_uri_handler_set_uri (GstURIHandler *handler,
    const gchar *uri, GError **error)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (handler);
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
    if ((supported = gst_uri_has_protocol (uri, protocols[i])))
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

  if (!clapper_enhancers_loader_check (CLAPPER_TYPE_EXTRACTABLE,
      g_uri_get_scheme (guri), g_uri_get_host (guri), NULL)) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "None of the available enhancers can handle this URI");
    g_uri_unref (guri);

    return FALSE;
  }

  GST_OBJECT_LOCK (self);

  g_set_str (&self->uri, uri);
  g_clear_pointer (&self->guri, g_uri_unref);
  self->guri = guri;

  GST_INFO_OBJECT (self, "URI changed to: \"%s\"", self->uri);

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static void
_uri_handler_iface_init (GstURIHandlerInterface *iface)
{
  iface->get_type = clapper_enhancer_src_uri_handler_get_type;
  iface->get_protocols = clapper_enhancer_src_uri_handler_get_protocols;
  iface->get_uri = clapper_enhancer_src_uri_handler_get_uri;
  iface->set_uri = clapper_enhancer_src_uri_handler_set_uri;
}

#define parent_class clapper_enhancer_src_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperEnhancerSrc, clapper_enhancer_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, _uri_handler_iface_init));
GST_ELEMENT_REGISTER_DEFINE (clapperenhancersrc, "clapperenhancersrc",
    512, CLAPPER_TYPE_ENHANCER_SRC);

static gboolean
clapper_enhancer_src_start (GstBaseSrc *base_src)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (base_src);
  gboolean can_start;

  GST_DEBUG_OBJECT (self, "Start");

  GST_OBJECT_LOCK (self);
  can_start = (self->guri != NULL);
  GST_OBJECT_UNLOCK (self);

  if (G_UNLIKELY (!can_start)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("No media URI"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
clapper_enhancer_src_stop (GstBaseSrc *base_src)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (base_src);

  GST_DEBUG_OBJECT (self, "Stop");

  self->buf_size = 0;

  return TRUE;
}

static gboolean
clapper_enhancer_src_get_size (GstBaseSrc *base_src, guint64 *size)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (base_src);

  if (self->buf_size > 0) {
    *size = self->buf_size;
    return TRUE;
  }

  return FALSE;
}

static gboolean
clapper_enhancer_src_is_seekable (GstBaseSrc *base_src)
{
  return FALSE;
}

static gboolean
clapper_enhancer_src_unlock (GstBaseSrc *base_src)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (base_src);

  GST_LOG_OBJECT (self, "Cancel triggered");
  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
clapper_enhancer_src_unlock_stop (GstBaseSrc *base_src)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (base_src);

  GST_LOG_OBJECT (self, "Resetting cancellable");

  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();

  return TRUE;
}

/* Pushes tags, toc and request headers downstream (all transfer full) */
static void
_push_events (ClapperEnhancerSrc *self, GstTagList *tags, GstToc *toc,
    GstStructure *headers, gboolean updated)
{
  GstEvent *event;

  if (tags) {
    if (!gst_tag_list_is_empty (tags)) {
      GST_DEBUG_OBJECT (self, "Pushing %" GST_PTR_FORMAT, tags);

      /* XXX: Normally, we should only be posting event to make it reach
       * the app after stream start, but currently it is lost that way */
      gst_element_post_message (GST_ELEMENT (self),
          gst_message_new_tag (GST_OBJECT_CAST (self), tags));
    } else {
      gst_tag_list_unref (tags);
    }
  }

  if (toc) {
    if (g_list_length (gst_toc_get_entries (toc)) > 0) {
      GST_DEBUG_OBJECT (self, "Pushing TOC"); // TOC is not printable

      /* XXX: Normally, we should only be posting event to make it reach
       * the app after stream start, but currently it is lost that way */
      gst_element_post_message (GST_ELEMENT (self),
          gst_message_new_toc (GST_OBJECT_CAST (self), toc, updated));
    }
    gst_toc_unref (toc);
  }

  if (headers) {
    GstStructure *http_headers;

    GST_DEBUG_OBJECT (self, "Pushing %" GST_PTR_FORMAT, headers);

    http_headers = gst_structure_new ("http-headers",
        "request-headers", GST_TYPE_STRUCTURE, headers,
        NULL);
    gst_structure_free (headers);

    event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, http_headers);
    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);
  }

  GST_DEBUG_OBJECT (self, "Pushed all events");
}

static GstFlowReturn
clapper_enhancer_src_create (GstPushSrc *push_src, GstBuffer **outbuf)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (push_src);
  GUri *guri;
  GCancellable *cancellable;
  ClapperHarvest *harvest;
  GstCaps *caps = NULL;
  GstTagList *tags = NULL;
  GstToc *toc = NULL;
  GstStructure *headers = NULL;
  GError *error = NULL;
  gboolean unpacked;

  /* When non-zero, we already returned complete data */
  if (self->buf_size > 0)
    return GST_FLOW_EOS;

  /* Ensure director is created. Since it spins up its own
   * thread, create it here as we know that it will be used. */
  if (!self->director)
    self->director = clapper_enhancer_director_new ();

  GST_OBJECT_LOCK (self);
  guri = g_uri_ref (self->guri);
  cancellable = g_object_ref (self->cancellable);
  GST_OBJECT_UNLOCK (self);

  harvest = clapper_enhancer_director_extract (self->director, guri, cancellable, &error);

  g_uri_unref (guri);
  g_object_unref (cancellable);

  if (!harvest) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("%s", error->message), (NULL));
    g_clear_error (&error);

    return GST_FLOW_ERROR;
  }

  unpacked = clapper_harvest_unpack (harvest, outbuf, &self->buf_size,
      &caps, &tags, &toc, &headers);
  gst_object_unref (harvest);

  if (!unpacked) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Extraction harvest is empty"), (NULL));
    return GST_FLOW_ERROR;
  }

  if (gst_base_src_set_caps (GST_BASE_SRC (self), caps))
    GST_INFO_OBJECT (self, "Using caps: %" GST_PTR_FORMAT, caps);
  else
    GST_ERROR_OBJECT (self, "Current caps could not be set");

  gst_clear_caps (&caps);

  /* Now push all events before buffer */
  _push_events (self, tags, toc, headers, FALSE);

  return GST_FLOW_OK;
}

static inline gboolean
_handle_uri_query (GstQuery *query)
{
  /* Since our URI does not actually lead to manifest data, we answer
   * with "nodata" equivalent, so upstream will not try to fetch it */
  gst_query_set_uri (query, "data:,");

  return TRUE;
}

static gboolean
clapper_enhancer_src_query (GstBaseSrc *base_src, GstQuery *query)
{
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      ret = _handle_uri_query (query);
      break;
    default:
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (base_src, query);

  return ret;
}

static void
clapper_enhancer_src_init (ClapperEnhancerSrc *self)
{
  self->cancellable = g_cancellable_new ();
}

static void
clapper_enhancer_src_dispose (GObject *object)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (object);

  GST_OBJECT_LOCK (self);
  g_clear_object (&self->director);
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_enhancer_src_finalize (GObject *object)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_clear_object (&self->cancellable);
  g_free (self->uri);
  g_clear_pointer (&self->guri, g_uri_unref);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_enhancer_src_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (object);

  switch (prop_id) {
    case PROP_URI:{
      GError *error = NULL;
      if (!gst_uri_handler_set_uri (GST_URI_HANDLER (self),
          g_value_get_string (value), &error)) {
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
clapper_enhancer_src_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperEnhancerSrc *self = CLAPPER_ENHANCER_SRC_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_take_string (value, gst_uri_handler_get_uri (GST_URI_HANDLER (self)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_enhancer_src_class_init (ClapperEnhancerSrcClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancersrc", 0,
      "Clapper Enhancer Source");

  gobject_class->set_property = clapper_enhancer_src_set_property;
  gobject_class->get_property = clapper_enhancer_src_get_property;
  gobject_class->dispose = clapper_enhancer_src_dispose;
  gobject_class->finalize = clapper_enhancer_src_finalize;

  gstbasesrc_class->start = clapper_enhancer_src_start;
  gstbasesrc_class->stop = clapper_enhancer_src_stop;
  gstbasesrc_class->get_size = clapper_enhancer_src_get_size;
  gstbasesrc_class->is_seekable = clapper_enhancer_src_is_seekable;
  gstbasesrc_class->unlock = clapper_enhancer_src_unlock;
  gstbasesrc_class->unlock_stop = clapper_enhancer_src_unlock_stop;
  gstbasesrc_class->query = clapper_enhancer_src_query;

  gstpushsrc_class->create = clapper_enhancer_src_create;

  param_specs[PROP_URI] = g_param_spec_string ("uri",
      "URI", "URI", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Clapper Enhancer Source",
      "Source", "A source element that uses Clapper Enhancers to produce data",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
