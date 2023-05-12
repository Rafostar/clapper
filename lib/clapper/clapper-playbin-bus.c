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

#include <gst/pbutils/pbutils.h>
#include <gst/audio/streamvolume.h>

#include "clapper-bus-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-player-private.h"
#include "clapper-media-item-private.h"
#include "clapper-utils-private.h"

#define GST_CAT_DEFAULT clapper_playbin_bus_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  CLAPPER_PLAYBIN_BUS_STRUCTURE_UNKNOWN = 0,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_SET_PROP,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_SEEK,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_RATE_CHANGE,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_CURRENT_ITEM_CHANGE
};

static ClapperBusQuark _structure_quarks[] = {
  {"unknown", 0},
  {"set-prop", 0},
  {"seek", 0},
  {"rate-change", 0},
  {"current-item-change", 0},
  {NULL, 0}
};

enum
{
  CLAPPER_PLAYBIN_BUS_FIELD_UNKNOWN = 0,
  CLAPPER_PLAYBIN_BUS_FIELD_NAME,
  CLAPPER_PLAYBIN_BUS_FIELD_VALUE,
  CLAPPER_PLAYBIN_BUS_FIELD_POSITION,
  CLAPPER_PLAYBIN_BUS_FIELD_RATE,
  CLAPPER_PLAYBIN_BUS_FIELD_SEEK_METHOD,
  CLAPPER_PLAYBIN_BUS_FIELD_MEDIA_ITEM
};

static ClapperBusQuark _field_quarks[] = {
  {"unknown", 0},
  {"name", 0},
  {"value", 0},
  {"position", 0},
  {"rate", 0},
  {"seek-method", 0},
  {"media-item", 0},
  {NULL, 0}
};

#define _STRUCTURE_QUARK(q) (_structure_quarks[CLAPPER_PLAYBIN_BUS_STRUCTURE_##q].quark)
#define _FIELD_QUARK(q) (_field_quarks[CLAPPER_PLAYBIN_BUS_FIELD_##q].quark)
#define _FIELD_NAME(q) (_field_quarks[CLAPPER_PLAYBIN_BUS_FIELD_##q].name)
#define _MESSAGE_SRC_GOBJECT(msg) ((GObject *) GST_MESSAGE_SRC (msg))

void
clapper_playbin_bus_initialize (void)
{
  guint i;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperplaybinbus", 0,
      "Clapper Playbin Bus");

  for (i = 0; _structure_quarks[i].name; ++i)
    _structure_quarks[i].quark = g_quark_from_static_string (_structure_quarks[i].name);
  for (i = 0; _field_quarks[i].name; ++i)
    _field_quarks[i].quark = g_quark_from_static_string (_field_quarks[i].name);
}
/*
static gboolean
_set_object_prop (GQuark field_id, const GValue *value, GstObject *object)
{
  const gchar *prop_name = g_quark_to_string (field_id);

  GST_DEBUG ("Setting %s property: %s", GST_OBJECT_NAME (object), prop_name);
  g_object_set_property (G_OBJECT (object), prop_name, value);

  return G_SOURCE_CONTINUE;
}
*/

static void
_update_current_duration (ClapperPlayer *player)
{
  gint64 duration = GST_CLOCK_TIME_NONE;

  if (!gst_element_query_duration (player->playbin, GST_FORMAT_TIME, &duration))
    return;

  if (G_UNLIKELY (duration < 0))
    duration = 0;

  if (G_LIKELY (player->played_item != NULL)) {
    gfloat duration_flt = (gfloat) duration / GST_SECOND;

    clapper_media_item_set_duration (player->played_item, duration_flt, player->app_bus);
  }
}

static gboolean
_iterate_decoder_pads (ClapperPlayer *player, GstElement *element,
    const gchar *stream_id, GstElementFactoryListType type)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;
  gboolean found = FALSE;

  iter = gst_element_iterate_src_pads (element);

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *decoder_pad = g_value_get_object (&value);
    gchar *decoder_sid = gst_pad_get_stream_id (decoder_pad);

    GST_DEBUG_OBJECT (player, "Decoder stream: %s", decoder_sid);

    if ((found = (g_strcmp0 (decoder_sid, stream_id) == 0))) {
      GST_DEBUG_OBJECT (player, "Found decoder for stream: %s", stream_id);

      if ((type & GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO) == GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO)
        clapper_player_handle_playbin_video_decoder_changed (player, element);
      else if ((type & GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO) == GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO)
        clapper_player_handle_playbin_audio_decoder_changed (player, element);
    }

    g_free (decoder_sid);
    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);

  return found;
}

