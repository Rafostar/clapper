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

/**
 * ClapperMediaItem:
 *
 * Represents a media item.
 *
 * A newly created media item must be added to player [class@Clapper.Queue]
 * first in order to be played.
 */

#include "clapper-media-item-private.h"
#include "clapper-timeline-private.h"
#include "clapper-player-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-features-manager-private.h"
#include "clapper-utils-private.h"

#define GST_CAT_DEFAULT clapper_media_item_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperMediaItem
{
  GstObject parent;

  gchar *uri;
  gchar *suburi;

  ClapperTimeline *timeline;

  guint id;
  gchar *title;
  gchar *container_format;
  gdouble duration;

  gchar *cache_uri;

  /* For shuffle */
  gboolean used;
};

enum
{
  PROP_0,
  PROP_ID,
  PROP_URI,
  PROP_SUBURI,
  PROP_CACHE_LOCATION,
  PROP_TITLE,
  PROP_CONTAINER_FORMAT,
  PROP_DURATION,
  PROP_TIMELINE,
  PROP_LAST
};

#define parent_class clapper_media_item_parent_class
G_DEFINE_TYPE (ClapperMediaItem, clapper_media_item, GST_TYPE_OBJECT);

static guint _item_id = 0;
static GMutex id_lock;
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * clapper_media_item_new:
 * @uri: a media URI
 *
 * Creates new #ClapperMediaItem from URI.
 *
 * Use one of the URI protocols supported by plugins in #GStreamer
 * installation. For local files you can use either "file" protocol
 * or [ctor@Clapper.MediaItem.new_from_file] method.
 *
 * It is considered a programmer error trying to create new media item from
 * invalid URI. If URI is valid, but unsupported by installed plugins on user
 * system, [class@Clapper.Player] will emit a [signal@Clapper.Player::missing-plugin]
 * signal upon playback.
 *
 * Returns: (transfer full): a new #ClapperMediaItem.
 */
ClapperMediaItem *
clapper_media_item_new (const gchar *uri)
{
  ClapperMediaItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  item = g_object_new (CLAPPER_TYPE_MEDIA_ITEM, "uri", uri, NULL);
  gst_object_ref_sink (item);

  g_mutex_lock (&id_lock);
  item->id = _item_id;
  _item_id++;
  g_mutex_unlock (&id_lock);

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
 * Same as [ctor@Clapper.MediaItem.new], but uses a [iface@Gio.File]
 * for convenience in some situations instead of an URI.
 *
 * Returns: (transfer full): a new #ClapperMediaItem.
 */
ClapperMediaItem *
clapper_media_item_new_from_file (GFile *file)
{
  ClapperMediaItem *item;
  gchar *uri;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uri = clapper_utils_uri_from_file (file);
  item = clapper_media_item_new (uri);

  g_free (uri);

  return item;
}

/**
 * clapper_media_item_new_cached:
 * @uri: a media URI
 * @location: (type filename) (nullable): a path to downloaded file
 *
 * Same as [ctor@Clapper.MediaItem.new], but allows to provide
 * a location of a cache file where particular media at @uri
 * is supposed to be found.
 *
 * File at @location existence will be checked upon starting playback
 * of created item. If cache file is not found, media item @uri will be
 * used as fallback. In this case when [property@Clapper.Player:download-enabled]
 * is set to %TRUE, item will be downloaded and cached again if possible.
 *
 * Returns: (transfer full): a new #ClapperMediaItem.
 *
 * Since: 0.8
 */
ClapperMediaItem *
clapper_media_item_new_cached (const gchar *uri, const gchar *location)
{
  ClapperMediaItem *item = clapper_media_item_new (uri);

  if (location && G_LIKELY (item != NULL))
    clapper_media_item_set_cache_location (item, location);

  return item;
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

/**
 * clapper_media_item_set_suburi:
 * @item: a #ClapperMediaItem
 * @suburi: an additional URI
 *
 * Set the additional URI of #ClapperMediaItem.
 *
 * This is typically used to add an external subtitles URI to the @item.
 */
void
clapper_media_item_set_suburi (ClapperMediaItem *self, const gchar *suburi)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->suburi, suburi);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));

    if (player) {
      clapper_app_bus_post_prop_notify (player->app_bus,
          GST_OBJECT_CAST (self), param_specs[PROP_SUBURI]);
      clapper_playbin_bus_post_item_suburi_change (player->bus, self);

      gst_object_unref (player);
    }
  }
}

/**
 * clapper_media_item_get_suburi:
 * @item: a #ClapperMediaItem
 *
 * Get the additional URI of #ClapperMediaItem.
 *
 * Returns: (transfer full) (nullable): an additional URI of #ClapperMediaItem.
 */
gchar *
clapper_media_item_get_suburi (ClapperMediaItem *self)
{
  gchar *suburi;

  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  suburi = g_strdup (self->suburi);
  GST_OBJECT_UNLOCK (self);

  return suburi;
}

static gboolean
clapper_media_item_take_title (ClapperMediaItem *self, gchar *title,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = g_strcmp0 (self->title, title) != 0)) {
    g_free (self->title);
    self->title = title;
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_TITLE]);
  else
    g_free (title);

  return changed;
}

