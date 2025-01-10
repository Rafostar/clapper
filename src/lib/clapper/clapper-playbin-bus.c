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

#include <gst/pbutils/pbutils.h>
#include <gst/audio/streamvolume.h>

#include "clapper-bus-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-app-bus-private.h"
#include "clapper-player-private.h"
#include "clapper-queue-private.h"
#include "clapper-media-item-private.h"
#include "clapper-timeline-private.h"
#include "clapper-stream-private.h"
#include "clapper-stream-list-private.h"

#include "clapper-functionalities-availability.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include "gst/clapper-enhancer-src-private.h"
#endif

#define GST_CAT_DEFAULT clapper_playbin_bus_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  CLAPPER_PLAYBIN_BUS_STRUCTURE_UNKNOWN = 0,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_SET_PROP,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_SET_PLAY_FLAG,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_SEEK,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_RATE_CHANGE,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_STREAM_CHANGE,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_CURRENT_ITEM_CHANGE,
  CLAPPER_PLAYBIN_BUS_STRUCTURE_ITEM_SUBURI_CHANGE
};

static ClapperBusQuark _structure_quarks[] = {
  {"unknown", 0},
  {"set-prop", 0},
  {"set-play-flag", 0},
  {"seek", 0},
  {"rate-change", 0},
  {"stream-change", 0},
  {"current-item-change", 0},
  {"item-suburi-change", 0},
  {NULL, 0}
};

enum
{
  CLAPPER_PLAYBIN_BUS_FIELD_UNKNOWN = 0,
  CLAPPER_PLAYBIN_BUS_FIELD_NAME,
  CLAPPER_PLAYBIN_BUS_FIELD_VALUE,
  CLAPPER_PLAYBIN_BUS_FIELD_FLAG,
  CLAPPER_PLAYBIN_BUS_FIELD_POSITION,
  CLAPPER_PLAYBIN_BUS_FIELD_RATE,
  CLAPPER_PLAYBIN_BUS_FIELD_SEEK_METHOD,
  CLAPPER_PLAYBIN_BUS_FIELD_MEDIA_ITEM,
  CLAPPER_PLAYBIN_BUS_FIELD_ITEM_CHANGE_MODE
};

static ClapperBusQuark _field_quarks[] = {
  {"unknown", 0},
  {"name", 0},
  {"value", 0},
  {"flag", 0},
  {"position", 0},
  {"rate", 0},
  {"seek-method", 0},
  {"media-item", 0},
  {"item-change-mode", 0},
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

static inline void
dump_dot_file (ClapperPlayer *player, const gchar *name)
{
  gchar full_name[40];

  g_snprintf (full_name, sizeof (full_name), "clapper.%p.%s", player, name);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN_CAST (player->playbin),
      GST_DEBUG_GRAPH_SHOW_ALL, full_name);
}

static void
_perform_flush_seek (ClapperPlayer *player)
{
  GstEvent *seek_event;
  GstSeekFlags flags = GST_SEEK_FLAG_FLUSH;
  gint64 position = GST_CLOCK_TIME_NONE;
  gdouble rate = clapper_player_get_speed (player);

  if (rate != 1.0)
    flags |= GST_SEEK_FLAG_TRICKMODE;

  if (gst_element_query (player->playbin, player->position_query))
    gst_query_parse_position (player->position_query, NULL, &position);

  if (rate >= 0) {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  } else {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0), GST_SEEK_TYPE_SET, position);
  }
  clapper_player_remove_tick_source (player);

  GST_DEBUG_OBJECT (player, "Flush seeking with rate %.2lf to: %" GST_TIME_FORMAT,
      rate, GST_TIME_ARGS (position));

  if (!gst_element_send_event (player->playbin, seek_event))
    GST_WARNING_OBJECT (player, "Could not perform a flush seek");
}