static gboolean
_find_active_decoder_with_stream_id (ClapperPlayer *player, GstElementFactoryListType type,
    const gchar *stream_id)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;
  gboolean found = FALSE;

  GST_DEBUG_OBJECT (player, "Searching for decoder with stream: %s", stream_id);

  iter = gst_bin_iterate_recurse (GST_BIN (player->playbin));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstElement *element = g_value_get_object (&value);
    GstElementFactory *factory = gst_element_get_factory (element);

    if (factory && gst_element_factory_list_is_type (factory, type))
      found = _iterate_decoder_pads (player, element, stream_id, type);

    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);

  return found;
}

static inline void
_handle_buffering_msg (GstMessage *msg, ClapperPlayer *player)
{
  gint percent;
  gboolean is_buffering;

  gst_message_parse_buffering (msg, &percent);
  GST_LOG_OBJECT (player, "Buffering: %i%%", percent);

  is_buffering = (percent < 100);

  /* If no change return */
  if (player->is_buffering == is_buffering)
    return;

  player->is_buffering = is_buffering;

  /* TODO: Review this code later */
  if (player->target_state > GST_STATE_PAUSED) {
    GstStateChangeReturn ret;

    ret = gst_element_set_state (player->playbin,
        (is_buffering) ? GST_STATE_PAUSED : GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE)
      GST_FIXME_OBJECT (player, "HANDLE BUFFERING STATE CHANGE ERROR");
  }
}

void
clapper_playbin_bus_post_set_volume (GstBus *bus, GstElement *playbin, gfloat volume)
{
  GValue value = G_VALUE_INIT;
  gdouble volume_linear;

  volume_linear = gst_stream_volume_convert_volume (
      GST_STREAM_VOLUME_FORMAT_CUBIC,
      GST_STREAM_VOLUME_FORMAT_LINEAR,
      volume);

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, volume_linear);

  clapper_playbin_bus_post_set_prop (bus, GST_OBJECT_CAST (playbin), "volume", &value);
}

/* GValue is transfer-full!!! */
void
clapper_playbin_bus_post_set_prop (GstBus *bus, GstObject *src,
    const gchar *name, GValue *value)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (SET_PROP),
      _FIELD_QUARK (NAME), G_TYPE_STRING, name,
      NULL);
  gst_structure_id_take_value (structure, _FIELD_QUARK (VALUE), value);
  gst_bus_post (bus, gst_message_new_application (src, structure));
}

static inline void
_handle_set_prop_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  const gchar *prop_name = gst_structure_get_string (structure, _FIELD_NAME (NAME));
  const GValue *value = gst_structure_id_get_value (structure, _FIELD_QUARK (VALUE));

  /* We cannot change some playbin properties, until pipeline is running.
   * Notify user about change immediatelly and we will apply value on preroll. */
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (player->playbin)
      && player->current_state <= GST_STATE_READY) {
    if (strcmp (prop_name, "volume") == 0) {
      clapper_player_handle_playbin_volume_changed (player, value);
      return;
    } else if (strcmp (prop_name, "mute") == 0) {
      clapper_player_handle_playbin_mute_changed (player, value);
      return;
    }
  }

  GST_DEBUG ("Setting %s property: %s", GST_OBJECT_NAME (GST_MESSAGE_SRC (msg)), prop_name);
  g_object_set_property (_MESSAGE_SRC_GOBJECT (msg), prop_name, value);
}

void
clapper_playbin_bus_post_request_state (GstBus *bus, ClapperPlayer *player, GstState state)
{
  gst_bus_post (bus, gst_message_new_request_state (GST_OBJECT_CAST (player), state));
}

static inline void
_handle_request_state_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstState state;

  gst_message_parse_request_state (msg, &state);

  /* If message came from player, update user requested target state */
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (player))
    player->target_state = state;

  /* FIXME: Also ignore play/pause call for live content */

  /* Ignore play/pause state requests if we are buffering,
   * just update target state for later */
  if (player->is_buffering && state > GST_STATE_READY)
    return;

  GST_DEBUG_OBJECT (player, "Changing state to: %s",
      gst_element_state_get_name (state));
  gst_element_set_state (player->playbin, state);
}

