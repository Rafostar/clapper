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
 * SECTION:clapper-mpris
 * @title: ClapperMpris
 * @short_description: a MPRIS feature
 *
 * An optional MPRIS feature to add to the player.
 *
 * Not every OS supports MPRIS. Use #CLAPPER_HAVE_MPRIS macro
 * to check if Clapper API was compiled with MPRIS support.
 */

#include "clapper-mpris.h"
#include "clapper-mpris-gdbus.h"
#include "clapper-player.h"

#define CLAPPER_MPRIS_DO_WITH_PLAYER(mpris, _player_dst, ...) {                           \
    *_player_dst = CLAPPER_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (mpris))); \
    if (G_LIKELY (*_player_dst != NULL))                                                  \
      __VA_ARGS__                                                                         \
    gst_clear_object (_player_dst); }

#define CLAPPER_MPRIS_SECONDS_TO_USECONDS(seconds) ((gint64) (seconds * G_GINT64_CONSTANT (1000000)))
#define CLAPPER_MPRIS_USECONDS_TO_SECONDS(useconds) ((gfloat) useconds / G_GINT64_CONSTANT (1000000))

/* FIXME: Avoid string comparisons */
#define CLAPPER_MPRIS_COMPARE(a,b) (strcmp (a,b) == 0)
#define CLAPPER_MPRIS_FLT_IS_DIFFERENT(a,b) (!G_APPROX_VALUE (a, b, FLT_EPSILON))

#define CLAPPER_MPRIS_NO_TRACK "/org/mpris/MediaPlayer2/TrackList/NoTrack"

#define CLAPPER_MPRIS_PLAYBACK_STATUS_PLAYING "Playing"
#define CLAPPER_MPRIS_PLAYBACK_STATUS_PAUSED "Paused"
#define CLAPPER_MPRIS_PLAYBACK_STATUS_STOPPED "Stopped"

#define CLAPPER_MPRIS_LOOP_NONE "None"
#define CLAPPER_MPRIS_LOOP_TRACK "Track"
#define CLAPPER_MPRIS_LOOP_PLAYLIST "Playlist"

#define GST_CAT_DEFAULT clapper_mpris_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperMpris
{
  ClapperFeature parent;

  ClapperMprisMediaPlayer2 *base_skeleton;
  ClapperMprisMediaPlayer2Player *player_skeleton;
  ClapperMprisMediaPlayer2TrackList *tracks_skeleton;

  gboolean base_exported;
  gboolean player_exported;
  gboolean tracks_exported;

  guint name_id;
  gchar *mpris_name;

  gfloat last_volume;
  ClapperQueueProgressionMode last_progression;

  gchar *own_name;
  gchar *identity;
  gchar *desktop_entry;
  gchar *art_url;
  gchar *fallback_art_url;
};

enum
{
  PROP_0,
  PROP_OWN_NAME,
  PROP_IDENTITY,
  PROP_DESKTOP_ENTRY,
  PROP_ART_URL,
  PROP_FALLBACK_ART_URL,
  PROP_LAST
};

#define parent_class clapper_mpris_parent_class
G_DEFINE_TYPE (ClapperMpris, clapper_mpris, CLAPPER_TYPE_FEATURE);