static void
_update_current_duration (ClapperPlayer *player)
{
  gint64 duration;

  if (!gst_element_query_duration (player->playbin, GST_FORMAT_TIME, &duration))
    return;

  if (G_UNLIKELY (duration < 0))
    duration = 0;

  if (G_LIKELY (player->played_item != NULL)) {
    gdouble duration_dbl = (gdouble) duration / GST_SECOND;

    if (clapper_media_item_set_duration (player->played_item, duration_dbl, player->app_bus)) {
      ClapperFeaturesManager *features_manager;

      if ((features_manager = clapper_player_get_features_manager (player)))
        clapper_features_manager_trigger_item_updated (features_manager, player->played_item);
    }
  }
}

static inline void
_handle_warning_msg (GstMessage *msg, ClapperPlayer *player)
{
  GError *error = NULL;
  gchar *debug_info = NULL;
  guint signal_id;

  gst_message_parse_warning (msg, &error, &debug_info);
  GST_WARNING_OBJECT (player, "Warning: %s", error->message);

  dump_dot_file (player, "WARNING");

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

  dump_dot_file (player, "ERROR");

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

  /* When buffering we need to manually refresh to enter buffering state
   * while later playbin PLAYING state message will trigger leave */
  if (player->is_buffering || player->target_state < GST_STATE_PLAYING)
    clapper_player_handle_playbin_state_changed (player);

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
clapper_playbin_bus_post_set_volume (GstBus *bus, GstElement *playbin, gdouble volume)
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
clapper_playbin_bus_post_set_play_flag (GstBus *bus,
    ClapperPlayerPlayFlags flag, gboolean enabled)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (SET_PLAY_FLAG),
      _FIELD_QUARK (FLAG), G_TYPE_FLAGS, flag,
      _FIELD_QUARK (VALUE), G_TYPE_BOOLEAN, enabled,
      NULL);
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_set_play_flag_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  ClapperPlayerPlayFlags flag = 0;
  gboolean enabled, enable = FALSE;
  gint flags = 0;

  gst_structure_id_get (structure,
      _FIELD_QUARK (FLAG), G_TYPE_FLAGS, &flag,
      _FIELD_QUARK (VALUE), G_TYPE_BOOLEAN, &enable,
      NULL);

  g_object_get (player->playbin, "flags", &flags, NULL);
  enabled = ((flags & flag) == flag);

  if (enabled != enable) {
    if (enable)
      flags |= flag;
    else
      flags &= ~flag;

    GST_DEBUG_OBJECT (player, "%sabling play flag: %i", (enable) ? "En" : "Dis", flag);
    g_object_set (player->playbin, "flags", flags, NULL);
  }
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

  if (state > GST_STATE_READY) {
    gboolean has_item;

    GST_OBJECT_LOCK (player);
    has_item = (player->played_item || player->pending_item);
    GST_OBJECT_UNLOCK (player);

    if (!has_item)
      return;
  }

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
clapper_playbin_bus_post_seek (GstBus *bus, gdouble position, ClapperPlayerSeekMethod seek_method)
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
  gdouble rate;
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
    player->pending_position = (gdouble) position / GST_SECOND;
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

  if (!(player->seeking = gst_element_send_event (player->playbin, seek_event))) {
    /* FIXME: Should we maybe call _handle_error_msg with
     * some error here? Or will playbin post such message for us? */
    GST_ERROR ("Could not seek");
  }
}