void
clapper_playbin_bus_post_seek (GstBus *bus, gfloat position, ClapperPlayerSeekMethod seek_method)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (SEEK),
      _FIELD_QUARK (POSITION), G_TYPE_INT64, (gint64) (position * GST_SECOND),
      _FIELD_QUARK (SEEK_METHOD), G_TYPE_ENUM, seek_method,
      NULL);
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_seek_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  GstEvent *seek_event;
  gint64 position = 0;
  gfloat rate;
  ClapperPlayerSeekMethod seek_method = CLAPPER_PLAYER_SEEK_METHOD_NORMAL;
  GstSeekFlags flags = GST_SEEK_FLAG_FLUSH;

  /* We should ignore seek if pipeline is going to be stopped */
  if (player->target_state < GST_STATE_PAUSED)
    return;

  gst_structure_id_get (structure,
      _FIELD_QUARK (POSITION), G_TYPE_INT64, &position,
      _FIELD_QUARK (SEEK_METHOD), G_TYPE_ENUM, &seek_method,
      NULL);

  /* If we are starting playback, do a seek after preroll */
  if (player->current_state < GST_STATE_PAUSED) {
    player->pending_position = (gfloat) position / GST_SECOND;
    return;
  }

  switch (seek_method) {
    case CLAPPER_PLAYER_SEEK_METHOD_FAST:
      flags |= (GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_NEAREST);
      break;
    case CLAPPER_PLAYER_SEEK_METHOD_NORMAL:
      break;
    case CLAPPER_PLAYER_SEEK_METHOD_ACCURATE:
      flags |= GST_SEEK_FLAG_ACCURATE;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  rate = clapper_player_get_speed (player);

  if (rate != 1.0)
    flags |= GST_SEEK_FLAG_TRICKMODE;

  if (rate >= 0) {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  } else {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0), GST_SEEK_TYPE_SET, position);
  }

  GST_DEBUG ("Seeking with rate %.2lf to: %" GST_TIME_FORMAT,
      rate, GST_TIME_ARGS (position));

  clapper_player_remove_tick_source (player);

  if (!gst_element_send_event (player->playbin, seek_event)) {
    /* FIXME: Should we maybe call _handle_error_msg with
     * some error here? Or will playbin post such message for us? */
    GST_ERROR ("Could not seek");
  }
}

void
clapper_playbin_bus_post_rate_change (GstBus *bus, gfloat rate)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (RATE_CHANGE),
      _FIELD_QUARK (RATE), G_TYPE_FLOAT, rate,
      NULL);
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_rate_change_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  GstEvent *seek_event;
  gint64 position = GST_CLOCK_TIME_NONE;
  GstSeekType seek_type = GST_SEEK_TYPE_NONE;
  GstSeekFlags flags = GST_SEEK_FLAG_NONE;
  gfloat /* current_rate,*/ rate = 1.0;

  gst_structure_id_get (structure,
      _FIELD_QUARK (RATE), G_TYPE_FLOAT, &rate,
      NULL);

  /* We cannot perform playback rate changes until pipeline is running.
   * Notify user about change immediatelly and we will apply value on preroll. */
  if (player->current_state < GST_STATE_PAUSED
      || player->target_state < GST_STATE_PAUSED) {
    clapper_player_handle_playbin_rate_changed (player, rate);
    return;
  }

  /* FIXME: Using GST_SEEK_FLAG_INSTANT_RATE_CHANGE, audio-filter stops
   * working with playbin2 and seek event fails with playbin3 :-( */
