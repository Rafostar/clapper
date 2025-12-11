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
#define CLAPPER_CLAPS_MEDIA_TYPE "text/clapper-claps"
#define URI_LIST_MEDIA_TYPE "text/uri-list"
#define DATA_CHUNK_SIZE 4096

#define NTH_REDIRECT_STRUCTURE_NAME "ClapperQueryNthRedirect"
#define NTH_REDIRECT_FIELD "nth-redirect"
#define MAX_REDIRECTS 10

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
    GST_STATIC_CAPS (CLAPPER_PLAYLIST_MEDIA_TYPE ";" CLAPPER_CLAPS_MEDIA_TYPE ";" URI_LIST_MEDIA_TYPE));

static GstStaticCaps clapper_playlist_caps = GST_STATIC_CAPS (CLAPPER_PLAYLIST_MEDIA_TYPE);
static GstStaticCaps clapper_claps_caps = GST_STATIC_CAPS (CLAPPER_CLAPS_MEDIA_TYPE);

static void
clapper_playlist_type_find (GstTypeFind *tf, ClapperEnhancerProxy *proxy)
{
  const gchar *prefix, *contains, *excludes, *regex, *module_name;

  if (!clapper_enhancer_proxy_get_target_creation_allowed (proxy))
    return;

  if ((prefix = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Prefix"))) {
    size_t len = strlen (prefix);
    const gchar *data = (const gchar *) gst_type_find_peek (tf, 0, (guint) len);

    if (!data || memcmp (data, prefix, len) != 0)
      return;
  }

  contains = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Contains");
  excludes = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Excludes");
  regex = clapper_enhancer_proxy_get_extra_data (proxy, "X-Data-Regex");

  if (contains || excludes || regex) {
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

    if ((contains && !g_strstr_len (data, data_size, contains))
        || (excludes && g_strstr_len (data, data_size, excludes)))
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
_is_claps_possible (const guint8 *data, gsize len)
{
  gboolean possible = FALSE;

  /* Linux file path */
  if (len >= 2)
    possible = (data[0] == '/' && g_ascii_isalnum (data[1]));

#ifdef G_OS_WIN32
  /* Windows file path ("C:\..." or "D:/...") */
  if (!possible && len >= 3) {
    possible = (g_ascii_isalpha (data[0]) && data[1] == ':' && (data[2] == '\\' || data[2] == '/'));

    /* Windows UNC path */
    if (!possible)
      possible = (data[0] == '\\' && data[1] == '\\' && g_ascii_isalnum (data[2]));
  }
#endif

  /* Check for URI (at least 3 characters before colon) */
  if (!possible && len > 3) {
    guint i = 0, end = MIN (len, 16);

    while (i < end && g_ascii_isalpha (data[i]))
      ++i;

    if (i >= 3 && i < end)
      possible = (data[i] == ':');
  }

  return possible;
}

/* Finds text file of full file paths. Claps file might also use URIs,
 * but in that case lets GStreamer built-in type finders find that as
 * "text/uri-list" and we will handle it with this element too. */
static void
clapper_claps_type_find (GstTypeFind *tf, gpointer user_data G_GNUC_UNUSED)
{
  const guint8 *data;
  guint64 data_size = 16;

  if (!(data = gst_type_find_peek (tf, 0, data_size))) {
    if ((data_size = gst_type_find_get_length (tf)) > 0)
      data = gst_type_find_peek (tf, 0, data_size);

    if (!data)
      return;
  }

  /* Continue parsing only if start looks like
   * file path, otherwise reject data early */
  if (_is_claps_possible (data, data_size)) {
    guint probability = GST_TYPE_FIND_POSSIBLE;
    data_size = 1024;

    if (!(data = gst_type_find_peek (tf, 0, data_size)))
      if ((data_size = gst_type_find_get_length (tf)) > 0)
        data = gst_type_find_peek (tf, 0, data_size);

    if (data) {
      const guint8 *line_start = data;
      const guint8 *end = data + data_size;
      guint pathlike = 0, total = 0;

      while (line_start < end) {
        const guint8 *newline = memchr (line_start, '\n', end - line_start);
        gsize len = newline ? (newline - line_start) : (end - line_start);

        if (len > 0) {
          ++total;
          if (_is_claps_possible (line_start, len))
            ++pathlike;
        }

        if (!newline)
          break;

        line_start = newline + 1;
      }

      /* Multiple lines and most of them looks like a file path */
      if (total > 1 && pathlike >= MAX (2, total * 3 / 4))
        probability = GST_TYPE_FIND_LIKELY;

      GST_INFO ("Suggesting %s type: " CLAPPER_CLAPS_MEDIA_TYPE,
          (probability >= GST_TYPE_FIND_LIKELY) ? "likely" : "possible");
      gst_type_find_suggest_empty_simple (tf, probability, CLAPPER_CLAPS_MEDIA_TYPE);
    }
  }
}

static gboolean
type_find_register (GstPlugin *plugin)
{
  ClapperEnhancerProxyList *global_proxies = clapper_get_global_enhancer_proxies ();
  GstCaps *reg_caps;
  guint i, n_proxies = clapper_enhancer_proxy_list_get_n_proxies (global_proxies);
  gboolean res;

  reg_caps = gst_static_caps_get (&clapper_claps_caps);
  res = gst_type_find_register (plugin, "clapper-claps",
      GST_RANK_MARGINAL + 1, (GstTypeFindFunction) clapper_claps_type_find,
      "claps", reg_caps, NULL, NULL);
  gst_clear_caps (&reg_caps);

  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (global_proxies, i);

    /* No "X-Data-Excludes" check here, because it can not be
     * used alone to determine whether data is a playlist */
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

static GListStore *
_parse_uri_list (ClapperPlaylistDemux *self, GUri *uri, GstBuffer *buffer,
    GCancellable *cancellable, GError **error)
{
  GListStore *playlist;
  GstMemory *mem;
  GstMapInfo info;
  const gchar *ptr, *end;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!mem || !gst_memory_map (mem, &info, GST_MAP_READ)) {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        "Could not read URI list buffer data");
    return NULL;
  }

  playlist = g_list_store_new (CLAPPER_TYPE_MEDIA_ITEM);
  ptr = (gchar *) info.data;
  end = ptr + info.size;

  while (ptr < end) {
    ClapperMediaItem *item = NULL;
    const gchar *nl = memchr (ptr, '\n', end - ptr);
    gsize len = nl ? nl - ptr : end - ptr;
    gchar *line;

    if (g_cancellable_is_cancelled (cancellable))
      break;

    line = g_strndup (ptr, len);
    GST_DEBUG_OBJECT (self, "Parsing line: %s", line);

    if (gst_uri_is_valid (line)) {
      GST_DEBUG_OBJECT (self, "Found URI: %s", line);
      item = clapper_media_item_new (line);
    } else {
      gchar *base_uri, *res_uri;

      base_uri = g_uri_to_string (uri);
      res_uri = g_uri_resolve_relative (base_uri, line, G_URI_FLAGS_ENCODED, error);
      g_free (base_uri);

      if (res_uri) {
        GST_DEBUG_OBJECT (self, "Resolved URI: %s", res_uri);
        item = clapper_media_item_new (res_uri);
        g_free (res_uri);
      }
    }

    g_free (line);

    if (G_UNLIKELY (*error != NULL)) {
      g_clear_object (&playlist);
      break;
    }

    if (G_LIKELY (item != NULL)) {
      g_list_store_append (playlist, (GObject *) item);
      gst_object_unref (item);
    }

    /* Advance to the next line */
    ptr = nl ? (nl + 1) : end;
  }

  gst_memory_unmap (mem, &info);

  return playlist;
}

static gboolean
_caps_have_media_type (GstCaps *caps, const gchar *media_type)
{
  GstStructure *structure;
  gboolean is_media_type = FALSE;

  if (caps && (structure = gst_caps_get_structure (caps, 0)))
    is_media_type = gst_structure_has_name (structure, media_type);

  return is_media_type;
}

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
  ClapperMediaItem *item;
  GstStructure *structure;
  const gchar *uri;
  gboolean success;

  if (g_cancellable_is_cancelled (cancellable)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Playlist parsing was cancelled"), (NULL));
    return FALSE;
  }

  item = g_list_model_get_item (G_LIST_MODEL (playlist), 0);

  if (G_UNLIKELY (item == NULL)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("This playlist appears to be empty"), (NULL));
    return FALSE;
  }

  /* Post playlist before setting an URI, so it arrives
   * before eventual error (e.g. non-existing file) */
  structure = gst_structure_new ("ClapperPlaylistParsed",
      "playlist", G_TYPE_LIST_STORE, playlist, NULL);
  gst_element_post_message (GST_ELEMENT_CAST (self),
      gst_message_new_element (GST_OBJECT_CAST (self), structure));

  uri = clapper_media_item_get_uri (item);
  success = clapper_uri_base_demux_set_uri (CLAPPER_URI_BASE_DEMUX_CAST (self), uri, NULL);
  gst_object_unref (item);

  if (G_UNLIKELY (!success)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Resolved item URI was rejected"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static void
_query_parse_nth_redirect (GstQuery *query, guint *nth_redirect)
{
  const GstStructure *structure = gst_query_get_structure (query);
  *nth_redirect = g_value_get_uint (gst_structure_get_value (structure, NTH_REDIRECT_FIELD));
}

static void
_query_set_nth_redirect (GstQuery *query, guint nth_redirect)
{
  GstStructure *structure = gst_query_writable_structure (query);
  gst_structure_set (structure, NTH_REDIRECT_FIELD, G_TYPE_UINT, nth_redirect, NULL);
}

static gboolean
clapper_playlist_demux_handle_custom_query (ClapperUriBaseDemux *uri_bd, GstQuery *query)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (uri_bd);
  const GstStructure *structure = gst_query_get_structure (query);

  if (gst_structure_has_name (structure, NTH_REDIRECT_STRUCTURE_NAME)) {
    GstPad *sink_pad;

    GST_LOG_OBJECT (self, "Received custom query: " NTH_REDIRECT_STRUCTURE_NAME);

    sink_pad = gst_element_get_static_pad (GST_ELEMENT_CAST (self), "sink");
    gst_pad_peer_query (sink_pad, query);
    gst_object_unref (sink_pad);

    if (G_LIKELY (gst_query_is_writable (query))) {
      guint nth_redirect = 0;

      _query_parse_nth_redirect (query, &nth_redirect);
      _query_set_nth_redirect (query, ++nth_redirect);
    } else {
      GST_ERROR_OBJECT (self, "Unwritable custom query: " NTH_REDIRECT_STRUCTURE_NAME);
    }

    return TRUE;
  }

  return FALSE;
}

static gboolean
clapper_playlist_demux_process_buffer (ClapperUriBaseDemux *uri_bd,
    GstBuffer *buffer, GCancellable *cancellable)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (uri_bd);
  GstPad *sink_pad;
  GstQuery *query;
  GstStructure *query_structure;
  GUri *uri = NULL;
  GListStore *playlist;
  GError *error = NULL;
  guint nth_redirect = 0;
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

  query_structure = gst_structure_new (NTH_REDIRECT_STRUCTURE_NAME,
      NTH_REDIRECT_FIELD, G_TYPE_UINT, 0, NULL);
  query = gst_query_new_custom (GST_QUERY_CUSTOM, query_structure);

  if (gst_pad_peer_query (sink_pad, query))
    _query_parse_nth_redirect (query, &nth_redirect);

  GST_DEBUG_OBJECT (self, "Current number of redirects: %u", nth_redirect);

  gst_query_unref (query);
  gst_object_unref (sink_pad);

  if (G_UNLIKELY (uri == NULL)) {
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
        ("Could not query source URI"), (NULL));
    return FALSE;
  }
  if (G_UNLIKELY (nth_redirect > MAX_REDIRECTS)) {
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
        ("Too many nested playlists"), (NULL));
    return FALSE;
  }

  if (_caps_have_media_type (self->caps, CLAPPER_PLAYLIST_MEDIA_TYPE)) {
    ClapperEnhancerProxyList *proxies;
    GList *filtered_proxies;

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

    if (!self->director)
      self->director = clapper_enhancer_director_new ();

    filtered_proxies = _filter_playlistables (self, self->caps, proxies);
    gst_object_unref (proxies);

    playlist = clapper_enhancer_director_parse (self->director,
        filtered_proxies, uri, buffer, cancellable, &error);

    g_clear_list (&filtered_proxies, gst_object_unref);
  } else if (_caps_have_media_type (self->caps, URI_LIST_MEDIA_TYPE)
      || _caps_have_media_type (self->caps, CLAPPER_CLAPS_MEDIA_TYPE)) {
    playlist = _parse_uri_list (self, uri, buffer, cancellable, &error);
  } else { // Should never happen
    playlist = NULL;
    error = g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
        "Unsupported media type in caps");
  }

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
  clapperuribd_class->handle_custom_query = clapper_playlist_demux_handle_custom_query;
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