void
clapper_playbin_bus_post_rate_change (GstBus *bus, gdouble rate)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (RATE_CHANGE),
      _FIELD_QUARK (RATE), G_TYPE_DOUBLE, rate,
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
  gdouble /* current_rate,*/ rate = 1.0;

  gst_structure_id_get (structure,
      _FIELD_QUARK (RATE), G_TYPE_DOUBLE, &rate,
      NULL);

  if (player->speed_changing && player->requested_speed != 0) {
    player->pending_speed = rate;
    return;
  }

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

    if (gst_element_query (player->playbin, player->position_query))
      gst_query_parse_position (player->position_query, NULL, &position);
  }

  /* Round playback rate to 1.0 */
  if (G_APPROX_VALUE (rate, 1.0, FLT_EPSILON))
    rate = 1.0;

  if (rate != 1.0)
    flags |= GST_SEEK_FLAG_TRICKMODE;

  if (rate >= 0) {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        seek_type, position, seek_type, GST_CLOCK_TIME_NONE);
  } else {
    seek_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        seek_type, (position < 0) ? GST_CLOCK_TIME_NONE : G_GINT64_CONSTANT (0),
        seek_type, position);
  }

  GST_DEBUG_OBJECT (player, "Changing rate to: %.2lf", rate);

  /* Similarly as in normal seek */
  if ((flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE) == 0)
    clapper_player_remove_tick_source (player);

  if (gst_element_send_event (player->playbin, seek_event)) {
    if ((flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE) == 0) {
      player->requested_speed = rate;
      player->speed_changing = TRUE;
    } else {
      player->requested_speed = 0;
      player->pending_speed = 0;
      player->speed_changing = FALSE;
      clapper_player_handle_playbin_rate_changed (player, rate);
    }
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
  gboolean preroll, eos;

  /* We only care about our parent bin state changes */
  if (GST_MESSAGE_SRC (msg) != GST_OBJECT_CAST (player->playbin))
    return;

  gst_message_parse_state_changed (msg, &old_state, &player->current_state, &pending_state);
  GST_LOG_OBJECT (player, "State changed, old: %i, current: %i, pending: %i",
      old_state, player->current_state, pending_state);

  dump_dot_file (player, gst_element_state_get_name (player->current_state));

  /* Seek operation is progressing as expected. Return as we do not
   * want to change ClapperPlayerState when seeking or rate changing. */
  if ((player->seeking || player->speed_changing)
      && player->current_state > GST_STATE_READY)
    return;

  if ((eos = (player->pending_eos && player->current_state == GST_STATE_PAUSED)))
    player->pending_eos = FALSE;

  g_atomic_int_set (&player->eos, (gint) eos);

  if (player->current_state <= GST_STATE_READY)
    clapper_player_reset (player, FALSE);

  if (player->current_state == GST_STATE_PLAYING)
    clapper_player_add_tick_source (player);
  else
    clapper_player_remove_tick_source (player);

  /* Notify user about current position either right before or after
   * changed playback (so it does not look like seek after paused) */
  if (player->current_state < old_state)
    clapper_player_refresh_position (player);

  clapper_player_handle_playbin_state_changed (player);

  if (player->current_state > old_state)
    clapper_player_refresh_position (player);

  preroll = (old_state == GST_STATE_READY
      && player->current_state == GST_STATE_PAUSED
      && (pending_state == GST_STATE_VOID_PENDING || pending_state == GST_STATE_PLAYING));

  if (preroll) {
    gdouble speed;

    GST_DEBUG ("Setting cached playbin props after preroll");

    clapper_player_set_volume (player, clapper_player_get_volume (player));
    clapper_player_set_mute (player, clapper_player_get_mute (player));

    speed = clapper_player_get_speed (player);

    /* Playback always starts with normal speed and from zero.
     * When not changed do not post seek event. */
    if (!G_APPROX_VALUE (speed, 1.0, FLT_EPSILON))
      clapper_player_set_speed (player, speed);
    if (!G_APPROX_VALUE (player->pending_position, 0, FLT_EPSILON)) {
      clapper_player_seek (player, player->pending_position);
      player->pending_position = 0;
    }

    _update_current_duration (player);

    if (!player->use_playbin3)
      clapper_player_playbin_update_current_decoders (player);
  }
}

