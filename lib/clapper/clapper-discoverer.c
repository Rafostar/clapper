/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

/**
 * SECTION:clapper-discoverer
 * @title: ClapperDiscoverer
 * @short_description: Media information discovery
 */

#include <gst/pbutils/pbutils.h>

#include "clapper-discoverer.h"
#include "clapper-media-item-private.h"
#include "clapper-app-bus-private.h"

#define GST_CAT_DEFAULT clapper_discoverer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperDiscoverer
{
  GstObject parent;

  GThreadPool *pool;
  GPtrArray *discoverers;

  ClapperAppBus *app_bus;
};

#define parent_class clapper_discoverer_parent_class
G_DEFINE_TYPE (ClapperDiscoverer, clapper_discoverer, GST_TYPE_OBJECT);

typedef struct
{
  GstDiscoverer *discoverer;
  gboolean in_use;
} ClapperDiscovererData;

static ClapperDiscovererData *
clapper_discoverer_make_discoverer_data (void)
{
  GstDiscoverer *discoverer;
  ClapperDiscovererData *data;
  GError *error = NULL;

  discoverer = gst_discoverer_new (10 * GST_SECOND, &error);

  if (G_UNLIKELY (error != NULL)) {
    GST_ERROR ("GstDiscoverer could not be created, reason: %s", error->message);
    g_error_free (error);

    return NULL;
  }

  GST_TRACE ("Created new GstDiscoverer: %" GST_PTR_FORMAT, discoverer);

  /* FIXME: Caching in GStreamer is broken. Does not save container tags, such as media title.
   * Disable it until completely fixed upsteam. Once fixed change to %TRUE. */
  g_object_set (discoverer, "use-cache", FALSE, NULL);

  data = g_new0 (ClapperDiscovererData, 1);
  data->discoverer = discoverer;

  return data;
}

static void
clapper_discoverer_data_free (ClapperDiscovererData *data)
{
  g_clear_object (&data->discoverer);
  g_free (data);
}

static ClapperDiscovererData *
clapper_discoverer_get_unused_discoverer_data (ClapperDiscoverer *self)
{
  ClapperDiscovererData *data = NULL;
  gint i;

  GST_OBJECT_LOCK (self);

  for (i = 0; i < self->discoverers->len; ++i) {
    ClapperDiscovererData *tmp_data = g_ptr_array_index (self->discoverers, i);

    if (!tmp_data->in_use) {
      data = tmp_data;
      break;
    }
  }

  if (!data) {
    gint max_threads = g_thread_pool_get_max_threads (self->pool);

    /* Something is wrong if all discoverers are in use
     * and we somehow reached pool threads limit */
    if (G_LIKELY (max_threads > self->discoverers->len)) {
      if ((data = clapper_discoverer_make_discoverer_data ()))
        g_ptr_array_add (self->discoverers, data);
    }
  }

  if (G_LIKELY (data != NULL))
    data->in_use = TRUE;

  GST_OBJECT_UNLOCK (self);

  return data;
}

static void
clapper_discoverer_data_mark_unused (ClapperDiscoverer *self, ClapperDiscovererData *data)
{
  GST_OBJECT_LOCK (self);
  data->in_use = FALSE;
  GST_OBJECT_UNLOCK (self);
}
/*
static gboolean
_extract_tag_from_stream_list (GList *streams, const gchar *tag, gchar **string)
{
  GList *l;
  gboolean found = FALSE;

  for (l = streams; l; l = l->next) {
    GstDiscovererStreamInfo *sinfo = (GstDiscovererStreamInfo *) l->data;
    const GstTagList *tags;

    if ((tags = gst_discoverer_stream_info_get_tags (sinfo)))
      if ((found = gst_tag_list_get_string (tags, tag, string)))
        break;
  }

  return found;
}

static gchar *
clapper_discoverer_info_obtain_title (GstDiscovererInfo *info)
{
  GstDiscovererStreamInfo *sinfo;
  gchar *title = NULL;
*/
  /* First check container tags, in most cases title tag
   * should be there if it exists */
/*
  for (sinfo = gst_discoverer_info_get_stream_info (info);
      sinfo != NULL;
      sinfo = gst_discoverer_stream_info_get_next (sinfo)) {
    gboolean found = FALSE;

    if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
      GstDiscovererContainerInfo *cinfo = (GstDiscovererContainerInfo *) sinfo;
      const GstTagList *tags;

      if ((tags = gst_discoverer_container_info_get_tags (cinfo)))
        found = gst_tag_list_get_string (tags, "title", &title);
    }
    gst_discoverer_stream_info_unref (sinfo);

    if (found)
      break;
  }
*/
  /* When no title in container tags, then check video streams. In case there
   * are none, also check audio streams to extract title from audio files */
/*
  if (!title) {
    GList *streams = gst_discoverer_info_get_video_streams (info);
*/
    /* Replace video streams with audio streams if list is empty */
/*
    if (g_list_length (streams) == 0) {
      gst_discoverer_stream_info_list_free (streams);
      streams = gst_discoverer_info_get_audio_streams (info);
    }

    _extract_tag_from_stream_list (streams, "title", &title);
    gst_discoverer_stream_info_list_free (streams);
  }

  return title;
}
*/