static const gchar *const empty_tracklist[] = { NULL, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static gchar *
_mpris_make_track_id (ClapperMpris *self, ClapperMediaItem *item)
{
  gchar *track_id;

  /* MPRIS docs: "Media players may not use any paths starting with /org/mpris
   * unless explicitly allowed by this specification. Such paths are intended to
   * have special meaning, such as /org/mpris/MediaPlayer2/TrackList/NoTrack" */
  GST_OBJECT_LOCK (item);
  track_id = g_strdup_printf ("/org/clapper/%s/%s", self->mpris_name, GST_OBJECT_NAME (item));
  GST_OBJECT_UNLOCK (item);

  GST_LOG_OBJECT (self, "Created track ID: %s", track_id);

  return track_id;
}

static gboolean
_mpris_find_track_id (ClapperMpris *self, ClapperQueue *queue, const gchar *search_id,
    ClapperMediaItem **found_item, guint *index)
{
  ClapperMediaItem *item = NULL;
  guint i = 0;
  gboolean found = FALSE;

  while ((item = clapper_queue_get_item (queue, i))) {
    gchar *track_id;

    track_id = _mpris_make_track_id (self, item);
    found = CLAPPER_MPRIS_COMPARE (search_id, track_id);

    g_free (track_id);

    if (found) {
      if (found_item)
        *found_item = item;
      else
        gst_object_unref (item);

      if (index)
        *index = i;

      break;
    }

    gst_object_unref (item);
    i++;
  }

  return found;
}

/*
 * @track_id: (inout) (nullable): If input is set it will not be modified
 */
static GVariant *
_mpris_build_track_metadata (ClapperMpris *self, ClapperMediaItem *item, gchar **track_id)
{
  GVariantBuilder builder;
  GVariant *variant;
  const gchar *uri, *art_url;
  gchar *title, *tmp_track_id = NULL;
  gint64 duration;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  if (track_id) {
    if (*track_id == NULL)
      *track_id = _mpris_make_track_id (self, item);
  } else {
    tmp_track_id = _mpris_make_track_id (self, item);
  }

  uri = clapper_media_item_get_uri (item);
  title = clapper_media_item_get_title (item);
  duration = CLAPPER_MPRIS_SECONDS_TO_USECONDS (
      clapper_media_item_get_duration (item));

  g_variant_builder_add (&builder, "{sv}", "mpris:trackid",
      g_variant_new_string ((track_id && *track_id != NULL) ? *track_id : tmp_track_id));
  g_free (tmp_track_id);

  g_variant_builder_add (&builder, "{sv}", "mpris:length",
      g_variant_new_int64 (duration));
  g_variant_builder_add (&builder, "{sv}", "xesam:url",
      g_variant_new_string (uri));
  g_variant_builder_add (&builder, "{sv}", "xesam:title",
      g_variant_new_string (title));

  /* TODO: Fill more xesam props from tags within media info */

  GST_OBJECT_LOCK (self);

  /* TODO: Support image sample */
  art_url = (self->art_url)
      ? self->art_url
      : (self->fallback_art_url)
      ? self->fallback_art_url
      : NULL;

  if (art_url) {
    g_variant_builder_add (&builder, "{sv}", "mpris:artUrl",
        g_variant_new_string (art_url));
  }

  GST_OBJECT_UNLOCK (self);

  variant = g_variant_builder_end (&builder);

  g_free (title);

  return variant;
}

static gchar **
_filter_names (const gchar *const *all_names)
{
  GStrvBuilder *builder;
  gchar **filtered_names;
  guint i;

  builder = g_strv_builder_new ();

  for (i = 0; all_names[i]; ++i) {
    const gchar *const *remaining_names = all_names + i + 1;

    if (*remaining_names && g_strv_contains (remaining_names, all_names[i]))
      continue;

    GST_LOG ("Found: %s", all_names[i]);
    g_strv_builder_add (builder, all_names[i]);
  }

  filtered_names = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  return filtered_names;
}

static gchar **
clapper_mpris_get_supported_uri_schemes (ClapperMpris *self)
{
  GStrvBuilder *builder;
  gchar **all_schemes, **filtered_schemes;
  GList *elements, *el;
  guint i;

  GST_DEBUG_OBJECT (self, "Checking supported URI schemes");

  builder = g_strv_builder_new ();
  elements = gst_element_factory_list_get_elements (
      GST_ELEMENT_FACTORY_TYPE_SRC, GST_RANK_NONE);

  for (el = elements; el != NULL; el = el->next) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY (el->data);
    const gchar *const *protocols;

    if (gst_element_factory_get_uri_type (factory) != GST_URI_SRC)
      continue;

    if (!(protocols = gst_element_factory_get_uri_protocols (factory)))
      continue;

    for (i = 0; protocols[i]; ++i)
      g_strv_builder_add (builder, protocols[i]);
  }

  all_schemes = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  gst_plugin_feature_list_free (elements);

  filtered_schemes = _filter_names ((const gchar *const *) all_schemes);
  g_strfreev (all_schemes);

  return filtered_schemes;
}

static gchar **
clapper_mpris_get_supported_mime_types (ClapperMpris *self)
{
  GStrvBuilder *builder;
  gchar **all_types, **filtered_types;
  GList *elements, *el;

  GST_DEBUG_OBJECT (self, "Checking supported mime-types");

  builder = g_strv_builder_new ();
  elements = gst_element_factory_list_get_elements (
      GST_ELEMENT_FACTORY_TYPE_DEMUXER, GST_RANK_NONE);

  for (el = elements; el != NULL; el = el->next) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY (el->data);
    const GList *pad_templates, *pt;

    pad_templates = gst_element_factory_get_static_pad_templates (factory);

    for (pt = pad_templates; pt != NULL; pt = pt->next) {
      GstStaticPadTemplate *template = (GstStaticPadTemplate *) pt->data;
      GstCaps *caps;
      guint i, size;

      if (template->direction != GST_PAD_SINK)
        continue;

      caps = gst_static_pad_template_get_caps (template);
      size = gst_caps_get_size (caps);

      for (i = 0; i < size; ++i) {
        GstStructure *structure = gst_caps_get_structure (caps, i);
        const gchar *name = gst_structure_get_name (structure);

        /* Skip GStreamer internal mime types */
        if (g_str_has_prefix (name, "application/x-gst-"))
          continue;

        /* GStreamer uses "video/quicktime" for MP4. If we can
         * handle it, then also add more generic ones. */
        if (strcmp (name, "video/quicktime") == 0) {
          g_strv_builder_add (builder, "video/mp4");
          g_strv_builder_add (builder, "audio/mp4");
        }

        g_strv_builder_add (builder, name);
      }

      gst_caps_unref (caps);
    }
  }

  all_types = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  gst_plugin_feature_list_free (elements);

  filtered_types = _filter_names ((const gchar *const *) all_types);
  g_strfreev (all_types);

  return filtered_types;
}

static void
clapper_mpris_unregister (ClapperMpris *self)
{
  GST_DEBUG_OBJECT (self, "Unregister");

  if (self->base_exported) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->base_skeleton));
    self->base_exported = FALSE;
  }
  if (self->player_exported) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->player_skeleton));
    self->player_exported = FALSE;
  }
  if (self->tracks_exported) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->tracks_skeleton));
    self->tracks_exported = FALSE;
  }
}

/*
 * @current_item: (nullable): Can be NULL to reset metadata and
 *     disable some playback features
 */
static void
clapper_mpris_read_current_media_item (ClapperMpris *self, ClapperMediaItem *current_item)
{
  GVariant *variant = NULL;
  gboolean is_live = FALSE;

  GST_LOG_OBJECT (self, "Reading current media item");

  /* Current item might be NULL */
  if (current_item)
    variant = _mpris_build_track_metadata (self, current_item, NULL);

  /* Set or clear metadata */
  clapper_mpris_media_player2_player_set_metadata (self->player_skeleton, variant);

  /* Properties related to media item availablity, not current state */
  clapper_mpris_media_player2_player_set_can_play (self->player_skeleton, current_item != NULL);
  clapper_mpris_media_player2_player_set_can_pause (self->player_skeleton, current_item != NULL);

  /* FIXME: Also disable for LIVE content */
  clapper_mpris_media_player2_player_set_can_seek (self->player_skeleton, current_item != NULL);
  clapper_mpris_media_player2_player_set_minimum_rate (self->player_skeleton, (is_live) ? 1.0 : G_MINFLOAT);
  clapper_mpris_media_player2_player_set_maximum_rate (self->player_skeleton, (is_live) ? 1.0 : G_MAXFLOAT);
}

