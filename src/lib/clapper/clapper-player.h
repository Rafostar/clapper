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

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-threaded-object.h>
#include <clapper/clapper-queue.h>
#include <clapper/clapper-stream-list.h>
#include <clapper/clapper-feature.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_PLAYER (clapper_player_get_type())
#define CLAPPER_PLAYER_CAST(obj) ((ClapperPlayer *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperPlayer, clapper_player, CLAPPER, PLAYER, ClapperThreadedObject)

CLAPPER_API
ClapperPlayer * clapper_player_new (void);

CLAPPER_API
ClapperQueue * clapper_player_get_queue (ClapperPlayer *player);

CLAPPER_API
ClapperStreamList * clapper_player_get_video_streams (ClapperPlayer *player);

CLAPPER_API
ClapperStreamList * clapper_player_get_audio_streams (ClapperPlayer *player);

CLAPPER_API
ClapperStreamList * clapper_player_get_subtitle_streams (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_autoplay (ClapperPlayer *player, gboolean enabled);

CLAPPER_API
gboolean clapper_player_get_autoplay (ClapperPlayer *player);

CLAPPER_API
gdouble clapper_player_get_position (ClapperPlayer *player);

CLAPPER_API
ClapperPlayerState clapper_player_get_state (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_mute (ClapperPlayer *player, gboolean mute);

CLAPPER_API
gboolean clapper_player_get_mute (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_volume (ClapperPlayer *player, gdouble volume);

CLAPPER_API
gdouble clapper_player_get_volume (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_speed (ClapperPlayer *player, gdouble speed);

CLAPPER_API
gdouble clapper_player_get_speed (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_video_sink (ClapperPlayer *player, GstElement *element);

CLAPPER_API
GstElement * clapper_player_get_video_sink (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_audio_sink (ClapperPlayer *player, GstElement *element);

CLAPPER_API
GstElement * clapper_player_get_audio_sink (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_video_filter (ClapperPlayer *player, GstElement *element);

CLAPPER_API
GstElement * clapper_player_get_video_filter (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_audio_filter (ClapperPlayer *player, GstElement *element);

CLAPPER_API
GstElement * clapper_player_get_audio_filter (ClapperPlayer *player);

CLAPPER_API
GstElement * clapper_player_get_current_video_decoder (ClapperPlayer *player);

CLAPPER_API
GstElement * clapper_player_get_current_audio_decoder (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_video_enabled (ClapperPlayer *player, gboolean enabled);

CLAPPER_API
gboolean clapper_player_get_video_enabled (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_audio_enabled (ClapperPlayer *player, gboolean enabled);

CLAPPER_API
gboolean clapper_player_get_audio_enabled (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_subtitles_enabled (ClapperPlayer *player, gboolean enabled);

CLAPPER_API
gboolean clapper_player_get_subtitles_enabled (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_download_dir (ClapperPlayer *player, const gchar *path);

CLAPPER_API
gchar * clapper_player_get_download_dir (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_download_enabled (ClapperPlayer *player, gboolean enabled);

CLAPPER_API
gboolean clapper_player_get_download_enabled (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_adaptive_start_bitrate (ClapperPlayer *player, guint bitrate);

CLAPPER_API
guint clapper_player_get_adaptive_start_bitrate (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_adaptive_min_bitrate (ClapperPlayer *player, guint bitrate);

CLAPPER_API
guint clapper_player_get_adaptive_min_bitrate (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_adaptive_max_bitrate (ClapperPlayer *player, guint bitrate);

CLAPPER_API
guint clapper_player_get_adaptive_max_bitrate (ClapperPlayer *player);

CLAPPER_API
guint clapper_player_get_adaptive_bandwidth (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_audio_offset (ClapperPlayer *player, gdouble offset);

CLAPPER_API
gdouble clapper_player_get_audio_offset (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_subtitle_offset (ClapperPlayer *player, gdouble offset);

CLAPPER_API
gdouble clapper_player_get_subtitle_offset (ClapperPlayer *player);

CLAPPER_API
void clapper_player_set_subtitle_font_desc (ClapperPlayer *player, const gchar *font_desc);

CLAPPER_API
gchar * clapper_player_get_subtitle_font_desc (ClapperPlayer *player);

CLAPPER_API
void clapper_player_play (ClapperPlayer *player);

CLAPPER_API
void clapper_player_pause (ClapperPlayer *player);

CLAPPER_API
void clapper_player_stop (ClapperPlayer *player);

CLAPPER_API
void clapper_player_seek (ClapperPlayer *player, gdouble position);

CLAPPER_API
void clapper_player_seek_custom (ClapperPlayer *player, gdouble position, ClapperPlayerSeekMethod method);

CLAPPER_API
void clapper_player_add_feature (ClapperPlayer *player, ClapperFeature *feature);

G_END_DECLS