void
clapper_playbin_bus_post_current_item_change (GstBus *bus, ClapperMediaItem *current_item,
    ClapperQueueItemChangeMode mode)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (CURRENT_ITEM_CHANGE),
      _FIELD_QUARK (MEDIA_ITEM), CLAPPER_TYPE_MEDIA_ITEM, current_item,
      _FIELD_QUARK (ITEM_CHANGE_MODE), G_TYPE_ENUM, mode,
      NULL);
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_current_item_change_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  ClapperMediaItem *current_item = NULL;
  ClapperQueueItemChangeMode mode = CLAPPER_QUEUE_ITEM_CHANGE_NORMAL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (MEDIA_ITEM), CLAPPER_TYPE_MEDIA_ITEM, &current_item,
      _FIELD_QUARK (ITEM_CHANGE_MODE), G_TYPE_ENUM, &mode,
      NULL);

  player->pending_position = 0; // We store pending position for played item, so reset

  if (player->current_state < GST_STATE_READY || mode == CLAPPER_QUEUE_ITEM_CHANGE_NORMAL)
    gst_element_set_state (player->playbin, GST_STATE_READY);

  clapper_player_set_pending_item (player, current_item, mode);

  if (!current_item) {
    player->target_state = GST_STATE_READY;
  } else {
    GST_OBJECT_LOCK (player);
    if (player->autoplay)
      player->target_state = GST_STATE_PLAYING;
    GST_OBJECT_UNLOCK (player);
  }

  if ((mode == CLAPPER_QUEUE_ITEM_CHANGE_NORMAL && player->target_state > GST_STATE_READY)
      || player->current_state != player->target_state)
    gst_element_set_state (player->playbin, player->target_state);

  gst_clear_object (&current_item);
}

void
clapper_playbin_bus_post_item_suburi_change (GstBus *bus, ClapperMediaItem *item)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (ITEM_SUBURI_CHANGE),
      _FIELD_QUARK (MEDIA_ITEM), CLAPPER_TYPE_MEDIA_ITEM, item,
      NULL);
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_item_suburi_change_msg (GstMessage *msg, const GstStructure *structure, ClapperPlayer *player)
{
  ClapperMediaItem *item = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (MEDIA_ITEM), CLAPPER_TYPE_MEDIA_ITEM, &item,
      NULL);

  if (item == player->played_item) {
    gst_element_set_state (player->playbin, GST_STATE_READY);
    clapper_player_set_pending_item (player, item, CLAPPER_QUEUE_ITEM_CHANGE_NORMAL);
    gst_element_set_state (player->playbin, player->target_state);
  }

  gst_object_unref (item);
}

void
clapper_playbin_bus_post_stream_change (GstBus *bus)
{
  GstStructure *structure = gst_structure_new_id_empty (_STRUCTURE_QUARK (STREAM_CHANGE));
  gst_bus_post (bus, gst_message_new_application (NULL, structure));
}

