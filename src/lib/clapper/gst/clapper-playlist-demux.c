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
#include "../clapper-playlistable.h"

#define CLAPPER_PLAYLIST_MEDIA_TYPE "text/clapper-playlist"

#define GST_CAT_DEFAULT clapper_playlist_demux_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperPlaylistDemux
{
  ClapperUriBaseDemux parent;

  ClapperEnhancerDirector *director;
  GCancellable *cancellable;

  ClapperEnhancerProxyList *enhancer_proxies;
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CLAPPER_PLAYLIST_MEDIA_TYPE));

static GstStaticCaps type_find_caps = GST_STATIC_CAPS (CLAPPER_PLAYLIST_MEDIA_TYPE);
#define TYPE_FIND_CAPS (gst_static_caps_get(&type_find_caps))

static void
clapper_playlist_type_find (GstTypeFind *tf, ClapperEnhancerProxy *proxy)
{
  g_print ("DOING TYPEFIND: %s\n", clapper_enhancer_proxy_get_module_name (proxy));
}

static gboolean
type_find_register (GstPlugin *plugin)
{
  ClapperEnhancerProxyList *global_proxies = clapper_get_global_enhancer_proxies ();
  guint i, n_proxies = clapper_enhancer_proxy_list_get_n_proxies (global_proxies);
  gboolean res = FALSE;

  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (global_proxies, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_PLAYLISTABLE)) {
      res |= gst_type_find_register (plugin, clapper_enhancer_proxy_get_module_name (proxy),
          GST_RANK_MARGINAL + 1, (GstTypeFindFunction) clapper_playlist_type_find,
          NULL, TYPE_FIND_CAPS, proxy, NULL);
    }
  }

  return res;
}

#define parent_class clapper_playlist_demux_parent_class
G_DEFINE_TYPE (ClapperPlaylistDemux, clapper_playlist_demux, CLAPPER_TYPE_URI_BASE_DEMUX);
GST_TYPE_FIND_REGISTER_DEFINE_CUSTOM (clapperplaylistdemux, type_find_register);
GST_ELEMENT_REGISTER_DEFINE (clapperplaylistdemux, "clapperplaylistdemux",
    512, CLAPPER_TYPE_PLAYLIST_DEMUX);

static GList *
_filter_playlistables (ClapperPlaylistDemux *self, ClapperEnhancerProxyList *proxies)
{
  GList *sublist = NULL;
  ClapperEnhancerProxy *proxy;

  /* FIXME: Use name from CAPS */
  if ((proxy = clapper_enhancer_proxy_list_get_by_module (proxies, "clapper-playlist-m3u")))
    sublist = g_list_append (sublist, proxy);

  return sublist;
}

static gboolean
clapper_playlist_demux_process_buffer (ClapperUriBaseDemux *uri_bd, GstBuffer *buffer)
{
  ClapperPlaylistDemux *self = CLAPPER_PLAYLIST_DEMUX_CAST (uri_bd);
  ClapperEnhancerProxyList *proxies;
  GList *filtered_proxies;
  GCancellable *cancellable;
  GListStore *playlist;
  gboolean success = FALSE;

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

  cancellable = g_object_ref (self->cancellable);

  GST_OBJECT_UNLOCK (self);

  filtered_proxies = _filter_playlistables (self, proxies);
  gst_object_unref (proxies);

  playlist = clapper_enhancer_director_parse (self->director,
      filtered_proxies, buffer, cancellable, &error);

  g_clear_list (&filtered_proxies, gst_object_unref);
  g_object_unref (cancellable);

  if (!playlist)
    return FALSE;

  /* TODO: Handle playlist */

  return TRUE;
}

static void
clapper_playlist_demux_init (ClapperPlaylistDemux *self)
{
  self->cancellable = g_cancellable_new ();
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

  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_playlist_demux_class_init (ClapperPlaylistDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  ClapperUriBaseDemuxClass *clapperuribd_class = (ClapperUriBaseDemuxClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperplaylistdemux", 0,
      "Clapper Playlist Demux");

  gobject_class->dispose = clapper_playlist_demux_dispose;
  gobject_class->finalize = clapper_playlist_demux_finalize;

  clapperuribd_class->process_buffer = clapper_playlist_demux_process_buffer;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class, "Clapper Playlist Demux",
      "Demuxer", "A custom demuxer for playlists",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
