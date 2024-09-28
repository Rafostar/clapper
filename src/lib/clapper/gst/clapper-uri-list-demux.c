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

#include <gst/base/gstadapter.h>

#include "clapper-uri-list-demux-private.h"

#define GST_CAT_DEFAULT clapper_uri_list_demux_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperUriListDemux
{
  GstBin parent;

  GMutex lock;

  GstAdapter *input_adapter;

  GstElement *uri_handler;
  GstElement *typefind;

  GstPad *typefind_src;

  GstStructure *http_headers;
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/uri-list, source=(string)clapper-harvest"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

#define parent_class clapper_uri_list_demux_parent_class
G_DEFINE_TYPE (ClapperUriListDemux, clapper_uri_list_demux, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (clapperurilistdemux, "clapperurilistdemux",
    512, CLAPPER_TYPE_URI_LIST_DEMUX);

static void
_set_property (GstObject *obj, const gchar *prop_name, gpointer value)
{
  g_object_set (G_OBJECT (obj), prop_name, value, NULL);

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *el_name;

    el_name = gst_object_get_name (obj);
    GST_DEBUG ("Set %s %s", el_name, prop_name);

    g_free (el_name);
  }
}

static gboolean
configure_deep_element (GQuark field_id, const GValue *value, GstElement *child)
{
  GObjectClass *gobject_class;
  const GstStructure *substructure;

  if (!GST_VALUE_HOLDS_STRUCTURE (value))
    return TRUE;

  substructure = gst_value_get_structure (value);

  if (!gst_structure_has_name (substructure, "request-headers"))
    return TRUE;

  gobject_class = G_OBJECT_GET_CLASS (child);

  if (g_object_class_find_property (gobject_class, "user-agent")) {
    const gchar *ua;

    if ((ua = gst_structure_get_string (substructure, "User-Agent")))
      _set_property (GST_OBJECT_CAST (child), "user-agent", (gchar *) ua);
  }

  if (g_object_class_find_property (gobject_class, "extra-headers")) {
    GstStructure *extra_headers;

    extra_headers = gst_structure_copy (substructure);
    gst_structure_set_name (extra_headers, "extra-headers");
    gst_structure_remove_field (extra_headers, "User-Agent");

    _set_property (GST_OBJECT_CAST (child), "extra-headers", extra_headers);

    gst_structure_free (extra_headers);
  }

  return TRUE;
}

static void
clapper_uri_list_demux_deep_element_added (GstBin *bin, GstBin *sub_bin, GstElement *child)
{
  if (GST_OBJECT_FLAG_IS_SET (child, GST_ELEMENT_FLAG_SOURCE)) {
    ClapperUriListDemux *self = CLAPPER_URI_LIST_DEMUX_CAST (bin);

    g_mutex_lock (&self->lock);

    if (self->http_headers) {
      gst_structure_foreach (self->http_headers,
          (GstStructureForeachFunc) configure_deep_element, child);
    }

    g_mutex_unlock (&self->lock);
  }
}

static gboolean
remove_sometimes_pad_cb (GstElement *element, GstPad *pad, ClapperUriListDemux *self)
{
  GstPadTemplate *template = gst_pad_get_pad_template (pad);
  GstPadPresence presence = GST_PAD_TEMPLATE_PRESENCE (template);

  gst_object_unref (template);

  if (presence == GST_PAD_SOMETIMES) {
    GST_DEBUG_OBJECT (self, "Removing src pad");

    gst_pad_set_active (pad, FALSE);

    if (G_UNLIKELY (!gst_element_remove_pad (element, pad)))
      g_critical ("Failed to remove pad from bin");
  }

  return TRUE;
}

static void
clapper_uri_list_demux_reset (ClapperUriListDemux *self)
{
  GstElement *element = GST_ELEMENT_CAST (self);

  gst_element_foreach_pad (element, (GstElementForeachPadFunc) remove_sometimes_pad_cb, NULL);
}

static GstStateChangeReturn
clapper_uri_list_demux_change_state (GstElement *element, GstStateChange transition)
{
  ClapperUriListDemux *self = CLAPPER_URI_LIST_DEMUX_CAST (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      clapper_uri_list_demux_reset (self);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
_feature_filter (GstPluginFeature *feature, const gchar *search_proto)
{
  GstElementFactory *factory;
  const gchar *const *protocols;
  const gchar *feature_name;
  guint i;

  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;

  factory = GST_ELEMENT_FACTORY_CAST (feature);

  if (gst_element_factory_get_uri_type (factory) != GST_URI_SRC)
    return FALSE;

  feature_name = gst_plugin_feature_get_name (feature);

  /* Do not loop endlessly creating our own sources and demuxers */
  if (!feature_name || strcmp (feature_name, "clapperenhancersrc") == 0)
    return FALSE;

  protocols = gst_element_factory_get_uri_protocols (factory);

  if (protocols) {
    for (i = 0; protocols[i]; ++i) {
      if (g_ascii_strcasecmp (protocols[i], search_proto) == 0)
        return TRUE;
    }
  }

  return FALSE;
}

static GstElement *
_make_handler_for_uri (ClapperUriListDemux *self, const gchar *uri)
{
  GstElement *element = NULL;
  GList *factories, *f;
  gchar *protocol;

  if (!gst_uri_is_valid (uri)) {
    GST_ERROR_OBJECT (self, "Cannot create handler for invalid URI: \"%s\"", uri);
    return NULL;
  }

  protocol = gst_uri_get_protocol (uri);
  factories = gst_registry_feature_filter (gst_registry_get (),
      (GstPluginFeatureFilter) _feature_filter, FALSE, protocol);
  g_free (protocol);

  factories = g_list_sort (factories,
      (GCompareFunc) gst_plugin_feature_rank_compare_func);

  for (f = factories; f; f = g_list_next (f)) {
    GstElementFactory *factory = f->data;

    if ((element = gst_element_factory_create (factory, NULL))
        && gst_uri_handler_set_uri (GST_URI_HANDLER (element), uri, NULL))
      break;

    gst_clear_object (&element);
  }

  gst_plugin_feature_list_free (factories);

  GST_DEBUG_OBJECT (self, "Created URI handler: %s",
      GST_OBJECT_NAME (element));

  return element;
}

static gboolean
clapper_uri_list_demux_process_buffer (ClapperUriListDemux *self, GstBuffer *buffer)
{
  GstMemory *mem;
  GstMapInfo info;

  mem = gst_buffer_peek_memory (buffer, 0);

  if (mem && gst_memory_map (mem, &info, GST_MAP_READ)) {
    GstPad *uri_handler_src, *typefind_sink, *src_ghostpad;
    GstPadLinkReturn pad_link_ret;

    GST_DEBUG_OBJECT (self, "Stream URI: %s", (const gchar *) info.data);

    if (self->uri_handler) {
      GST_DEBUG_OBJECT (self, "Trying to reuse existing URI handler");

      if (gst_uri_handler_set_uri (GST_URI_HANDLER (self->uri_handler),
          (const gchar *) info.data, NULL)) {
        GST_DEBUG_OBJECT (self, "Reused existing URI handler");
      } else {
        GST_DEBUG_OBJECT (self, "Could not reuse existing URI handler");

        if (self->typefind_src) {
          gst_element_remove_pad (GST_ELEMENT_CAST (self), self->typefind_src);
          gst_clear_object (&self->typefind_src);
        }

        gst_bin_remove (GST_BIN_CAST (self), self->uri_handler);
        gst_bin_remove (GST_BIN_CAST (self), self->typefind);

        self->uri_handler = NULL;
        self->typefind = NULL;
      }
    }

    if (!self->uri_handler) {
      GST_DEBUG_OBJECT (self, "Creating new URI handler element");

      self->uri_handler = _make_handler_for_uri (self, (const gchar *) info.data);

      if (G_UNLIKELY (!self->uri_handler)) {
        GST_ERROR_OBJECT (self, "Could not create URI handler element");

        GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN,
            ("Missing plugin to handle URI: %s", info.data), (NULL));
        gst_memory_unmap (mem, &info);

        return FALSE;
      }

      gst_bin_add (GST_BIN_CAST (self), self->uri_handler);

      self->typefind = gst_element_factory_make ("typefind", NULL);
      gst_bin_add (GST_BIN_CAST (self), self->typefind);

      uri_handler_src = gst_element_get_static_pad (self->uri_handler, "src");
      typefind_sink = gst_element_get_static_pad (self->typefind, "sink");

      pad_link_ret = gst_pad_link_full (uri_handler_src, typefind_sink,
          GST_PAD_LINK_CHECK_NOTHING);

      if (pad_link_ret != GST_PAD_LINK_OK)
        g_critical ("Failed to link bin elements");

      g_object_unref (uri_handler_src);
      g_object_unref (typefind_sink);

      self->typefind_src = gst_element_get_static_pad (self->typefind, "src");

      src_ghostpad = gst_ghost_pad_new_from_template ("src", self->typefind_src,
          gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "src"));

      gst_pad_set_active (src_ghostpad, TRUE);

      if (!gst_element_add_pad (GST_ELEMENT_CAST (self), src_ghostpad)) {
        g_critical ("Failed to add source pad to bin");
      } else {
        GST_DEBUG_OBJECT (self, "Added src pad, signalling \"no-more-pads\"");
        gst_element_no_more_pads (GST_ELEMENT_CAST (self));
      }
    }

    gst_memory_unmap (mem, &info);

    gst_element_sync_state_with_parent (self->typefind);
    gst_element_sync_state_with_parent (self->uri_handler);
  }

  return TRUE;
}

static gboolean
clapper_uri_list_demux_sink_event (GstPad *pad, GstObject *parent, GstEvent *event)
{
  ClapperUriListDemux *self = CLAPPER_URI_LIST_DEMUX_CAST (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:{
      GstBuffer *buffer;
      gsize size;
      gboolean success;

      size = gst_adapter_available (self->input_adapter);

      if (size == 0) {
        GST_WARNING_OBJECT (self, "Received EOS without URI data");
        break;
      }

      buffer = gst_adapter_take_buffer (self->input_adapter, size);
      success = clapper_uri_list_demux_process_buffer (self, buffer);
      gst_buffer_unref (buffer);

      if (success) {
        gst_event_unref (event);
        return TRUE;
      }
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      const GstStructure *structure = gst_event_get_structure (event);

      if (structure && gst_structure_has_name (structure, "http-headers")) {
        GST_DEBUG_OBJECT (self, "Received \"http-headers\" custom event");
        g_mutex_lock (&self->lock);

        gst_clear_structure (&self->http_headers);
        self->http_headers = gst_structure_copy (structure);

        g_mutex_unlock (&self->lock);
      }
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstFlowReturn
clapper_uri_list_demux_sink_chain (GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
  ClapperUriListDemux *self = CLAPPER_URI_LIST_DEMUX_CAST (parent);

  gst_adapter_push (self->input_adapter, buffer);
  GST_DEBUG_OBJECT (self, "Received buffer, total collected: %" G_GSIZE_FORMAT " bytes",
      gst_adapter_available (self->input_adapter));

  return GST_FLOW_OK;
}

static void
clapper_uri_list_demux_init (ClapperUriListDemux *self)
{
  GstPad *sink_pad;

  g_mutex_init (&self->lock);

  self->input_adapter = gst_adapter_new ();

  sink_pad = gst_pad_new_from_template (gst_element_class_get_pad_template (
      GST_ELEMENT_GET_CLASS (self), "sink"), "sink");
  gst_pad_set_event_function (sink_pad,
      GST_DEBUG_FUNCPTR (clapper_uri_list_demux_sink_event));
  gst_pad_set_chain_function (sink_pad,
      GST_DEBUG_FUNCPTR (clapper_uri_list_demux_sink_chain));

  gst_pad_set_active (sink_pad, TRUE);

  if (!gst_element_add_pad (GST_ELEMENT_CAST (self), sink_pad))
    g_critical ("Failed to add sink pad to bin");
}

static void
clapper_uri_list_demux_finalize (GObject *object)
{
  ClapperUriListDemux *self = CLAPPER_URI_LIST_DEMUX_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_object_unref (self->input_adapter);
  gst_clear_object (&self->typefind_src);
  gst_clear_structure (&self->http_headers);

  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_uri_list_demux_class_init (ClapperUriListDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperurilistdemux", 0,
      "Clapper URI List Demux");

  gobject_class->finalize = clapper_uri_list_demux_finalize;

  gstbin_class->deep_element_added = clapper_uri_list_demux_deep_element_added;

  gstelement_class->change_state = clapper_uri_list_demux_change_state;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Clapper URI List Demux",
      "Demuxer", "A custom demuxer for URI lists",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
