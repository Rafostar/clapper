/*
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

/**
 * ClapperMediaItem:
 *
 * Represents a media item.
 *
 * A newly created media item must be added to player [class@Clapper.Queue]
 * first in order to be played.
 */

#include "clapper-media-item.h"
#include "clapper-media-item-private.h"
#include "clapper-stream-list-private.h"
#include "clapper-stream-private.h"
#include "clapper-player-private.h"
#include "clapper-features-manager-private.h"
#include "clapper-utils-private.h"

#define GST_CAT_DEFAULT clapper_media_item_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperMediaItem
{
  GstObject parent;

  gchar *uri;

  guint id;
  gchar *title;
  gchar *container_format;
  gfloat duration;

  GstStreamCollection *collection;
  gulong stream_notify_id;

  ClapperStreamList *video_streams;
  ClapperStreamList *audio_streams;
  ClapperStreamList *subtitle_streams;
};

enum
{
  PROP_0,
  PROP_ID,
  PROP_URI,
  PROP_TITLE,
  PROP_CONTAINER_FORMAT,
  PROP_DURATION,
  PROP_VIDEO_STREAMS,
  PROP_AUDIO_STREAMS,
  PROP_SUBTITLE_STREAMS,
  PROP_LAST
};

#define parent_class clapper_media_item_parent_class
G_DEFINE_TYPE (ClapperMediaItem, clapper_media_item, GST_TYPE_OBJECT);

static guint _item_id = 0;
static GMutex id_lock;
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static gchar *
_get_title_from_uri (const gchar *uri)
{
  gchar *proto = gst_uri_get_protocol (uri);
  gchar *title = NULL;

  if (G_UNLIKELY (proto == NULL))
    return NULL;

  if (strcmp (proto, "file") == 0) {
    const gchar *ext = strrchr (uri, '.');

    if (ext && strlen (ext) < 8) {
      gchar *filename = g_filename_from_uri (uri, NULL, NULL);

      if (filename) {
        gchar *base = g_path_get_basename (filename);

        title = g_strndup (base, strlen (base) - strlen (ext));

        g_free (filename);
        g_free (base);
      }
    }
  } else if (strcmp (proto, "dvb") == 0) {
    const gchar *channel = strrchr (uri, '/') + 1;
    title = g_strdup (channel);
  }

  g_free (proto);

  return (title != NULL) ? title : g_strdup (uri);
}

static gboolean
_list_has_tag (const GstTagList *tags, const gchar *tag)
{
  gint i, n_tags = gst_tag_list_n_tags (tags);
  gboolean found = FALSE;

  for (i = 0; i < n_tags; ++i) {
    if ((found = (strcmp (gst_tag_list_nth_tag_name (tags, i), tag) == 0)))
      break;
  }

  return found;
}

static gboolean
clapper_media_item_update_from_container_tags (ClapperMediaItem *self, const GstTagList *tags,
    ClapperAppBus *app_bus)
{
  gchar *string = NULL;
  gboolean changed = FALSE;

  if (gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &string))
    changed |= clapper_media_item_take_container_format (self, string, app_bus);
  if (gst_tag_list_get_string (tags, GST_TAG_TITLE, &string))
    changed |= clapper_media_item_take_title (self, string, app_bus);

  return changed;
}

void
clapper_media_item_update_from_tag_list (ClapperMediaItem *self, const GstTagList *tags,
    ClapperPlayer *player)
{
  GstTagScope scope = gst_tag_list_get_scope (tags);
  gboolean changed = FALSE;

  switch (scope) {
    case GST_TAG_SCOPE_GLOBAL:
      changed |= clapper_media_item_update_from_container_tags (self, tags, player->app_bus);
      break;
    case GST_TAG_SCOPE_STREAM:
      /* TODO */
      GST_FIXME_OBJECT (self, "Handle stream scope tags");
      break;
    default:
      break;
  }

  if (changed) {
    ClapperFeaturesManager *features_manager;

    if ((features_manager = clapper_player_get_features_manager (player)))
      clapper_features_manager_trigger_item_updated (features_manager, self);
  }
}

