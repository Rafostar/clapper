/* Clapper Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "clapper-playlist-demux-private.h"
#include "clapper-enhancer-director-private.h"

#include "../clapper-basic-functions.h"
#include "../clapper-enhancer-proxy.h"
#include "../clapper-enhancer-proxy-list.h"
#include "../clapper-media-item.h"
#include "../clapper-playlistable.h"

#define CLAPPER_PLAYLIST_MEDIA_TYPE "application/clapper-playlist"
#define DATA_CHUNK_SIZE 4096

#define GST_CAT_DEFAULT clapper_playlist_demux_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperPlaylistDemux
{
  ClapperUriBaseDemux parent;

  GstCaps *caps;

  ClapperEnhancerDirector *director;
  ClapperEnhancerProxyList *enhancer_proxies;
};

enum
{
  PROP_0,
  PROP_ENHANCER_PROXIES,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CLAPPER_PLAYLIST_MEDIA_TYPE));

static GstStaticCaps clapper_playlist_caps = GST_STATIC_CAPS (CLAPPER_PLAYLIST_MEDIA_TYPE);

static void
clapper_playlist_type_find (GstTypeFind *tf, ClapperEnhancerProxy *proxy)
{
  const gchar *prefix, *contains, *regex, *module_name;

  if (!clapper_enhancer_proxy_get_target_creation_allowed (proxy))
    return;

  if ((prefix = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Prefix"))) {
    size_t len = strlen (prefix);
    const gchar *data = (const gchar *) gst_type_find_peek (tf, 0, (guint) len);

    if (!data || memcmp (data, prefix, len) != 0)
      return;
  }

  contains = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Contains");
  regex = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Regex");

  if (contains || regex) {
    const gchar *data;
    guint data_size = DATA_CHUNK_SIZE;

    if (!(data = (const gchar *) gst_type_find_peek (tf, 0, data_size))) {
      guint64 data_len = gst_type_find_get_length (tf);

      if (G_LIKELY (data_len < DATA_CHUNK_SIZE)) { // likely, since whole chunk read failed
        data_size = (guint) data_len;
        data = (const gchar *) gst_type_find_peek (tf, 0, data_size);
      }
    }

    if (G_UNLIKELY (data == NULL)) {
      GST_ERROR ("Could not read data!");
      return;
    }

    if (contains && !g_strstr_len (data, data_size, contains))
      return;

    if (regex) {
      GRegex *reg;
      GError *error = NULL;
      gboolean matched;

      if (!(reg = g_regex_new (regex, 0, 0, &error))) {
        GST_ERROR ("Could not compile regex, reason: %s", error->message);
        g_error_free (error);

        return;
      }

      matched = g_regex_match_full (reg, data, (gssize) data_size, 0, 0, NULL, NULL);
      g_regex_unref (reg);

      if (!matched)
        return;
    }
  }

  module_name = clapper_enhancer_proxy_get_module_name (proxy);
  GST_INFO ("Suggesting likely type: " CLAPPER_PLAYLIST_MEDIA_TYPE
      ", enhancer: %s", module_name);

  gst_type_find_suggest_simple (tf, GST_TYPE_FIND_LIKELY,
      CLAPPER_PLAYLIST_MEDIA_TYPE, "enhancer", G_TYPE_STRING, module_name, NULL);
}

static gboolean
type_find_register (GstPlugin *plugin)
{
  ClapperEnhancerProxyList *global_proxies = clapper_get_global_enhancer_proxies ();
  GstCaps *reg_caps = NULL;
  guint i, n_proxies = clapper_enhancer_proxy_list_get_n_proxies (global_proxies);
  gboolean res = FALSE;

  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (global_proxies, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_PLAYLISTABLE)
        && (clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Prefix")
        || clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Contains")
        || clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Regex"))) {
      if (!reg_caps)
        reg_caps = gst_static_caps_get (&clapper_playlist_caps);

      res |= gst_type_find_register (plugin, clapper_enhancer_proxy_get_module_name (proxy),
          GST_RANK_MARGINAL + 1, (GstTypeFindFunction) clapper_playlist_type_find,
          NULL, reg_caps, proxy, NULL);
    }
  }

  gst_clear_caps (&reg_caps);

  return res;
}

#define parent_class clapper_playlist_demux_parent_class
G_DEFINE_TYPE (ClapperPlaylistDemux, clapper_playlist_demux, CLAPPER_TYPE_URI_BASE_DEMUX);
GST_TYPE_FIND_REGISTER_DEFINE_CUSTOM (clapperplaylistdemux, type_find_register);
GST_ELEMENT_REGISTER_DEFINE (clapperplaylistdemux, "clapperplaylistdemux",
    512, CLAPPER_TYPE_PLAYLIST_DEMUX);

static void
clapper_playlist_demux_handle_caps (ClapperUriBaseDemux *uri_bd, GstCaps *caps)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (uri_bd);

  gst_caps_replace (&self->caps, caps);
  GST_DEBUG_OBJECT (self, "Set caps: %" GST_PTR_FORMAT, caps);
}

static GList *
_filter_playlistables (ClapperPlaylistDemux *self, GstCaps *caps, ClapperEnhancerProxyList *proxies)
{
  GList *sublist = NULL;
  GstStructure *structure;
  ClapperEnhancerProxy *proxy;

  if (caps && (structure = gst_caps_get_structure (self->caps, 0))) {
    const gchar *module_name = gst_structure_get_string (structure, "enhancer");

    if (module_name && (proxy = clapper_enhancer_proxy_list_get_proxy_by_module (proxies, module_name)))
      sublist = g_list_append (sublist, proxy);
  }

  return sublist;
}

static inline gboolean
_handle_playlist (ClapperPlaylistDemux *self, GListStore *playlist, GCancellable *cancellable)
{
  ClapperMediaItem *item = g_list_model_get_item (G_LIST_MODEL (playlist), 0);
  const gchar *uri;
  gboolean success;

  if (G_UNLIKELY (item == NULL)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("This playlist appears to be empty"), (NULL));
    return FALSE;
  }

  uri = clapper_media_item_get_uri (item);
  success = clapper_uri_base_demux_set_uri (CLAPPER_URI_BASE_DEMUX_CAST (self), uri, NULL);
  gst_object_unref (item);

  if (G_UNLIKELY (!success)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Resolved item URI was rejected"), (NULL));
    return FALSE;
  }

  if (!g_cancellable_is_cancelled (cancellable)) {
    GstStructure *structure = gst_structure_new ("ClapperPlaylistParsed",
        "playlist", G_TYPE_LIST_STORE, playlist, NULL);

    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_message_new_element (GST_OBJECT_CAST (self), structure));
  }

  return TRUE;
}

static gboolean
clapper_playlist_demux_process_buffer (ClapperUriBaseDemux *uri_bd,
    GstBuffer *buffer, GCancellable *cancellable)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (uri_bd);
  ClapperEnhancerProxyList *proxies;
  GList *filtered_proxies;
  GstPad *sink_pad;
  GstQuery *query;
  GUri *uri = NULL;
  GListStore *playlist;
  GError *error = NULL;
  gboolean handled;

  sink_pad = gst_element_get_static_pad (GST_ELEMENT_CAST (self), "sink");
  query = gst_query_new_uri ();

  if (gst_pad_peer_query (sink_pad, query)) {
    gchar *query_uri;

    gst_query_parse_uri (query, &query_uri);
    GST_DEBUG_OBJECT (self, "Source URI: %s", query_uri);

    if (query_uri) {
      uri = g_uri_parse (query_uri, G_URI_FLAGS_ENCODED, NULL);
      g_free (query_uri);
    }
  }

  gst_query_unref (query);
  gst_object_unref (sink_pad);

  if (G_UNLIKELY (uri == NULL)) {
    GST_ERROR_OBJECT (self, "Could not query source URI");
    return FALSE;
  }

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

  GST_OBJECT_UNLOCK (self);

  filtered_proxies = _filter_playlistables (self, self->caps, proxies);
  gst_object_unref (proxies);

  playlist = clapper_enhancer_director_parse (self->director,
      filtered_proxies, uri, buffer, cancellable, &error);

  g_clear_list (&filtered_proxies, gst_object_unref);
  g_uri_unref (uri);

  if (!playlist) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("%s", error->message), (NULL));
    g_clear_error (&error);

    return FALSE;
  }

  handled = _handle_playlist (self, playlist, cancellable);
  g_object_unref (playlist);

  return handled;
}

static void
clapper_playlist_demux_set_enhancer_proxies (ClapperPlaylistDemux *self,
    ClapperEnhancerProxyList *enhancer_proxies)
{
  GST_OBJECT_LOCK (self);
  gst_object_replace ((GstObject **) &self->enhancer_proxies,
      GST_OBJECT_CAST (enhancer_proxies));
  GST_OBJECT_UNLOCK (self);
}

static void
clapper_playlist_demux_init (ClapperPlaylistDemux *self)
{
}

static void
clapper_playlist_demux_dispose (GObject *object)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (object);

  GST_OBJECT_LOCK (self);
  g_clear_object (&self->director);
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_playlist_demux_finalize (GObject *object)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_caps (&self->caps);
  gst_clear_object (&self->enhancer_proxies);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_playlist_demux_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (object);

  switch (prop_id) {
    case PROP_ENHANCER_PROXIES:
      clapper_playlist_demux_set_enhancer_proxies (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_playlist_demux_class_init (ClapperPlaylistDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  ClapperUriBaseDemuxClass *clapperuribd_class = (ClapperUriBaseDemuxClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperplaylistdemux", 0,
      "Clapper Playlist Demux");

  gobject_class->set_property = clapper_playlist_demux_set_property;
  gobject_class->dispose = clapper_playlist_demux_dispose;
  gobject_class->finalize = clapper_playlist_demux_finalize;

  clapperuribd_class->handle_caps = clapper_playlist_demux_handle_caps;
  clapperuribd_class->process_buffer = clapper_playlist_demux_process_buffer;

  param_specs[PROP_ENHANCER_PROXIES] = g_param_spec_object ("enhancer-proxies",
      NULL, NULL, CLAPPER_TYPE_ENHANCER_PROXY_LIST,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class, "Clapper Playlist Demux",
      "Demuxer", "A custom demuxer for playlists",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