static inline void
_handle_stream_change_msg (GstMessage *msg,
    const GstStructure *structure G_GNUC_UNUSED, ClapperPlayer *player)
{
  GST_DEBUG_OBJECT (player, "Requested stream change");

  if (player->use_playbin3) {
    GList *list = NULL;
    ClapperStreamList *vstream_list, *astream_list, *sstream_list;
    ClapperStream *vstream = NULL, *astream = NULL, *sstream = NULL;

    vstream_list = clapper_player_get_video_streams (player);
    if ((vstream = clapper_stream_list_get_current_stream (vstream_list))) {
      GstStream *gst_stream = clapper_stream_get_gst_stream (vstream);
      list = g_list_append (list, (gpointer) gst_stream_get_stream_id (gst_stream));
    }

    astream_list = clapper_player_get_audio_streams (player);
    if ((astream = clapper_stream_list_get_current_stream (astream_list))) {
      GstStream *gst_stream = clapper_stream_get_gst_stream (astream);
      list = g_list_append (list, (gpointer) gst_stream_get_stream_id (gst_stream));
    }

    sstream_list = clapper_player_get_subtitle_streams (player);
    if ((sstream = clapper_stream_list_get_current_stream (sstream_list))) {
      GstStream *gst_stream = clapper_stream_get_gst_stream (sstream);
      list = g_list_append (list, (gpointer) gst_stream_get_stream_id (gst_stream));
    }

    if (list) {
      if (gst_element_send_event (player->playbin, gst_event_new_select_streams (list))
          && player->current_state >= GST_STATE_PAUSED) {
        /* XXX: I am not sure if we "officially" need to flush seek after select
         * streams, but as of GStreamer 1.22 it doesn't work otherwise. */
        player->pending_flush = TRUE;
      }
      g_list_free (list);
    }

    /* Need to hold ref until after event is
     * sent to ensure ID pointer lifespan */
    gst_clear_object (&vstream);
    gst_clear_object (&astream);
    gst_clear_object (&sstream);
  } else {
    ClapperStreamList *stream_list;
    gint current_video = -1, current_audio = -1, current_text = -1;
    guint index;

    g_object_get (player->playbin,
        "current-video", &current_video,
        "current-audio", &current_audio,
        "current-text", &current_text, NULL);

    stream_list = clapper_player_get_video_streams (player);
    index = clapper_stream_list_get_current_index (stream_list);

    if (index != (guint) current_video)
      g_object_set (player->playbin, "current-video", index, NULL);

    stream_list = clapper_player_get_audio_streams (player);
    index = clapper_stream_list_get_current_index (stream_list);

    if (index != (guint) current_audio)
      g_object_set (player->playbin, "current-audio", index, NULL);

    stream_list = clapper_player_get_subtitle_streams (player);
    index = clapper_stream_list_get_current_index (stream_list);

    if (index != (guint) current_text)
      g_object_set (player->playbin, "current-text", index, NULL);
  }
}

static inline void
_handle_app_msg (GstMessage *msg, ClapperPlayer *player)
{
  const GstStructure *structure = gst_message_get_structure (msg);
  GQuark quark = gst_structure_get_name_id (structure);

  if (quark == _STRUCTURE_QUARK (SET_PROP))
    _handle_set_prop_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (SET_PLAY_FLAG))
    _handle_set_play_flag_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (SEEK))
    _handle_seek_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (RATE_CHANGE))
    _handle_rate_change_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (STREAM_CHANGE))
    _handle_stream_change_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (CURRENT_ITEM_CHANGE))
    _handle_current_item_change_msg (msg, structure, player);
  else if (quark == _STRUCTURE_QUARK (ITEM_SUBURI_CHANGE))
    _handle_item_suburi_change_msg (msg, structure, player);
}

static inline void
_handle_element_msg (GstMessage *msg, ClapperPlayer *player)
{
  if (gst_is_missing_plugin_message (msg)) {
    gchar *name, *details;
    guint signal_id;

    name = gst_missing_plugin_message_get_description (msg);
    details = gst_missing_plugin_message_get_installer_detail (msg);
    signal_id = g_signal_lookup ("missing-plugin", CLAPPER_TYPE_PLAYER);

    clapper_app_bus_post_desc_with_details_signal (player->app_bus,
        GST_OBJECT_CAST (player), signal_id, name, details);

    g_free (name);
    g_free (details);
  } else if (gst_message_has_name (msg, "GstCacheDownloadComplete")) {
    ClapperMediaItem *downloaded_item = NULL;
    const GstStructure *structure;
    const gchar *location;
    guint signal_id;

    GST_OBJECT_LOCK (player);

    /* Short video might be fully downloaded before playback starts */
    if (player->pending_item)
      downloaded_item = gst_object_ref (player->pending_item);
    else if (player->played_item)
      downloaded_item = gst_object_ref (player->played_item);

    GST_OBJECT_UNLOCK (player);

    if (G_UNLIKELY (downloaded_item == NULL)) {
      GST_WARNING_OBJECT (player, "Download completed without media item set");
      return;
    }

    structure = gst_message_get_structure (msg);
    location = gst_structure_get_string (structure, "location");
    signal_id = g_signal_lookup ("download-complete", CLAPPER_TYPE_PLAYER);

    GST_INFO_OBJECT (player, "Download of %" GST_PTR_FORMAT
        " complete: %s", downloaded_item, location);
    clapper_media_item_set_cache_location (downloaded_item, location);

    clapper_app_bus_post_object_desc_signal (player->app_bus,
        GST_OBJECT_CAST (player), signal_id,
        GST_OBJECT_CAST (downloaded_item), location);

    gst_object_unref (downloaded_item);
  }
}