#if 0
  /* We can only do instant rate changes without flushing when
   * playback direction stays the same. Otherwise get most current
   * position, so we can seek back as close to it as possible. */
  current_rate = clapper_player_get_speed (player);

  if ((rate < 0 && current_rate < 0) || (rate > 0 && current_rate > 0)) {
#else
  if (FALSE) {
#endif
    flags |= GST_SEEK_FLAG_INSTANT_RATE_CHANGE;
  } else {
    seek_type = GST_SEEK_TYPE_SET;
    flags |= GST_SEEK_FLAG_FLUSH;

    gst_element_query_position (player->playbin, GST_FORMAT_TIME, &position);
  }

  /* Round playback rate to 1.0 */
  if (!FLT_IS_DIFFERENT (rate, 1.0))
    rate = 1.0;

  if (rate != 1.0)
    flags |= GST_SEEK_FLAG_TRICKMODE;

  if (rate >= 0) {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        seek_type, position, seek_type, GST_CLOCK_TIME_NONE);
  } else {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        seek_type, (position == GST_CLOCK_TIME_NONE) ? GST_CLOCK_TIME_NONE : G_GINT64_CONSTANT (0),
        seek_type, position);
  }

  GST_DEBUG ("Changing rate to: %.2lf", rate);

  /* Similarly as in normal seek */
  if ((flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE) == 0)
    clapper_player_remove_tick_source (player);

  if (gst_element_send_event (player->playbin, seek_event)) {
    clapper_player_handle_playbin_rate_changed (player, rate);
  } else {
    /* FIXME: Should we maybe call _handle_error_msg with
     * some error here? Or will playbin post such message for us? */
    GST_ERROR ("Could not change rate");
  }
}

static inline void
_handle_state_changed_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstState old_state, pending_state;
  gboolean preroll;

  /* We only care about our parent bin state changes */
  if (GST_MESSAGE_SRC (msg) != GST_OBJECT_CAST (player->playbin))
    return;

  gst_message_parse_state_changed (msg, &old_state, &player->current_state, &pending_state);
  GST_LOG_OBJECT (player, "State changed, old: %i, current: %i, pending: %i",
      old_state, player->current_state, pending_state);

  if (player->current_state <= GST_STATE_READY)
    clapper_player_reset (player, FALSE);

  if (player->current_state == GST_STATE_PLAYING)
    clapper_player_add_tick_source (player);
  else
    clapper_player_remove_tick_source (player);

  /* Notify user about current position either right before or after
   * changed playback (so it does not look like seek after paused) */
  if (player->current_state < old_state)
    clapper_player_query_position (player);

  clapper_player_handle_playbin_state_changed (player);

  if (player->current_state > old_state)
    clapper_player_query_position (player);

  preroll = (old_state == GST_STATE_READY
      && player->current_state == GST_STATE_PAUSED
      && (pending_state == GST_STATE_VOID_PENDING || pending_state == GST_STATE_PLAYING));

  if (preroll) {
    gfloat speed;

    GST_DEBUG ("Setting cached playbin props after preroll");

    clapper_player_set_volume (player, clapper_player_get_volume (player));
    clapper_player_set_mute (player, clapper_player_get_mute (player));

    speed = clapper_player_get_speed (player);

    /* Playback always starts with normal speed and from zero.
     * When not changed do not post seek event. */
    if (FLT_IS_DIFFERENT (speed, 1.0))
      clapper_player_set_speed (player, speed);
    if (FLT_IS_DIFFERENT (player->pending_position, 0)) {
      clapper_player_seek (player, player->pending_position);
      player->pending_position = 0;
    }

    _update_current_duration (player);
  }
}

void
clapper_playbin_bus_post_current_item_change (GstBus *bus, ClapperMediaItem *current_item)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (CURRENT_ITEM_CHANGE),
      _FIELD_QUARK (MEDIA_ITEM), CLAPPER_TYPE_MEDIA_ITEM, current_item,
      NULL);
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_current_item_change_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  ClapperMediaItem *current_item = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (MEDIA_ITEM), CLAPPER_TYPE_MEDIA_ITEM, &current_item,
      NULL);

  /* Do not act when the same item is reselected */
  if (player->played_item != current_item) {
    const gchar *uri = NULL;

    /* Might be NULL (e.g. after queue is cleared) */
    if (current_item)
      uri = clapper_media_item_get_uri (current_item);

    GST_INFO_OBJECT (player, "Changing URI to: \"%s\"", GST_STR_NULL (uri));

    player->pending_position = 0; // We store pending position for played item, so reset
    gst_element_set_state (player->playbin, GST_STATE_READY);

    gst_object_replace ((GstObject **) &player->played_item, GST_OBJECT_CAST (current_item));
    g_object_set (player->playbin, "suburi", NULL, "uri", uri, NULL);

    if (clapper_player_get_have_features (player))
      clapper_features_manager_trigger_current_media_item_changed (player->features_manager, current_item);

    if (!player->played_item)
      player->target_state = GST_STATE_READY;

    gst_element_set_state (player->playbin, player->target_state);
  }

  gst_clear_object (&current_item);
}

