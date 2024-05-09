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

#pragma once

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <clapper/clapper-threaded-object.h>
#include <clapper/clapper-queue.h>
#include <clapper/clapper-stream-list.h>
#include <clapper/clapper-feature.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_PLAYER (clapper_player_get_type())
#define CLAPPER_PLAYER_CAST(obj) ((ClapperPlayer *)(obj))

G_DECLARE_FINAL_TYPE (ClapperPlayer, clapper_player, CLAPPER, PLAYER, ClapperThreadedObject)

ClapperPlayer * clapper_player_new (void);

ClapperQueue * clapper_player_get_queue (ClapperPlayer *player);

ClapperStreamList * clapper_player_get_video_streams (ClapperPlayer *player);

ClapperStreamList * clapper_player_get_audio_streams (ClapperPlayer *player);

ClapperStreamList * clapper_player_get_subtitle_streams (ClapperPlayer *player);

void clapper_player_set_autoplay (ClapperPlayer *player, gboolean enabled);

gboolean clapper_player_get_autoplay (ClapperPlayer *player);

gdouble clapper_player_get_position (ClapperPlayer *player);

ClapperPlayerState clapper_player_get_state (ClapperPlayer *player);

void clapper_player_set_mute (ClapperPlayer *player, gboolean mute);

gboolean clapper_player_get_mute (ClapperPlayer *player);

void clapper_player_set_volume (ClapperPlayer *player, gdouble volume);

gdouble clapper_player_get_volume (ClapperPlayer *player);

void clapper_player_set_speed (ClapperPlayer *player, gdouble speed);

gdouble clapper_player_get_speed (ClapperPlayer *player);

void clapper_player_set_video_sink (ClapperPlayer *player, GstElement *element);

GstElement * clapper_player_get_video_sink (ClapperPlayer *player);

void clapper_player_set_audio_sink (ClapperPlayer *player, GstElement *element);

GstElement * clapper_player_get_audio_sink (ClapperPlayer *player);

void clapper_player_set_video_filter (ClapperPlayer *player, GstElement *element);

GstElement * clapper_player_get_video_filter (ClapperPlayer *player);

void clapper_player_set_audio_filter (ClapperPlayer *player, GstElement *element);

GstElement * clapper_player_get_audio_filter (ClapperPlayer *player);

GstElement * clapper_player_get_current_video_decoder (ClapperPlayer *player);

GstElement * clapper_player_get_current_audio_decoder (ClapperPlayer *player);

void clapper_player_set_video_enabled (ClapperPlayer *player, gboolean enabled);

gboolean clapper_player_get_video_enabled (ClapperPlayer *player);

void clapper_player_set_audio_enabled (ClapperPlayer *player, gboolean enabled);

gboolean clapper_player_get_audio_enabled (ClapperPlayer *player);

void clapper_player_set_subtitles_enabled (ClapperPlayer *player, gboolean enabled);

gboolean clapper_player_get_subtitles_enabled (ClapperPlayer *player);

void clapper_player_set_download_dir (ClapperPlayer *player, const gchar *path);

gchar * clapper_player_get_download_dir (ClapperPlayer *player);

void clapper_player_set_download_enabled (ClapperPlayer *player, gboolean enabled);

gboolean clapper_player_get_download_enabled (ClapperPlayer *player);

void clapper_player_set_audio_offset (ClapperPlayer *player, gdouble offset);

gdouble clapper_player_get_audio_offset (ClapperPlayer *player);

void clapper_player_set_subtitle_offset (ClapperPlayer *player, gdouble offset);

gdouble clapper_player_get_subtitle_offset (ClapperPlayer *player);

void clapper_player_set_subtitle_font_desc (ClapperPlayer *player, const gchar *font_desc);

gchar * clapper_player_get_subtitle_font_desc (ClapperPlayer *player);

void clapper_player_play (ClapperPlayer *player);

void clapper_player_pause (ClapperPlayer *player);

void clapper_player_stop (ClapperPlayer *player);

void clapper_player_seek (ClapperPlayer *player, gdouble position);

void clapper_player_seek_custom (ClapperPlayer *player, gdouble position, ClapperPlayerSeekMethod method);

void clapper_player_add_feature (ClapperPlayer *player, ClapperFeature *feature);

G_END_DECLS