static inline void
_handle_tag_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstObject *src = GST_MESSAGE_SRC (msg);
  GstTagList *tags = NULL;
  gboolean from_enhancer_src;

  /* Tag messages should only be posted by sink elements */
  if (G_UNLIKELY (!src))
    return;

  gst_message_parse_tag (msg, &tags);

  GST_LOG_OBJECT (player, "Got tags from element: %s: %" GST_PTR_FORMAT,
      GST_OBJECT_NAME (src), tags);

#if CLAPPER_WITH_ENHANCERS_LOADER
  from_enhancer_src = CLAPPER_IS_ENHANCER_SRC (src);
#else
  from_enhancer_src = FALSE;
#endif

  /* ClapperEnhancerSrc determines tags before stream start */
  if (from_enhancer_src) {
    if (player->pending_tags) {
      gst_tag_list_unref (player->pending_tags);
    }
    player->pending_tags = gst_tag_list_ref (tags);
  } else if (G_LIKELY (player->played_item != NULL)) {
    clapper_media_item_update_from_tag_list (player->played_item, tags, player);
  }

  gst_tag_list_unref (tags);
}

static inline void
_handle_toc_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstObject *src = GST_MESSAGE_SRC (msg);
  GstToc *toc = NULL;
  gboolean from_enhancer_src, updated = FALSE;

  /* TOC messages should only be posted by sink elements */
  if (G_UNLIKELY (!src))
    return;

  /* Either new TOC was found or previous one was updated */
  gst_message_parse_toc (msg, &toc, &updated);

  GST_DEBUG_OBJECT (player, "Got TOC (%" GST_PTR_FORMAT ")"
      " from element: %s, updated: %s",
      toc, GST_OBJECT_NAME (src), (updated) ? "yes" : "no");

#if CLAPPER_WITH_ENHANCERS_LOADER
  from_enhancer_src = CLAPPER_IS_ENHANCER_SRC (src);
#else
  from_enhancer_src = FALSE;
#endif

  /* ClapperEnhancerSrc determines TOC before stream start */
  if (from_enhancer_src) {
    if (player->pending_toc) {
      gst_toc_unref (player->pending_toc);
    }
    player->pending_toc = gst_toc_ref (toc);
  } else if (G_LIKELY (player->played_item != NULL)) {
    ClapperTimeline *timeline;

    timeline = clapper_media_item_get_timeline (player->played_item);

    if (clapper_timeline_set_toc (timeline, toc, updated)) {
      clapper_app_bus_post_refresh_timeline (player->app_bus,
          GST_OBJECT_CAST (player->played_item));
    }
  }

  gst_toc_unref (toc);
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
  else if (strcmp (prop_name, "flags") == 0)
    clapper_player_handle_playbin_flags_changed (player, value);
  else if (strcmp (prop_name, "av-offset") == 0)
    clapper_player_handle_playbin_av_offset_changed (player, value);
  else if (strcmp (prop_name, "text-offset") == 0)
    clapper_player_handle_playbin_text_offset_changed (player, value);
  else
    clapper_player_handle_playbin_common_prop_changed (player, prop_name);
}

static inline void
_handle_stream_collection_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstStreamCollection *collection = NULL;

  GST_INFO_OBJECT (player, "Stream collection");

  gst_message_parse_stream_collection (msg, &collection);
  clapper_player_take_stream_collection (player, collection);
}