/**
 * clapper_media_item_get_title:
 * @item: a #ClapperMediaItem
 *
 * Get media item title.
 *
 * The title can be either text detected by media discovery once it
 * completes. Otherwise whenever possible this will try to return a title
 * extracted from media URI e.g. basename without extension for local files.
 *
 * Returns: (transfer full) (nullable): media title.
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

static gboolean
clapper_media_item_take_container_format (ClapperMediaItem *self, gchar *container_format,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = g_strcmp0 (self->container_format, container_format) != 0)) {
    g_free (self->container_format);
    self->container_format = container_format;
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_CONTAINER_FORMAT]);
  else
    g_free (container_format);

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
clapper_media_item_set_duration (ClapperMediaItem *self, gdouble duration,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->duration, duration, FLT_EPSILON)))
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
gdouble
clapper_media_item_get_duration (ClapperMediaItem *self)
{
  gdouble duration;

  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), 0);

  GST_OBJECT_LOCK (self);
  duration = self->duration;
  GST_OBJECT_UNLOCK (self);

  return duration;
}

/**
 * clapper_media_item_get_timeline:
 * @item: a #ClapperMediaItem
 *
 * Get the [class@Clapper.Timeline] assosiated with @item.
 *
 * Returns: (transfer none): a #ClapperTimeline of item.
 */
ClapperTimeline *
clapper_media_item_get_timeline (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  return self->timeline;
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

  if (scope == GST_TAG_SCOPE_GLOBAL) {
    gboolean changed = clapper_media_item_update_from_container_tags (self, tags, player->app_bus);

    if (changed) {
      ClapperFeaturesManager *features_manager;

      if ((features_manager = clapper_player_get_features_manager (player)))
        clapper_features_manager_trigger_item_updated (features_manager, self);
    }
  }
}

void
clapper_media_item_update_from_discoverer_info (ClapperMediaItem *self, GstDiscovererInfo *info)
{
  ClapperPlayer *player;
  GstDiscovererStreamInfo *sinfo;
  GstClockTime duration;
  gdouble val_dbl;
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

  val_dbl = (gdouble) duration / GST_SECOND;
  changed |= clapper_media_item_set_duration (self, val_dbl, player->app_bus);

  if (changed) {
    ClapperFeaturesManager *features_manager;

    if ((features_manager = clapper_player_get_features_manager (player)))
      clapper_features_manager_trigger_item_updated (features_manager, self);
  }

  gst_object_unref (player);
}

/* XXX: Must be set from player thread or upon construction */
void
clapper_media_item_set_cache_location (ClapperMediaItem *self, const gchar *location)
{
  g_clear_pointer (&self->cache_uri, g_free);

  if (location)
    self->cache_uri = g_filename_to_uri (location, NULL, NULL);

  GST_DEBUG_OBJECT (self, "Set cache URI: \"%s\"",
      GST_STR_NULL (self->cache_uri));
}

/* XXX: Can only be read from player thread.
 * Returns cache URI if available, item URI otherwise. */
inline const gchar *
clapper_media_item_get_playback_uri (ClapperMediaItem *self)
{
  if (self->cache_uri) {
    GFile *file = g_file_new_for_uri (self->cache_uri);
    gboolean exists;

    /* It is an app error if it removes files in non-stopped state,
     * and this function is only called when starting playback */
    exists = g_file_query_exists (file, NULL);
    g_object_unref (file);

    if (exists)
      return self->cache_uri;

    /* Do not test file existence next time */
    clapper_media_item_set_cache_location (self, NULL);
  }

  return self->uri;
}

void
clapper_media_item_set_used (ClapperMediaItem *self, gboolean used)
{
  GST_OBJECT_LOCK (self);
  self->used = used;
  GST_OBJECT_UNLOCK (self);
}

gboolean
clapper_media_item_get_used (ClapperMediaItem *self)
{
  gboolean used;

  GST_OBJECT_LOCK (self);
  used = self->used;
  GST_OBJECT_UNLOCK (self);

  return used;
}

static void
clapper_media_item_init (ClapperMediaItem *self)
{
  self->timeline = clapper_timeline_new ();
  gst_object_set_parent (GST_OBJECT_CAST (self->timeline), GST_OBJECT_CAST (self));
}

static void
clapper_media_item_constructed (GObject *object)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  /* Be safe when someone incorrectly constructs item without URI */
  if (G_UNLIKELY (self->uri == NULL))
    self->uri = g_strdup ("file://");

  self->title = clapper_utils_title_from_uri (self->uri);

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

  gst_object_unparent (GST_OBJECT_CAST (self->timeline));
  gst_object_unref (self->timeline);

  g_free (self->cache_uri);

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
    case PROP_SUBURI:
      clapper_media_item_set_suburi (self, g_value_get_string (value));
      break;
    case PROP_CACHE_LOCATION:
      clapper_media_item_set_cache_location (self, g_value_get_string (value));
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
    case PROP_SUBURI:
      g_value_take_string (value, clapper_media_item_get_suburi (self));
      break;
    case PROP_TITLE:
      g_value_take_string (value, clapper_media_item_get_title (self));
      break;
    case PROP_CONTAINER_FORMAT:
      g_value_take_string (value, clapper_media_item_get_container_format (self));
      break;
    case PROP_DURATION:
      g_value_set_double (value, clapper_media_item_get_duration (self));
      break;
    case PROP_TIMELINE:
      g_value_set_object (value, clapper_media_item_get_timeline (self));
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
   * ClapperMediaItem:suburi:
   *
   * Media additional URI.
   */
  param_specs[PROP_SUBURI] = g_param_spec_string ("suburi",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:cache-location:
   *
   * Media downloaded cache file location.
   *
   * Since: 0.8
   */
  param_specs[PROP_CACHE_LOCATION] = g_param_spec_string ("cache-location",
      NULL, NULL, NULL,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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
  param_specs[PROP_DURATION] = g_param_spec_double ("duration",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:timeline:
   *
   * Media timeline.
   */
  param_specs[PROP_TIMELINE] = g_param_spec_object ("timeline",
      NULL, NULL, CLAPPER_TYPE_TIMELINE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