void
clapper_media_item_update_from_discoverer_info (ClapperMediaItem *self, GstDiscovererInfo *info)
{
  ClapperPlayer *player;
  GstDiscovererStreamInfo *sinfo;
  GstClockTime duration;
  gfloat val_flt;
  gboolean changed = FALSE;

  if (!(player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self))))
    return;

  for (sinfo = gst_discoverer_info_get_stream_info (info);
      sinfo != NULL;
      sinfo = gst_discoverer_stream_info_get_next (sinfo)) {
    const GstTagList *tags;

    if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
      GstDiscovererContainerInfo *cinfo = (GstDiscovererContainerInfo *) sinfo;

      if ((tags = gst_discoverer_container_info_get_tags (cinfo)))
        changed |= clapper_media_item_update_from_container_tags (self, tags, player->app_bus);
    }
    gst_discoverer_stream_info_unref (sinfo);
  }

  duration = gst_discoverer_info_get_duration (info);

  if (G_UNLIKELY (duration == GST_CLOCK_TIME_NONE))
    duration = 0;

  val_flt = (gfloat) duration / GST_SECOND;
  changed |= clapper_media_item_set_duration (self, val_flt, player->app_bus);

  if (changed) {
    ClapperFeaturesManager *features_manager;

    if ((features_manager = clapper_player_get_features_manager (player)))
      clapper_features_manager_trigger_item_updated (features_manager, self);
  }

  gst_object_unref (player);
}

/**
 * clapper_media_item_new:
 * @uri: a media URI
 *
 * Creates new #ClapperMediaItem from URI.
 *
 * Use one of the URI protocols supported by plugins in #GStreamer
 * installation. For local files you can use either "file" protocol
 * or clapper_media_item_new_from_file() method.
 *
 * This method can only fail when no URI is provided. It is considered
 * a programmer error trying to create new media item from invalid URI.
 * If URI is valid, but unsupported by user installed plugins, #ClapperPlayer
 * will emit a #ClapperPlayer::missing-plugin signal upon playback.
 *
 * Returns: (transfer full) (nullable): a new #ClapperMediaItem,
 *   %NULL when no URI provided.
 */
ClapperMediaItem *
clapper_media_item_new (const gchar *uri)
{
  return clapper_media_item_new_take (g_strdup (uri));
}

/**
 * clapper_media_item_new_take: (skip)
 * @uri: (transfer full): a media URI
 *
 * Creates new #ClapperMediaItem from URI.
 *
 * Same as clapper_media_item_new(), but takes ownership of passed URI.
 *
 * Returns: (transfer full) (nullable): a new #ClapperMediaItem,
 *   %NULL when no URI provided.
 */
ClapperMediaItem *
clapper_media_item_new_take (gchar *uri)
{
  ClapperMediaItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  if (G_UNLIKELY (uri == NULL))
    return NULL;

  item = g_object_new (CLAPPER_TYPE_MEDIA_ITEM, NULL);
  item->uri = uri;
  item->title = _get_title_from_uri (uri);

  g_mutex_lock (&id_lock);
  item->id = _item_id;
  _item_id++;
  g_mutex_unlock (&id_lock);

  gst_object_ref_sink (item);

  /* FIXME: Set initial container format from file extension parsing */

  GST_TRACE_OBJECT (item, "New media item, ID: %u, URI: %s, title: %s",
      item->id, item->uri, item->title);

  return item;
}

/**
 * clapper_media_item_new_from_file:
 * @file: a #GFile
 *
 * Creates new #ClapperMediaItem from #GFile.
 *
 * Same as clapper_media_item_new(), but uses a #GFile for convenience
 * in some situations instead of an URI.
 *
 * Returns: (transfer full) (nullable): a new #ClapperMediaItem,
 *   %NULL when no #GFile provided.
 */
ClapperMediaItem *
clapper_media_item_new_from_file (GFile *file)
{
  gchar *uri;
  gsize length;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uri = g_file_get_uri (file);
  length = strlen (uri);

  /* GFile might incorrectly append "/" at the end of an URI,
   * remove it to make it work with GStreamer URI handling */
  if (uri[length - 1] == '/') {
    gchar *fixed_uri;

    /* NULL terminated copy without last character */
    fixed_uri = g_new0 (gchar, length);
    memcpy (fixed_uri, uri, length - 1);

    g_free (uri);
    uri = fixed_uri;
  }

  return clapper_media_item_new_take (uri);
}

/**
 * clapper_media_item_get_id:
 * @item: a #ClapperMediaItem
 *
 * Get the unique ID of #ClapperMediaItem.
 *
 * Returns: an ID of #ClapperMediaItem.
 */
guint
clapper_media_item_get_id (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), G_MAXUINT);

  return self->id;
}

