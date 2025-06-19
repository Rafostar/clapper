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

#include "config.h"

#include "clapper-extractable-src-private.h"
#include "clapper-enhancer-director-private.h"

#include "../clapper-basic-functions.h"
#include "../clapper-enhancer-proxy.h"
#include "../clapper-enhancer-proxy-list.h"
#include "../clapper-extractable.h"
#include "../clapper-harvest-private.h"

#define GST_CAT_DEFAULT clapper_extractable_src_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define CHECK_SCHEME_IS_HTTPS(scheme) (g_str_has_prefix (scheme, "http") \
    && (scheme[4] == '\0' || (scheme[4] == 's' && scheme[5] == '\0')))

struct _ClapperExtractableSrc
{
  GstPushSrc parent;

  GCancellable *cancellable;
  gsize buf_size;

  ClapperEnhancerDirector *director;

  gchar *uri;
  GUri *guri;

  ClapperEnhancerProxyList *enhancer_proxies;
};

enum
{
  PROP_0,
  PROP_URI,
  PROP_ENHANCER_PROXIES,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstURIType
clapper_extractable_src_uri_handler_get_type (GType type)
{
  return GST_URI_SRC;
}

/*
 * _make_schemes:
 *
 * Make supported schemes array for a given interface type.
 * The returned array consists of unique strings (no duplicates).
 *
 * Returns: (transfer full): all supported schemes by enhancers of @iface_type.
 */
static gchar **
_make_schemes (gpointer user_data G_GNUC_UNUSED)
{
  ClapperEnhancerProxyList *proxies = clapper_get_global_enhancer_proxies ();
  GSList *found_schemes = NULL, *fs;
  gchar **schemes_strv;
  guint i, n_schemes, n_proxies = clapper_enhancer_proxy_list_get_n_proxies (proxies);

  GST_DEBUG ("Checking for supported URI schemes");

  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (proxies, i);
    const gchar *schemes;

    if (clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_EXTRACTABLE)
        && (schemes = clapper_enhancer_proxy_get_extra_data (proxy, "X-Schemes"))) {
      gchar **tmp_strv;
      gint j;

      tmp_strv = g_strsplit (schemes, ";", 0);

      for (j = 0; tmp_strv[j]; ++j) {
        const gchar *scheme = tmp_strv[j];

        if (!found_schemes || !g_slist_find_custom (found_schemes,
            scheme, (GCompareFunc) strcmp)) {
          found_schemes = g_slist_append (found_schemes, g_strdup (scheme));
          GST_INFO ("Found supported URI scheme: \"%s\"", scheme);
        }
      }

      g_strfreev (tmp_strv);
    }
  }

  n_schemes = g_slist_length (found_schemes);
  schemes_strv = g_new0 (gchar *, n_schemes + 1);

  fs = found_schemes;
  for (i = 0; i < n_schemes; ++i) {
    schemes_strv[i] = fs->data;
    fs = fs->next;
  }

  GST_DEBUG ("Total found URI schemes: %u", n_schemes);

  /* Since string pointers were taken,
   * free list without content */
  g_slist_free (found_schemes);

  return schemes_strv;
}

static inline const gchar *
_host_fixup (const gchar *host)
{
  /* Strip common subdomains, so plugins do not
   * have to list all combinations */
  if (g_str_has_prefix (host, "www."))
    host += 4;
  else if (g_str_has_prefix (host, "m."))
    host += 2;

  return host;
}

/*
 * _extractable_check_for_uri:
 * @self: a #ClapperExtractableSrc
 * @uri: a #GUri
 *
 * Check whether there is at least one extractable enhancer for @uri in global list.
 * This is used to reject URI early, thus making playbin choose different
 * source element. It uses global list, since at this stage element is not
 * yet placed within pipeline, so it cannot get proxies from player.
 *
 * Returns: whether at least one extractable enhancer advertises support for given URI.
 */
