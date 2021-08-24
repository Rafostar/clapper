/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_CLAPPER_H__
#define __GST_CLAPPER_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <gst/clapper/clapper-prelude.h>
#include <gst/clapper/gstclapper-types.h>
#include <gst/clapper/gstclapper-signal-dispatcher.h>
#include <gst/clapper/gstclapper-video-renderer.h>
#include <gst/clapper/gstclapper-media-info.h>
#include <gst/clapper/gstclapper-mpris.h>

G_BEGIN_DECLS

/* ClapperState */
GST_CLAPPER_API
GType         gst_clapper_state_get_type                (void);
#define       GST_TYPE_CLAPPER_STATE                    (gst_clapper_state_get_type ())

/**
 * GstClapperState:
 * @GST_CLAPPER_STATE_STOPPED: clapper is stopped.
 * @GST_CLAPPER_STATE_BUFFERING: clapper is buffering.
 * @GST_CLAPPER_STATE_PAUSED: clapper is paused.
 * @GST_CLAPPER_STATE_PLAYING: clapper is currently playing a stream.
 */
typedef enum
{
  GST_CLAPPER_STATE_STOPPED,
  GST_CLAPPER_STATE_BUFFERING,
  GST_CLAPPER_STATE_PAUSED,
  GST_CLAPPER_STATE_PLAYING
} GstClapperState;

GST_CLAPPER_API
const gchar * gst_clapper_state_get_name                (GstClapperState state);

/* ClapperSeekMode */
GST_CLAPPER_API
GType gst_clapper_seek_mode_get_type                    (void);
#define GST_TYPE_CLAPPER_SEEK_MODE                      (gst_clapper_seek_mode_get_type ())

/**
 * GstClapperSeekMode:
 * @GST_CLAPPER_SEEK_MODE_DEFAULT: default seek method (flush only).
 * @GST_CLAPPER_SEEK_MODE_ACCURATE: accurate seek method.
 * @GST_CLAPPER_SEEK_MODE_FAST: fast seek method (next keyframe).
 */
typedef enum
{
  GST_CLAPPER_SEEK_MODE_DEFAULT,
  GST_CLAPPER_SEEK_MODE_ACCURATE,
  GST_CLAPPER_SEEK_MODE_FAST,
} GstClapperSeekMode;

/* ClapperError */
GST_CLAPPER_API
GQuark        gst_clapper_error_quark                   (void);

GST_CLAPPER_API
GType         gst_clapper_error_get_type                (void);
#define       GST_CLAPPER_ERROR                         (gst_clapper_error_quark ())
#define       GST_TYPE_CLAPPER_ERROR                    (gst_clapper_error_get_type ())

/**
 * GstClapperError:
 * @GST_CLAPPER_ERROR_FAILED: generic error.
 */
typedef enum {
  GST_CLAPPER_ERROR_FAILED = 0
} GstClapperError;

GST_CLAPPER_API
const gchar * gst_clapper_error_get_name                (GstClapperError error);

/* ClapperColorBalanceType */
GST_CLAPPER_API
GType gst_clapper_color_balance_type_get_type           (void);
#define GST_TYPE_CLAPPER_COLOR_BALANCE_TYPE             (gst_clapper_color_balance_type_get_type ())

/**
 * GstClapperColorBalanceType:
 * @GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS: brightness or black level.
 * @GST_CLAPPER_COLOR_BALANCE_CONTRAST: contrast or luma gain.
 * @GST_CLAPPER_COLOR_BALANCE_SATURATION: color saturation or chroma
 * gain.
 * @GST_CLAPPER_COLOR_BALANCE_HUE: hue or color balance.
 */
typedef enum
{
  GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS,
  GST_CLAPPER_COLOR_BALANCE_CONTRAST,
  GST_CLAPPER_COLOR_BALANCE_SATURATION,
  GST_CLAPPER_COLOR_BALANCE_HUE,
} GstClapperColorBalanceType;

GST_CLAPPER_API
const gchar * gst_clapper_color_balance_type_get_name   (GstClapperColorBalanceType type);

/* ClapperSnapshotFormat */

/**
 * GstClapperSnapshotFormat:
 * @GST_CLAPPER_THUMBNAIL_RAW_NATIVE: RAW Native.
 * @GST_CLAPPER_THUMBNAIL_RAW_xRGB: RAW xRGB.
 * @GST_CLAPPER_THUMBNAIL_RAW_BGRx: RAW BGRx.
 * @GST_CLAPPER_THUMBNAIL_JPG: JPG.
 * @GST_CLAPPER_THUMBNAIL_PNG: PNG.
 */
typedef enum
{
  GST_CLAPPER_THUMBNAIL_RAW_NATIVE = 0,
  GST_CLAPPER_THUMBNAIL_RAW_xRGB,
  GST_CLAPPER_THUMBNAIL_RAW_BGRx,
  GST_CLAPPER_THUMBNAIL_JPG,
  GST_CLAPPER_THUMBNAIL_PNG
} GstClapperSnapshotFormat;

#define GST_TYPE_CLAPPER             (gst_clapper_get_type ())
#define GST_IS_CLAPPER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER))
#define GST_IS_CLAPPER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER))
#define GST_CLAPPER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER, GstClapperClass))
#define GST_CLAPPER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER, GstClapper))
#define GST_CLAPPER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER, GstClapperClass))
#define GST_CLAPPER_CAST(obj)        ((GstClapper*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstClapper, gst_object_unref)
#endif