static inline void
_handle_app_msg (GstMessage *msg, ClapperPlayer *player)
{
  const GstStructure *structure = gst_message_get_structure (msg);
  GQuark quark = gst_structure_get_name_id (structure);

  if (quark == _STRUCTURE_QUARK (SET_PROP))
    _handle_set_prop_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (SEEK))
    _handle_seek_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (RATE_CHANGE))
    _handle_rate_change_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (CURRENT_ITEM_CHANGE))
    _handle_current_item_change_msg (msg, structure, player);
}

static inline void
_handle_element_msg (GstMessage *msg, ClapperPlayer *player)
{
  if (gst_is_missing_plugin_message (msg)) {
    gchar *desc, *details;
    guint signal_id;

    desc = gst_missing_plugin_message_get_description (msg);
    details = gst_missing_plugin_message_get_installer_detail (msg);
    signal_id = g_signal_lookup ("missing-plugin", CLAPPER_TYPE_PLAYER);

    clapper_app_bus_post_desc_with_details_signal (player->app_bus,
        GST_OBJECT_CAST (player), signal_id, desc, details);

    g_free (desc);
    g_free (details);
  }
}

static inline void
_handle_tag_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstTagList *tags = NULL;

  gst_message_parse_tag (msg, &tags);

  GST_LOG_OBJECT (player, "Got tags from element: %s: %" GST_PTR_FORMAT,
      GST_OBJECT_NAME (GST_MESSAGE_SRC (msg)), tags);

  if (G_LIKELY (player->played_item != NULL))
    clapper_media_item_update_from_tag_list (player->played_item, tags, player->app_bus);

  gst_tag_list_unref (tags);
}

static inline void
_handle_property_notify_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstObject *src = NULL;
  const gchar *prop_name = NULL;
  const GValue *value = NULL;

  gst_message_parse_property_notify (msg, &src, &prop_name, &value);
  GST_DEBUG ("Received info about changed %s property: %s",
      GST_OBJECT_NAME (src), prop_name);

  /* Since we manually need to request elements to post this message,
   * any other element posting this is unlikely */
  if (G_UNLIKELY (src != GST_OBJECT_CAST (player->playbin)))
    return;

  if (strcmp (prop_name, "volume") == 0)
    clapper_player_handle_playbin_volume_changed (player, value);
  else if (strcmp (prop_name, "mute") == 0)
    clapper_player_handle_playbin_mute_changed (player, value);
  else
    clapper_player_handle_playbin_common_prop_changed (player, prop_name);
}

static inline void
_handle_streams_selected_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstStreamCollection *collection = NULL;
  gchar *video_sid = NULL, *audio_sid = NULL, *subtitle_sid = NULL;
  guint i;

  gst_message_parse_streams_selected (msg, &collection);

  if (!collection)
    return;

  /* FIXME: Use collection "stream-notify" signal to keep updating current media item */

  gst_object_unref (collection);

  for (i = 0; i < gst_message_streams_selected_get_size (msg); ++i) {
    GstStream *stream = gst_message_streams_selected_get_stream (msg, i);
    GstStreamType stream_type = gst_stream_get_stream_type (stream);
    const gchar *stream_id = gst_stream_get_stream_id (stream);

    if ((stream_type & GST_STREAM_TYPE_VIDEO) == GST_STREAM_TYPE_VIDEO) {
      if (G_LIKELY (!video_sid)) {
        video_sid = g_strdup (stream_id);
        continue;
      }
    } else if ((stream_type & GST_STREAM_TYPE_AUDIO) == GST_STREAM_TYPE_AUDIO) {
      if (G_LIKELY (!audio_sid)) {
        audio_sid = g_strdup (stream_id);
        continue;
      }
    } else if ((stream_type & GST_STREAM_TYPE_TEXT) == GST_STREAM_TYPE_TEXT) {
      if (G_LIKELY (!subtitle_sid)) {
        subtitle_sid = g_strdup (stream_id);
        continue;
      }
    } else {
      GST_WARNING_OBJECT (player, "Unhandled stream type %s",
          gst_stream_type_get_name (stream_type));
      continue;
    }

    GST_FIXME_OBJECT (player,
        "Multiple streams are selected for type %s, using first one",
        gst_stream_type_get_name (stream_type));
  }

  if (video_sid) {
    if (G_UNLIKELY (!_find_active_decoder_with_stream_id (player,
        GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, video_sid)))
      GST_WARNING_OBJECT (player, "Could not find active video decoder");
  }
  if (audio_sid) {
    if (G_UNLIKELY (!_find_active_decoder_with_stream_id (player,
        GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, audio_sid)))
      GST_WARNING_OBJECT (player, "Could not find active audio decoder");
  }

  GST_OBJECT_LOCK (player);

  g_free (player->video_sid);
  player->video_sid = video_sid;

  g_free (player->audio_sid);
  player->audio_sid = audio_sid;

  g_free (player->subtitle_sid);
  player->subtitle_sid = subtitle_sid;

  GST_OBJECT_UNLOCK (player);
}