static void
clapper_mpris_refresh_can_go_next_previous (ClapperMpris *self, ClapperQueue *queue, ClapperMediaItem *current_item)
{
  guint n_items = clapper_queue_get_n_items (queue);
  gboolean can_previous = FALSE, can_next = FALSE;

  if (n_items > 0 && current_item) {
    guint index = 0;

    if (clapper_queue_find_item (queue, current_item, &index)) {
      can_previous = (index > 0);
      can_next = (index < n_items - 1);
    }
  }

  clapper_mpris_media_player2_player_set_can_go_previous (self->player_skeleton, can_previous);
  clapper_mpris_media_player2_player_set_can_go_next (self->player_skeleton, can_next);
}

static void
clapper_mpris_refresh_tracks_list (ClapperMpris *self, ClapperQueue *queue)
{
  ClapperMediaItem *item;
  GStrvBuilder *builder = NULL;
  guint i = 0;

  while ((item = clapper_queue_get_item (queue, i))) {
    gchar *track_id;

    if (!builder)
      builder = g_strv_builder_new ();

    track_id = _mpris_make_track_id (self, item);
    g_strv_builder_add (builder, track_id);

    g_free (track_id);
    gst_object_unref (item);

    i++;
  }

  /* Some tracks were added */
  if (builder) {
    gchar **tracks = g_strv_builder_end (builder);
    g_strv_builder_unref (builder);

    clapper_mpris_media_player2_track_list_set_tracks (self->tracks_skeleton, (const gchar *const *) tracks);
    g_strfreev (tracks);
  } else {
    clapper_mpris_media_player2_track_list_set_tracks (self->tracks_skeleton, empty_tracklist);
  }
}

static void
clapper_mpris_state_changed (ClapperFeature *feature, ClapperPlayerState state)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  const gchar *status_str = CLAPPER_MPRIS_PLAYBACK_STATUS_STOPPED;

  switch (state) {
    case CLAPPER_PLAYER_STATE_PLAYING:
      status_str = CLAPPER_MPRIS_PLAYBACK_STATUS_PLAYING;
      break;
    case CLAPPER_PLAYER_STATE_PAUSED:
    case CLAPPER_PLAYER_STATE_BUFFERING:
      status_str = CLAPPER_MPRIS_PLAYBACK_STATUS_PAUSED;
      break;
    default:
      break;
  }

  GST_DEBUG_OBJECT (self, "Playback status changed to: %s", status_str);
  clapper_mpris_media_player2_player_set_playback_status (self->player_skeleton, status_str);
}

static void
clapper_mpris_position_changed (ClapperFeature *feature, gfloat position)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);

  GST_LOG_OBJECT (self, "Position changed to: %f", position);

  clapper_mpris_media_player2_player_set_position (self->player_skeleton,
      CLAPPER_MPRIS_SECONDS_TO_USECONDS (position));
}

static void
clapper_mpris_speed_changed (ClapperFeature *feature, gfloat speed)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  gfloat mpris_speed;

  mpris_speed = (gfloat) clapper_mpris_media_player2_player_get_rate (self->player_skeleton);

  if (CLAPPER_MPRIS_FLT_IS_DIFFERENT (speed, mpris_speed)) {
    GST_LOG_OBJECT (self, "Speed changed to: %f", speed);
    clapper_mpris_media_player2_player_set_rate (self->player_skeleton, speed);
  }
}

static void
clapper_mpris_volume_changed (ClapperFeature *feature, gfloat volume)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  gfloat mpris_volume;

  if (G_UNLIKELY (volume < 0))
    volume = 0;

  mpris_volume = (gfloat) clapper_mpris_media_player2_player_get_volume (self->player_skeleton);

  if (CLAPPER_MPRIS_FLT_IS_DIFFERENT (volume, mpris_volume)) {
    GST_LOG_OBJECT (self, "Volume changed to: %f", volume);
    clapper_mpris_media_player2_player_set_volume (self->player_skeleton, volume);

    /* Store volume to restore after toggling mute */
    self->last_volume = volume;
  }
}

static void
clapper_mpris_mute_changed (ClapperFeature *feature, gboolean mute)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);

  GST_LOG_OBJECT (self, "Mute changed to: %smuted", (mute) ? "" : "un");

  /* MPRIS uses 0 volume instead of proper mute state */
  clapper_mpris_media_player2_player_set_volume (self->player_skeleton,
      (mute) ? 0 : self->last_volume);
}

static void
clapper_mpris_current_media_item_changed (ClapperFeature *feature, ClapperMediaItem *current_item)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  ClapperPlayer *player;

  GST_LOG_OBJECT (self, "Current media item changed to: %" GST_PTR_FORMAT, current_item);

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    clapper_mpris_refresh_can_go_next_previous (self, queue, current_item);
  });

  clapper_mpris_read_current_media_item (self, current_item);
}

static void
clapper_mpris_media_item_updated (ClapperFeature *feature, ClapperMediaItem *item)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  ClapperPlayer *player;

  GST_LOG_OBJECT (self, "Media item updated: %" GST_PTR_FORMAT, item);

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);
    GVariant *variant;
    gchar *track_id = NULL;

    variant = _mpris_build_track_metadata (self, item, &track_id);

    clapper_mpris_media_player2_track_list_emit_track_metadata_changed (self->tracks_skeleton,
        track_id, variant);
    g_free (track_id);

    if (item == current_item)
      clapper_mpris_read_current_media_item (self, item);

    gst_object_unref (current_item);
  });
}

