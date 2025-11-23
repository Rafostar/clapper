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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include <gst/base/gstadapter.h>

#include "clapper-uri-base-demux-private.h"

#define GST_CAT_DEFAULT clapper_uri_base_demux_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperUriBaseDemuxPrivate ClapperUriBaseDemuxPrivate;

struct _ClapperUriBaseDemuxPrivate
{
  GstAdapter *input_adapter;

  GstElement *uri_handler;
  GstElement *typefind;

  GstPad *typefind_src;

  GCancellable *cancellable;
};

typedef struct
{
  const gchar *search_proto;
  const gchar *blacklisted_el;
} ClapperUriBaseDemuxFilterData;

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

#define parent_class clapper_uri_base_demux_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (ClapperUriBaseDemux, clapper_uri_base_demux, GST_TYPE_BIN);

static gboolean
remove_sometimes_pad_cb (GstElement *element, GstPad *pad, ClapperUriBaseDemux *self)
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
clapper_uri_base_demux_reset (ClapperUriBaseDemux *self)
{
  ClapperUriBaseDemuxPrivate *priv = clapper_uri_base_demux_get_instance_private (self);
  GstElement *element = GST_ELEMENT_CAST (self);

  GST_OBJECT_LOCK (self);

  GST_LOG_OBJECT (self, "Resetting cancellable");

  g_cancellable_cancel (priv->cancellable);
  g_object_unref (priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  GST_OBJECT_UNLOCK (self);

  gst_element_foreach_pad (element, (GstElementForeachPadFunc) remove_sometimes_pad_cb, NULL);
}

static GstStateChangeReturn
clapper_uri_base_demux_change_state (GstElement *element, GstStateChange transition)
{
  ClapperUriBaseDemux *self = CLAPPER_URI_BASE_DEMUX_CAST (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      clapper_uri_base_demux_reset (self);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
_feature_filter (GstPluginFeature *feature, ClapperUriBaseDemuxFilterData *filter_data)
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
  if (!feature_name || g_strcmp0 (feature_name, filter_data->blacklisted_el) == 0)
    return FALSE;

  protocols = gst_element_factory_get_uri_protocols (factory);

  if (protocols) {
    for (i = 0; protocols[i]; ++i) {
      if (g_ascii_strcasecmp (protocols[i], filter_data->search_proto) == 0)
        return TRUE;
    }
  }

  return FALSE;
}

static GstElement *
_make_handler_for_uri (ClapperUriBaseDemux *self, const gchar *uri, const gchar *blacklisted_el)
{
  GstElement *element = NULL;
  GList *factories, *f;
  ClapperUriBaseDemuxFilterData filter_data;
  gchar *protocol;

  if (!gst_uri_is_valid (uri)) {
    GST_ERROR_OBJECT (self, "Cannot create handler for invalid URI: \"%s\"", uri);
    return NULL;
  }

  protocol = gst_uri_get_protocol (uri);

  filter_data.search_proto = protocol;
  filter_data.blacklisted_el = blacklisted_el;

  factories = gst_registry_feature_filter (gst_registry_get (),
      (GstPluginFeatureFilter) _feature_filter, FALSE, &filter_data);

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
_src_pad_query_func (GstPad *pad, GstObject *parent, GstQuery *query)
{
  if (GST_QUERY_TYPE (query) == GST_QUERY_CUSTOM) {
    ClapperUriBaseDemux *self = CLAPPER_URI_BASE_DEMUX_CAST (parent);
    ClapperUriBaseDemuxClass *uri_bd_class = CLAPPER_URI_BASE_DEMUX_GET_CLASS (self);

    if (uri_bd_class->handle_custom_query && uri_bd_class->handle_custom_query (self, query))
      return TRUE;
  }

  return gst_pad_query_default (pad, parent, query);
}

gboolean
clapper_uri_base_demux_set_uri (ClapperUriBaseDemux *self, const gchar *uri, const gchar *blacklisted_el)
{
  ClapperUriBaseDemuxPrivate *priv = clapper_uri_base_demux_get_instance_private (self);
  GstPad *uri_handler_src, *typefind_sink, *src_ghostpad;
  GstPadLinkReturn pad_link_ret;

  GST_DEBUG_OBJECT (self, "Stream URI: %s", uri);

  if (priv->uri_handler) {
    GST_DEBUG_OBJECT (self, "Trying to reuse existing URI handler");

    if (gst_uri_handler_set_uri (GST_URI_HANDLER (priv->uri_handler), uri, NULL)) {
      GST_DEBUG_OBJECT (self, "Reused existing URI handler");
    } else {
      GST_DEBUG_OBJECT (self, "Could not reuse existing URI handler");

      if (priv->typefind_src) {
        gst_element_remove_pad (GST_ELEMENT_CAST (self), priv->typefind_src);
        gst_clear_object (&priv->typefind_src);
      }

      gst_bin_remove (GST_BIN_CAST (self), priv->uri_handler);
      gst_bin_remove (GST_BIN_CAST (self), priv->typefind);

      priv->uri_handler = NULL;
      priv->typefind = NULL;
    }
  }

  if (!priv->uri_handler) {
    GST_DEBUG_OBJECT (self, "Creating new URI handler element");

    priv->uri_handler = _make_handler_for_uri (self, uri, blacklisted_el);

    if (G_UNLIKELY (!priv->uri_handler)) {
      GST_ERROR_OBJECT (self, "Could not create URI handler element");

      GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN,
          ("Missing plugin to handle URI: %s", uri), (NULL));

      return FALSE;
    }

    gst_bin_add (GST_BIN_CAST (self), priv->uri_handler);

    priv->typefind = gst_element_factory_make ("typefind", NULL);
    gst_bin_add (GST_BIN_CAST (self), priv->typefind);

    uri_handler_src = gst_element_get_static_pad (priv->uri_handler, "src");
    typefind_sink = gst_element_get_static_pad (priv->typefind, "sink");

    pad_link_ret = gst_pad_link_full (uri_handler_src, typefind_sink,
        GST_PAD_LINK_CHECK_NOTHING);

    if (pad_link_ret != GST_PAD_LINK_OK)
      g_critical ("Failed to link bin elements");

    g_object_unref (uri_handler_src);
    g_object_unref (typefind_sink);

    priv->typefind_src = gst_element_get_static_pad (priv->typefind, "src");

    src_ghostpad = gst_ghost_pad_new_from_template ("src", priv->typefind_src,
        gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "src"));
    gst_pad_set_query_function (src_ghostpad, (GstPadQueryFunction) _src_pad_query_func);

    gst_pad_set_active (src_ghostpad, TRUE);

    if (!gst_element_add_pad (GST_ELEMENT_CAST (self), src_ghostpad)) {
      g_critical ("Failed to add source pad to bin");
    } else {
      GST_DEBUG_OBJECT (self, "Added src pad, signalling \"no-more-pads\"");
      gst_element_no_more_pads (GST_ELEMENT_CAST (self));
    }
  }

  gst_element_sync_state_with_parent (priv->typefind);
  gst_element_sync_state_with_parent (priv->uri_handler);

  return TRUE;
}

static gboolean
clapper_uri_base_demux_sink_event (GstPad *pad, GstObject *parent, GstEvent *event)
{
  ClapperUriBaseDemux *self = CLAPPER_URI_BASE_DEMUX_CAST (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      ClapperUriBaseDemuxClass *uri_bd_class = CLAPPER_URI_BASE_DEMUX_GET_CLASS (self);

      if (uri_bd_class->handle_caps) {
        GstCaps *caps;

        gst_event_parse_caps (event, &caps);

        if (gst_caps_is_fixed (caps))
          uri_bd_class->handle_caps (self, caps);
      }
      break;
    }
    case GST_EVENT_EOS:{
      ClapperUriBaseDemuxPrivate *priv = clapper_uri_base_demux_get_instance_private (self);
      GCancellable *cancellable;
      GstBuffer *buffer;
      gsize size;
      gboolean success;

      size = gst_adapter_available (priv->input_adapter);

      if (size == 0) {
        GST_WARNING_OBJECT (self, "Received EOS without URI data");
        break;
      }

      GST_OBJECT_LOCK (self);
      cancellable = g_object_ref (priv->cancellable);
      GST_OBJECT_UNLOCK (self);

      buffer = gst_adapter_take_buffer (priv->input_adapter, size);
      success = CLAPPER_URI_BASE_DEMUX_GET_CLASS (self)->process_buffer (self, buffer, cancellable);
      gst_buffer_unref (buffer);
      g_object_unref (cancellable);

      if (success) {
        gst_event_unref (event);
        return TRUE;
      }
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      ClapperUriBaseDemuxClass *uri_bd_class = CLAPPER_URI_BASE_DEMUX_GET_CLASS (self);

      if (uri_bd_class->handle_custom_event)
        uri_bd_class->handle_custom_event (self, event);

      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstFlowReturn
clapper_uri_base_demux_sink_chain (GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
  ClapperUriBaseDemux *self = CLAPPER_URI_BASE_DEMUX_CAST (parent);
  ClapperUriBaseDemuxPrivate *priv = clapper_uri_base_demux_get_instance_private (self);

  gst_adapter_push (priv->input_adapter, buffer);
  GST_DEBUG_OBJECT (self, "Received buffer, total collected: %" G_GSIZE_FORMAT " bytes",
      gst_adapter_available (priv->input_adapter));

  return GST_FLOW_OK;
}

static void
clapper_uri_base_demux_init (ClapperUriBaseDemux *self)
{
  ClapperUriBaseDemuxPrivate *priv = clapper_uri_base_demux_get_instance_private (self);

  priv->input_adapter = gst_adapter_new ();
  priv->cancellable = g_cancellable_new ();
}

static void
clapper_uri_base_demux_constructed (GObject *object)
{
  ClapperUriBaseDemux *self = CLAPPER_URI_BASE_DEMUX_CAST (object);
  GstPad *sink_pad;

  sink_pad = gst_pad_new_from_template (gst_element_class_get_pad_template (
      GST_ELEMENT_GET_CLASS (self), "sink"), "sink");
  gst_pad_set_event_function (sink_pad,
      GST_DEBUG_FUNCPTR (clapper_uri_base_demux_sink_event));
  gst_pad_set_chain_function (sink_pad,
      GST_DEBUG_FUNCPTR (clapper_uri_base_demux_sink_chain));

  gst_pad_set_active (sink_pad, TRUE);

  if (!gst_element_add_pad (GST_ELEMENT_CAST (self), sink_pad))
    g_critical ("Failed to add sink pad to bin");

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_uri_base_demux_finalize (GObject *object)
{
  ClapperUriBaseDemux *self = CLAPPER_URI_BASE_DEMUX_CAST (object);
  ClapperUriBaseDemuxPrivate *priv = clapper_uri_base_demux_get_instance_private (self);

  g_object_unref (priv->input_adapter);
  g_object_unref (priv->cancellable);
  gst_clear_object (&priv->typefind_src);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_uri_base_demux_class_init (ClapperUriBaseDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperuribasedemux", 0,
      "Clapper URI Base Demux");

  gobject_class->constructed = clapper_uri_base_demux_constructed;
  gobject_class->finalize = clapper_uri_base_demux_finalize;

  gstelement_class->change_state = clapper_uri_base_demux_change_state;

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
}