static gboolean
_extractable_check_for_uri (ClapperExtractableSrc *self, GUri *uri)
{
  ClapperEnhancerProxyList *proxies = clapper_get_global_enhancer_proxies ();
  gboolean is_https;
  guint i, n_proxies;
  const gchar *scheme = g_uri_get_scheme (uri);
  const gchar *host = g_uri_get_host (uri);

  if (host)
    host = _host_fixup (host);

  GST_INFO_OBJECT (self, "Extractable check, scheme: \"%s\", host: \"%s\"",
      scheme, GST_STR_NULL (host));

  /* Whether "http(s)" scheme is used */
  is_https = CHECK_SCHEME_IS_HTTPS (scheme);

  if (!host && is_https)
    return FALSE;

  n_proxies = clapper_enhancer_proxy_list_get_n_proxies (proxies);
  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (proxies, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_EXTRACTABLE)
        && clapper_enhancer_proxy_extra_data_lists_value (proxy, "X-Schemes", scheme)
        && (!is_https || clapper_enhancer_proxy_extra_data_lists_value (proxy, "X-Hosts", host)))
      return TRUE;
  }

  return FALSE;
}

/*
 * _filter_extractables_for_uri:
 * @self: a #ClapperExtractableSrc
 * @proxies: a #ClapperEnhancerProxyList
 * @uri: a #GUri
 *
 * Finds all enhancer proxies of target implementing "Extractable"
 * interface, which advertise support for given @uri.
 *
 * Returns: (transfer full): A sublist in the form of #GList with proxies.
 */
static GList *
_filter_extractables_for_uri (ClapperExtractableSrc *self,
    ClapperEnhancerProxyList *proxies, GUri *uri)
{
  GList *sublist = NULL;
  guint i, n_proxies;
  gboolean is_https;
  const gchar *scheme = g_uri_get_scheme (uri);
  const gchar *host = g_uri_get_host (uri);

  if (host)
    host = _host_fixup (host);

  GST_INFO_OBJECT (self, "Extractable filter, scheme: \"%s\", host: \"%s\"",
      scheme, GST_STR_NULL (host));

  /* Whether "http(s)" scheme is used */
  is_https = CHECK_SCHEME_IS_HTTPS (scheme);

  if (!host && is_https)
    return NULL;

  n_proxies = clapper_enhancer_proxy_list_get_n_proxies (proxies);
  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (proxies, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_EXTRACTABLE)
        && clapper_enhancer_proxy_extra_data_lists_value (proxy, "X-Schemes", scheme)
        && (!is_https || clapper_enhancer_proxy_extra_data_lists_value (proxy, "X-Hosts", host))) {
      sublist = g_list_append (sublist, gst_object_ref (proxy));
      break;
    }
  }

  return sublist;
}

static const gchar *const *
clapper_extractable_src_uri_handler_get_protocols (GType type)
{
  static GOnce schemes_once = G_ONCE_INIT;

  g_once (&schemes_once, (GThreadFunc) _make_schemes, NULL);
  return (const gchar *const *) schemes_once.retval;
}

static gchar *
clapper_extractable_src_uri_handler_get_uri (GstURIHandler *handler)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (handler);
  gchar *uri;

  GST_OBJECT_LOCK (self);
  uri = g_strdup (self->uri);
  GST_OBJECT_UNLOCK (self);

  return uri;
}

static gboolean
clapper_extractable_src_uri_handler_set_uri (GstURIHandler *handler,
    const gchar *uri, GError **error)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (handler);
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

  if (!_extractable_check_for_uri (self, guri)) {
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
  iface->get_type = clapper_extractable_src_uri_handler_get_type;
  iface->get_protocols = clapper_extractable_src_uri_handler_get_protocols;
  iface->get_uri = clapper_extractable_src_uri_handler_get_uri;
  iface->set_uri = clapper_extractable_src_uri_handler_set_uri;
}

#define parent_class clapper_extractable_src_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperExtractableSrc, clapper_extractable_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, _uri_handler_iface_init));
GST_ELEMENT_REGISTER_DEFINE (clapperextractablesrc, "clapperextractablesrc",
    512, CLAPPER_TYPE_EXTRACTABLE_SRC);

static gboolean
clapper_extractable_src_start (GstBaseSrc *base_src)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (base_src);
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
clapper_extractable_src_stop (GstBaseSrc *base_src)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (base_src);

  GST_DEBUG_OBJECT (self, "Stop");

  self->buf_size = 0;

  return TRUE;
}

static gboolean
clapper_extractable_src_get_size (GstBaseSrc *base_src, guint64 *size)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (base_src);

  if (self->buf_size > 0) {
    *size = self->buf_size;
    return TRUE;
  }

  return FALSE;
}

static gboolean
clapper_extractable_src_is_seekable (GstBaseSrc *base_src)
{
  return FALSE;
}