static void
clapper_mpris_queue_item_added (ClapperFeature *feature, ClapperMediaItem *item)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  ClapperPlayer *player;

  GST_LOG_OBJECT (self, "Queue item added");

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *current_item;
    guint index = 0;

    clapper_mpris_refresh_tracks_list (self, queue);

    if (clapper_queue_find_item (queue, item, &index)) {
      GVariant *variant = _mpris_build_track_metadata (self, item, NULL);
      gchar *prev_track_id = NULL;

      if (index > 0) {
        ClapperMediaItem *previous_item;

        if ((previous_item = clapper_queue_get_item (queue, index - 1))) {
          if (previous_item != item) // In case previous item was just removed
            prev_track_id = _mpris_make_track_id (self, previous_item);

          gst_object_unref (previous_item);
        }
      }

      /* NoTrack when item is added at first position in queue */
      clapper_mpris_media_player2_track_list_emit_track_added (self->tracks_skeleton,
          variant, (prev_track_id != NULL) ? prev_track_id : CLAPPER_MPRIS_NO_TRACK);

      g_free (prev_track_id);
    }

    current_item = clapper_queue_get_current_item (queue);
    clapper_mpris_refresh_can_go_next_previous (self, queue, current_item);

    gst_clear_object (&current_item);
  });
}

static void
clapper_mpris_queue_item_removed (ClapperFeature *feature, ClapperMediaItem *item)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  ClapperPlayer *player;
  gchar *track_id;

  GST_LOG_OBJECT (self, "Queue item removed");

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *current_item;

    clapper_mpris_refresh_tracks_list (self, queue);

    current_item = clapper_queue_get_current_item (queue);
    clapper_mpris_refresh_can_go_next_previous (self, queue, current_item);

    gst_clear_object (&current_item);
  });

  track_id = _mpris_make_track_id (self, item);
  clapper_mpris_media_player2_track_list_emit_track_removed (self->tracks_skeleton, track_id);

  g_free (track_id);
}

static void
clapper_mpris_queue_cleared (ClapperFeature *feature)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);

  /* Set everything directly, so it wont be racy if user already
   * started adding items to the queue after clearing it */
  clapper_mpris_media_player2_track_list_set_tracks (self->tracks_skeleton, empty_tracklist);
  clapper_mpris_media_player2_player_set_can_go_previous (self->player_skeleton, FALSE);
  clapper_mpris_media_player2_player_set_can_go_next (self->player_skeleton, FALSE);

  clapper_mpris_media_player2_track_list_emit_track_list_replaced (self->tracks_skeleton,
      empty_tracklist, CLAPPER_MPRIS_NO_TRACK);
}

static void
clapper_mpris_queue_progression_changed (ClapperFeature *feature, ClapperQueueProgressionMode mode)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  const gchar *current_loop_status, *loop_status = CLAPPER_MPRIS_LOOP_NONE;
  gboolean current_shuffle, shuffle = FALSE;
  gboolean loop_changed, shuffle_changed;

  if (self->last_progression == mode)
    return;

  self->last_progression = mode;

  current_loop_status = clapper_mpris_media_player2_player_get_loop_status (self->player_skeleton);
  current_shuffle = clapper_mpris_media_player2_player_get_shuffle (self->player_skeleton);

  switch (mode) {
    case CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM:
      loop_status = CLAPPER_MPRIS_LOOP_TRACK;
      break;
    case CLAPPER_QUEUE_PROGRESSION_CAROUSEL:
      loop_status = CLAPPER_MPRIS_LOOP_PLAYLIST;
      break;
    case CLAPPER_QUEUE_PROGRESSION_SHUFFLE:
      shuffle = TRUE;
      break;
    default:
      break;
  }

  loop_changed = !CLAPPER_MPRIS_COMPARE (loop_status, current_loop_status);
  shuffle_changed = (shuffle != current_shuffle);

  if (loop_changed || shuffle_changed) {
    GST_LOG_OBJECT (self, "Queue progression changed");

    if (loop_changed)
      clapper_mpris_media_player2_player_set_loop_status (self->player_skeleton, loop_status);

    if (shuffle_changed)
      clapper_mpris_media_player2_player_set_shuffle (self->player_skeleton, shuffle);

    self->last_progression = mode;
  }
}