static inline void
_handle_streams_selected_msg (GstMessage *msg, ClapperPlayer *player)
{
  /* NOTE: Streams selected message carries whole collection
   * and allows reading actually selected streams from it
   * via gst_message_streams_selected_* methods */

  GST_INFO_OBJECT (player, "Streams selected");

  if (player->use_playbin3) {
    guint i, n_streams = gst_message_streams_selected_get_size (msg);

    for (i = 0; i < n_streams; ++i) {
      GstStream *stream = gst_message_streams_selected_get_stream (msg, i);
      GstStreamType stream_type = gst_stream_get_stream_type (stream);

      if ((stream_type & GST_STREAM_TYPE_VIDEO) == GST_STREAM_TYPE_VIDEO) {
        if (!clapper_player_find_active_decoder_with_stream_id (player,
            GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, gst_stream_get_stream_id (stream)))
          GST_DEBUG_OBJECT (player, "Active video decoder not found");
      } else if ((stream_type & GST_STREAM_TYPE_AUDIO) == GST_STREAM_TYPE_AUDIO) {
        if (!clapper_player_find_active_decoder_with_stream_id (player,
            GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, gst_stream_get_stream_id (stream)))
          GST_DEBUG_OBJECT (player, "Active audio decoder not found");
      }
    }
  } else {
    /* In playbin2 we do not know real stream IDs, so
     * we iterate in search for all active ones */
    clapper_player_playbin_update_current_decoders (player);
  }

  if (player->pending_flush) {
    player->pending_flush = FALSE;

    if (player->current_state >= GST_STATE_PAUSED)
      _perform_flush_seek (player);
  }
}

static inline void
_handle_stream_start_msg (GstMessage *msg, ClapperPlayer *player)
{
  guint group = 0;
  gboolean changed;

  /* We only care about our parent bin start which
   * happens after all sinks have started */
  if (GST_MESSAGE_SRC (msg) != GST_OBJECT_CAST (player->playbin))
    return;

  if (!gst_message_parse_group_id (msg, &group))
    return;

  GST_INFO_OBJECT (player, "Stream start, group: %u", group);

  GST_OBJECT_LOCK (player);

  /* This should never happen, but better be safe */
  if (G_UNLIKELY (player->pending_item == NULL)) {
    GST_ERROR_OBJECT (player, "Starting some stream, but there was no pending one!");
    GST_OBJECT_UNLOCK (player);

    return;
  }

  changed = gst_object_replace ((GstObject **) &player->played_item, GST_OBJECT_CAST (player->pending_item));
  gst_clear_object (&player->pending_item);

  GST_OBJECT_UNLOCK (player);

  if (G_LIKELY (changed)) {
    clapper_queue_handle_played_item_changed (player->queue, player->played_item, player->app_bus);

    if (clapper_player_get_have_features (player))
      clapper_features_manager_trigger_played_item_changed (player->features_manager, player->played_item);
  }

  clapper_app_bus_post_refresh_streams (player->app_bus, GST_OBJECT_CAST (player));

  /* Update position on start after announcing item change,
   * since we will not do this on state change when gapless */
  clapper_player_refresh_position (player);

  /* With playbin2 we update all decoders at once after stream start */
  if (!player->use_playbin3)
    clapper_player_playbin_update_current_decoders (player);

  if (player->pending_tags) {
    if (G_LIKELY (player->played_item != NULL))
      clapper_media_item_update_from_tag_list (player->played_item, player->pending_tags, player);

    gst_clear_tag_list (&player->pending_tags);
  }
  if (player->pending_toc) {
    if (G_LIKELY (player->played_item != NULL)) {
      ClapperTimeline *timeline = clapper_media_item_get_timeline (player->played_item);

      if (clapper_timeline_set_toc (timeline, player->pending_toc, FALSE)) {
        clapper_app_bus_post_refresh_timeline (player->app_bus,
            GST_OBJECT_CAST (player->played_item));
      }
    }

    gst_toc_unref (player->pending_toc);
    player->pending_toc = NULL;
  }
}

static inline void
_handle_duration_changed_msg (GstMessage *msg G_GNUC_UNUSED, ClapperPlayer *player)
{
  _update_current_duration (player);
}