/**
 * clapper_media_item_get_uri:
 * @item: a #ClapperMediaItem
 *
 * Get the URI of #ClapperMediaItem.
 *
 * Returns: an URI of #ClapperMediaItem.
 */
const gchar *
clapper_media_item_get_uri (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  return self->uri;
}

gboolean
clapper_media_item_take_title (ClapperMediaItem *self, gchar *title,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->title, title);
  GST_OBJECT_UNLOCK (self);

  if (changed)
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_TITLE]);

  return changed;
}

/**
 * clapper_media_item_get_title:
 * @item: a #ClapperMediaItem
 *
 * Get media item title for displaying in app UI. This function
 * always returns a media title of some sort for convenience
 * of displaying it in the application UI.
 *
 * The title can be either text detected by media discovery once it
 * completes, file basename for local files or eventually a media URI.
 *
 * Returns: (transfer full): media title.
 */
gchar *
clapper_media_item_get_title (ClapperMediaItem *self)
{
  gchar *title;

  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  title = g_strdup (self->title);
  GST_OBJECT_UNLOCK (self);

  return title;
}

gboolean
clapper_media_item_take_container_format (ClapperMediaItem *self, gchar *container_format,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->container_format, container_format);
  GST_OBJECT_UNLOCK (self);

  if (changed)
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_CONTAINER_FORMAT]);

  return changed;
}

/**
 * clapper_media_item_get_container_format:
 * @item: a #ClapperMediaItem
 *
 * Get media item container format.
 *
 * Returns: (transfer full) (nullable): media container format.
 */
gchar *
clapper_media_item_get_container_format (ClapperMediaItem *self)
{
  gchar *container_format;

  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  container_format = g_strdup (self->container_format);
  GST_OBJECT_UNLOCK (self);

  return container_format;
}

gboolean
clapper_media_item_set_duration (ClapperMediaItem *self, gfloat duration,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = FLT_IS_DIFFERENT (self->duration, duration)))
    self->duration = duration;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_DEBUG_OBJECT (self, "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (duration * GST_SECOND));
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_DURATION]);
  }

  return changed;
}

/**
 * clapper_media_item_get_duration:
 * @item: a #ClapperMediaItem
 *
 * Get media item duration as decimal number in seconds.
 *
 * Returns: media duration.
 */
gfloat
clapper_media_item_get_duration (ClapperMediaItem *self)
{
  gfloat duration;

  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), 0);

  GST_OBJECT_LOCK (self);
  duration = self->duration;
  GST_OBJECT_UNLOCK (self);

  return duration;
}

void
clapper_media_item_take_stream_collection (ClapperMediaItem *self, GstStreamCollection *collection)
{
  GST_OBJECT_LOCK (self);

  if (self->stream_notify_id != 0) {
    g_signal_handler_disconnect (self->collection, self->stream_notify_id);
    self->stream_notify_id = 0;
  }
  gst_clear_object (&self->collection);
  self->collection = collection;

  GST_OBJECT_UNLOCK (self);
}

static gboolean
_collection_has_gst_stream_unlocked (ClapperMediaItem *self, GstStream *gst_stream)
{
  guint i, n_streams = gst_stream_collection_get_size (self->collection);

  for (i = 0; i < n_streams; ++i) {
    if (gst_stream_collection_get_stream (self->collection, i) == gst_stream)
      return TRUE;
  }

  return FALSE;
}

gboolean
clapper_media_item_matches_stream_collection (ClapperMediaItem *self, GstStreamCollection *collection)
{
  guint i, n_streams = gst_stream_collection_get_size (collection);
  gboolean found = FALSE;

  GST_OBJECT_LOCK (self);

  for (i = 0; i < n_streams; ++i) {
    GstStream *gst_stream = gst_stream_collection_get_stream (collection, i);
    GstStreamType stream_type = gst_stream_get_stream_type (gst_stream);

    if ((stream_type & GST_STREAM_TYPE_VIDEO) == 0
        && (stream_type & GST_STREAM_TYPE_AUDIO) == 0
        && (stream_type & GST_STREAM_TYPE_TEXT) == 0)
      continue;

    found = _collection_has_gst_stream_unlocked (self, gst_stream);

    break; // Enough to check just one
  }

  GST_OBJECT_UNLOCK (self);

  return found;
}