static gboolean
clapper_extractable_src_unlock (GstBaseSrc *base_src)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (base_src);

  GST_LOG_OBJECT (self, "Cancel triggered");
  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
clapper_extractable_src_unlock_stop (GstBaseSrc *base_src)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (base_src);

  GST_LOG_OBJECT (self, "Resetting cancellable");

  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();

  return TRUE;
}

/* Pushes tags, toc and request headers downstream (all transfer full) */
static void
_push_events (ClapperExtractableSrc *self, GstTagList *tags, GstToc *toc,
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
clapper_extractable_src_create (GstPushSrc *push_src, GstBuffer **outbuf)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (push_src);
  ClapperEnhancerProxyList *proxies;
  GList *filtered_proxies;
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

  if (G_LIKELY (self->enhancer_proxies != NULL)) {
    GST_INFO_OBJECT (self, "Using enhancer proxies: %" GST_PTR_FORMAT, self->enhancer_proxies);
    proxies = gst_object_ref (self->enhancer_proxies);
  } else {
    /* Compat for old ClapperDiscoverer feature that does not set this property */
    GST_WARNING_OBJECT (self, "Falling back to using global enhancer proxy list!");
    proxies = gst_object_ref (clapper_get_global_enhancer_proxies ());
  }

  guri = g_uri_ref (self->guri);
  cancellable = g_object_ref (self->cancellable);

  GST_OBJECT_UNLOCK (self);

  filtered_proxies = _filter_extractables_for_uri (self, proxies, guri);
  gst_object_unref (proxies);

  harvest = clapper_enhancer_director_extract (self->director,
      filtered_proxies, guri, cancellable, &error);

  g_clear_list (&filtered_proxies, gst_object_unref);
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
clapper_extractable_src_query (GstBaseSrc *base_src, GstQuery *query)
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
clapper_extractable_src_set_enhancer_proxies (ClapperExtractableSrc *self,
    ClapperEnhancerProxyList *enhancer_proxies)
{
  GST_OBJECT_LOCK (self);
  gst_object_replace ((GstObject **) &self->enhancer_proxies,
      GST_OBJECT_CAST (enhancer_proxies));
  GST_OBJECT_UNLOCK (self);
}

static void
clapper_extractable_src_init (ClapperExtractableSrc *self)
{
  self->cancellable = g_cancellable_new ();
}

static void
clapper_extractable_src_dispose (GObject *object)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (object);

  GST_OBJECT_LOCK (self);
  g_clear_object (&self->director);
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_extractable_src_finalize (GObject *object)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_clear_object (&self->cancellable);
  g_free (self->uri);
  g_clear_pointer (&self->guri, g_uri_unref);
  gst_clear_object (&self->enhancer_proxies);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_extractable_src_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (object);

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
    case PROP_ENHANCER_PROXIES:
      clapper_extractable_src_set_enhancer_proxies (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_extractable_src_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperExtractableSrc *self = CLAPPER_EXTRACTABLE_SRC_CAST (object);

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
clapper_extractable_src_class_init (ClapperExtractableSrcClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperextractablesrc", 0,
      "Clapper Extractable Source");

  gobject_class->set_property = clapper_extractable_src_set_property;
  gobject_class->get_property = clapper_extractable_src_get_property;
  gobject_class->dispose = clapper_extractable_src_dispose;
  gobject_class->finalize = clapper_extractable_src_finalize;

  gstbasesrc_class->start = clapper_extractable_src_start;
  gstbasesrc_class->stop = clapper_extractable_src_stop;
  gstbasesrc_class->get_size = clapper_extractable_src_get_size;
  gstbasesrc_class->is_seekable = clapper_extractable_src_is_seekable;
  gstbasesrc_class->unlock = clapper_extractable_src_unlock;
  gstbasesrc_class->unlock_stop = clapper_extractable_src_unlock_stop;
  gstbasesrc_class->query = clapper_extractable_src_query;

  gstpushsrc_class->create = clapper_extractable_src_create;

  param_specs[PROP_URI] = g_param_spec_string ("uri",
      "URI", "URI", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_ENHANCER_PROXIES] = g_param_spec_object ("enhancer-proxies",
      NULL, NULL, CLAPPER_TYPE_ENHANCER_PROXY_LIST,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Clapper Extractable Source",
      "Source", "A source element that uses Clapper extractable enhancers to produce data",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