static inline void
_handle_async_done_msg (GstMessage *msg G_GNUC_UNUSED, ClapperPlayer *player)
{
  if (player->seeking) {
    guint signal_id;

    player->seeking = FALSE;

    GST_DEBUG_OBJECT (player, "Seek done");
    signal_id = g_signal_lookup ("seek-done", CLAPPER_TYPE_PLAYER);

    /* Update current position first, then announce seek done */
    clapper_player_refresh_position (player);
    clapper_app_bus_post_simple_signal (player->app_bus,
        GST_OBJECT_CAST (player), signal_id);
  }
  if (player->speed_changing) {
    if (player->pending_speed != 0) {
      GST_DEBUG_OBJECT (player, "Changing rate to pending value: %.2lf -> %.2lf",
          player->speed, player->pending_speed);
      clapper_player_set_speed (player, player->pending_speed);
      player->pending_speed = 0;
    } else {
      clapper_player_handle_playbin_rate_changed (player, player->requested_speed);
      player->speed_changing = FALSE;
    }
    player->requested_speed = 0;
  }
}

static inline void
_handle_latency_msg (GstMessage *msg G_GNUC_UNUSED, ClapperPlayer *player)
{
  GST_LOG_OBJECT (player, "Latency changed");
  gst_bin_recalculate_latency (GST_BIN_CAST (player->playbin));
}

static inline void
_handle_clock_lost_msg (GstMessage *msg, ClapperPlayer *player)
{
  GstStateChangeReturn ret;

  if (player->target_state != GST_STATE_PLAYING)
    return;

  GST_DEBUG_OBJECT (player, "Clock lost");

  ret = gst_element_set_state (player->playbin, GST_STATE_PAUSED);
  if (ret != GST_STATE_CHANGE_FAILURE)
    ret = gst_element_set_state (player->playbin, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    GstMessage *msg;
    GError *error;

    error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_STATE_CHANGE,
        "Could not recover with changing state after clock was lost");
    msg = gst_message_new_error (GST_OBJECT (player), error, NULL);

    _handle_error_msg (msg, player);

    g_error_free (error);
    gst_message_unref (msg);
  }
}

static inline void
_handle_eos_msg (GstMessage *msg G_GNUC_UNUSED, ClapperPlayer *player)
{
  gboolean had_error;

  /* EOS happens after "about-to-finish" if URI did not change.
   * Changing items should be done in former one while pausing
   * after playback here. */

  GST_INFO_OBJECT (player, "EOS");

  /* This is also used in another thread */
  GST_OBJECT_LOCK (player);
  had_error = player->had_error;
  GST_OBJECT_UNLOCK (player);

  /* Error handling already changes state to READY */
  if (G_UNLIKELY (had_error))
    return;

  if (!clapper_queue_handle_eos (player->queue, player)) {
    player->pending_eos = TRUE;
    gst_element_set_state (player->playbin, GST_STATE_PAUSED);
  }
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
    case GST_MESSAGE_TOC:
      _handle_toc_msg (msg, player);
      break;
    case GST_MESSAGE_PROPERTY_NOTIFY:
      _handle_property_notify_msg (msg, player);
      break;
    case GST_MESSAGE_STREAM_COLLECTION:
      _handle_stream_collection_msg (msg, player);
      break;
    case GST_MESSAGE_STREAMS_SELECTED:
      _handle_streams_selected_msg (msg, player);
      break;
    case GST_MESSAGE_STREAM_START:
      _handle_stream_start_msg (msg, player);
      break;
    case GST_MESSAGE_DURATION_CHANGED:
      _handle_duration_changed_msg (msg, player);
      break;
    case GST_MESSAGE_ASYNC_DONE:
      _handle_async_done_msg (msg, player);
      break;
    case GST_MESSAGE_LATENCY:
      _handle_latency_msg (msg, player);
      break;
    case GST_MESSAGE_CLOCK_LOST:
      _handle_clock_lost_msg (msg, player);
      break;
    case GST_MESSAGE_EOS:
      _handle_eos_msg (msg, player);
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