static void
_stream_notify_cb (GstStreamCollection *collection,
    GstStream *gst_stream, GParamSpec *prop, ClapperMediaItem *self)
{
  /* FIXME: Use collection "stream-notify" signal to keep updating current media item */
  GST_FIXME_OBJECT (self, "Handle stream notify");
}

void
clapper_media_item_refresh_streams (ClapperMediaItem *self)
{
  guint i, n_streams;
  guint prev_n_vstreams, prev_n_astreams, prev_n_sstreams;
  guint n_vstreams, n_astreams, n_sstreams;

  /* FIXME: Do not remove everything, update with new/same streams instead.
   * What should we do with external subtitle streams? */
  GST_TRACE_OBJECT (self, "Removing all obsolete streams");

  prev_n_vstreams = clapper_stream_list_get_n_streams (self->video_streams);
  prev_n_astreams = clapper_stream_list_get_n_streams (self->audio_streams);
  prev_n_sstreams = clapper_stream_list_get_n_streams (self->subtitle_streams);

  clapper_stream_list_clear (self->video_streams);
  clapper_stream_list_clear (self->audio_streams);
  clapper_stream_list_clear (self->subtitle_streams);

  GST_OBJECT_LOCK (self);

  n_streams = gst_stream_collection_get_size (self->collection);

  for (i = 0; i < n_streams; ++i) {
    GstStream *gst_stream = gst_stream_collection_get_stream (self->collection, i);
    GstStreamType stream_type = gst_stream_get_stream_type (gst_stream);

    GST_LOG_OBJECT (self, "Found %" GST_PTR_FORMAT, gst_stream);

    if (stream_type & GST_STREAM_TYPE_VIDEO) {
      clapper_stream_list_add_stream (self->video_streams, clapper_stream_new (gst_stream));
    } else if (stream_type & GST_STREAM_TYPE_AUDIO) {
      clapper_stream_list_add_stream (self->audio_streams, clapper_stream_new (gst_stream));
    } else if (stream_type & GST_STREAM_TYPE_TEXT) {
      clapper_stream_list_add_stream (self->subtitle_streams, clapper_stream_new (gst_stream));
    } else {
      GST_WARNING_OBJECT (self, "Unhandled stream type: %s",
          gst_stream_type_get_name (stream_type));
    }
  }

  if (G_LIKELY (self->stream_notify_id == 0)) {
    self->stream_notify_id = g_signal_connect (self->collection, "stream-notify",
        G_CALLBACK (_stream_notify_cb), self);
  }

  GST_OBJECT_UNLOCK (self);

  n_vstreams = clapper_stream_list_get_n_streams (self->video_streams);
  n_astreams = clapper_stream_list_get_n_streams (self->audio_streams);
  n_sstreams = clapper_stream_list_get_n_streams (self->subtitle_streams);

  if (prev_n_vstreams > 0 || n_vstreams > 0)
    g_list_model_items_changed (G_LIST_MODEL (self->video_streams), 0, prev_n_vstreams, n_vstreams);
  if (prev_n_astreams > 0 || n_astreams > 0)
    g_list_model_items_changed (G_LIST_MODEL (self->audio_streams), 0, prev_n_astreams, n_astreams);
  if (prev_n_sstreams > 0 || n_sstreams > 0)
    g_list_model_items_changed (G_LIST_MODEL (self->subtitle_streams), 0, prev_n_sstreams, n_sstreams);

  /* FIXME: Is this needed?
  if (n_vstreams > 0)
    clapper_stream_list_select_index (self->video_streams, 0);
  if (n_astreams > 0)
    clapper_stream_list_select_index (self->audio_streams, 0);
  if (n_sstreams > 0)
    clapper_stream_list_select_index (self->subtitle_streams, 0);
  */
}

/**
 * clapper_media_item_get_video_streams:
 * @item: a #ClapperMediaItem
 *
 * Get a list of video streams within media item.
 *
 * Returns: (transfer none): a #ClapperStreamList of video #ClapperStream.
 */
ClapperStreamList *
clapper_media_item_get_video_streams (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  return self->video_streams;
}

/**
 * clapper_media_item_get_audio_streams:
 * @item: a #ClapperMediaItem
 *
 * Get a list of audio streams within media item.
 *
 * Returns: (transfer none): a #ClapperStreamList of audio #ClapperStream.
 */
ClapperStreamList *
clapper_media_item_get_audio_streams (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  return self->audio_streams;
}