static inline void
_update_media_item (ClapperDiscoverer *self, ClapperMediaItem *item, GstDiscovererInfo *info)
{
  GstDiscovererStreamInfo *sinfo;

  for (sinfo = gst_discoverer_info_get_stream_info (info);
      sinfo != NULL;
      sinfo = gst_discoverer_stream_info_get_next (sinfo)) {
    const GstTagList *tags;

    if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
      GstDiscovererContainerInfo *cinfo = (GstDiscovererContainerInfo *) sinfo;

      if ((tags = gst_discoverer_container_info_get_tags (cinfo)))
        clapper_media_item_update_from_container_tags (item, tags, self->app_bus);
    }
    gst_discoverer_stream_info_unref (sinfo);
  }
}

static void
_thread_func (ClapperMediaItem *item, ClapperDiscoverer *self)
{
  ClapperDiscovererData *data;
  GMainContext *context;
  gboolean success = FALSE;

  /* We hold a sole reference to this media item, which
   * means that running media discovery on it is pointless */
  if (GST_OBJECT_REFCOUNT_VALUE (item) == 1)
    goto finish;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  data = clapper_discoverer_get_unused_discoverer_data (self);

  if (G_LIKELY (data != NULL)) {
    GstDiscovererInfo *info;
    GError *error = NULL;
    const gchar *uri = clapper_media_item_get_uri (item);

    GST_DEBUG_OBJECT (self, "Running discovery of %" GST_PTR_FORMAT
        "(%s) with: %" GST_PTR_FORMAT, item, uri, data->discoverer);

    info = gst_discoverer_discover_uri (data->discoverer, uri, &error);

    if (!error) {
      GST_DEBUG_OBJECT (self, "Finished discovery of %" GST_PTR_FORMAT
          "(%s) with: %" GST_PTR_FORMAT, item, uri, data->discoverer);

      _update_media_item (self, item, info);
      success = TRUE;

      gst_discoverer_info_unref (info);
    } else {
      GST_ERROR_OBJECT (self, "Discovery of %" GST_PTR_FORMAT
          "(%s) failed, reason: %s", item, uri, error->message);
      g_error_free (error);

      /* FIXME: Maybe we should retry after a while? */
    }

    clapper_discoverer_data_mark_unused (self, data);
  }

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);

finish:
  GST_OBJECT_LOCK (item);
  item->pending_discovery = FALSE;
  item->discovered = success;
  GST_OBJECT_UNLOCK (item);

  /* We push to pool a new reference, so must unref */
  gst_object_unref (item);
}

static void
clapper_discoverer_init (ClapperDiscoverer *self)
{
  /* Note that not only discoverer uses a new thread, but also
   * underlying pipeline will use a few. We need quite a few threads
   * to run multiple discoverers smoothly without affecting playback */
  gint max_threads = (g_get_num_processors () >= 10) ? 2 : 1;

  self->pool = g_thread_pool_new_full ((GFunc) _thread_func, self,
      (GDestroyNotify) gst_object_unref, max_threads, FALSE, NULL);
  self->discoverers = g_ptr_array_new_with_free_func (
      (GDestroyNotify) clapper_discoverer_data_free);
}

static void
clapper_discoverer_constructed (GObject *object)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (object);

  self->app_bus = clapper_app_bus_new ();

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_discoverer_dispose (GObject *object)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (object);

  GST_OBJECT_LOCK (self);

  if (self->pool) {
    g_thread_pool_free (self->pool, TRUE, TRUE);
    self->pool = NULL;
  }
  if (self->app_bus) {
    gst_bus_set_flushing (GST_BUS_CAST (self->app_bus), TRUE);
    gst_bus_remove_watch (GST_BUS_CAST (self->app_bus));
    gst_clear_object (&self->app_bus);
  }

  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_discoverer_finalize (GObject *object)
{
  ClapperDiscoverer *self = CLAPPER_DISCOVERER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_ptr_array_unref (self->discoverers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * clapper_discoverer_new:
 *
 * Returns: (transfer full): a new #ClapperDiscoverer instance.
 */
ClapperDiscoverer *
clapper_discoverer_new (void)
{
  ClapperDiscoverer *discoverer;

  discoverer = g_object_new (CLAPPER_TYPE_DISCOVERER, NULL);
  gst_object_ref_sink (discoverer);

  return discoverer;
}

/**
 * clapper_discoverer_discover_item:
 * @discoverer: a #ClapperDiscoverer
 * @item: a #ClapperMediaItem
 *
 * Queues media item for discovery.
 *
 * If media item is already queued for discovery or was
 * discovered earlier, this funcion will simply return,
 * so it is safe to call this multiple times when for
 * e.g. user is scrolling through playback queue.
 */
void
clapper_discoverer_discover_item (ClapperDiscoverer *self, ClapperMediaItem *item)
{
  gboolean discover;

  g_return_if_fail (CLAPPER_IS_DISCOVERER (self));
  g_return_if_fail (CLAPPER_IS_MEDIA_ITEM (item));

  GST_OBJECT_LOCK (item);
  if ((discover = (!item->discovered && !item->pending_discovery)))
    item->pending_discovery = TRUE;
  GST_OBJECT_UNLOCK (item);

  /* Run async item discovery if it was not discovered already */
  if (discover) {
    g_thread_pool_push (self->pool, gst_object_ref (item), NULL);
    GST_INFO_OBJECT (self, "Queued discovery of %" GST_PTR_FORMAT, item);
  }
}

static void
clapper_discoverer_class_init (ClapperDiscovererClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperdiscoverer", 0,
      "Clapper Discoverer");

  gobject_class->constructed = clapper_discoverer_constructed;
  gobject_class->dispose = clapper_discoverer_dispose;
  gobject_class->finalize = clapper_discoverer_finalize;
}
