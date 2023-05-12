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

#pragma once

#include <clapper/clapper-player.h>
#include <clapper/clapper-queue.h>
#include <clapper/clapper-enums.h>

#include "clapper-app-bus-private.h"
#include "clapper-features-manager-private.h"

G_BEGIN_DECLS

#define clapper_player_set_have_features(player,have) (g_atomic_int_set (&player->have_features, (gint) have))
#define clapper_player_get_have_features(player) (g_atomic_int_get (&player->have_features) == 1)

struct _ClapperPlayer
{
  ClapperThreadedObject parent;

  ClapperQueue *queue;

  ClapperFeaturesManager *features_manager;
  gint have_features; // atomic integer

  /* This is different from queue current item as it is used/changed only
   * on player thread, so we can always update correct item without lock */
  ClapperMediaItem *played_item;

  GstElement *playbin;

  GstBus *bus;
  ClapperAppBus *app_bus;

  GSource *tick_source;

  /* Must only be used from player thread */
  GstState current_state; // reported from playbin
  GstState target_state;  // state requested by user
  gboolean is_buffering;
  gfloat pending_position; // store seek before playback

  gchar *video_sid;
  gchar *audio_sid;
  gchar *subtitle_sid;

  /* Extra params */
  gboolean had_error; // so we do not do stuff after error

  /* Props */
  gboolean mute;
  gfloat volume;
  gfloat speed;
  gfloat position;
  ClapperPlayerState state;
  GstElement *video_decoder;
  GstElement *audio_decoder;
};

ClapperPlayer * clapper_player_get_from_ancestor (GstObject *object);

gboolean clapper_player_query_position (ClapperPlayer *player);

void clapper_player_add_tick_source (ClapperPlayer *player);

void clapper_player_remove_tick_source (ClapperPlayer *player);

void clapper_player_handle_playbin_state_changed (ClapperPlayer *player);

void clapper_player_handle_playbin_volume_changed (ClapperPlayer *player, const GValue *value);

void clapper_player_handle_playbin_mute_changed (ClapperPlayer *player, const GValue *value);

void clapper_player_handle_playbin_common_prop_changed (ClapperPlayer *player, const gchar *prop_name);

void clapper_player_handle_playbin_rate_changed (ClapperPlayer *player, gfloat speed);

void clapper_player_handle_playbin_video_decoder_changed (ClapperPlayer *player, GstElement *element);

void clapper_player_handle_playbin_audio_decoder_changed (ClapperPlayer *player, GstElement *element);

void clapper_player_reset (ClapperPlayer *player, gboolean pending_dispose);

G_END_DECLS