static gboolean
_handle_open_uri_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, const gchar *uri, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperMediaItem *item = clapper_media_item_new (uri);
    ClapperQueue *queue = clapper_player_get_queue (player);

    clapper_queue_add_item (queue, item);
    clapper_queue_select_item (queue, item);
    clapper_player_play (player);

    gst_object_unref (item);
  });

  clapper_mpris_media_player2_player_complete_open_uri (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_play_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    clapper_player_play (player);
  });

  clapper_mpris_media_player2_player_complete_play (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_pause_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    clapper_player_pause (player);
  });

  clapper_mpris_media_player2_player_complete_pause (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_play_pause_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    const gchar *status;

    status = clapper_mpris_media_player2_player_get_playback_status (player_skeleton);

    if (CLAPPER_MPRIS_COMPARE (status, CLAPPER_MPRIS_PLAYBACK_STATUS_PAUSED))
      clapper_player_play (player);
    else
      clapper_player_pause (player);
  });

  clapper_mpris_media_player2_player_complete_play_pause (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_stop_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    clapper_player_stop (player);
  });

  clapper_mpris_media_player2_player_complete_stop (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_next_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    clapper_queue_select_next_item (queue);
  });

  clapper_mpris_media_player2_player_complete_next (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_previous_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    clapper_queue_select_previous_item (queue);
  });

  clapper_mpris_media_player2_player_complete_previous (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_seek_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, gint64 offset, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *current_item;

    if ((current_item = clapper_queue_get_current_item (queue))) {
      gfloat position, seek_position;

      position = clapper_player_get_position (player);
      seek_position = position + CLAPPER_MPRIS_USECONDS_TO_SECONDS (offset);

      if (seek_position <= 0) {
        clapper_player_seek (player, 0);
      } else {
        gfloat duration = clapper_media_item_get_duration (current_item);

        if (seek_position > duration)
          clapper_queue_select_next_item (queue);
        else
          clapper_player_seek (player, seek_position);
      }

      gst_object_unref (current_item);
    }
  });

  clapper_mpris_media_player2_player_complete_seek (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_set_position_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, const gchar *track_id,
    gint64 position, ClapperMpris *self)
{
  ClapperPlayer *player;

  if (G_UNLIKELY (position < 0))
    goto done;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *current_item;

    if ((current_item = clapper_queue_get_current_item (queue))) {
      gchar *current_track_id = _mpris_make_track_id (self, current_item);

      if (strcmp (track_id, current_track_id) == 0) {
        gfloat duration, position_flt;

        duration = clapper_media_item_get_duration (current_item);
        position_flt = CLAPPER_MPRIS_USECONDS_TO_SECONDS (position);

        if (position_flt <= duration)
          clapper_player_seek (player, position_flt);
      }

      g_free (current_track_id);
    }
  });

done:
  clapper_mpris_media_player2_player_complete_set_position (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
_handle_rate_notify_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    gfloat speed, player_speed;

    speed = (gfloat) clapper_mpris_media_player2_player_get_rate (player_skeleton);
    player_speed = clapper_player_get_speed (player);

    if (CLAPPER_MPRIS_FLT_IS_DIFFERENT (speed, player_speed))
      clapper_player_set_speed (player, speed);
  });
}

static void
_handle_volume_notify_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    gfloat volume, player_volume;

    volume = (gfloat) clapper_mpris_media_player2_player_get_volume (player_skeleton);
    player_volume = clapper_player_get_volume (player);

    if (CLAPPER_MPRIS_FLT_IS_DIFFERENT (volume, player_volume))
      clapper_player_set_volume (player, volume);
  });
}

static void
_handle_loop_status_notify_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperQueueProgressionMode mode, player_mode;
    const gchar *loop_status;

    loop_status = clapper_mpris_media_player2_player_get_loop_status (player_skeleton);
    player_mode = clapper_queue_get_progression_mode (queue);

    /* In shuffle MPRIS mode is set to none (consecutive) */
    if (player_mode == CLAPPER_QUEUE_PROGRESSION_SHUFFLE)
      player_mode = CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE;

    mode = CLAPPER_MPRIS_COMPARE (loop_status, CLAPPER_MPRIS_LOOP_TRACK)
        ? CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM
        : CLAPPER_MPRIS_COMPARE (loop_status, CLAPPER_MPRIS_LOOP_PLAYLIST)
        ? CLAPPER_QUEUE_PROGRESSION_CAROUSEL
        : CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE;

    if (mode != player_mode)
      clapper_queue_set_progression_mode (queue, mode);
  });
}

static void
_handle_shuffle_notify_cb (ClapperMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperQueueProgressionMode player_mode;
    gboolean shuffle, player_shuffle;

    player_mode = clapper_queue_get_progression_mode (queue);

    shuffle = clapper_mpris_media_player2_player_get_shuffle (player_skeleton);
    player_shuffle = (player_mode == CLAPPER_QUEUE_PROGRESSION_SHUFFLE);

    if (shuffle != player_shuffle) {
      clapper_queue_set_progression_mode (queue,
          (shuffle) ? CLAPPER_QUEUE_PROGRESSION_SHUFFLE : self->last_progression);
    }
  });
}