/**
 * clapper_media_item_get_subtitle_streams:
 * @item: a #ClapperMediaItem
 *
 * Get a list of subtitle streams within media item.
 *
 * Returns: (transfer none): a #ClapperStreamList of subtitle #ClapperStream.
 */
ClapperStreamList *
clapper_media_item_get_subtitle_streams (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  return self->subtitle_streams;
}

static void
clapper_media_item_init (ClapperMediaItem *self)
{
  self->video_streams = clapper_stream_list_new ();
  gst_object_set_parent (GST_OBJECT (self->video_streams), GST_OBJECT (self));

  self->audio_streams = clapper_stream_list_new ();
  gst_object_set_parent (GST_OBJECT (self->audio_streams), GST_OBJECT (self));

  self->subtitle_streams = clapper_stream_list_new ();
  gst_object_set_parent (GST_OBJECT (self->subtitle_streams), GST_OBJECT (self));
}

static void
clapper_media_item_constructed (GObject *object)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  if (self->uri)
    self->title = _get_title_from_uri (self->uri);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_media_item_finalize (GObject *object)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->uri);
  g_free (self->title);
  g_free (self->container_format);

  if (self->stream_notify_id != 0)
    g_signal_handler_disconnect (self->collection, self->stream_notify_id);

  gst_clear_object (&self->collection);

  gst_object_unparent (GST_OBJECT (self->video_streams));
  gst_object_unref (self->video_streams);

  gst_object_unparent (GST_OBJECT (self->audio_streams));
  gst_object_unref (self->audio_streams);

  gst_object_unparent (GST_OBJECT (self->subtitle_streams));
  gst_object_unref (self->subtitle_streams);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_media_item_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_media_item_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_ID:
      g_value_set_uint (value, clapper_media_item_get_id (self));
      break;
    case PROP_URI:
      g_value_set_string (value, clapper_media_item_get_uri (self));
      break;
    case PROP_TITLE:
      g_value_take_string (value, clapper_media_item_get_title (self));
      break;
    case PROP_CONTAINER_FORMAT:
      g_value_take_string (value, clapper_media_item_get_container_format (self));
      break;
    case PROP_DURATION:
      g_value_set_float (value, clapper_media_item_get_duration (self));
      break;
    case PROP_VIDEO_STREAMS:
      g_value_set_object (value, clapper_media_item_get_video_streams (self));
      break;
    case PROP_AUDIO_STREAMS:
      g_value_set_object (value, clapper_media_item_get_audio_streams (self));
      break;
    case PROP_SUBTITLE_STREAMS:
      g_value_set_object (value, clapper_media_item_get_subtitle_streams (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_media_item_class_init (ClapperMediaItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappermediaitem", 0,
      "Clapper Media Item");

  gobject_class->constructed = clapper_media_item_constructed;
  gobject_class->set_property = clapper_media_item_set_property;
  gobject_class->get_property = clapper_media_item_get_property;
  gobject_class->finalize = clapper_media_item_finalize;

  /**
   * ClapperMediaItem:id:
   *
   * Media Item ID.
   */
  param_specs[PROP_ID] = g_param_spec_uint ("id",
      NULL, NULL, 0, G_MAXUINT, G_MAXUINT,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:uri:
   *
   * Media URI.
   */
  param_specs[PROP_URI] = g_param_spec_string ("uri",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:title:
   *
   * Media title.
   */
  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:container-format:
   *
   * Media container format.
   */
  param_specs[PROP_CONTAINER_FORMAT] = g_param_spec_string ("container-format",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:duration:
   *
   * Media duration as a decimal number in seconds.
   */
  param_specs[PROP_DURATION] = g_param_spec_float ("duration",
      NULL, NULL, 0, G_MAXFLOAT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:video-streams:
   *
   * List of available video streams.
   */
  param_specs[PROP_VIDEO_STREAMS] = g_param_spec_object ("video-streams",
      NULL, NULL, CLAPPER_TYPE_STREAM_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:audio-streams:
   *
   * List of available audio streams.
   */
  param_specs[PROP_AUDIO_STREAMS] = g_param_spec_object ("audio-streams",
      NULL, NULL, CLAPPER_TYPE_STREAM_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:subtitle-streams:
   *
   * List of available subtitle streams.
   */
  param_specs[PROP_SUBTITLE_STREAMS] = g_param_spec_object ("subtitle-streams",
      NULL, NULL, CLAPPER_TYPE_STREAM_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