GST_CLAPPER_API
GType        gst_clapper_get_type                           (void);

GST_CLAPPER_API
void         gst_clapper_gst_init                           (int *argc, char **argv[]);

GST_CLAPPER_API
GstClapper * gst_clapper_new                                (GstClapperVideoRenderer *video_renderer, GstClapperSignalDispatcher *signal_dispatcher,
                                                                GstClapperMpris *mpris);

GST_CLAPPER_API
void         gst_clapper_play                               (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_pause                              (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_toggle_play                        (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_stop                               (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_seek                               (GstClapper *clapper, GstClockTime position);

GST_CLAPPER_API
void         gst_clapper_seek_offset                        (GstClapper *clapper, GstClockTime offset);

GST_CLAPPER_API
GstClapperState
             gst_clapper_get_state                          (GstClapper *clapper);

GST_CLAPPER_API
GstClapperSeekMode
             gst_clapper_get_seek_mode                      (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_seek_mode                      (GstClapper *clapper, GstClapperSeekMode mode);

GST_CLAPPER_API
void         gst_clapper_set_rate                           (GstClapper *clapper, gdouble rate);

GST_CLAPPER_API
gdouble      gst_clapper_get_rate                           (GstClapper *clapper);

GST_CLAPPER_API
gchar *      gst_clapper_get_uri                            (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_uri                            (GstClapper *clapper, const gchar *uri);

GST_CLAPPER_API
gchar *      gst_clapper_get_subtitle_uri                   (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_subtitle_uri                   (GstClapper *clapper, const gchar *uri);

GST_CLAPPER_API
GstClockTime gst_clapper_get_position                       (GstClapper *clapper);

GST_CLAPPER_API
GstClockTime gst_clapper_get_duration                       (GstClapper *clapper);

GST_CLAPPER_API
gdouble      gst_clapper_get_volume                         (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_volume                         (GstClapper *clapper, gdouble val);

GST_CLAPPER_API
gboolean     gst_clapper_get_mute                           (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_mute                           (GstClapper *clapper, gboolean val);

GST_CLAPPER_API
GstElement * gst_clapper_get_pipeline                       (GstClapper *clapper);

GST_CLAPPER_API
GstClapperMpris *
             gst_clapper_get_mpris                          (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_video_track_enabled            (GstClapper *clapper, gboolean enabled);

GST_CLAPPER_API
void         gst_clapper_set_audio_track_enabled            (GstClapper *clapper, gboolean enabled);

GST_CLAPPER_API
void         gst_clapper_set_subtitle_track_enabled         (GstClapper *clapper, gboolean enabled);

GST_CLAPPER_API
gboolean     gst_clapper_set_audio_track                    (GstClapper *clapper, gint stream_index);

GST_CLAPPER_API
gboolean     gst_clapper_set_video_track                    (GstClapper *clapper, gint stream_index);

GST_CLAPPER_API
gboolean     gst_clapper_set_subtitle_track                 (GstClapper *clapper, gint stream_index);

GST_CLAPPER_API
GstClapperMediaInfo *
             gst_clapper_get_media_info                     (GstClapper *clapper);

GST_CLAPPER_API
GstClapperAudioInfo *
             gst_clapper_get_current_audio_track            (GstClapper *clapper);

GST_CLAPPER_API
GstClapperVideoInfo *
             gst_clapper_get_current_video_track            (GstClapper *clapper);

GST_CLAPPER_API
GstClapperSubtitleInfo *
             gst_clapper_get_current_subtitle_track         (GstClapper *clapper);

GST_CLAPPER_API
gboolean     gst_clapper_set_visualization                  (GstClapper *clapper, const gchar *name);

GST_CLAPPER_API
void         gst_clapper_set_visualization_enabled          (GstClapper *clapper, gboolean enabled);

GST_CLAPPER_API
gchar *      gst_clapper_get_current_visualization          (GstClapper *clapper);

GST_CLAPPER_API
gboolean     gst_clapper_has_color_balance                  (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_color_balance                  (GstClapper *clapper, GstClapperColorBalanceType type, gdouble value);

GST_CLAPPER_API
gdouble      gst_clapper_get_color_balance                  (GstClapper *clapper, GstClapperColorBalanceType type);

GST_CLAPPER_API
GstVideoMultiviewFramePacking
             gst_clapper_get_multiview_mode                 (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_multiview_mode                 (GstClapper *clapper, GstVideoMultiviewFramePacking mode);

GST_CLAPPER_API
GstVideoMultiviewFlags
             gst_clapper_get_multiview_flags                (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_multiview_flags                (GstClapper *clapper, GstVideoMultiviewFlags flags);

GST_CLAPPER_API
gint64       gst_clapper_get_audio_video_offset             (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_audio_video_offset             (GstClapper *clapper, gint64 offset);

GST_CLAPPER_API
gint64       gst_clapper_get_subtitle_video_offset          (GstClapper *clapper);

GST_CLAPPER_API
void         gst_clapper_set_subtitle_video_offset          (GstClapper *clapper, gint64 offset);

GST_CLAPPER_API
GstSample *  gst_clapper_get_video_snapshot                 (GstClapper *clapper, GstClapperSnapshotFormat format, const GstStructure *config);

G_END_DECLS

#endif /* __GST_CLAPPER_H__ */