static gboolean
_handle_get_tracks_metadata_cb (ClapperMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *const *tracks_ids, ClapperMpris *self)
{
  ClapperPlayer *player;
  GVariant *tracks_variant = NULL;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    GVariantBuilder builder;
    gboolean initialized = FALSE;
    guint i;

    for (i = 0; tracks_ids[i]; ++i) {
      ClapperMediaItem *item = NULL;
      const gchar *track_id = tracks_ids[i];

      if (_mpris_find_track_id (self, queue, track_id, &item, NULL)) {
        GVariant *variant = _mpris_build_track_metadata (self, item, (gchar **) &track_id);

        if (!initialized) {
          g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
          initialized = TRUE;
        }
        g_variant_builder_add_value (&builder, variant);

        gst_object_unref (item);
      }
    }

    if (initialized)
      tracks_variant = g_variant_builder_end (&builder);
  });

  clapper_mpris_media_player2_track_list_complete_get_tracks_metadata (tracks_skeleton,
      invocation, tracks_variant);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_add_track_cb (ClapperMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *uri,
    const gchar *after_track, gboolean set_current, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *item = clapper_media_item_new (uri);
    guint index = 0;
    gboolean added = FALSE;

    if ((added = CLAPPER_MPRIS_COMPARE (after_track, CLAPPER_MPRIS_NO_TRACK)))
      clapper_queue_insert_item (queue, item, 0);
    else if ((added = _mpris_find_track_id (self, queue, after_track, NULL, &index)))
      clapper_queue_insert_item (queue, item, index + 1);

    if (added && set_current) {
      clapper_queue_select_item (queue, item);
      clapper_player_play (player);
    }

    gst_object_unref (item);
  });

  clapper_mpris_media_player2_track_list_complete_add_track (tracks_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_remove_track_cb (ClapperMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *track_id, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *item = NULL;

    if (_mpris_find_track_id (self, queue, track_id, &item, NULL)) {
      clapper_queue_remove_item (queue, item);
      gst_object_unref (item);
    }
  });

  clapper_mpris_media_player2_track_list_complete_remove_track (tracks_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_go_to_cb (ClapperMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *track_id, ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *item = NULL;

    if (_mpris_find_track_id (self, queue, track_id, &item, NULL)) {
      clapper_queue_select_item (queue, item);
      clapper_player_play (player);
      gst_object_unref (item);
    }
  });

  clapper_mpris_media_player2_track_list_complete_go_to (tracks_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
_name_acquired_cb (GDBusConnection *connection, const gchar *name, ClapperMpris *self)
{
  ClapperPlayer *player;
  GError *error = NULL;
  gchar **uri_schemes, **mime_types;

  GST_DEBUG_OBJECT (self, "Name acquired: %s", name);

  if (!(self->base_exported = g_dbus_interface_skeleton_export (
      G_DBUS_INTERFACE_SKELETON (self->base_skeleton),
      connection, "/org/mpris/MediaPlayer2", &error))) {
    goto finish;
  }
  if (!(self->player_exported = g_dbus_interface_skeleton_export (
      G_DBUS_INTERFACE_SKELETON (self->player_skeleton),
      connection, "/org/mpris/MediaPlayer2", &error))) {
    goto finish;
  }
  if (!(self->tracks_exported = g_dbus_interface_skeleton_export (
      G_DBUS_INTERFACE_SKELETON (self->tracks_skeleton),
      connection, "/org/mpris/MediaPlayer2", &error))) {
    goto finish;
  }

  clapper_mpris_media_player2_set_identity (self->base_skeleton, self->identity);
  clapper_mpris_media_player2_set_desktop_entry (self->base_skeleton, self->desktop_entry);

  uri_schemes = clapper_mpris_get_supported_uri_schemes (self);
  clapper_mpris_media_player2_set_supported_uri_schemes (self->base_skeleton,
      (const gchar *const *) uri_schemes);
  g_strfreev (uri_schemes);

  mime_types = clapper_mpris_get_supported_mime_types (self);
  clapper_mpris_media_player2_set_supported_mime_types (self->base_skeleton,
      (const gchar *const *) mime_types);
  g_strfreev (mime_types);

  clapper_mpris_media_player2_player_set_minimum_rate (self->player_skeleton, G_MINFLOAT);
  clapper_mpris_media_player2_player_set_maximum_rate (self->player_skeleton, G_MAXFLOAT);

  /* As stated in MPRIS docs: "This property is not expected to change,
   * as it describes an intrinsic capability of the implementation." */
  clapper_mpris_media_player2_player_set_can_control (self->player_skeleton, TRUE);
  clapper_mpris_media_player2_set_has_track_list (self->base_skeleton, TRUE);
  clapper_mpris_media_player2_track_list_set_can_edit_tracks (self->tracks_skeleton, TRUE);

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue;
    ClapperMediaItem *current_item;

    queue = clapper_player_get_queue (player);
    current_item = clapper_queue_get_current_item (queue);

    clapper_mpris_refresh_tracks_list (self, queue);

    /* Can also pass NULL as item here to clear metadata */
    clapper_mpris_read_current_media_item (self, current_item);
    clapper_mpris_refresh_can_go_next_previous (self, queue, current_item);
    gst_clear_object (&current_item);

    /* Set initial progression, so LoopStatus will not be NULL */
    self->last_progression = CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE;
    clapper_mpris_media_player2_player_set_loop_status (self->player_skeleton, CLAPPER_MPRIS_LOOP_NONE);
    clapper_mpris_media_player2_player_set_shuffle (self->player_skeleton, FALSE);

    /* Trigger update with current values */
    clapper_mpris_state_changed (CLAPPER_FEATURE (self), clapper_player_get_state (player));
    clapper_mpris_position_changed (CLAPPER_FEATURE (self), clapper_player_get_position (player));
    clapper_mpris_speed_changed (CLAPPER_FEATURE (self), clapper_player_get_speed (player));
    clapper_mpris_volume_changed (CLAPPER_FEATURE (self), clapper_player_get_volume (player));
    clapper_mpris_queue_progression_changed (CLAPPER_FEATURE (self), clapper_queue_get_progression_mode (queue));
  });

finish:
  if (error) {
    GST_ERROR_OBJECT (self, "Error: %s", (error && error->message)
        ? error->message : "Unknown DBUS error occured");
    g_error_free (error);

    clapper_mpris_unregister (self);
  }
}

static void
_name_lost_cb (GDBusConnection *connection, const gchar *name, ClapperMpris *self)
{
  GST_DEBUG_OBJECT (self, "Name lost: %s", name);

  clapper_mpris_unregister (self);
}

static gboolean
clapper_mpris_prepare (ClapperFeature *feature)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);
  GDBusConnection *connection;
  gchar *address;

  GST_DEBUG_OBJECT (self, "Prepare");

  if (!(address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, NULL))) {
    GST_WARNING_OBJECT (self, "No MPRIS bus address");
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Obtained MPRIS DBus address: %s", address);

  connection = g_dbus_connection_new_for_address_sync (address,
      G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT
      | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, NULL);
  g_free (address);

  if (!connection) {
    GST_WARNING_OBJECT (self, "No MPRIS bus connection");
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Obtained MPRIS DBus connection");

  self->name_id = g_bus_own_name_on_connection (connection, self->own_name,
      G_BUS_NAME_OWNER_FLAGS_NONE,
      (GBusNameAcquiredCallback) _name_acquired_cb,
      (GBusNameLostCallback) _name_lost_cb,
      self, NULL);
  g_object_unref (connection);

  GST_DEBUG_OBJECT (self, "Own name ID: %u", self->name_id);

  return TRUE;
}

static gboolean
clapper_mpris_unprepare (ClapperFeature *feature)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (feature);

  GST_DEBUG_OBJECT (self, "Unprepare");

  clapper_mpris_unregister (self);

  if (self->name_id > 0) {
    g_bus_unown_name (self->name_id);
    self->name_id = 0;
  }

  return TRUE;
}