static inline void
_handle_duration_changed_msg (GstMessage *msg G_GNUC_UNUSED, ClapperPlayer *player)
{
  _update_current_duration (player);
}

static inline void
_handle_warning_msg (GstMessage *msg, ClapperPlayer *player)
{
  GError *error = NULL;
  gchar *debug_info = NULL;
  guint signal_id;

  gst_message_parse_warning (msg, &error, &debug_info);
  GST_WARNING_OBJECT (player, "Warning: %s", error->message);

  signal_id = g_signal_lookup ("warning", CLAPPER_TYPE_PLAYER);

  clapper_app_bus_post_error_signal (player->app_bus,
      GST_OBJECT_CAST (player), signal_id, error, debug_info);

  g_clear_error (&error);
  g_free (debug_info);
}

static inline void
_handle_error_msg (GstMessage *msg, ClapperPlayer *player)
{
  GError *error = NULL;
  gchar *debug_info = NULL;
  guint signal_id;

  gst_message_parse_error (msg, &error, &debug_info);
  GST_ERROR_OBJECT (player, "Error: %s", error->message);

  GST_OBJECT_LOCK (player);
  player->had_error = TRUE;
  GST_OBJECT_UNLOCK (player);

  /* Remove position query, since there was an error */
  clapper_player_remove_tick_source (player);

  /* After error we should go to READY, so all elements will stop processing buffers */
  gst_element_set_state (player->playbin, GST_STATE_READY);

  signal_id = g_signal_lookup ("error", CLAPPER_TYPE_PLAYER);

  clapper_app_bus_post_error_signal (player->app_bus,
      GST_OBJECT_CAST (player), signal_id, error, debug_info);

  g_clear_error (&error);
  g_free (debug_info);
}

gboolean
clapper_playbin_bus_message_func (GstBus *bus, GstMessage *msg, ClapperPlayer *player)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_BUFFERING:
      _handle_buffering_msg (msg, player);
      break;
    case GST_MESSAGE_REQUEST_STATE:
      _handle_request_state_msg (msg, player);
      break;
    case GST_MESSAGE_STATE_CHANGED:
      _handle_state_changed_msg (msg, player);
      break;
    case GST_MESSAGE_APPLICATION:
      _handle_app_msg (msg, player);
      break;
    case GST_MESSAGE_ELEMENT:
      _handle_element_msg (msg, player);
      break;
    case GST_MESSAGE_TAG:
      _handle_tag_msg (msg, player);
      break;
    case GST_MESSAGE_PROPERTY_NOTIFY:
      _handle_property_notify_msg (msg, player);
      break;
    case GST_MESSAGE_STREAMS_SELECTED:
      _handle_streams_selected_msg (msg, player);
      break;
    case GST_MESSAGE_DURATION_CHANGED:
      _handle_duration_changed_msg (msg, player);
      break;
    case GST_MESSAGE_WARNING:
      _handle_warning_msg (msg, player);
      break;
    case GST_MESSAGE_ERROR:
      _handle_error_msg (msg, player);
      break;
    default:
      break;
  }

  return G_SOURCE_CONTINUE;
}