static gboolean
_media_refresh_invoke_func (ClapperMpris *self)
{
  ClapperPlayer *player;

  CLAPPER_MPRIS_DO_WITH_PLAYER (self, &player, {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperMediaItem *current_item;

    /* No need to refresh if there is no item to show the changes */
    if ((current_item = clapper_queue_get_current_item (queue))) {
      clapper_mpris_read_current_media_item (self, current_item);
      gst_object_unref (current_item);
    }
  });

  return G_SOURCE_REMOVE;
}

static void
clapper_mpris_invoke_media_refresh (ClapperMpris *self)
{
  GMainContext *context;

  context = clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT (self));
  g_main_context_invoke (context, (GSourceFunc) _media_refresh_invoke_func, self);
}

/**
 * clapper_mpris_new:
 * @own_name: an unique DBus name with "org.mpris.MediaPlayer2." prefix
 * @identity: a media player friendly name
 * @desktop_entry: (nullable): desktop entry filename
 *
 * Creates a new #ClapperMpris instance.
 *
 * Returns: (transfer full): a new #ClapperMpris instance.
 */
ClapperMpris *
clapper_mpris_new (const gchar *own_name, const gchar *identity,
    const gchar *desktop_entry)
{
  ClapperMpris *mpris;

  mpris = g_object_new (CLAPPER_TYPE_MPRIS,
      "own-name", own_name,
      "identity", identity,
      "desktop-entry", desktop_entry,
      NULL);
  gst_object_ref_sink (mpris);

  return mpris;
}

/**
 * clapper_mpris_set_art_url:
 * @mpris: a #ClapperMpris
 * @art_url: (nullable): an art URL
 *
 * Set artwork to show for media. Takes priority over muxed one.
 */
void
clapper_mpris_set_art_url (ClapperMpris *self, const gchar *art_url)
{
  g_return_if_fail (CLAPPER_IS_MPRIS (self));

  GST_OBJECT_LOCK (self);
  g_free (self->art_url);
  self->art_url = g_strdup (art_url);
  GST_OBJECT_UNLOCK (self);

  clapper_mpris_invoke_media_refresh (self);
}

/**
 * clapper_mpris_get_art_url:
 * @mpris: a #ClapperMpris
 *
 * Get art URL earlier set by user.
 *
 * Returns: (transfer full): art URL.
 */
gchar *
clapper_mpris_get_art_url (ClapperMpris *self)
{
  gchar *art_url;

  g_return_val_if_fail (CLAPPER_IS_MPRIS (self), NULL);

  GST_OBJECT_LOCK (self);
  art_url = g_strdup (self->art_url);
  GST_OBJECT_UNLOCK (self);

  return art_url;
}

/**
 * clapper_mpris_set_fallback_art_url:
 * @mpris: a #ClapperMpris
 * @art_url: (nullable): an art URL
 *
 * Set fallback artwork to show when media does not provide one.
 */
void
clapper_mpris_set_fallback_art_url (ClapperMpris *self, const gchar *art_url)
{
  g_return_if_fail (CLAPPER_IS_MPRIS (self));

  GST_OBJECT_LOCK (self);
  g_free (self->fallback_art_url);
  self->fallback_art_url = g_strdup (art_url);
  GST_OBJECT_UNLOCK (self);

  clapper_mpris_invoke_media_refresh (self);
}

/**
 * clapper_mpris_get_fallback_art_url:
 * @mpris: a #ClapperMpris
 *
 * Get fallback art URL earlier set by user.
 *
 * Returns: (transfer full): fallback art URL.
 */
gchar *
clapper_mpris_get_fallback_art_url (ClapperMpris *self)
{
  gchar *art_url;

  g_return_val_if_fail (CLAPPER_IS_MPRIS (self), NULL);

  GST_OBJECT_LOCK (self);
  art_url = g_strdup (self->fallback_art_url);
  GST_OBJECT_UNLOCK (self);

  return art_url;
}

static void
clapper_mpris_init (ClapperMpris *self)
{
  self->base_skeleton = clapper_mpris_media_player2_skeleton_new ();
  self->player_skeleton = clapper_mpris_media_player2_player_skeleton_new ();
  self->tracks_skeleton = clapper_mpris_media_player2_track_list_skeleton_new ();

  g_signal_connect (self->player_skeleton, "handle-open-uri",
      G_CALLBACK (_handle_open_uri_cb), self);
  g_signal_connect (self->player_skeleton, "handle-play",
      G_CALLBACK (_handle_play_cb), self);
  g_signal_connect (self->player_skeleton, "handle-pause",
      G_CALLBACK (_handle_pause_cb), self);
  g_signal_connect (self->player_skeleton, "handle-play-pause",
      G_CALLBACK (_handle_play_pause_cb), self);
  g_signal_connect (self->player_skeleton, "handle-stop",
      G_CALLBACK (_handle_stop_cb), self);
  g_signal_connect (self->player_skeleton, "handle-next",
      G_CALLBACK (_handle_next_cb), self);
  g_signal_connect (self->player_skeleton, "handle-previous",
      G_CALLBACK (_handle_previous_cb), self);
  g_signal_connect (self->player_skeleton, "handle-seek",
      G_CALLBACK (_handle_seek_cb), self);
  g_signal_connect (self->player_skeleton, "handle-set-position",
      G_CALLBACK (_handle_set_position_cb), self);
  g_signal_connect (self->player_skeleton, "notify::rate",
      G_CALLBACK (_handle_rate_notify_cb), self);
  g_signal_connect (self->player_skeleton, "notify::volume",
      G_CALLBACK (_handle_volume_notify_cb), self);
  g_signal_connect (self->player_skeleton, "notify::loop-status",
      G_CALLBACK (_handle_loop_status_notify_cb), self);
  g_signal_connect (self->player_skeleton, "notify::shuffle",
      G_CALLBACK (_handle_shuffle_notify_cb), self);

  g_signal_connect (self->tracks_skeleton, "handle-get-tracks-metadata",
      G_CALLBACK (_handle_get_tracks_metadata_cb), self);
  g_signal_connect (self->tracks_skeleton, "handle-add-track",
      G_CALLBACK (_handle_add_track_cb), self);
  g_signal_connect (self->tracks_skeleton, "handle-remove-track",
      G_CALLBACK (_handle_remove_track_cb), self);
  g_signal_connect (self->tracks_skeleton, "handle-go-to",
      G_CALLBACK (_handle_go_to_cb), self);
}

static void
clapper_mpris_constructed (GObject *object)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (object);

  self->mpris_name = gst_object_get_name (GST_OBJECT_CAST (object));

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_mpris_finalize (GObject *object)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (object);

  g_object_unref (self->base_skeleton);
  g_object_unref (self->player_skeleton);
  g_object_unref (self->tracks_skeleton);

  g_free (self->mpris_name);

  g_free (self->own_name);
  g_free (self->identity);
  g_free (self->desktop_entry);
  g_free (self->art_url);
  g_free (self->fallback_art_url);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_mpris_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (object);

  switch (prop_id) {
    case PROP_OWN_NAME:
      self->own_name = g_value_dup_string (value);
      break;
    case PROP_IDENTITY:
      self->identity = g_value_dup_string (value);
      break;
    case PROP_DESKTOP_ENTRY:
      self->desktop_entry = g_value_dup_string (value);
      break;
    case PROP_ART_URL:
      clapper_mpris_set_art_url (self, g_value_get_string (value));
      break;
    case PROP_FALLBACK_ART_URL:
      clapper_mpris_set_fallback_art_url (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_mpris_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperMpris *self = CLAPPER_MPRIS_CAST (object);

  switch (prop_id) {
    case PROP_OWN_NAME:
      g_value_set_string (value, self->own_name);
      break;
    case PROP_IDENTITY:
      g_value_set_string (value, self->identity);
      break;
    case PROP_DESKTOP_ENTRY:
      g_value_set_string (value, self->desktop_entry);
      break;
    case PROP_ART_URL:
      g_value_take_string (value, clapper_mpris_get_art_url (self));
      break;
    case PROP_FALLBACK_ART_URL:
      g_value_take_string (value, clapper_mpris_get_fallback_art_url (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_mpris_class_init (ClapperMprisClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperFeatureClass *feature_class = (ClapperFeatureClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappermpris", 0,
      "Clapper Mpris");

  gobject_class->constructed = clapper_mpris_constructed;
  gobject_class->get_property = clapper_mpris_get_property;
  gobject_class->set_property = clapper_mpris_set_property;
  gobject_class->finalize = clapper_mpris_finalize;

  feature_class->prepare = clapper_mpris_prepare;
  feature_class->unprepare = clapper_mpris_unprepare;
  feature_class->state_changed = clapper_mpris_state_changed;
  feature_class->position_changed = clapper_mpris_position_changed;
  feature_class->speed_changed = clapper_mpris_speed_changed;
  feature_class->volume_changed = clapper_mpris_volume_changed;
  feature_class->mute_changed = clapper_mpris_mute_changed;
  feature_class->current_media_item_changed = clapper_mpris_current_media_item_changed;
  feature_class->media_item_updated = clapper_mpris_media_item_updated;
  feature_class->queue_item_added = clapper_mpris_queue_item_added;
  feature_class->queue_item_removed = clapper_mpris_queue_item_removed;
  feature_class->queue_cleared = clapper_mpris_queue_cleared;
  feature_class->queue_progression_changed = clapper_mpris_queue_progression_changed;

  /**
   * ClapperMpris:own-name:
   *
   * DBus name to own on connection.
   *
   * Must be written as a reverse DNS format starting with "org.mpris.MediaPlayer2." prefix.
   * Each #ClapperMpris instance running on the same system must have an unique.
   *
   * Example: "org.mpris.MediaPlayer2.MyPlayer1"
   */
  param_specs[PROP_OWN_NAME] = g_param_spec_string ("own-name",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMpris:identity:
   *
   * A friendly name to identify the media player.
   *
   * Example: "My Player"
   */
  param_specs[PROP_IDENTITY] = g_param_spec_string ("identity",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMpris:desktop-entry:
   *
   * The basename of an installed .desktop file with the ".desktop" extension stripped.
   */
  param_specs[PROP_DESKTOP_ENTRY] = g_param_spec_string ("desktop-entry",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMpris:art-url:
   *
   * Artwork to show for media. Takes priority over muxed one.
   */
  param_specs[PROP_ART_URL] = g_param_spec_string ("art-url",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMpris:fallback-art-url:
   *
   * Fallback artwork to show when media does not provide one.
   */
  param_specs[PROP_FALLBACK_ART_URL] = g_param_spec_string ("fallback-art-url",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
