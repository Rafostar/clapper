/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
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

/**
 * SECTION:gstclapper
 * @title: GstClapper
 * @short_description: Clapper
 * @symbols:
 * - GstClapper
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/colorbalance.h>
#include <gst/audio/streamvolume.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/descriptions.h>

#include <string.h>

#include "gstclapper.h"
#include "gstclapper-signal-dispatcher-private.h"
#include "gstclapper-video-renderer-private.h"
#include "gstclapper-media-info-private.h"
#include "gstclapper-mpris-private.h"

GST_DEBUG_CATEGORY_STATIC (gst_clapper_debug);
#define GST_CAT_DEFAULT gst_clapper_debug

#define DEFAULT_USE_PLAYBIN3 FALSE
#define DEFAULT_USE_PIPEWIRE FALSE
#define DEFAULT_STATE GST_CLAPPER_STATE_STOPPED
#define DEFAULT_URI NULL
#define DEFAULT_POSITION GST_CLOCK_TIME_NONE
#define DEFAULT_DURATION GST_CLOCK_TIME_NONE
#define DEFAULT_VOLUME 1.0
#define DEFAULT_MUTE FALSE
#define DEFAULT_RATE 1.0
#define DEFAULT_POSITION_UPDATE_INTERVAL_MS 1000
#define DEFAULT_AUDIO_VIDEO_OFFSET 0
#define DEFAULT_SUBTITLE_VIDEO_OFFSET 0
#define DEFAULT_SEEK_MODE GST_CLAPPER_SEEK_MODE_DEFAULT

static gboolean gst_clapper_gstreamer_prepared = FALSE;

/**
 * gst_clapper_error_quark:
 */
GQuark
gst_clapper_error_quark (void)
{
  return g_quark_from_static_string ("gst-clapper-error-quark");
}

enum
{
  PROP_0,
  PROP_VIDEO_RENDERER,
  PROP_SIGNAL_DISPATCHER,
  PROP_MPRIS,
  PROP_USE_PLAYBIN3,
  PROP_USE_PIPEWIRE,
  PROP_STATE,
  PROP_URI,
  PROP_SUBURI,
  PROP_POSITION,
  PROP_DURATION,
  PROP_MEDIA_INFO,
  PROP_CURRENT_AUDIO_TRACK,
  PROP_CURRENT_VIDEO_TRACK,
  PROP_CURRENT_SUBTITLE_TRACK,
  PROP_VOLUME,
  PROP_MUTE,
  PROP_RATE,
  PROP_PIPELINE,
  PROP_VIDEO_MULTIVIEW_MODE,
  PROP_VIDEO_MULTIVIEW_FLAGS,
  PROP_AUDIO_VIDEO_OFFSET,
  PROP_SUBTITLE_VIDEO_OFFSET,
  PROP_SEEK_MODE,
  PROP_LAST
};

enum
{
  SIGNAL_URI_LOADED,
  SIGNAL_POSITION_UPDATED,
  SIGNAL_DURATION_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_BUFFERING,
  SIGNAL_END_OF_STREAM,
  SIGNAL_ERROR,
  SIGNAL_WARNING,
  SIGNAL_VIDEO_DIMENSIONS_CHANGED,
  SIGNAL_MEDIA_INFO_UPDATED,
  SIGNAL_VIDEO_DECODER_CHANGED,
  SIGNAL_AUDIO_DECODER_CHANGED,
  SIGNAL_LAST
};

enum
{
  GST_PLAY_FLAG_VIDEO = (1 << 0),
  GST_PLAY_FLAG_AUDIO = (1 << 1),
  GST_PLAY_FLAG_SUBTITLE = (1 << 2),
  GST_PLAY_FLAG_VIS = (1 << 3)
};

struct _GstClapper
{
  GstObject parent;

  GstClapperVideoRenderer *video_renderer;
  GstClapperSignalDispatcher *signal_dispatcher;
  GstClapperMpris *mpris;

  gchar *uri;
  gchar *redirect_uri;
  gchar *suburi;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;

  GstElement *playbin;
  GstBus *bus;
  GstState target_state, current_state;
  gboolean is_live;
  GSource *tick_source;
  GstClockTime cached_duration;

  gdouble rate;

  /* Prevent unnecessary signals emissions */
  gdouble last_volume;
  gboolean last_mute;

  GstClapperState app_state;
  gint buffering;

  GstTagList *global_tags;
  GstToc *global_toc;
  GstClapperMediaInfo *media_info;

  GstElement *current_vis_element;

  GstClapperSeekMode seek_mode;

  /* Protected by lock */
  gboolean seek_pending;        /* Only set from main context */
  GstClockTime last_seek_time;  /* Only set from main context */
  GSource *seek_source;
  GstClockTime seek_position;
  /* If TRUE, all signals are inhibited except the
   * state-changed:GST_CLAPPER_STATE_STOPPED/PAUSED. This ensures that no signal
   * is emitted after gst_clapper_stop/pause() has been called by the user. */
  gboolean inhibit_sigs;

  /* If TRUE, player is in initial ready state after
   * new media was loaded and it can be played */
  gboolean can_start;

  /* If should emit media info updated signal */
  gboolean needs_info_update;

  /* Prevent notify with the same decoders */
  gchar *last_vdecoder;
  gchar *last_adecoder;

  /* For playbin3 */
  gboolean use_playbin3;
  GstStreamCollection *collection;
  gchar *video_sid;
  gchar *audio_sid;
  gchar *subtitle_sid;
  gulong stream_notify_id;

  gboolean use_pipewire;
};

struct _GstClapperClass
{
  GstObjectClass parent_class;
};

#define parent_class gst_clapper_parent_class
G_DEFINE_TYPE (GstClapper, gst_clapper, GST_TYPE_OBJECT);

static guint signals[SIGNAL_LAST] = { 0, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_clapper_dispose (GObject * object);
static void gst_clapper_finalize (GObject * object);
static void gst_clapper_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_clapper_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_clapper_constructed (GObject * object);

static gpointer gst_clapper_main (gpointer data);

static void gst_clapper_seek_internal_locked (GstClapper * self);
static void gst_clapper_stop_internal (GstClapper * self, gboolean transient);
static gboolean gst_clapper_pause_internal (gpointer user_data);
static gboolean gst_clapper_play_internal (gpointer user_data);
static gboolean gst_clapper_seek_internal (gpointer user_data);
static void gst_clapper_set_rate_internal (GstClapper * self);
static void change_state (GstClapper * self, GstClapperState state);

static GstClapperMediaInfo *gst_clapper_media_info_create (GstClapper * self);

static void gst_clapper_streams_info_create (GstClapper * self,
    GstClapperMediaInfo * media_info, const gchar * prop, GType type);
static void gst_clapper_stream_info_update (GstClapper * self,
    GstClapperStreamInfo * s);
static void gst_clapper_stream_info_update_tags_and_caps (GstClapper * self,
    GstClapperStreamInfo * s);
static GstClapperStreamInfo *gst_clapper_stream_info_find (GstClapperMediaInfo *
    media_info, GType type, gint stream_index);
static GstClapperStreamInfo *gst_clapper_stream_info_get_current (GstClapper *
    self, const gchar * prop, GType type);

static void gst_clapper_video_info_update (GstClapper * self,
    GstClapperStreamInfo * stream_info);
static void gst_clapper_audio_info_update (GstClapper * self,
    GstClapperStreamInfo * stream_info);
static void gst_clapper_subtitle_info_update (GstClapper * self,
    GstClapperStreamInfo * stream_info);

static gboolean find_active_decoder_with_stream_id (GstClapper * self,
    GstElementFactoryListType type, const gchar * stream_id);

/* For playbin3 */
static void gst_clapper_streams_info_create_from_collection (GstClapper * self,
    GstClapperMediaInfo * media_info, GstStreamCollection * collection);
static void gst_clapper_stream_info_update_from_stream (GstClapper * self,
    GstClapperStreamInfo * s, GstStream * stream);
static GstClapperStreamInfo *gst_clapper_stream_info_find_from_stream_id
    (GstClapperMediaInfo * media_info, const gchar * stream_id);
static GstClapperStreamInfo *gst_clapper_stream_info_get_current_from_stream_id
    (GstClapper * self, const gchar * stream_id, GType type);
static void stream_notify_cb (GstStreamCollection * collection,
    GstStream * stream, GParamSpec * pspec, GstClapper * self);

static void *get_title (GstTagList * tags);
static void *get_container_format (GstTagList * tags);
static void *get_from_tags (GstClapper * self, GstClapperMediaInfo * media_info,
    void *(*func) (GstTagList *));
static void *get_cover_sample (GstTagList * tags);

static void remove_seek_source (GstClapper * self);

static void
gst_clapper_init (GstClapper * self)
{
  GST_TRACE_OBJECT (self, "Initializing");

  self = gst_clapper_get_instance_private (self);

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
  self->seek_pending = FALSE;
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;
  self->inhibit_sigs = FALSE;
  self->needs_info_update = FALSE;
  self->can_start = FALSE;
  self->app_state = GST_CLAPPER_STATE_STOPPED;

  GST_TRACE_OBJECT (self, "Initialized");
}

static void
gst_clapper_class_init (GstClapperClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_clapper_set_property;
  gobject_class->get_property = gst_clapper_get_property;
  gobject_class->dispose = gst_clapper_dispose;
  gobject_class->finalize = gst_clapper_finalize;
  gobject_class->constructed = gst_clapper_constructed;

  GST_DEBUG_CATEGORY_INIT (gst_clapper_debug, "Clapper", 0, "GstClapper");

  param_specs[PROP_VIDEO_RENDERER] =
      g_param_spec_object ("video-renderer",
      "Video Renderer", "Video renderer to use for rendering videos",
      GST_TYPE_CLAPPER_VIDEO_RENDERER,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SIGNAL_DISPATCHER] =
      g_param_spec_object ("signal-dispatcher",
      "Signal Dispatcher", "Dispatcher for the signals to e.g. event loops",
      GST_TYPE_CLAPPER_SIGNAL_DISPATCHER,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MPRIS] =
      g_param_spec_object ("mpris",
      "MPRIS", "Clapper MPRIS for playback control over DBus",
      GST_TYPE_CLAPPER_MPRIS,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_USE_PLAYBIN3] =
      g_param_spec_boolean ("use-playbin3", "Use playbin3", "Use playbin3",
      DEFAULT_USE_PLAYBIN3, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_USE_PIPEWIRE] =
      g_param_spec_boolean ("use-pipewire", "Use PipeWire", "PipeWire audio output",
      DEFAULT_USE_PIPEWIRE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_STATE] =
      g_param_spec_enum ("state", "Clapper State", "Current player state",
      GST_TYPE_CLAPPER_STATE, DEFAULT_STATE, G_PARAM_READABLE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_URI] = g_param_spec_string ("uri", "URI", "Current URI",
      DEFAULT_URI, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SUBURI] = g_param_spec_string ("suburi", "Subtitle URI",
      "Current Subtitle URI", NULL, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_POSITION] =
      g_param_spec_uint64 ("position", "Position", "Current Position",
      0, G_MAXUINT64, DEFAULT_POSITION,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MEDIA_INFO] =
      g_param_spec_object ("media-info", "Media Info",
      "Current media information", GST_TYPE_CLAPPER_MEDIA_INFO,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_AUDIO_TRACK] =
      g_param_spec_object ("current-audio-track", "Current Audio Track",
      "Current audio track information", GST_TYPE_CLAPPER_AUDIO_INFO,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_VIDEO_TRACK] =
      g_param_spec_object ("current-video-track", "Current Video Track",
      "Current video track information", GST_TYPE_CLAPPER_VIDEO_INFO,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_SUBTITLE_TRACK] =
      g_param_spec_object ("current-subtitle-track", "Current Subtitle Track",
      "Current audio subtitle information", GST_TYPE_CLAPPER_SUBTITLE_INFO,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DURATION] =
      g_param_spec_uint64 ("duration", "Duration", "Duration",
      0, G_MAXUINT64, DEFAULT_DURATION,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VOLUME] =
      g_param_spec_double ("volume", "Volume", "Volume",
      0, 1.5, DEFAULT_VOLUME, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MUTE] =
      g_param_spec_boolean ("mute", "Mute", "Mute",
      DEFAULT_MUTE, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_PIPELINE] =
      g_param_spec_object ("pipeline", "Pipeline",
      "GStreamer pipeline that is used",
      GST_TYPE_ELEMENT, G_PARAM_READABLE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_RATE] =
      g_param_spec_double ("rate", "rate", "Playback rate",
      -64.0, 64.0, DEFAULT_RATE, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VIDEO_MULTIVIEW_MODE] =
      g_param_spec_enum ("video-multiview-mode",
      "Multiview Mode Override",
      "Re-interpret a video stream as one of several frame-packed stereoscopic modes.",
      GST_TYPE_VIDEO_MULTIVIEW_FRAME_PACKING,
      GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VIDEO_MULTIVIEW_FLAGS] =
      g_param_spec_flags ("video-multiview-flags",
      "Multiview Flags Override",
      "Override details of the multiview frame layout",
      GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_AUDIO_VIDEO_OFFSET] =
      g_param_spec_int64 ("audio-video-offset", "Audio Video Offset",
      "The synchronisation offset between audio and video in nanoseconds",
      G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SUBTITLE_VIDEO_OFFSET] =
      g_param_spec_int64 ("subtitle-video-offset", "Text Video Offset",
      "The synchronisation offset between text and video in nanoseconds",
      G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_SEEK_MODE] =
      g_param_spec_enum ("seek-mode", "Clapper Seek Mode",
      "Selected seek mode to use when performing seeks",
      GST_TYPE_CLAPPER_SEEK_MODE, DEFAULT_SEEK_MODE, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  signals[SIGNAL_URI_LOADED] =
      g_signal_new ("uri-loaded", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_POSITION_UPDATED] =
      g_signal_new ("position-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DURATION_CHANGED] =
      g_signal_new ("duration-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_STATE_CHANGED] =
      g_signal_new ("state-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLAPPER_STATE);

  signals[SIGNAL_BUFFERING] =
      g_signal_new ("buffering", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_END_OF_STREAM] =
      g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_ERROR] =
      g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  signals[SIGNAL_MEDIA_INFO_UPDATED] =
      g_signal_new ("media-info-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLAPPER_MEDIA_INFO);

  signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED] =
      g_signal_new ("video-dimensions-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  signals[SIGNAL_WARNING] =
      g_signal_new ("warning", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  signals[SIGNAL_VIDEO_DECODER_CHANGED] =
      g_signal_new ("video-decoder-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_AUDIO_DECODER_CHANGED] =
      g_signal_new ("audio-decoder-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
gst_clapper_dispose (GObject * object)
{
  GstClapper *self = GST_CLAPPER (object);

  GST_TRACE_OBJECT (self, "Stopping main thread");

  if (self->loop) {
    g_main_loop_quit (self->loop);

    if (self->thread != g_thread_self ())
      g_thread_join (self->thread);
    else
      g_thread_unref (self->thread);
    self->thread = NULL;

    g_main_loop_unref (self->loop);
    self->loop = NULL;

    g_main_context_unref (self->context);
    self->context = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_clapper_finalize (GObject * object)
{
  GstClapper *self = GST_CLAPPER (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->uri);
  g_free (self->redirect_uri);
  g_free (self->suburi);
  g_free (self->last_vdecoder);
  g_free (self->last_adecoder);
  g_free (self->video_sid);
  g_free (self->audio_sid);
  g_free (self->subtitle_sid);
  if (self->global_tags)
    gst_tag_list_unref (self->global_tags);
  if (self->global_toc)
    gst_toc_unref (self->global_toc);
  if (self->video_renderer)
    g_object_unref (self->video_renderer);
  if (self->signal_dispatcher)
    g_object_unref (self->signal_dispatcher);
  if (self->mpris)
    g_object_unref (self->mpris);
  if (self->current_vis_element)
    gst_object_unref (self->current_vis_element);
  if (self->collection)
    gst_object_unref (self->collection);
  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_clapper_constructed (GObject * object)
{
  GstClapper *self = GST_CLAPPER (object);

  GST_TRACE_OBJECT (self, "Constructed");

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstClapper", gst_clapper_main, self);
  while (!self->loop || !g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

typedef struct
{
  GstClapper *clapper;
  gchar *uri;
} UriLoadedSignalData;

static void
uri_loaded_dispatch (gpointer user_data)
{
  UriLoadedSignalData *data = user_data;

  g_signal_emit (data->clapper, signals[SIGNAL_URI_LOADED], 0, data->uri);
}

static void
uri_loaded_signal_data_free (UriLoadedSignalData * data)
{
  g_object_unref (data->clapper);
  g_free (data->uri);
  g_free (data);
}

static gboolean
gst_clapper_set_uri_internal (gpointer user_data)
{
  GstClapper *self = user_data;

  gst_clapper_stop_internal (self, FALSE);

  g_mutex_lock (&self->lock);
  GST_DEBUG_OBJECT (self, "Changing URI to '%s'", GST_STR_NULL (self->uri));
  g_object_set (self->playbin, "uri", self->uri, NULL);
  g_object_set (self->playbin, "suburi", NULL, NULL);
  self->can_start = TRUE;

  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_URI_LOADED], 0, NULL, NULL, NULL) != 0) {
    UriLoadedSignalData *data = g_new (UriLoadedSignalData, 1);

    data->clapper = g_object_ref (self);
    data->uri = g_strdup (self->uri);
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        uri_loaded_dispatch, data,
        (GDestroyNotify) uri_loaded_signal_data_free);
  }

  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gboolean
gst_clapper_set_suburi_internal (gpointer user_data)
{
  GstClapper *self = user_data;
  GstClockTime position;
  GstState target_state;

  /* save the state and position */
  target_state = self->target_state;
  position = gst_clapper_get_position (self);

  gst_clapper_stop_internal (self, TRUE);
  g_mutex_lock (&self->lock);

  GST_DEBUG_OBJECT (self, "Changing SUBURI to '%s'",
      GST_STR_NULL (self->suburi));

  g_object_set (self->playbin, "suburi", self->suburi, NULL);

  g_mutex_unlock (&self->lock);

  /* restore state and position */
  if (position != GST_CLOCK_TIME_NONE)
    gst_clapper_seek (self, position);
  if (target_state == GST_STATE_PAUSED)
    gst_clapper_pause_internal (self);
  else if (target_state == GST_STATE_PLAYING)
    gst_clapper_play_internal (self);

  return G_SOURCE_REMOVE;
}

static void
gst_clapper_set_rate_internal (GstClapper * self)
{
  self->seek_position = gst_clapper_get_position (self);

  /* If there is no seek being dispatch to the main context currently do that,
   * otherwise we just updated the rate so that it will be taken by
   * the seek handler from the main context instead of the old one.
   */
  if (!self->seek_source) {
    /* If no seek is pending then create new seek source */
    if (!self->seek_pending) {
      self->seek_source = g_idle_source_new ();
      g_source_set_callback (self->seek_source,
          (GSourceFunc) gst_clapper_seek_internal, self, NULL);
      g_source_attach (self->seek_source, self->context);
    }
  }
}

static void
gst_clapper_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstClapper *self = GST_CLAPPER (object);

  switch (prop_id) {
    case PROP_VIDEO_RENDERER:
      self->video_renderer = g_value_dup_object (value);
      break;
    case PROP_SIGNAL_DISPATCHER:
      self->signal_dispatcher = g_value_dup_object (value);
      break;
    case PROP_MPRIS:
      self->mpris = g_value_dup_object (value);
      break;
    case PROP_USE_PLAYBIN3:
      self->use_playbin3 = g_value_get_boolean (value);
      break;
    case PROP_USE_PIPEWIRE:
      self->use_pipewire = g_value_get_boolean (value);
      break;
    case PROP_URI:{
      g_mutex_lock (&self->lock);
      g_free (self->uri);
      g_free (self->redirect_uri);
      self->redirect_uri = NULL;

      g_free (self->suburi);
      self->suburi = NULL;

      self->uri = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "Set uri=%s", self->uri);
      g_mutex_unlock (&self->lock);

      g_main_context_invoke_full (self->context, G_PRIORITY_DEFAULT,
          gst_clapper_set_uri_internal, self, NULL);
      break;
    }
    case PROP_SUBURI:{
      g_mutex_lock (&self->lock);
      g_free (self->suburi);

      self->suburi = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "Set suburi=%s", self->suburi);
      g_mutex_unlock (&self->lock);

      g_main_context_invoke_full (self->context, G_PRIORITY_DEFAULT,
          gst_clapper_set_suburi_internal, self, NULL);
      break;
    }
    case PROP_VOLUME: {
      GValue volume_linear = G_VALUE_INIT;
      gdouble volume = g_value_get_double (value);

      GST_DEBUG_OBJECT (self, "Set volume=%lf", volume);
      volume = gst_stream_volume_convert_volume (
          GST_STREAM_VOLUME_FORMAT_CUBIC, GST_STREAM_VOLUME_FORMAT_LINEAR, volume);
      GST_DEBUG_OBJECT (self, "Converted linear volume=%lf", volume);

      g_value_init (&volume_linear, G_TYPE_DOUBLE);
      g_value_set_double (&volume_linear, volume);
      g_object_set_property (G_OBJECT (self->playbin), "volume", &volume_linear);

      g_value_unset (&volume_linear);
      break;
    }
    case PROP_RATE:
      g_mutex_lock (&self->lock);
      self->rate = g_value_get_double (value);
      GST_DEBUG_OBJECT (self, "Set rate=%lf", g_value_get_double (value));
      gst_clapper_set_rate_internal (self);
      g_mutex_unlock (&self->lock);
      break;
    case PROP_MUTE:
      GST_DEBUG_OBJECT (self, "Set mute=%d", g_value_get_boolean (value));
      g_object_set_property (G_OBJECT (self->playbin), "mute", value);
      break;
    case PROP_VIDEO_MULTIVIEW_MODE:
      GST_DEBUG_OBJECT (self, "Set multiview mode=%u",
          g_value_get_enum (value));
      g_object_set_property (G_OBJECT (self->playbin), "video-multiview-mode",
          value);
      break;
    case PROP_VIDEO_MULTIVIEW_FLAGS:
      GST_DEBUG_OBJECT (self, "Set multiview flags=%x",
          g_value_get_flags (value));
      g_object_set_property (G_OBJECT (self->playbin), "video-multiview-flags",
          value);
      break;
    case PROP_AUDIO_VIDEO_OFFSET:
      g_object_set_property (G_OBJECT (self->playbin), "av-offset", value);
      break;
    case PROP_SUBTITLE_VIDEO_OFFSET:
      g_object_set_property (G_OBJECT (self->playbin), "text-offset", value);
      break;
    case PROP_SEEK_MODE:
      g_mutex_lock (&self->lock);
      self->seek_mode = g_value_get_enum (value);
      g_mutex_unlock (&self->lock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstClapper *self = GST_CLAPPER (object);

  switch (prop_id) {
    case PROP_MPRIS:
      g_mutex_lock (&self->lock);
      g_value_set_object (value, self->mpris);
      g_mutex_unlock (&self->lock);
      break;
    case PROP_STATE:
      g_mutex_lock (&self->lock);
      g_value_set_enum (value, self->app_state);
      g_mutex_unlock (&self->lock);
      break;
    case PROP_URI:
      g_mutex_lock (&self->lock);
      g_value_set_string (value, self->uri);
      g_mutex_unlock (&self->lock);
      break;
    case PROP_SUBURI:
      g_mutex_lock (&self->lock);
      g_value_set_string (value, self->suburi);
      g_mutex_unlock (&self->lock);
      GST_DEBUG_OBJECT (self, "Returning suburi=%s",
          g_value_get_string (value));
      break;
    case PROP_POSITION:{
      gint64 position = GST_CLOCK_TIME_NONE;

      gst_element_query_position (self->playbin, GST_FORMAT_TIME, &position);
      g_value_set_uint64 (value, position);
      GST_TRACE_OBJECT (self, "Returning position=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_DURATION:{
      g_value_set_uint64 (value, self->cached_duration);
      GST_TRACE_OBJECT (self, "Returning duration=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_MEDIA_INFO:{
      GstClapperMediaInfo *media_info = gst_clapper_get_media_info (self);
      g_value_take_object (value, media_info);
      break;
    }
    case PROP_CURRENT_AUDIO_TRACK:{
      GstClapperAudioInfo *audio_info =
          gst_clapper_get_current_audio_track (self);
      g_value_take_object (value, audio_info);
      break;
    }
    case PROP_CURRENT_VIDEO_TRACK:{
      GstClapperVideoInfo *video_info =
          gst_clapper_get_current_video_track (self);
      g_value_take_object (value, video_info);
      break;
    }
    case PROP_CURRENT_SUBTITLE_TRACK:{
      GstClapperSubtitleInfo *subtitle_info =
          gst_clapper_get_current_subtitle_track (self);
      g_value_take_object (value, subtitle_info);
      break;
    }
    case PROP_VOLUME: {
      gdouble volume;

      g_object_get_property (G_OBJECT (self->playbin), "volume", value);
      volume = g_value_get_double (value);
      volume = gst_stream_volume_convert_volume (
          GST_STREAM_VOLUME_FORMAT_LINEAR, GST_STREAM_VOLUME_FORMAT_CUBIC, volume);
      g_value_set_double (value, volume);
      GST_TRACE_OBJECT (self, "Returning volume=%lf", volume);
      break;
    }
    case PROP_RATE:
      g_mutex_lock (&self->lock);
      g_value_set_double (value, self->rate);
      g_mutex_unlock (&self->lock);
      break;
    case PROP_MUTE:
      g_object_get_property (G_OBJECT (self->playbin), "mute", value);
      GST_TRACE_OBJECT (self, "Returning mute=%d", g_value_get_boolean (value));
      break;
    case PROP_PIPELINE:
      g_value_set_object (value, self->playbin);
      break;
    case PROP_VIDEO_MULTIVIEW_MODE:{
      g_object_get_property (G_OBJECT (self->playbin), "video-multiview-mode",
          value);
      GST_TRACE_OBJECT (self, "Return multiview mode=%d",
          g_value_get_enum (value));
      break;
    }
    case PROP_VIDEO_MULTIVIEW_FLAGS:{
      g_object_get_property (G_OBJECT (self->playbin), "video-multiview-flags",
          value);
      GST_TRACE_OBJECT (self, "Return multiview flags=%x",
          g_value_get_flags (value));
      break;
    }
    case PROP_AUDIO_VIDEO_OFFSET:
      g_object_get_property (G_OBJECT (self->playbin), "av-offset", value);
      break;
    case PROP_SUBTITLE_VIDEO_OFFSET:
      g_object_get_property (G_OBJECT (self->playbin), "text-offset", value);
      break;
    case PROP_SEEK_MODE:
      g_mutex_lock (&self->lock);
      g_value_set_enum (value, self->seek_mode);
      g_mutex_unlock (&self->lock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
main_loop_running_cb (gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  GST_TRACE_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

typedef struct
{
  GstClapper *clapper;
  GstClapperMediaInfo *info;
} MediaInfoUpdatedSignalData;

static void
media_info_updated_dispatch (gpointer user_data)
{
  MediaInfoUpdatedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  if (data->clapper->target_state >= GST_STATE_PAUSED) {
    g_signal_emit (data->clapper, signals[SIGNAL_MEDIA_INFO_UPDATED], 0,
        data->info);
  }
}

static void
free_media_info_updated_signal_data (MediaInfoUpdatedSignalData * data)
{
  g_object_unref (data->clapper);
  g_object_unref (data->info);
  g_free (data);
}

static void
emit_media_info_updated (GstClapper * self)
{
  MediaInfoUpdatedSignalData *data = g_new (MediaInfoUpdatedSignalData, 1);
  self->needs_info_update = FALSE;
  data->clapper = g_object_ref (self);
  g_mutex_lock (&self->lock);
  data->info = gst_clapper_media_info_copy (self->media_info);
  g_mutex_unlock (&self->lock);

  gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
      media_info_updated_dispatch, data,
      (GDestroyNotify) free_media_info_updated_signal_data);
}

typedef struct
{
  GstClapper *clapper;
  GstClapperState state;
} StateChangedSignalData;

static void
state_changed_dispatch (gpointer user_data)
{
  StateChangedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs && data->state != GST_CLAPPER_STATE_STOPPED
      && data->state != GST_CLAPPER_STATE_PAUSED)
    return;

  g_signal_emit (data->clapper, signals[SIGNAL_STATE_CHANGED], 0, data->state);
}

static void
state_changed_signal_data_free (StateChangedSignalData * data)
{
  g_object_unref (data->clapper);
  g_free (data);
}

static void
rate_notify_dispatch (gpointer user_data)
{
  GstClapper *clapper = user_data;

  if (clapper->inhibit_sigs)
    return;

  g_object_notify_by_pspec (G_OBJECT (clapper), param_specs[PROP_RATE]);
}

static void
emit_rate_notify (GstClapper * self)
{
  gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
      rate_notify_dispatch, g_object_ref (self),
      (GDestroyNotify) g_object_unref);
}

static void
change_state (GstClapper * self, GstClapperState state)
{
  if (state == self->app_state)
    return;

  GST_DEBUG_OBJECT (self, "Changing app state from %s to %s",
      gst_clapper_state_get_name (self->app_state),
      gst_clapper_state_get_name (state));
  self->app_state = state;

  if (state == GST_CLAPPER_STATE_STOPPED) {
    self->needs_info_update = FALSE;
    if (self->rate != 1.0) {
      self->rate = 1.0;
      emit_rate_notify (self);
    }
  }

  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_STATE_CHANGED], 0, NULL, NULL, NULL) != 0) {
    StateChangedSignalData *data = g_new (StateChangedSignalData, 1);

    data->clapper = g_object_ref (self);
    data->state = state;
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        state_changed_dispatch, data,
        (GDestroyNotify) state_changed_signal_data_free);
  }

  if (!self->mpris)
    return;

  switch (state) {
    case GST_CLAPPER_STATE_STOPPED:
      gst_clapper_mpris_set_playback_status (self->mpris, "Stopped");
      break;
    case GST_CLAPPER_STATE_PAUSED:
      gst_clapper_mpris_set_playback_status (self->mpris, "Paused");
      break;
    case GST_CLAPPER_STATE_PLAYING:
      gst_clapper_mpris_set_playback_status (self->mpris, "Playing");
      break;
    default:
      break;
  }
}

typedef struct
{
  GstClapper *clapper;
  GstClockTime position;
} PositionUpdatedSignalData;

static void
position_updated_dispatch (gpointer user_data)
{
  PositionUpdatedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  if (data->clapper->target_state >= GST_STATE_PAUSED) {
    g_signal_emit (data->clapper, signals[SIGNAL_POSITION_UPDATED], 0,
        data->position);
  }
}

static void
position_updated_signal_data_free (PositionUpdatedSignalData * data)
{
  g_object_unref (data->clapper);
  g_free (data);
}

static gboolean
tick_cb (gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  gint64 position;

  if (self->target_state >= GST_STATE_PAUSED
      && gst_element_query_position (self->playbin, GST_FORMAT_TIME,
          &position)) {
    GST_LOG_OBJECT (self, "Position %" GST_TIME_FORMAT,
        GST_TIME_ARGS (position));

    if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
            signals[SIGNAL_POSITION_UPDATED], 0, NULL, NULL, NULL) != 0) {
      PositionUpdatedSignalData *data = g_new (PositionUpdatedSignalData, 1);

      data->clapper = g_object_ref (self);
      data->position = position;
      gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
          position_updated_dispatch, data,
          (GDestroyNotify) position_updated_signal_data_free);
    }
    if (self->mpris)
      gst_clapper_mpris_set_position (self->mpris, position);
  }

  return G_SOURCE_CONTINUE;
}

static void
add_tick_source (GstClapper * self)
{
  if (self->tick_source)
    return;

  self->tick_source = g_timeout_source_new (DEFAULT_POSITION_UPDATE_INTERVAL_MS);
  g_source_set_callback (self->tick_source, (GSourceFunc) tick_cb, self, NULL);
  g_source_attach (self->tick_source, self->context);
}

static void
remove_tick_source (GstClapper * self)
{
  if (!self->tick_source)
    return;

  g_source_destroy (self->tick_source);
  g_source_unref (self->tick_source);
  self->tick_source = NULL;
}

typedef struct
{
  GstClapper *clapper;
  GError *err;
} ErrorSignalData;

static void
error_dispatch (gpointer user_data)
{
  ErrorSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  g_signal_emit (data->clapper, signals[SIGNAL_ERROR], 0, data->err);
}

static void
free_error_signal_data (ErrorSignalData * data)
{
  g_object_unref (data->clapper);
  g_clear_error (&data->err);
  g_free (data);
}

static void
emit_error (GstClapper * self, GError * err)
{
  GST_ERROR_OBJECT (self, "Error: %s (%s, %d)", err->message,
      g_quark_to_string (err->domain), err->code);

  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_ERROR], 0, NULL, NULL, NULL) != 0) {
    ErrorSignalData *data = g_new (ErrorSignalData, 1);

    data->clapper = g_object_ref (self);
    data->err = g_error_copy (err);
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        error_dispatch, data, (GDestroyNotify) free_error_signal_data);
  }

  g_error_free (err);

  remove_tick_source (self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  self->is_live = FALSE;
  gst_element_set_state (self->playbin, GST_STATE_NULL);
  change_state (self, GST_CLAPPER_STATE_STOPPED);
  self->buffering = 100;

  g_mutex_lock (&self->lock);
  if (self->media_info) {
    g_object_unref (self->media_info);
    self->media_info = NULL;
  }
  if (self->global_tags) {
    gst_tag_list_unref (self->global_tags);
    self->global_tags = NULL;
  }
  if (self->global_toc) {
    gst_toc_unref (self->global_toc);
    self->global_toc = NULL;
  }
  self->seek_pending = FALSE;
  remove_seek_source (self);
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&self->lock);
}

static void
dump_dot_file (GstClapper * self, const gchar * name)
{
  gchar *full_name;

  full_name = g_strdup_printf ("gst-clapper.%p.%s", self, name);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (self->playbin),
      GST_DEBUG_GRAPH_SHOW_ALL, full_name);

  g_free (full_name);
}

typedef struct
{
  GstClapper *clapper;
  GError *err;
} WarningSignalData;

static void
warning_dispatch (gpointer user_data)
{
  WarningSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  g_signal_emit (data->clapper, signals[SIGNAL_WARNING], 0, data->err);
}

static void
free_warning_signal_data (WarningSignalData * data)
{
  g_object_unref (data->clapper);
  g_clear_error (&data->err);
  g_free (data);
}

static void
emit_warning (GstClapper * self, GError * err)
{
  GST_ERROR_OBJECT (self, "Warning: %s (%s, %d)", err->message,
      g_quark_to_string (err->domain), err->code);

  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_WARNING], 0, NULL, NULL, NULL) != 0) {
    WarningSignalData *data = g_new (WarningSignalData, 1);

    data->clapper = g_object_ref (self);
    data->err = g_error_copy (err);
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        warning_dispatch, data, (GDestroyNotify) free_warning_signal_data);
  }

  g_error_free (err);
}

static void
error_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GError *err, *clapper_err;
  gchar *name, *debug, *message, *full_message;

  dump_dot_file (self, "error");

  gst_message_parse_error (msg, &err, &debug);

  name = gst_object_get_path_string (msg->src);
  message = gst_error_get_message (err->domain, err->code);

  if (debug)
    full_message =
        g_strdup_printf ("Error from element %s: %s\n%s\n%s", name, message,
        err->message, debug);
  else
    full_message =
        g_strdup_printf ("Error from element %s: %s\n%s", name, message,
        err->message);

  GST_ERROR_OBJECT (self, "ERROR: from element %s: %s", name, err->message);
  if (debug != NULL)
    GST_ERROR_OBJECT (self, "Additional debug info: %s", debug);

  clapper_err =
      g_error_new_literal (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
      full_message);
  emit_error (self, clapper_err);

  g_clear_error (&err);
  g_free (debug);
  g_free (name);
  g_free (full_message);
  g_free (message);
}

static void
warning_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GError *err, *clapper_err;
  gchar *name, *debug, *message, *full_message;

  dump_dot_file (self, "warning");

  gst_message_parse_warning (msg, &err, &debug);

  name = gst_object_get_path_string (msg->src);
  message = gst_error_get_message (err->domain, err->code);

  if (debug)
    full_message =
        g_strdup_printf ("Warning from element %s: %s\n%s\n%s", name, message,
        err->message, debug);
  else
    full_message =
        g_strdup_printf ("Warning from element %s: %s\n%s", name, message,
        err->message);

  GST_WARNING_OBJECT (self, "WARNING: from element %s: %s", name, err->message);
  if (debug != NULL)
    GST_WARNING_OBJECT (self, "Additional debug info: %s", debug);

  clapper_err =
      g_error_new_literal (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
      full_message);
  emit_warning (self, clapper_err);

  g_clear_error (&err);
  g_free (debug);
  g_free (name);
  g_free (full_message);
  g_free (message);
}

static void
eos_dispatch (gpointer user_data)
{
  GstClapper *clapper = user_data;

  if (clapper->inhibit_sigs)
    return;

  g_signal_emit (clapper, signals[SIGNAL_END_OF_STREAM], 0);
}

static void
eos_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  GST_DEBUG_OBJECT (self, "End of stream");

  tick_cb (self);
  remove_tick_source (self);

  /* When connected client should handle what to do (stop/repeat) */
  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_END_OF_STREAM], 0, NULL, NULL, NULL) != 0) {
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        eos_dispatch, g_object_ref (self), (GDestroyNotify) g_object_unref);
  } else
    gst_clapper_stop_internal (self, FALSE);
}

typedef struct
{
  GstClapper *clapper;
  gint percent;
} BufferingSignalData;

static void
buffering_dispatch (gpointer user_data)
{
  BufferingSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  if (data->clapper->target_state >= GST_STATE_PAUSED) {
    g_signal_emit (data->clapper, signals[SIGNAL_BUFFERING], 0, data->percent);
  }
}

static void
buffering_signal_data_free (BufferingSignalData * data)
{
  g_object_unref (data->clapper);
  g_free (data);
}

static void
buffering_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  gint percent;

  if (self->target_state < GST_STATE_PAUSED)
    return;
  if (self->is_live)
    return;

  gst_message_parse_buffering (msg, &percent);
  GST_LOG_OBJECT (self, "Buffering %d%%", percent);

  if (percent < 100 && self->target_state >= GST_STATE_PAUSED) {
    GstStateChangeReturn state_ret;

    GST_DEBUG_OBJECT (self, "Waiting for buffering to finish");
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
              "Failed to handle buffering"));
      return;
    }

    change_state (self, GST_CLAPPER_STATE_BUFFERING);
  }

  if (self->buffering != percent) {
    if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
            signals[SIGNAL_BUFFERING], 0, NULL, NULL, NULL) != 0) {
      BufferingSignalData *data = g_new (BufferingSignalData, 1);

      data->clapper = g_object_ref (self);
      data->percent = percent;
      gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
          buffering_dispatch, data,
          (GDestroyNotify) buffering_signal_data_free);
    }

    self->buffering = percent;
  }


  g_mutex_lock (&self->lock);
  if (percent == 100 && (self->seek_position != GST_CLOCK_TIME_NONE ||
          self->seek_pending)) {
    g_mutex_unlock (&self->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - seek pending");
  } else if (percent == 100 && self->target_state >= GST_STATE_PLAYING
      && self->current_state >= GST_STATE_PAUSED) {
    GstStateChangeReturn state_ret;

    g_mutex_unlock (&self->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - going to PLAYING");
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
    /* Application state change is happening when the state change happened */
    if (state_ret == GST_STATE_CHANGE_FAILURE)
      emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
              "Failed to handle buffering"));
  } else if (percent == 100 && self->target_state >= GST_STATE_PAUSED) {
    g_mutex_unlock (&self->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - staying PAUSED");
    change_state (self, GST_CLAPPER_STATE_PAUSED);
  } else {
    g_mutex_unlock (&self->lock);
  }
}

static void
clock_lost_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Clock lost");
  if (self->target_state >= GST_STATE_PLAYING) {
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
    if (state_ret != GST_STATE_CHANGE_FAILURE)
      state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);

    if (state_ret == GST_STATE_CHANGE_FAILURE)
      emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
              "Failed to handle clock loss"));
  }
}

typedef struct
{
  GstClapper *clapper;
  gint width, height;
} VideoDimensionsChangedSignalData;

static void
video_dimensions_changed_dispatch (gpointer user_data)
{
  VideoDimensionsChangedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  if (data->clapper->target_state >= GST_STATE_PAUSED) {
    g_signal_emit (data->clapper, signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0,
        data->width, data->height);
  }
}

static void
video_dimensions_changed_signal_data_free (VideoDimensionsChangedSignalData *
    data)
{
  g_object_unref (data->clapper);
  g_free (data);
}

static void
check_video_dimensions_changed (GstClapper * self)
{
  GstElement *video_sink;
  GstPad *video_sink_pad;
  GstCaps *caps;
  GstVideoInfo info;
  gint width = 0, height = 0;

  g_object_get (self->playbin, "video-sink", &video_sink, NULL);
  if (!video_sink)
    goto out;

  video_sink_pad = gst_element_get_static_pad (video_sink, "sink");
  if (!video_sink_pad) {
    gst_object_unref (video_sink);
    goto out;
  }

  caps = gst_pad_get_current_caps (video_sink_pad);

  if (caps) {
    if (gst_video_info_from_caps (&info, caps)) {
      info.width = info.width * info.par_n / info.par_d;

      GST_DEBUG_OBJECT (self, "Video dimensions changed: %dx%d", info.width,
          info.height);
      width = info.width;
      height = info.height;
    }

    gst_caps_unref (caps);
  }
  gst_object_unref (video_sink_pad);
  gst_object_unref (video_sink);

out:
  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0, NULL, NULL, NULL) != 0) {
    VideoDimensionsChangedSignalData *data =
        g_new (VideoDimensionsChangedSignalData, 1);

    data->clapper = g_object_ref (self);
    data->width = width;
    data->height = height;
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        video_dimensions_changed_dispatch, data,
        (GDestroyNotify) video_dimensions_changed_signal_data_free);
  }
}

static void
notify_caps_cb (G_GNUC_UNUSED GObject * object,
    G_GNUC_UNUSED GParamSpec * pspec, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  if (self->target_state >= GST_STATE_PAUSED) {
    check_video_dimensions_changed (self);

    g_mutex_lock (&self->lock);
    if (self->media_info != NULL)
      self->needs_info_update = TRUE;
    g_mutex_unlock (&self->lock);
  }
}

typedef struct
{
  GstClapper *clapper;
  GstClockTime duration;
} DurationChangedSignalData;

static void
duration_changed_dispatch (gpointer user_data)
{
  DurationChangedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  if (data->clapper->target_state >= GST_STATE_PAUSED) {
    g_signal_emit (data->clapper, signals[SIGNAL_DURATION_CHANGED], 0,
        data->duration);
  }
}

static void
duration_changed_signal_data_free (DurationChangedSignalData * data)
{
  g_object_unref (data->clapper);
  g_free (data);
}

static void
emit_duration_changed (GstClapper * self, GstClockTime duration)
{
  if (self->cached_duration == duration
      || self->cached_duration / (250 * GST_MSECOND) == duration / (250 * GST_MSECOND))
    return;

  GST_DEBUG_OBJECT (self, "Duration changed %" GST_TIME_FORMAT,
      GST_TIME_ARGS (duration));

  self->cached_duration = duration;
  g_mutex_lock (&self->lock);
  if (self->media_info)
    self->media_info->duration = duration;
  g_mutex_unlock (&self->lock);

  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_DURATION_CHANGED], 0, NULL, NULL, NULL) != 0) {
    DurationChangedSignalData *data = g_new (DurationChangedSignalData, 1);

    data->clapper = g_object_ref (self);
    data->duration = duration;
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        duration_changed_dispatch, data,
        (GDestroyNotify) duration_changed_signal_data_free);
  }
}

static void
state_changed_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstState old_state, new_state, pending_state;

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (self->playbin)) {
    gchar *transition_name;

    GST_DEBUG_OBJECT (self, "Changed state old: %s new: %s pending: %s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state),
        gst_element_state_get_name (pending_state));

    transition_name = g_strdup_printf ("%s_%s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state));
    dump_dot_file (self, transition_name);
    g_free (transition_name);

    self->current_state = new_state;

    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      gint64 duration = -1;

      GST_DEBUG_OBJECT (self, "Initial PAUSED - pre-rolled");

      g_mutex_lock (&self->lock);
      if (self->media_info)
        g_object_unref (self->media_info);
      self->media_info = gst_clapper_media_info_create (self);
      g_mutex_unlock (&self->lock);
      check_video_dimensions_changed (self);
      if (gst_element_query_duration (self->playbin, GST_FORMAT_TIME,
              &duration)) {
        emit_duration_changed (self, duration);
      } else {
        self->cached_duration = GST_CLOCK_TIME_NONE;
      }
      emit_media_info_updated (self);
      if (self->mpris) {
        GstClapperMediaInfo *info;

        g_mutex_lock (&self->lock);
        info = gst_clapper_media_info_copy (self->media_info);
        g_mutex_unlock (&self->lock);

        gst_clapper_mpris_set_media_info (self->mpris, info);
      }
    }

    if (new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      remove_tick_source (self);

      g_mutex_lock (&self->lock);
      if (self->seek_pending) {
        self->seek_pending = FALSE;

        if (!self->media_info->seekable) {
          GST_DEBUG_OBJECT (self, "Media is not seekable");
          remove_seek_source (self);
          self->seek_position = GST_CLOCK_TIME_NONE;
          self->last_seek_time = GST_CLOCK_TIME_NONE;
        } else if (self->seek_source) {
          GST_DEBUG_OBJECT (self, "Seek finished but new seek is pending");
          gst_clapper_seek_internal_locked (self);
        } else {
          GST_DEBUG_OBJECT (self, "Seek finished");
        }
      }

      if (self->seek_position != GST_CLOCK_TIME_NONE) {
        GST_DEBUG_OBJECT (self, "Seeking now that we reached PAUSED state");
        gst_clapper_seek_internal_locked (self);
        g_mutex_unlock (&self->lock);
      } else if (!self->seek_pending) {
        g_mutex_unlock (&self->lock);

        tick_cb (self);

        if (self->target_state >= GST_STATE_PLAYING && self->buffering == 100) {
          GstStateChangeReturn state_ret;

          state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
          if (state_ret == GST_STATE_CHANGE_FAILURE)
            emit_error (self, g_error_new (GST_CLAPPER_ERROR,
                    GST_CLAPPER_ERROR_FAILED, "Failed to play"));
        } else if (self->buffering == 100) {
          change_state (self, GST_CLAPPER_STATE_PAUSED);
        }
      } else {
        g_mutex_unlock (&self->lock);
      }
    } else if (new_state == GST_STATE_PLAYING
        && pending_state == GST_STATE_VOID_PENDING) {

      /* If no seek is currently pending, add the tick source. This can happen
       * if we seeked already but the state-change message was still queued up */
      if (!self->seek_pending) {
        add_tick_source (self);
        change_state (self, GST_CLAPPER_STATE_PLAYING);
      }
    } else if (new_state == GST_STATE_READY && old_state > GST_STATE_READY) {
      change_state (self, GST_CLAPPER_STATE_STOPPED);
    } else {
      /* Otherwise we neither reached PLAYING nor PAUSED, so must
       * wait for something to happen... i.e. are BUFFERING now */
      change_state (self, GST_CLAPPER_STATE_BUFFERING);
    }
  }
}

static void
duration_changed_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  gint64 duration = GST_CLOCK_TIME_NONE;

  if (gst_element_query_duration (self->playbin, GST_FORMAT_TIME, &duration)) {
    emit_duration_changed (self, duration);
  }
}

static void
latency_cb (G_GNUC_UNUSED GstBus * bus, G_GNUC_UNUSED GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  GST_DEBUG_OBJECT (self, "Latency changed");

  gst_bin_recalculate_latency (GST_BIN (self->playbin));
}

static void
request_state_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstState state;
  GstStateChangeReturn state_ret;

  gst_message_parse_request_state (msg, &state);

  GST_DEBUG_OBJECT (self, "State %s requested",
      gst_element_state_get_name (state));

  self->target_state = state;
  state_ret = gst_element_set_state (self->playbin, state);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
            "Failed to change to requested state %s",
            gst_element_state_get_name (state)));
}

static void
media_info_update (GstClapper * self, GstClapperMediaInfo * info)
{
  /* Update title from new tags or leave the title from URI */
  gchar *tags_title = get_from_tags (self, info, get_title);
  if (tags_title) {
    g_free (info->title);
    info->title = tags_title;
  }

  g_free (info->container);
  info->container = get_from_tags (self, info, get_container_format);

  if (info->image_sample)
    gst_sample_unref (info->image_sample);
  info->image_sample = get_from_tags (self, info, get_cover_sample);

  GST_DEBUG_OBJECT (self, "title: %s, container: %s "
      "image_sample: %p", info->title, info->container, info->image_sample);
}

static void
merge_tags (GstTagList **my_tags, GstTagList *tags)
{
  if (*my_tags) {
    *my_tags = gst_tag_list_make_writable (*my_tags);
    gst_tag_list_insert (*my_tags, tags, GST_TAG_MERGE_REPLACE);
  } else {
    *my_tags = gst_tag_list_ref (tags);
  }
}

static void
tags_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstTagList *tags = NULL;

  gst_message_parse_tag (msg, &tags);

  GST_DEBUG_OBJECT (self, "received %s tags",
      gst_tag_list_get_scope (tags) ==
      GST_TAG_SCOPE_GLOBAL ? "global" : "stream");

  if (gst_tag_list_get_scope (tags) == GST_TAG_SCOPE_GLOBAL) {
    g_mutex_lock (&self->lock);
    if (self->media_info) {
      merge_tags (&self->media_info->tags, tags);
      media_info_update (self, self->media_info);
    } else {
      merge_tags (&self->global_tags, tags);
    }
    g_mutex_unlock (&self->lock);
  }

  gst_tag_list_unref (tags);
}

static void
toc_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstToc *toc = NULL;

  gst_message_parse_toc (msg, &toc, NULL);

  GST_DEBUG_OBJECT (self, "received %s toc",
      gst_toc_get_scope (toc) == GST_TOC_SCOPE_GLOBAL ? "global" : "stream");

  if (gst_toc_get_scope (toc) == GST_TOC_SCOPE_GLOBAL) {
    g_mutex_lock (&self->lock);
    if (self->media_info) {
      if (self->media_info->toc)
        gst_toc_unref (self->media_info->toc);
      self->media_info->toc = gst_toc_ref (toc);
      media_info_update (self, self->media_info);
      g_mutex_unlock (&self->lock);
    } else {
      if (self->global_toc)
        gst_toc_unref (self->global_toc);
      self->global_toc = gst_toc_ref (toc);
      g_mutex_unlock (&self->lock);
    }
  }

  gst_toc_unref (toc);
}

static void
element_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  const GstStructure *s;

  s = gst_message_get_structure (msg);
  if (gst_structure_has_name (s, "redirect")) {
    const gchar *new_location;

    new_location = gst_structure_get_string (s, "new-location");
    if (!new_location) {
      const GValue *locations_list, *location_val;
      guint i, size;

      locations_list = gst_structure_get_value (s, "locations");
      size = gst_value_list_get_size (locations_list);
      for (i = 0; i < size; ++i) {
        const GstStructure *location_s;

        location_val = gst_value_list_get_value (locations_list, i);
        if (!GST_VALUE_HOLDS_STRUCTURE (location_val))
          continue;

        location_s = (const GstStructure *) g_value_get_boxed (location_val);
        if (!gst_structure_has_name (location_s, "redirect"))
          continue;

        new_location = gst_structure_get_string (location_s, "new-location");
        if (new_location)
          break;
      }
    }

    if (new_location) {
      GstState target_state;

      GST_DEBUG_OBJECT (self, "Redirect to '%s'", new_location);

      /* Remember target state and restore after setting the URI */
      target_state = self->target_state;

      gst_clapper_stop_internal (self, TRUE);

      g_mutex_lock (&self->lock);
      g_free (self->redirect_uri);
      self->redirect_uri = g_strdup (new_location);
      g_object_set (self->playbin, "uri", self->redirect_uri, NULL);
      g_mutex_unlock (&self->lock);

      if (target_state == GST_STATE_PAUSED)
        gst_clapper_pause_internal (self);
      else if (target_state == GST_STATE_PLAYING)
        gst_clapper_play_internal (self);
    }
  }
}

static void
qos_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  gboolean live;
  guint64 running_time, stream_time, timestamp, duration;

  gst_message_parse_qos (msg, &live, &running_time, &stream_time,
      &timestamp, &duration);

  GST_DEBUG_OBJECT (self, "QOS dropped buffer"
      ", element live: %s"
      ", running time: %" GST_TIME_FORMAT
      ", stream time: %" GST_TIME_FORMAT
      ", timestamp: %" GST_TIME_FORMAT
      ", duration: %" GST_TIME_FORMAT,
      live ? "yes" : "no", GST_TIME_ARGS (running_time),
      GST_TIME_ARGS (stream_time), GST_TIME_ARGS (timestamp),
      GST_TIME_ARGS (duration));
}

/* Must be called with lock */
static gboolean
update_stream_collection (GstClapper * self, GstStreamCollection * collection)
{
  if (self->collection && self->collection == collection)
    return FALSE;

  if (self->collection && self->stream_notify_id)
    g_signal_handler_disconnect (self->collection, self->stream_notify_id);

  gst_object_replace ((GstObject **) & self->collection,
      (GstObject *) collection);
  if (self->media_info) {
    gst_object_unref (self->media_info);
    self->media_info = gst_clapper_media_info_create (self);
  }

  self->stream_notify_id =
      g_signal_connect (self->collection, "stream-notify",
      G_CALLBACK (stream_notify_cb), self);

  return TRUE;
}

static void
stream_collection_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstStreamCollection *collection = NULL;

  gst_message_parse_stream_collection (msg, &collection);

  if (!collection)
    return;

  g_mutex_lock (&self->lock);
  update_stream_collection (self, collection);
  gst_object_unref (collection);
  g_mutex_unlock (&self->lock);
}

static void
streams_selected_cb (G_GNUC_UNUSED GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstStreamCollection *collection = NULL;
  gchar *video_sid, *audio_sid;
  guint i, len;

  gst_message_parse_streams_selected (msg, &collection);

  if (!collection)
    return;

  g_mutex_lock (&self->lock);
  update_stream_collection (self, collection);
  gst_object_unref (collection);

  g_free (self->video_sid);
  g_free (self->audio_sid);
  g_free (self->subtitle_sid);
  self->video_sid = NULL;
  self->audio_sid = NULL;
  self->subtitle_sid = NULL;

  len = gst_message_streams_selected_get_size (msg);
  for (i = 0; i < len; i++) {
    GstStream *stream;
    GstStreamType stream_type;
    const gchar *stream_id;
    gchar **current_sid;
    stream = gst_message_streams_selected_get_stream (msg, i);
    stream_type = gst_stream_get_stream_type (stream);
    stream_id = gst_stream_get_stream_id (stream);
    if (stream_type & GST_STREAM_TYPE_AUDIO)
      current_sid = &self->audio_sid;
    else if (stream_type & GST_STREAM_TYPE_VIDEO)
      current_sid = &self->video_sid;
    else if (stream_type & GST_STREAM_TYPE_TEXT)
      current_sid = &self->subtitle_sid;
    else {
      GST_WARNING_OBJECT (self,
          "Unknown stream-id %s with type 0x%x", stream_id, stream_type);
      continue;
    }

    if (G_UNLIKELY (*current_sid)) {
      GST_FIXME_OBJECT (self,
          "Multiple streams are selected for type %s, choose the first one",
          gst_stream_type_get_name (stream_type));
      continue;
    }

    *current_sid = g_strdup (stream_id);
  }

  video_sid = g_strdup (self->video_sid);
  audio_sid = g_strdup (self->audio_sid);

  g_mutex_unlock (&self->lock);

  if (video_sid) {
    find_active_decoder_with_stream_id (self, GST_ELEMENT_FACTORY_TYPE_DECODER
        | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, video_sid);
    g_free (video_sid);
  }
  if (audio_sid) {
    find_active_decoder_with_stream_id (self, GST_ELEMENT_FACTORY_TYPE_DECODER
        | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, audio_sid);
    g_free (audio_sid);
  }
}

static gboolean
clapper_get_has_flag (GstClapper * self, gint pos)
{
  gint flags;

  g_object_get (self->playbin, "flags", &flags, NULL);

  return (flags & pos) == pos;
}

static void
clapper_set_flag (GstClapper * self, gint pos)
{
  gint flags;

  g_object_get (self->playbin, "flags", &flags, NULL);
  flags |= pos;
  g_object_set (self->playbin, "flags", flags, NULL);

  GST_DEBUG_OBJECT (self, "setting flags=%#x", flags);
}

static void
clapper_clear_flag (GstClapper * self, gint pos)
{
  gint flags;

  g_object_get (self->playbin, "flags", &flags, NULL);
  flags &= ~pos;
  g_object_set (self->playbin, "flags", flags, NULL);

  GST_DEBUG_OBJECT (self, "setting flags=%#x", flags);
}

static GstCaps *
get_caps (GstClapper * self, gint stream_index, GType type)
{
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

  if (type == GST_TYPE_CLAPPER_VIDEO_INFO)
    g_signal_emit_by_name (G_OBJECT (self->playbin),
        "get-video-pad", stream_index, &pad);
  else if (type == GST_TYPE_CLAPPER_AUDIO_INFO)
    g_signal_emit_by_name (G_OBJECT (self->playbin),
        "get-audio-pad", stream_index, &pad);
  else
    g_signal_emit_by_name (G_OBJECT (self->playbin),
        "get-text-pad", stream_index, &pad);

  if (pad) {
    caps = gst_pad_get_current_caps (pad);
    gst_object_unref (pad);
  }

  return caps;
}

static void
gst_clapper_subtitle_info_update (GstClapper * self,
    GstClapperStreamInfo * stream_info)
{
  GstClapperSubtitleInfo *info = (GstClapperSubtitleInfo *) stream_info;

  /* Free the old info */
  g_free (info->title);
  info->title = NULL;
  g_free (info->language);
  info->language = NULL;

  if (stream_info->tags) {
    gst_tag_list_get_string (stream_info->tags, GST_TAG_TITLE, &info->title);

    /* First try to get the language full name from tag, if name is not
     * available then try language code. If we find the language code
     * then use gstreamer api to translate code to full name.
     */
    gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_NAME,
        &info->language);
    if (!info->language) {
      gchar *lang_code = NULL;

      gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_CODE,
          &lang_code);
      if (lang_code) {
        info->language = g_strdup (gst_tag_get_language_name (lang_code));
        g_free (lang_code);
      }
    }

    /* If we are still failed to find language name then check if external
     * subtitle is loaded and compare the stream index between current sub
     * stream index with our stream index and if matches then declare it as
     * external subtitle and use the filename.
     */
    if (!info->language) {
      gint text_index = -1;
      gchar *suburi = NULL;

      g_object_get (G_OBJECT (self->playbin), "current-suburi", &suburi, NULL);
      if (suburi) {
        if (self->use_playbin3) {
          if (!g_strcmp0 (self->subtitle_sid, stream_info->stream_id))
            info->language = g_path_get_basename (suburi);
        } else {
          g_object_get (G_OBJECT (self->playbin), "current-text", &text_index,
              NULL);
          if (text_index == gst_clapper_stream_info_get_index (stream_info))
            info->language = g_path_get_basename (suburi);
        }
        g_free (suburi);
      }
    }
  }

  GST_DEBUG_OBJECT (self, "Subtitle title: %s", info->title);
  GST_DEBUG_OBJECT (self, "Subtitle language: %s", info->language);
}

static void
gst_clapper_video_info_update (GstClapper * self,
    GstClapperStreamInfo * stream_info)
{
  GstClapperVideoInfo *info = (GstClapperVideoInfo *) stream_info;

  if (stream_info->caps) {
    GstStructure *s;

    s = gst_caps_get_structure (stream_info->caps, 0);
    if (s) {
      gint width, height;
      gint fps_n, fps_d;
      gint par_n, par_d;

      if (gst_structure_get_int (s, "width", &width))
        info->width = width;
      else
        info->width = -1;

      if (gst_structure_get_int (s, "height", &height))
        info->height = height;
      else
        info->height = -1;

      if (gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d)) {
        info->framerate_num = fps_n;
        info->framerate_denom = fps_d;
      } else {
        info->framerate_num = 0;
        info->framerate_denom = 1;
      }


      if (gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d)) {
        info->par_num = par_n;
        info->par_denom = par_d;
      } else {
        info->par_num = 1;
        info->par_denom = 1;
      }
    }
  } else {
    info->width = info->height = -1;
    info->par_num = info->par_denom = 1;
    info->framerate_num = 0;
    info->framerate_denom = 1;
  }

  if (stream_info->tags) {
    guint bitrate, max_bitrate;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_BITRATE, &bitrate))
      info->bitrate = bitrate;
    else
      info->bitrate = -1;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_MAXIMUM_BITRATE,
            &max_bitrate) || gst_tag_list_get_uint (stream_info->tags,
            GST_TAG_NOMINAL_BITRATE, &max_bitrate))
      info->max_bitrate = max_bitrate;
    else
      info->max_bitrate = -1;
  } else {
    info->bitrate = info->max_bitrate = -1;
  }

  GST_DEBUG_OBJECT (self, "width=%d height=%d fps=%.2f par=%d:%d "
      "bitrate=%d max_bitrate=%d", info->width, info->height,
      (gdouble) info->framerate_num / info->framerate_denom,
      info->par_num, info->par_denom, info->bitrate, info->max_bitrate);
}

static void
gst_clapper_audio_info_update (GstClapper * self,
    GstClapperStreamInfo * stream_info)
{
  GstClapperAudioInfo *info = (GstClapperAudioInfo *) stream_info;

  if (stream_info->caps) {
    GstStructure *s;

    s = gst_caps_get_structure (stream_info->caps, 0);
    if (s) {
      gint rate, channels;

      if (gst_structure_get_int (s, "rate", &rate))
        info->sample_rate = rate;
      else
        info->sample_rate = -1;

      if (gst_structure_get_int (s, "channels", &channels))
        info->channels = channels;
      else
        info->channels = 0;
    }
  } else {
    info->sample_rate = -1;
    info->channels = 0;
  }

  if (stream_info->tags) {
    guint bitrate, max_bitrate;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_BITRATE, &bitrate))
      info->bitrate = bitrate;
    else
      info->bitrate = -1;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_MAXIMUM_BITRATE,
            &max_bitrate) || gst_tag_list_get_uint (stream_info->tags,
            GST_TAG_NOMINAL_BITRATE, &max_bitrate))
      info->max_bitrate = max_bitrate;
    else
      info->max_bitrate = -1;

    /* if we have old language the free it */
    g_free (info->language);
    info->language = NULL;

    /* First try to get the language full name from tag, if name is not
     * available then try language code. If we find the language code
     * then use gstreamer api to translate code to full name.
     */
    gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_NAME,
        &info->language);
    if (!info->language) {
      gchar *lang_code = NULL;

      gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_CODE,
          &lang_code);
      if (lang_code) {
        info->language = g_strdup (gst_tag_get_language_name (lang_code));
        g_free (lang_code);
      }
    }
  } else {
    g_free (info->language);
    info->language = NULL;
    info->max_bitrate = info->bitrate = -1;
  }

  GST_DEBUG_OBJECT (self, "language=%s rate=%d channels=%d bitrate=%d "
      "max_bitrate=%d", info->language, info->sample_rate, info->channels,
      info->bitrate, info->max_bitrate);
}

static GstClapperStreamInfo *
gst_clapper_stream_info_find (GstClapperMediaInfo * media_info,
    GType type, gint stream_index)
{
  GList *list, *l;
  GstClapperStreamInfo *info = NULL;

  if (!media_info)
    return NULL;

  list = gst_clapper_media_info_get_stream_list (media_info);
  for (l = list; l != NULL; l = l->next) {
    info = (GstClapperStreamInfo *) l->data;
    if ((G_OBJECT_TYPE (info) == type) && (info->stream_index == stream_index)) {
      return info;
    }
  }

  return NULL;
}

static GstClapperStreamInfo *
gst_clapper_stream_info_find_from_stream_id (GstClapperMediaInfo * media_info,
    const gchar * stream_id)
{
  GList *list, *l;
  GstClapperStreamInfo *info = NULL;

  if (!media_info)
    return NULL;

  list = gst_clapper_media_info_get_stream_list (media_info);
  for (l = list; l != NULL; l = l->next) {
    info = (GstClapperStreamInfo *) l->data;
    if (!g_strcmp0 (info->stream_id, stream_id)) {
      return info;
    }
  }

  return NULL;
}

static GstClapperStreamInfo *
gst_clapper_stream_info_get_current (GstClapper * self, const gchar * prop,
    GType type)
{
  gint current;
  GstClapperStreamInfo *info;

  if (!self->media_info)
    return NULL;

  g_object_get (G_OBJECT (self->playbin), prop, &current, NULL);
  g_mutex_lock (&self->lock);
  info = gst_clapper_stream_info_find (self->media_info, type, current);
  if (info)
    info = gst_clapper_stream_info_copy (info);
  g_mutex_unlock (&self->lock);

  return info;
}

static GstClapperStreamInfo *
gst_clapper_stream_info_get_current_from_stream_id (GstClapper * self,
    const gchar * stream_id, GType type)
{
  GstClapperStreamInfo *info;

  if (!self->media_info || !stream_id)
    return NULL;

  g_mutex_lock (&self->lock);
  info =
      gst_clapper_stream_info_find_from_stream_id (self->media_info, stream_id);
  if (info && G_OBJECT_TYPE (info) == type)
    info = gst_clapper_stream_info_copy (info);
  else
    info = NULL;
  g_mutex_unlock (&self->lock);

  return info;
}

static void
stream_notify_cb (GstStreamCollection * collection, GstStream * stream,
    GParamSpec * pspec, GstClapper * self)
{
  GstClapperStreamInfo *info;
  const gchar *stream_id;
  gboolean emit_update = FALSE;

  if (!self->media_info)
    return;

  if (G_PARAM_SPEC_VALUE_TYPE (pspec) != GST_TYPE_CAPS &&
      G_PARAM_SPEC_VALUE_TYPE (pspec) != GST_TYPE_TAG_LIST)
    return;

  stream_id = gst_stream_get_stream_id (stream);
  g_mutex_lock (&self->lock);
  info =
      gst_clapper_stream_info_find_from_stream_id (self->media_info, stream_id);
  if (info) {
    gst_clapper_stream_info_update_from_stream (self, info, stream);
    emit_update = (self->needs_info_update && GST_IS_CLAPPER_VIDEO_INFO (info));
  }
  g_mutex_unlock (&self->lock);

  if (emit_update)
    emit_media_info_updated (self);
}

static void
gst_clapper_stream_info_update (GstClapper * self, GstClapperStreamInfo * s)
{
  if (GST_IS_CLAPPER_VIDEO_INFO (s))
    gst_clapper_video_info_update (self, s);
  else if (GST_IS_CLAPPER_AUDIO_INFO (s))
    gst_clapper_audio_info_update (self, s);
  else
    gst_clapper_subtitle_info_update (self, s);
}

static gchar *
stream_info_get_codec (GstClapperStreamInfo * s)
{
  const gchar *type;
  GstTagList *tags;
  gchar *codec = NULL;

  if (GST_IS_CLAPPER_VIDEO_INFO (s))
    type = GST_TAG_VIDEO_CODEC;
  else if (GST_IS_CLAPPER_AUDIO_INFO (s))
    type = GST_TAG_AUDIO_CODEC;
  else
    type = GST_TAG_SUBTITLE_CODEC;

  tags = gst_clapper_stream_info_get_tags (s);
  if (tags) {
    gst_tag_list_get_string (tags, type, &codec);
    if (!codec)
      gst_tag_list_get_string (tags, GST_TAG_CODEC, &codec);
  }

  if (!codec) {
    GstCaps *caps;
    caps = gst_clapper_stream_info_get_caps (s);
    if (caps) {
      codec = gst_pb_utils_get_codec_description (caps);
    }
  }

  return codec;
}

static void
gst_clapper_stream_info_update_tags_and_caps (GstClapper * self,
    GstClapperStreamInfo * s)
{
  GstTagList *tags;
  gint stream_index;

  stream_index = gst_clapper_stream_info_get_index (s);

  if (GST_IS_CLAPPER_VIDEO_INFO (s))
    g_signal_emit_by_name (self->playbin, "get-video-tags",
        stream_index, &tags);
  else if (GST_IS_CLAPPER_AUDIO_INFO (s))
    g_signal_emit_by_name (self->playbin, "get-audio-tags",
        stream_index, &tags);
  else
    g_signal_emit_by_name (self->playbin, "get-text-tags", stream_index, &tags);

  if (s->tags)
    gst_tag_list_unref (s->tags);
  s->tags = tags;

  if (s->caps)
    gst_caps_unref (s->caps);
  s->caps = get_caps (self, stream_index, G_OBJECT_TYPE (s));

  g_free (s->codec);
  s->codec = stream_info_get_codec (s);

  GST_DEBUG_OBJECT (self, "%s index: %d tags: %p caps: %p",
      gst_clapper_stream_info_get_stream_type (s), stream_index,
      s->tags, s->caps);

  gst_clapper_stream_info_update (self, s);
}

static void
gst_clapper_streams_info_create (GstClapper * self,
    GstClapperMediaInfo * media_info, const gchar * prop, GType type)
{
  gint i;
  gint total = -1;
  GstClapperStreamInfo *s;

  if (!media_info)
    return;

  g_object_get (G_OBJECT (self->playbin), prop, &total, NULL);

  GST_DEBUG_OBJECT (self, "%s: %d", prop, total);

  for (i = 0; i < total; i++) {
    /* check if stream already exist in the list */
    s = gst_clapper_stream_info_find (media_info, type, i);

    if (!s) {
      /* create a new stream info instance */
      s = gst_clapper_stream_info_new (i, type);

      /* add the object in stream list */
      media_info->stream_list = g_list_append (media_info->stream_list, s);

      /* based on type, add the object in its corresponding stream_ list */
      if (GST_IS_CLAPPER_AUDIO_INFO (s))
        media_info->audio_stream_list = g_list_append
            (media_info->audio_stream_list, s);
      else if (GST_IS_CLAPPER_VIDEO_INFO (s))
        media_info->video_stream_list = g_list_append
            (media_info->video_stream_list, s);
      else
        media_info->subtitle_stream_list = g_list_append
            (media_info->subtitle_stream_list, s);

      GST_DEBUG_OBJECT (self, "create %s stream stream_index: %d",
          gst_clapper_stream_info_get_stream_type (s), i);
    }

    gst_clapper_stream_info_update_tags_and_caps (self, s);
  }
}

static void
gst_clapper_stream_info_update_from_stream (GstClapper * self,
    GstClapperStreamInfo * s, GstStream * stream)
{
  if (s->tags)
    gst_tag_list_unref (s->tags);
  s->tags = gst_stream_get_tags (stream);

  if (s->caps)
    gst_caps_unref (s->caps);
  s->caps = gst_stream_get_caps (stream);

  g_free (s->codec);
  s->codec = stream_info_get_codec (s);

  GST_DEBUG_OBJECT (self, "%s index: %d tags: %p caps: %p",
      gst_clapper_stream_info_get_stream_type (s), s->stream_index,
      s->tags, s->caps);

  gst_clapper_stream_info_update (self, s);
}

static void
gst_clapper_streams_info_create_from_collection (GstClapper * self,
    GstClapperMediaInfo * media_info, GstStreamCollection * collection)
{
  guint i;
  guint total;
  GstClapperStreamInfo *s;
  guint n_audio = 0;
  guint n_video = 0;
  guint n_text = 0;

  if (!media_info || !collection)
    return;

  total = gst_stream_collection_get_size (collection);

  for (i = 0; i < total; i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    GstStreamType stream_type = gst_stream_get_stream_type (stream);
    const gchar *stream_id = gst_stream_get_stream_id (stream);

    if (stream_type & GST_STREAM_TYPE_AUDIO) {
      s = gst_clapper_stream_info_new (n_audio, GST_TYPE_CLAPPER_AUDIO_INFO);
      n_audio++;
    } else if (stream_type & GST_STREAM_TYPE_VIDEO) {
      s = gst_clapper_stream_info_new (n_video, GST_TYPE_CLAPPER_VIDEO_INFO);
      n_video++;
    } else if (stream_type & GST_STREAM_TYPE_TEXT) {
      s = gst_clapper_stream_info_new (n_text, GST_TYPE_CLAPPER_SUBTITLE_INFO);
      n_text++;
    } else {
      GST_DEBUG_OBJECT (self, "Unknown type stream %d", i);
      continue;
    }

    s->stream_id = g_strdup (stream_id);

    /* add the object in stream list */
    media_info->stream_list = g_list_append (media_info->stream_list, s);

    /* based on type, add the object in its corresponding stream_ list */
    if (GST_IS_CLAPPER_AUDIO_INFO (s))
      media_info->audio_stream_list = g_list_append
          (media_info->audio_stream_list, s);
    else if (GST_IS_CLAPPER_VIDEO_INFO (s))
      media_info->video_stream_list = g_list_append
          (media_info->video_stream_list, s);
    else
      media_info->subtitle_stream_list = g_list_append
          (media_info->subtitle_stream_list, s);

    GST_DEBUG_OBJECT (self, "create %s stream stream_index: %d",
        gst_clapper_stream_info_get_stream_type (s), s->stream_index);

    gst_clapper_stream_info_update_from_stream (self, s, stream);
  }
}

static void
video_changed_cb (G_GNUC_UNUSED GObject * object, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  g_mutex_lock (&self->lock);
  gst_clapper_streams_info_create (self, self->media_info,
      "n-video", GST_TYPE_CLAPPER_VIDEO_INFO);
  g_mutex_unlock (&self->lock);
}

static void
audio_changed_cb (G_GNUC_UNUSED GObject * object, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  g_mutex_lock (&self->lock);
  gst_clapper_streams_info_create (self, self->media_info,
      "n-audio", GST_TYPE_CLAPPER_AUDIO_INFO);
  g_mutex_unlock (&self->lock);
}

static void
subtitle_changed_cb (G_GNUC_UNUSED GObject * object, gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  g_mutex_lock (&self->lock);
  gst_clapper_streams_info_create (self, self->media_info,
      "n-text", GST_TYPE_CLAPPER_SUBTITLE_INFO);
  g_mutex_unlock (&self->lock);
}

static gchar *
get_title_from_uri (const gchar * uri)
{
  gchar *proto = gst_uri_get_protocol (uri);
  gchar *title = NULL;

  if (strcmp (proto, "file") == 0) {
    const gchar *ext = strrchr (uri, '.');
    if (ext && strlen (ext) < 8) {
      gchar *filename = g_filename_from_uri (uri, NULL, NULL);
      if (filename) {
        gchar *base = g_path_get_basename (filename);
        g_free (filename);
        title = g_strndup (base, strlen (base) - strlen (ext));
        g_free (base);
      }
    }
  } else if (strcmp (proto, "dvb") == 0) {
    const gchar *channel = strrchr (uri, '/') + 1;
    title = g_strdup (channel);
  }
  g_free (proto);

  return title;
}

static void *
get_title (GstTagList * tags)
{
  gchar *title = NULL;

  gst_tag_list_get_string (tags, GST_TAG_TITLE, &title);
  if (!title)
    gst_tag_list_get_string (tags, GST_TAG_TITLE_SORTNAME, &title);

  return title;
}

static void *
get_container_format (GstTagList * tags)
{
  gchar *container = NULL;

  gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &container);

  /* TODO: If container is not available then maybe consider
   * parsing caps or file extension to guess the container format.
   */

  return container;
}

static void *
get_from_tags (GstClapper * self, GstClapperMediaInfo * media_info,
    void *(*func) (GstTagList *))
{
  GList *l;
  void *ret = NULL;

  if (media_info->tags) {
    ret = func (media_info->tags);
    if (ret)
      return ret;
  }

  /* if global tag does not exit then try video and audio streams */
  GST_DEBUG_OBJECT (self, "trying video tags");
  for (l = gst_clapper_media_info_get_video_streams (media_info); l != NULL;
      l = l->next) {
    GstTagList *tags;

    tags = gst_clapper_stream_info_get_tags ((GstClapperStreamInfo *) l->data);
    if (tags)
      ret = func (tags);

    if (ret)
      return ret;
  }

  GST_DEBUG_OBJECT (self, "trying audio tags");
  for (l = gst_clapper_media_info_get_audio_streams (media_info); l != NULL;
      l = l->next) {
    GstTagList *tags;

    tags = gst_clapper_stream_info_get_tags ((GstClapperStreamInfo *) l->data);
    if (tags)
      ret = func (tags);

    if (ret)
      return ret;
  }

  GST_DEBUG_OBJECT (self, "failed to get the information from tags");
  return NULL;
}

static void *
get_cover_sample (GstTagList * tags)
{
  GstSample *cover_sample = NULL;

  gst_tag_list_get_sample (tags, GST_TAG_IMAGE, &cover_sample);
  if (!cover_sample)
    gst_tag_list_get_sample (tags, GST_TAG_PREVIEW_IMAGE, &cover_sample);

  return cover_sample;
}

static GstClapperMediaInfo *
gst_clapper_media_info_create (GstClapper * self)
{
  GstClapperMediaInfo *media_info;
  GstQuery *query;

  GST_DEBUG_OBJECT (self, "begin");
  media_info = gst_clapper_media_info_new (self->uri);
  media_info->duration = gst_clapper_get_duration (self);
  media_info->tags = self->global_tags;
  media_info->toc = self->global_toc;
  media_info->is_live = self->is_live;
  self->global_tags = NULL;
  self->global_toc = NULL;

  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (gst_element_query (self->playbin, query))
    gst_query_parse_seeking (query, NULL, &media_info->seekable, NULL, NULL);
  gst_query_unref (query);

  if (self->use_playbin3 && self->collection) {
    gst_clapper_streams_info_create_from_collection (self, media_info,
        self->collection);
  } else {
    /* create audio/video/sub streams */
    gst_clapper_streams_info_create (self, media_info, "n-video",
        GST_TYPE_CLAPPER_VIDEO_INFO);
    gst_clapper_streams_info_create (self, media_info, "n-audio",
        GST_TYPE_CLAPPER_AUDIO_INFO);
    gst_clapper_streams_info_create (self, media_info, "n-text",
        GST_TYPE_CLAPPER_SUBTITLE_INFO);
  }

  media_info->title = get_from_tags (self, media_info, get_title);
  if (!media_info->title)
    media_info->title = get_title_from_uri (self->uri);

  media_info->container =
      get_from_tags (self, media_info, get_container_format);
  media_info->image_sample = get_from_tags (self, media_info, get_cover_sample);

  GST_DEBUG_OBJECT (self, "uri: %s, title: %s, duration: %" GST_TIME_FORMAT
      ", seekable: %s, live: %s, container: %s, image_sample %p",
      media_info->uri, media_info->title, GST_TIME_ARGS (media_info->duration),
      media_info->seekable ? "yes" : "no", media_info->is_live ? "yes" : "no",
      media_info->container, media_info->image_sample);

  GST_DEBUG_OBJECT (self, "end");
  return media_info;
}

static void
tags_changed_cb (GstClapper * self, gint stream_index, GType type)
{
  GstClapperStreamInfo *s;

  if (!self->media_info)
    return;

  /* update the stream information */
  g_mutex_lock (&self->lock);
  s = gst_clapper_stream_info_find (self->media_info, type, stream_index);
  gst_clapper_stream_info_update_tags_and_caps (self, s);
  g_mutex_unlock (&self->lock);
}

static void
video_tags_changed_cb (G_GNUC_UNUSED GstElement * playbin, gint stream_index,
    gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  tags_changed_cb (self, stream_index,
      GST_TYPE_CLAPPER_VIDEO_INFO);

  if (self->needs_info_update)
    emit_media_info_updated (self);
}

static void
audio_tags_changed_cb (G_GNUC_UNUSED GstElement * playbin, gint stream_index,
    gpointer user_data)
{
  tags_changed_cb (GST_CLAPPER (user_data), stream_index,
      GST_TYPE_CLAPPER_AUDIO_INFO);
}

static void
subtitle_tags_changed_cb (G_GNUC_UNUSED GstElement * playbin, gint stream_index,
    gpointer user_data)
{
  tags_changed_cb (GST_CLAPPER (user_data), stream_index,
      GST_TYPE_CLAPPER_SUBTITLE_INFO);
}

static void
volume_notify_dispatch (gpointer user_data)
{
  GstClapper *clapper = user_data;

  if (clapper->inhibit_sigs)
    return;

  g_object_notify_by_pspec (G_OBJECT (clapper), param_specs[PROP_VOLUME]);
}

static void
volume_notify_cb (G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GParamSpec * pspec,
    GstClapper * self)
{
  gdouble volume = gst_clapper_get_volume (self);

  if (self->last_volume != volume) {
    self->last_volume = volume;
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        volume_notify_dispatch, g_object_ref (self),
        (GDestroyNotify) g_object_unref);
  }
}

static void
mute_notify_dispatch (gpointer user_data)
{
  GstClapper *clapper = user_data;

  if (clapper->inhibit_sigs)
    return;

  g_object_notify_by_pspec (G_OBJECT (clapper), param_specs[PROP_MUTE]);
}

static void
mute_notify_cb (G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GParamSpec * pspec,
    GstClapper * self)
{
  gboolean mute = gst_clapper_get_mute (self);

  if (self->last_mute != mute) {
    self->last_mute = mute;
    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        mute_notify_dispatch, g_object_ref (self),
        (GDestroyNotify) g_object_unref);
  }
}

typedef struct
{
  GstClapper *clapper;
  gchar *decoder_name;
} DecoderChangedSignalData;

static void
video_decoder_changed_dispatch (gpointer user_data)
{
  DecoderChangedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  g_signal_emit (data->clapper, signals[SIGNAL_VIDEO_DECODER_CHANGED],
      0, data->decoder_name);
}

static void
audio_decoder_changed_dispatch (gpointer user_data)
{
  DecoderChangedSignalData *data = user_data;

  if (data->clapper->inhibit_sigs)
    return;

  g_signal_emit (data->clapper, signals[SIGNAL_AUDIO_DECODER_CHANGED],
      0, data->decoder_name);
}

static void
decoder_changed_signal_data_free (DecoderChangedSignalData * data)
{
  g_object_unref (data->clapper);
  g_free (data->decoder_name);
  g_free (data);
}

static void
emit_decoder_changed (GstClapper * self, gchar * decoder_name,
    GstElementFactoryListType type)
{
  GstClapperSignalDispatcherFunc func = NULL;

  if ((type & GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO) ==
      GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO) {
    if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
        signals[SIGNAL_VIDEO_DECODER_CHANGED], 0, NULL, NULL, NULL) != 0 &&
        g_strcmp0 (self->last_vdecoder, decoder_name) != 0) {
      func = video_decoder_changed_dispatch;
      g_free (self->last_vdecoder);
      self->last_vdecoder = g_strdup (decoder_name);
    }
  } else if ((type & GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO) ==
      GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO) {
    if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
        signals[SIGNAL_AUDIO_DECODER_CHANGED], 0, NULL, NULL, NULL) != 0 &&
        g_strcmp0 (self->last_adecoder, decoder_name) != 0) {
      func = audio_decoder_changed_dispatch;
      g_free (self->last_adecoder);
      self->last_adecoder = g_strdup (decoder_name);
    }
  }

  if (func) {
    DecoderChangedSignalData *data = g_new (DecoderChangedSignalData, 1);

    data->clapper = g_object_ref (self);
    data->decoder_name = g_strdup (decoder_name);

    gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, self,
        func, data, (GDestroyNotify) decoder_changed_signal_data_free);
  }
}

static gboolean
iterate_decoder_pads (GstClapper * self, GstElement * element,
    const gchar * stream_id, GstElementFactoryListType type)
{
  GstIterator *iter;
  GValue value = { 0, };
  gboolean found = FALSE;

  iter = gst_element_iterate_src_pads (element);

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *decoder_pad = g_value_get_object (&value);
    gchar *decoder_stream_id = gst_pad_get_stream_id (decoder_pad);

    GST_DEBUG_OBJECT (self, "Decoder stream: %s", decoder_stream_id);

    /* In case of playbin3, pad may not be active yet */
    if ((found = (g_strcmp0 (decoder_stream_id, stream_id) == 0
        || (!decoder_stream_id && self->use_playbin3)))) {
      GstElementFactory *factory;
      gchar *plugin_name;

      factory = gst_element_get_factory (element);
      plugin_name = gst_object_get_name (GST_OBJECT_CAST (factory));

      if (plugin_name) {
        GST_DEBUG_OBJECT (self, "Found decoder: %s", plugin_name);
        emit_decoder_changed (self, plugin_name, type);

        g_free (plugin_name);
      }
    }

    g_free (decoder_stream_id);
    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);

  return found;
}

static gboolean
find_active_decoder_with_stream_id (GstClapper * self, GstElementFactoryListType type,
    const gchar * stream_id)
{
  GstIterator *iter;
  GValue value = { 0, };
  gboolean found = FALSE;

  GST_DEBUG_OBJECT (self, "Searching for decoder with stream: %s", stream_id);

  iter = gst_bin_iterate_recurse (GST_BIN (self->playbin));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstElement *element = g_value_get_object (&value);
    GstElementFactory *factory = gst_element_get_factory (element);

    if (factory && gst_element_factory_list_is_type (factory, type))
      found = iterate_decoder_pads (self, element, stream_id, type);

    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);

  return found;
}

static void
update_current_decoder (GstClapper *self, GstElementFactoryListType type)
{
  GstIterator *iter;
  GValue value = { 0, };

  iter = gst_bin_iterate_all_by_element_factory_name (
      GST_BIN (self->playbin), "input-selector");

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstElement *element = g_value_get_object (&value);
    GstPad *active_pad;
    gboolean found = FALSE;

    g_object_get (G_OBJECT (element), "active-pad", &active_pad, NULL);

    if (active_pad) {
      gchar *stream_id;

      stream_id = gst_pad_get_stream_id (active_pad);
      gst_object_unref (active_pad);

      if (stream_id) {
        found = find_active_decoder_with_stream_id (self, type, stream_id);
        g_free (stream_id);
      }
    }

    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);
}

static void
current_video_notify_cb (G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GParamSpec * pspec,
    GstClapper * self)
{
  GstElementFactoryListType type = GST_ELEMENT_FACTORY_TYPE_DECODER
      | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO;

  update_current_decoder (self, type);
}

static void
current_audio_notify_cb (G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED GParamSpec * pspec,
    GstClapper * self)
{
  GstElementFactoryListType type = GST_ELEMENT_FACTORY_TYPE_DECODER
      | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO;

  update_current_decoder (self, type);
}

static void
element_setup_cb (GstElement * playbin, GstElement * element, GstClapper * self)
{
  GstElementFactory *factory;
  GParamSpec *prop;

  factory = gst_element_get_factory (element);
  if (factory) {
    gchar *plugin_name = gst_object_get_name (GST_OBJECT_CAST (factory));
    if (plugin_name) {
      GST_INFO_OBJECT (self, "Plugin setup: %s", plugin_name);

      /* TODO: Set plugin props */
    }
    g_free (plugin_name);
  }

  prop = g_object_class_find_property (G_OBJECT_GET_CLASS (element), "user-agent");
  if (prop && prop->value_type == G_TYPE_STRING) {
    const gchar *user_agent =
        "Mozilla/5.0 (X11; Linux x86_64; rv:86.0) Gecko/20100101 Firefox/86.0";

    GST_INFO_OBJECT (self, "Setting element user-agent: %s", user_agent);
    g_object_set (element, "user-agent", user_agent, NULL);
  }
}

static void
_update_from_env (gboolean * enabled, const gchar * env_name)
{
  const gchar *env = g_getenv (env_name);

  if (env) {
    if (g_str_has_prefix (env, "1"))
      *enabled = TRUE;
    else if (g_str_has_prefix (env, "0"))
      *enabled = FALSE;
  }
}

static gpointer
gst_clapper_main (gpointer data)
{
  GstClapper *self = GST_CLAPPER (data);
  GstBus *bus;
  GSource *source;
  GstElement *scaletempo, *pipewiresink;

  GST_TRACE_OBJECT (self, "Starting main thread");

  g_main_context_push_thread_default (self->context);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->context);
  g_source_unref (source);

  _update_from_env (&self->use_playbin3, "GST_CLAPPER_USE_PLAYBIN3");

  /* Takes precedence over `GST_CLAPPER_USE_PLAYBIN3` as it
   * influences element factory behavior */
  _update_from_env (&self->use_playbin3, "USE_PLAYBIN3");

  if (self->use_playbin3) {
    GST_DEBUG_OBJECT (self, "playbin3 enabled");
    self->playbin = gst_element_factory_make ("playbin3", "playbin3");
  } else {
    self->playbin = gst_element_factory_make ("playbin", "playbin");
  }

  if (!self->playbin) {
    g_error ("GstClapper: 'playbin' element not found, please check your setup");
    g_assert_not_reached ();
  }

  gst_object_ref_sink (self->playbin);

  /* Do not start muted and make it sync with last_mute value */
  g_object_set (self->playbin, "mute", FALSE, NULL);

  if (self->video_renderer) {
    GstElement *video_sink =
        gst_clapper_video_renderer_create_video_sink (self->video_renderer, self);
    if (video_sink) {
      const gchar *fps_env;
      GstPad *video_sink_pad = gst_element_get_static_pad (video_sink, "sink");
      if (video_sink_pad) {
        g_signal_connect (video_sink_pad, "notify::caps",
            (GCallback) notify_caps_cb, self);
        gst_object_unref (video_sink_pad);
      }
      fps_env = g_getenv ("GST_CLAPPER_DISPLAY_FPS");
      if (fps_env && g_str_has_prefix (fps_env, "1")) {
        GstElement *fpsdisplaysink =
            gst_element_factory_make ("fpsdisplaysink", "fpsdisplaysink");
        if (fpsdisplaysink) {
          GST_DEBUG_OBJECT (self, "FPS display enabled");
          g_object_set (fpsdisplaysink, "video-sink", video_sink, NULL);
          video_sink = fpsdisplaysink;
        }
      }
      g_object_set (self->playbin, "video-sink", video_sink, NULL);
    }
  }

  _update_from_env (&self->use_pipewire, "GST_CLAPPER_USE_PIPEWIRE");

  if (self->use_pipewire) {
    pipewiresink = gst_element_factory_make ("pipewiresink", NULL);
    if (pipewiresink) {
      g_object_set (self->playbin, "audio-sink", pipewiresink, NULL);
    } else {
      GstElement *fakesink;

      g_warning ("GstClapper: pipewiresink element not available");
      fakesink = gst_element_factory_make ("fakesink", "fakeaudiosink");
      if (fakesink) {
        g_object_set (fakesink, "sync", TRUE, NULL);
        g_object_set (self->playbin, "audio-sink", fakesink, NULL);
      } else {
        g_warning ("GstClapper: default audio sink will be used instead");
      }
    }
  }

  scaletempo = gst_element_factory_make ("scaletempo", NULL);
  if (scaletempo) {
    g_object_set (self->playbin, "audio-filter", scaletempo, NULL);
  } else {
    g_warning ("GstClapper: scaletempo element not available. "
        "Audio pitch will not be preserved during trick modes.");
  }

  self->bus = bus = gst_element_get_bus (self->playbin);
  gst_bus_add_signal_watch (bus);

  if (self->mpris)
    gst_clapper_mpris_set_clapper (self->mpris, self, self->signal_dispatcher);

  g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (error_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::warning", G_CALLBACK (warning_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::eos", G_CALLBACK (eos_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::state-changed",
      G_CALLBACK (state_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::buffering",
      G_CALLBACK (buffering_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::clock-lost",
      G_CALLBACK (clock_lost_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::duration-changed",
      G_CALLBACK (duration_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::latency",
      G_CALLBACK (latency_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::request-state",
      G_CALLBACK (request_state_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::element",
      G_CALLBACK (element_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::tag", G_CALLBACK (tags_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::toc", G_CALLBACK (toc_cb), self);

  if (gst_debug_category_get_threshold (gst_clapper_debug) >= GST_LEVEL_DEBUG)
    g_signal_connect (G_OBJECT (bus), "message::qos", G_CALLBACK (qos_cb), self);

  if (self->use_playbin3) {
    g_signal_connect (G_OBJECT (bus), "message::stream-collection",
        G_CALLBACK (stream_collection_cb), self);
    g_signal_connect (G_OBJECT (bus), "message::streams-selected",
        G_CALLBACK (streams_selected_cb), self);
  } else {
    g_signal_connect (self->playbin, "video-changed",
        G_CALLBACK (video_changed_cb), self);
    g_signal_connect (self->playbin, "audio-changed",
        G_CALLBACK (audio_changed_cb), self);
    g_signal_connect (self->playbin, "text-changed",
        G_CALLBACK (subtitle_changed_cb), self);

    g_signal_connect (self->playbin, "video-tags-changed",
        G_CALLBACK (video_tags_changed_cb), self);
    g_signal_connect (self->playbin, "audio-tags-changed",
        G_CALLBACK (audio_tags_changed_cb), self);
    g_signal_connect (self->playbin, "text-tags-changed",
        G_CALLBACK (subtitle_tags_changed_cb), self);

    g_signal_connect (self->playbin, "notify::current-video",
        G_CALLBACK (current_video_notify_cb), self);
    g_signal_connect (self->playbin, "notify::current-audio",
        G_CALLBACK (current_audio_notify_cb), self);
  }

  g_signal_connect (self->playbin, "notify::volume",
      G_CALLBACK (volume_notify_cb), self);
  g_signal_connect (self->playbin, "notify::mute",
      G_CALLBACK (mute_notify_cb), self);
  g_signal_connect (self->playbin, "element-setup",
      G_CALLBACK (element_setup_cb), self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  change_state (self, GST_CLAPPER_STATE_STOPPED);
  self->buffering = 100;
  self->is_live = FALSE;
  self->rate = 1.0;
  self->seek_mode = DEFAULT_SEEK_MODE;
  self->cached_duration = GST_CLOCK_TIME_NONE;

  GST_TRACE_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_TRACE_OBJECT (self, "Stopped main loop");

  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  remove_tick_source (self);

  g_mutex_lock (&self->lock);
  if (self->media_info) {
    g_object_unref (self->media_info);
    self->media_info = NULL;
  }

  remove_seek_source (self);
  g_mutex_unlock (&self->lock);

  g_main_context_pop_thread_default (self->context);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  if (self->playbin) {
    gst_element_set_state (self->playbin, GST_STATE_NULL);
    gst_object_unref (self->playbin);
    self->playbin = NULL;
  }

  GST_TRACE_OBJECT (self, "Stopped main thread");

  return NULL;
}

static gboolean
gst_clapper_set_feature_rank_versioned (const gchar * name, guint rank,
    guint min_major, guint min_minor, guint min_micro)
{
  GstRegistry *registry = gst_registry_get ();
  GstPluginFeature *feature = gst_registry_lookup_feature (registry, name);
  gboolean res = FALSE;

  if (!feature) {
    GST_DEBUG ("Cannot change rank of unavailable feature: %s", name);
    return res;
  }
  if (gst_plugin_feature_check_version (feature, min_major, min_minor, min_micro)) {
    guint old_rank = gst_plugin_feature_get_rank (feature);
    gst_plugin_feature_set_rank (feature, rank);
    res = TRUE;
    GST_DEBUG ("Changed rank: %i -> %i for %s", old_rank, rank, name);
  } else {
    GST_DEBUG ("Feature %s is at older version then required", name);
  }

  gst_object_unref (feature);
  return res;
}

static gboolean
gst_clapper_set_feature_rank (const gchar * name, guint rank)
{
  return gst_clapper_set_feature_rank_versioned (name, rank, 0, 0, 0);
}

static gboolean
gst_clapper_has_plugin_with_features (const gchar * name)
{
  GstRegistry *registry = gst_registry_get ();
  GList *features = gst_registry_get_feature_list_by_plugin (registry, name);

  gboolean ret = g_list_length (features) > 0;

  gst_plugin_feature_list_free (features);
  return ret;
}

static gboolean
parse_feature_name (gchar * str, const gchar ** feature)
{
  if (!str)
    return FALSE;

  g_strstrip (str);

  if (str[0] != '\0') {
    *feature = str;
    return TRUE;
  }

  return FALSE;
}

static gboolean
parse_feature_rank (gchar * str, GstRank * rank)
{
  if (!str)
    return FALSE;

  g_strstrip (str);

  if (g_ascii_isdigit (str[0])) {
    unsigned long l;
    char *endptr;
    l = strtoul (str, &endptr, 10);
    if (endptr > str && endptr[0] == 0) {
      *rank = (GstRank) l;
    } else {
      return FALSE;
    }
  } else if (g_ascii_strcasecmp (str, "NONE") == 0) {
    *rank = GST_RANK_NONE;
  } else if (g_ascii_strcasecmp (str, "MARGINAL") == 0) {
    *rank = GST_RANK_MARGINAL;
  } else if (g_ascii_strcasecmp (str, "SECONDARY") == 0) {
    *rank = GST_RANK_SECONDARY;
  } else if (g_ascii_strcasecmp (str, "PRIMARY") == 0) {
    *rank = GST_RANK_PRIMARY;
  } else if (g_ascii_strcasecmp (str, "MAX") == 0) {
    *rank = (GstRank) G_MAXINT;
  } else {
    return FALSE;
  }

  return TRUE;
}

static void
_env_feature_rank_update (void)
{
  const gchar *env;
  gchar **split, **walk;

  env = g_getenv ("GST_PLUGIN_FEATURE_RANK");

  if (!env)
    return;

  split = g_strsplit (env, ",", 0);

  for (walk = split; *walk; walk++) {
    if (strchr (*walk, ':')) {
      gchar **values;

      values = g_strsplit (*walk, ":", 2);
      if (values[0] && values[1]) {
        GstRank rank;
        const gchar *name;

        if (parse_feature_name (values[0], &name)
            && parse_feature_rank (values[1], &rank)) {
          GstPluginFeature *feature;

          feature = gst_registry_find_feature (gst_registry_get (), name,
              GST_TYPE_ELEMENT_FACTORY);
          if (feature) {
            GstRank old_rank;

            old_rank = gst_plugin_feature_get_rank (feature);
            if (old_rank != rank) {
              gst_plugin_feature_set_rank (feature, rank);
              GST_DEBUG ("Updated rank from env: %i -> %i for %s", old_rank, rank, name);
            }
            gst_object_unref (feature);
          }
        }
      }

      g_strfreev (values);
    }
  }

  g_strfreev (split);
}

static void
gst_clapper_prepare_gstreamer (void)
{
  const guint rank = GST_RANK_PRIMARY + 24;

  GST_DEBUG ("Preparing GStreamer plugins");

  /* Too many problems with VAAPI decodebin, meanwhile VA works
   * fine and there is already a pending MR on GStreamer to enable
   * it by default, so we do that a little early */
  if (gst_clapper_has_plugin_with_features ("va")) {
    gst_clapper_set_feature_rank ("vampeg2dec", rank);
    gst_clapper_set_feature_rank ("vah264dec", rank);
    gst_clapper_set_feature_rank ("vah265dec", rank);
    gst_clapper_set_feature_rank ("vavp8dec", rank);
    gst_clapper_set_feature_rank ("vavp9dec", rank);
    gst_clapper_set_feature_rank ("vaav1dec", rank);
  }
  /* We do promise working HW accel out of box, so enable NVDEC too */
  if (gst_clapper_has_plugin_with_features ("nvcodec")) {
    gst_clapper_set_feature_rank ("nvh264dec", rank + 4);
    gst_clapper_set_feature_rank ("nvh265dec", rank + 4);
    gst_clapper_set_feature_rank ("nvvp8dec", rank + 4);
    gst_clapper_set_feature_rank ("nvvp9dec", rank + 4);
  }

  /* After setting defaults, update them from ENV */
  _env_feature_rank_update ();

  gst_clapper_gstreamer_prepared = TRUE;
  GST_DEBUG ("GStreamer plugins prepared");
}

/**
 * gst_clapper_gst_init:
 * @argc: (inout) (allow-none): pointer to application's argc
 * @argv: (inout) (array length=argc) (allow-none): pointer to application's argv
 *
 * Automatically initializes GStreamer library if this was not done by the user yet
 * and tweaks some of its defaults to our liking. It is recommended to use this
 * function instead of plain gst_init().
 *
 * This also allows usage of GstClapper API alone without importing GStreamer
 * on the implementation side.
 */
void
gst_clapper_gst_init (int * argc, char ** argv[])
{
  if (!gst_is_initialized ())
    gst_init (argc, argv);
  if (!gst_clapper_gstreamer_prepared)
    gst_clapper_prepare_gstreamer ();
}

/**
 * gst_clapper_new:
 * @video_renderer: (transfer full) (allow-none): GstClapperVideoRenderer to use
 * @signal_dispatcher: (transfer full) (allow-none): GstClapperSignalDispatcher to use
 * @mpris: (transfer full) (allow-none): GstClapperMpris to use
 *
 * Creates a new #GstClapper instance that uses @signal_dispatcher to dispatch
 * signals to some event loop system, or emits signals directly if NULL is
 * passed. See gst_clapper_g_main_context_signal_dispatcher_new().
 *
 * Video is going to be rendered by @video_renderer, or if %NULL is provided
 * no special video set up will be done and some default handling will be
 * performed.
 *
 * Returns: (transfer full): a new #GstClapper instance
 */
GstClapper *
gst_clapper_new (GstClapperVideoRenderer * video_renderer,
    GstClapperSignalDispatcher * signal_dispatcher,
    GstClapperMpris * mpris)
{
  GstClapper *self;

  self = g_object_new (GST_TYPE_CLAPPER, "video-renderer", video_renderer,
      "signal-dispatcher", signal_dispatcher, "mpris", mpris, NULL);

  if (video_renderer)
    g_object_unref (video_renderer);
  if (signal_dispatcher)
    g_object_unref (signal_dispatcher);
  if (mpris)
    g_object_unref (mpris);

  return self;
}

static gboolean
gst_clapper_play_internal (gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Play");

  g_mutex_lock (&self->lock);
  if (!self->uri) {
    g_mutex_unlock (&self->lock);
    return G_SOURCE_REMOVE;
  }
  g_mutex_unlock (&self->lock);

  self->target_state = GST_STATE_PLAYING;

  if (self->current_state < GST_STATE_PAUSED)
    change_state (self, GST_CLAPPER_STATE_BUFFERING);

  if (self->current_state >= GST_STATE_PAUSED
      && self->buffering >= 100 && !(self->seek_position != GST_CLOCK_TIME_NONE
          || self->seek_pending)) {
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
  } else {
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
  }

  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
            "Failed to play"));
    return G_SOURCE_REMOVE;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  return G_SOURCE_REMOVE;
}

/**
 * gst_clapper_play:
 * @clapper: #GstClapper instance
 *
 * Request to play the loaded stream.
 */
void
gst_clapper_play (GstClapper * self)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (!self->can_start && self->app_state == GST_CLAPPER_STATE_STOPPED) {
    GST_DEBUG_OBJECT (self, "Player stopped, play request ignored");
    return;
  }

  g_mutex_lock (&self->lock);
  self->inhibit_sigs = FALSE;
  self->can_start = FALSE;
  g_mutex_unlock (&self->lock);

  g_main_context_invoke_full (self->context, G_PRIORITY_DEFAULT,
      gst_clapper_play_internal, self, NULL);
}

static gboolean
gst_clapper_pause_internal (gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Pause");

  g_mutex_lock (&self->lock);
  if (!self->uri) {
    g_mutex_unlock (&self->lock);
    return G_SOURCE_REMOVE;
  }
  g_mutex_unlock (&self->lock);

  tick_cb (self);
  remove_tick_source (self);

  self->target_state = GST_STATE_PAUSED;

  if (self->current_state < GST_STATE_PAUSED)
    change_state (self, GST_CLAPPER_STATE_BUFFERING);

  state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
            "Failed to pause"));
    return G_SOURCE_REMOVE;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  return G_SOURCE_REMOVE;
}

/**
 * gst_clapper_pause:
 * @clapper: #GstClapper instance
 *
 * Pauses the current stream.
 */
void
gst_clapper_pause (GstClapper * self)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (self->app_state == GST_CLAPPER_STATE_STOPPED) {
    GST_DEBUG_OBJECT (self, "Player stopped, pause request ignored");
    return;
  }

  if (G_UNLIKELY (self->cached_duration <= GST_SECOND)) {
    GST_DEBUG_OBJECT (self, "Cannot pause on this stream");
    return;
  }

  g_mutex_lock (&self->lock);
  self->inhibit_sigs = FALSE;
  g_mutex_unlock (&self->lock);

  g_main_context_invoke_full (self->context, G_PRIORITY_DEFAULT,
      gst_clapper_pause_internal, self, NULL);
}

/**
 * gst_clapper_toggle_play:
 * @clapper: #GstClapper instance
 *
 * Toggle between play and pause on the loaded stream.
 * This function does nothing if player is stopped.
 */
void
gst_clapper_toggle_play (GstClapper * self)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (self->app_state == GST_CLAPPER_STATE_PLAYING)
    gst_clapper_pause (self);
  else
    gst_clapper_play (self);
}

static void
gst_clapper_stop_internal (GstClapper * self, gboolean transient)
{
  /* directly return if we're already stopped */
  if (self->current_state <= GST_STATE_READY &&
      self->target_state <= GST_STATE_READY)
    return;

  GST_DEBUG_OBJECT (self, "Stop (transient %d)", transient);

  tick_cb (self);
  remove_tick_source (self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  self->is_live = FALSE;
  gst_bus_set_flushing (self->bus, TRUE);
  gst_element_set_state (self->playbin, GST_STATE_NULL);
  gst_bus_set_flushing (self->bus, FALSE);
  change_state (self, transient && self->app_state != GST_CLAPPER_STATE_STOPPED
      ? GST_CLAPPER_STATE_BUFFERING : GST_CLAPPER_STATE_STOPPED);
  self->buffering = 100;
  self->cached_duration = GST_CLOCK_TIME_NONE;
  g_mutex_lock (&self->lock);
  if (self->media_info) {
    g_object_unref (self->media_info);
    self->media_info = NULL;
  }
  if (self->global_tags) {
    gst_tag_list_unref (self->global_tags);
    self->global_tags = NULL;
  }
  if (self->global_toc) {
    gst_toc_unref (self->global_toc);
    self->global_toc = NULL;
  }
  self->seek_pending = FALSE;
  remove_seek_source (self);
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;
  if (self->collection) {
    if (self->stream_notify_id)
      g_signal_handler_disconnect (self->collection, self->stream_notify_id);
    self->stream_notify_id = 0;
    gst_object_unref (self->collection);
    self->collection = NULL;
  }
  g_free (self->video_sid);
  g_free (self->audio_sid);
  g_free (self->subtitle_sid);
  self->video_sid = NULL;
  self->audio_sid = NULL;
  self->subtitle_sid = NULL;
  g_mutex_unlock (&self->lock);
}

static gboolean
gst_clapper_stop_internal_dispatch (gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  gst_clapper_stop_internal (self, FALSE);

  return G_SOURCE_REMOVE;
}


/**
 * gst_clapper_stop:
 * @clapper: #GstClapper instance
 *
 * Stops playing the current stream and resets to the first position
 * in the stream.
 */
void
gst_clapper_stop (GstClapper * self)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_mutex_lock (&self->lock);
  self->inhibit_sigs = TRUE;
  g_mutex_unlock (&self->lock);

  g_main_context_invoke_full (self->context, G_PRIORITY_DEFAULT,
      gst_clapper_stop_internal_dispatch, self, NULL);
}

/* Must be called with lock from main context, releases lock! */
static void
gst_clapper_seek_internal_locked (GstClapper * self)
{
  gboolean ret;
  GstClockTime position;
  gdouble rate;
  GstStateChangeReturn state_ret;
  GstEvent *s_event;
  GstClapperSeekMode seek_mode;
  GstSeekFlags flags = 0;

  remove_seek_source (self);

  /* Only seek in PAUSED */
  if (self->current_state < GST_STATE_PAUSED) {
    return;
  } else if (self->current_state != GST_STATE_PAUSED) {
    g_mutex_unlock (&self->lock);
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
              "Failed to seek"));
    }
    g_mutex_lock (&self->lock);
    return;
  }

  self->last_seek_time = gst_util_get_timestamp ();
  position = self->seek_position;
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->seek_pending = TRUE;
  rate = self->rate;
  seek_mode = self->seek_mode;
  g_mutex_unlock (&self->lock);

  remove_tick_source (self);

  flags |= GST_SEEK_FLAG_FLUSH;

  switch (seek_mode) {
    case GST_CLAPPER_SEEK_MODE_ACCURATE:
      flags |= GST_SEEK_FLAG_ACCURATE;
      break;
    case GST_CLAPPER_SEEK_MODE_FAST:
      flags |= GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_NEAREST;
      break;
    default:
      break;
  }

  if (rate != 1.0)
    flags |= GST_SEEK_FLAG_TRICKMODE;

  if (rate >= 0.0) {
    s_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  } else {
    s_event = gst_event_new_seek (rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0), GST_SEEK_TYPE_SET, position);
  }

  GST_DEBUG_OBJECT (self, "Seek with rate %.2lf to %" GST_TIME_FORMAT,
      rate, GST_TIME_ARGS (position));

  ret = gst_element_send_event (self->playbin, s_event);
  if (!ret)
    emit_error (self, g_error_new (GST_CLAPPER_ERROR, GST_CLAPPER_ERROR_FAILED,
            "Failed to seek to %" GST_TIME_FORMAT, GST_TIME_ARGS (position)));

  g_mutex_lock (&self->lock);
}

static gboolean
gst_clapper_seek_internal (gpointer user_data)
{
  GstClapper *self = GST_CLAPPER (user_data);

  g_mutex_lock (&self->lock);
  gst_clapper_seek_internal_locked (self);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

/**
 * gst_clapper_set_rate:
 * @clapper: #GstClapper instance
 * @rate: playback rate
 *
 * Playback at specified rate
 */
void
gst_clapper_set_rate (GstClapper * self, gdouble rate)
{
  g_return_if_fail (GST_IS_CLAPPER (self));
  g_return_if_fail (rate != 0.0);

  g_object_set (self, "rate", rate, NULL);
}

/**
 * gst_clapper_get_rate:
 * @clapper: #GstClapper instance
 *
 * Returns: current playback rate
 */
gdouble
gst_clapper_get_rate (GstClapper * self)
{
  gdouble val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_RATE);

  g_object_get (self, "rate", &val, NULL);

  return val;
}

/**
 * gst_clapper_seek:
 * @clapper: #GstClapper instance
 * @position: position to seek in nanoseconds
 *
 * Seeks the currently-playing stream to the absolute @position time
 * in nanoseconds.
 */
void
gst_clapper_seek (GstClapper * self, GstClockTime position)
{
  g_return_if_fail (GST_IS_CLAPPER (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (position));

  g_mutex_lock (&self->lock);
  if (self->media_info && !self->media_info->seekable) {
    GST_DEBUG_OBJECT (self, "Media is not seekable");
    g_mutex_unlock (&self->lock);
    return;
  }

  self->seek_position = position;

  /* If there is no seek being dispatch to the main context currently do that,
   * otherwise we just updated the seek position so that it will be taken by
   * the seek handler from the main context instead of the old one.
   */
  if (!self->seek_source) {
    GstClockTime now = gst_util_get_timestamp ();

    /* If no seek is pending or it was started more than 250 mseconds ago seek
     * immediately, otherwise wait until the 250 mseconds have passed */
    if (!self->seek_pending || (now - self->last_seek_time > 250 * GST_MSECOND)) {
      self->seek_source = g_idle_source_new ();
      g_source_set_callback (self->seek_source,
          (GSourceFunc) gst_clapper_seek_internal, self, NULL);
      GST_TRACE_OBJECT (self, "Dispatching seek to position %" GST_TIME_FORMAT,
          GST_TIME_ARGS (position));
      g_source_attach (self->seek_source, self->context);
    } else {
      guint delay = 250000 - (now - self->last_seek_time) / 1000;

      /* Note that last_seek_time must be set to something at this point and
       * it must be smaller than 250 mseconds */
      self->seek_source = g_timeout_source_new (delay);
      g_source_set_callback (self->seek_source,
          (GSourceFunc) gst_clapper_seek_internal, self, NULL);

      GST_TRACE_OBJECT (self,
          "Delaying seek to position %" GST_TIME_FORMAT " by %u us",
          GST_TIME_ARGS (position), delay);
      g_source_attach (self->seek_source, self->context);
    }
  }
  g_mutex_unlock (&self->lock);
}

/**
 * gst_clapper_seek_offset:
 * @clapper: #GstClapper instance
 * @offset: offset from current position to seek to in nanoseconds
 *
 * Seeks the currently-playing stream to the @offset time
 * in nanoseconds.
 */
void
gst_clapper_seek_offset (GstClapper * self, GstClockTime offset)
{
  GstClockTime position;

  g_return_if_fail (GST_IS_CLAPPER (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (offset));

  position = gst_clapper_get_position (self);

  /* TODO: Prevent negative values */

  gst_clapper_seek (self, position + offset);
}

static void
remove_seek_source (GstClapper * self)
{
  if (!self->seek_source)
    return;

  g_source_destroy (self->seek_source);
  g_source_unref (self->seek_source);
  self->seek_source = NULL;
}

/**
 * gst_clapper_get_state:
 * @clapper: #GstClapper instance
 *
 * Returns: Current player state
 */
GstClapperState
gst_clapper_get_state (GstClapper * self)
{
  GstClapperState state;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_STATE);

  g_object_get (self, "state", &state, NULL);

  return state;
}

/**
 * gst_clapper_get_uri:
 * @clapper: #GstClapper instance
 *
 * Gets the URI of the currently-playing stream.
 *
 * Returns: (transfer full): a string containing the URI of the
 * currently-playing stream. g_free() after usage.
 */
gchar *
gst_clapper_get_uri (GstClapper * self)
{
  gchar *val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_URI);

  g_object_get (self, "uri", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_uri:
 * @clapper: #GstClapper instance
 * @uri: next URI to play.
 *
 * Sets the next URI to play.
 */
void
gst_clapper_set_uri (GstClapper * self, const gchar * val)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "uri", val, NULL);
}

/**
 * gst_clapper_set_subtitle_uri:
 * @clapper: #GstClapper instance
 * @uri: subtitle URI
 *
 * Sets the external subtitle URI. This should be combined with a call to
 * gst_clapper_set_subtitle_track_enabled(@clapper, TRUE) so the subtitles are actually
 * rendered.
 */
void
gst_clapper_set_subtitle_uri (GstClapper * self, const gchar * suburi)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "suburi", suburi, NULL);
}

/**
 * gst_clapper_get_subtitle_uri:
 * @clapper: #GstClapper instance
 *
 * current subtitle URI
 *
 * Returns: (transfer full): URI of the current external subtitle.
 *   g_free() after usage.
 */
gchar *
gst_clapper_get_subtitle_uri (GstClapper * self)
{
  gchar *val = NULL;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  g_object_get (self, "suburi", &val, NULL);

  return val;
}

/**
 * gst_clapper_get_position:
 * @clapper: #GstClapper instance
 *
 * Returns: the absolute position time, in nanoseconds, of the
 * currently-playing stream.
 */
GstClockTime
gst_clapper_get_position (GstClapper * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_POSITION);

  g_object_get (self, "position", &val, NULL);

  return val;
}

/**
 * gst_clapper_get_duration:
 * @clapper: #GstClapper instance
 *
 * Retrieves the duration of the media stream that self represents.
 *
 * Returns: the duration of the currently-playing media stream, in
 * nanoseconds.
 */
GstClockTime
gst_clapper_get_duration (GstClapper * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_DURATION);

  g_object_get (self, "duration", &val, NULL);

  return val;
}

/**
 * gst_clapper_get_volume:
 * @clapper: #GstClapper instance
 *
 * Returns the current volume level, as a percentage between 0 and 1.5
 *
 * Returns: the cubic volume as percentage between 0 and 1.5
 */
gdouble
gst_clapper_get_volume (GstClapper * self)
{
  gdouble val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_VOLUME);

  g_object_get (self, "volume", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_volume:
 * @clapper: #GstClapper instance
 * @val: the new volume level, as a percentage between 0 and 1.5
 *
 * Sets the volume level of the stream as a percentage between 0 and 1.5
 * Volume operates on a cubic scale.
 */
void
gst_clapper_set_volume (GstClapper * self, gdouble val)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "volume", val, NULL);
}

/**
 * gst_clapper_get_mute:
 * @clapper: #GstClapper instance
 *
 * Returns: %TRUE if the currently-playing stream is muted.
 */
gboolean
gst_clapper_get_mute (GstClapper * self)
{
  gboolean val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_MUTE);

  g_object_get (self, "mute", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_mute:
 * @clapper: #GstClapper instance
 * @val: Mute state the should be set
 *
 * %TRUE if the currently-playing stream should be muted.
 */
void
gst_clapper_set_mute (GstClapper * self, gboolean val)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "mute", val, NULL);
}

/**
 * gst_clapper_get_pipeline:
 * @clapper: #GstClapper instance
 *
 * Returns: (transfer full): The internal playbin instance.
 *
 * The caller should free it with g_object_unref()
 */
GstElement *
gst_clapper_get_pipeline (GstClapper * self)
{
  GstElement *val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  g_object_get (self, "pipeline", &val, NULL);

  return val;
}

/**
 * gst_clapper_get_mpris:
 * @clapper: #GstClapper instance
 *
 * A Function to get the #GstClapperMpris instance.
 *
 * Returns: (transfer full): mpris instance.
 *
 * The caller should free it with g_object_unref()
 */
GstClapperMpris *
gst_clapper_get_mpris (GstClapper * self)
{
  GstClapperMpris *val;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  g_object_get (self, "mpris", &val, NULL);

  return val;
}

/**
 * gst_clapper_get_media_info:
 * @clapper: #GstClapper instance
 *
 * A Function to get the current media info #GstClapperMediaInfo instance.
 *
 * Returns: (transfer full): media info instance.
 *
 * The caller should free it with g_object_unref()
 */
GstClapperMediaInfo *
gst_clapper_get_media_info (GstClapper * self)
{
  GstClapperMediaInfo *info;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  if (!self->media_info)
    return NULL;

  g_mutex_lock (&self->lock);
  info = gst_clapper_media_info_copy (self->media_info);
  g_mutex_unlock (&self->lock);

  return info;
}

/**
 * gst_clapper_get_current_audio_track:
 * @clapper: #GstClapper instance
 *
 * A Function to get current audio #GstClapperAudioInfo instance.
 *
 * Returns: (transfer full): current audio track.
 *
 * The caller should free it with g_object_unref()
 */
GstClapperAudioInfo *
gst_clapper_get_current_audio_track (GstClapper * self)
{
  GstClapperAudioInfo *info;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  if (!clapper_get_has_flag (self, GST_PLAY_FLAG_AUDIO))
    return NULL;

  if (self->use_playbin3) {
    info = (GstClapperAudioInfo *)
        gst_clapper_stream_info_get_current_from_stream_id (self,
        self->audio_sid, GST_TYPE_CLAPPER_AUDIO_INFO);
  } else {
    info = (GstClapperAudioInfo *) gst_clapper_stream_info_get_current (self,
        "current-audio", GST_TYPE_CLAPPER_AUDIO_INFO);
  }

  return info;
}

/**
 * gst_clapper_get_current_video_track:
 * @clapper: #GstClapper instance
 *
 * A Function to get current video #GstClapperVideoInfo instance.
 *
 * Returns: (transfer full): current video track.
 *
 * The caller should free it with g_object_unref()
 */
GstClapperVideoInfo *
gst_clapper_get_current_video_track (GstClapper * self)
{
  GstClapperVideoInfo *info;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  if (!clapper_get_has_flag (self, GST_PLAY_FLAG_VIDEO))
    return NULL;

  if (self->use_playbin3) {
    info = (GstClapperVideoInfo *)
        gst_clapper_stream_info_get_current_from_stream_id (self,
        self->video_sid, GST_TYPE_CLAPPER_VIDEO_INFO);
  } else {
    info = (GstClapperVideoInfo *) gst_clapper_stream_info_get_current (self,
        "current-video", GST_TYPE_CLAPPER_VIDEO_INFO);
  }

  return info;
}

/**
 * gst_clapper_get_current_subtitle_track:
 * @clapper: #GstClapper instance
 *
 * A Function to get current subtitle #GstClapperSubtitleInfo instance.
 *
 * Returns: (transfer full): current subtitle track.
 *
 * The caller should free it with g_object_unref()
 */
GstClapperSubtitleInfo *
gst_clapper_get_current_subtitle_track (GstClapper * self)
{
  GstClapperSubtitleInfo *info;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  if (!clapper_get_has_flag (self, GST_PLAY_FLAG_SUBTITLE))
    return NULL;

  if (self->use_playbin3) {
    info = (GstClapperSubtitleInfo *)
        gst_clapper_stream_info_get_current_from_stream_id (self,
        self->subtitle_sid, GST_TYPE_CLAPPER_SUBTITLE_INFO);
  } else {
    info = (GstClapperSubtitleInfo *) gst_clapper_stream_info_get_current (self,
        "current-text", GST_TYPE_CLAPPER_SUBTITLE_INFO);
  }

  return info;
}

/* Must be called with lock */
static gboolean
gst_clapper_select_streams (GstClapper * self)
{
  GList *stream_list = NULL;
  gboolean ret = FALSE;

  if (self->audio_sid)
    stream_list = g_list_append (stream_list, g_strdup (self->audio_sid));
  if (self->video_sid)
    stream_list = g_list_append (stream_list, g_strdup (self->video_sid));
  if (self->subtitle_sid)
    stream_list = g_list_append (stream_list, g_strdup (self->subtitle_sid));

  g_mutex_unlock (&self->lock);
  if (stream_list) {
    ret = gst_element_send_event (self->playbin,
        gst_event_new_select_streams (stream_list));
    g_list_free_full (stream_list, g_free);
  } else {
    GST_ERROR_OBJECT (self, "No available streams for select-streams");
  }
  g_mutex_lock (&self->lock);

  return ret;
}

/**
 * gst_clapper_set_audio_track:
 * @clapper: #GstClapper instance
 * @stream_index: stream index
 *
 * Returns: %TRUE or %FALSE
 *
 * Sets the audio track @stream_idex.
 */
gboolean
gst_clapper_set_audio_track (GstClapper * self, gint stream_index)
{
  GstClapperStreamInfo *info;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_CLAPPER (self), FALSE);

  g_mutex_lock (&self->lock);
  info = gst_clapper_stream_info_find (self->media_info,
      GST_TYPE_CLAPPER_AUDIO_INFO, stream_index);
  g_mutex_unlock (&self->lock);
  if (!info) {
    GST_ERROR_OBJECT (self, "invalid audio stream index %d", stream_index);
    return FALSE;
  }

  if (self->use_playbin3) {
    g_mutex_lock (&self->lock);
    g_free (self->audio_sid);
    self->audio_sid = g_strdup (info->stream_id);
    ret = gst_clapper_select_streams (self);
    g_mutex_unlock (&self->lock);
  } else {
    g_object_set (G_OBJECT (self->playbin), "current-audio", stream_index,
        NULL);
  }

  GST_DEBUG_OBJECT (self, "set stream index '%d'", stream_index);
  return ret;
}

/**
 * gst_clapper_set_video_track:
 * @clapper: #GstClapper instance
 * @stream_index: stream index
 *
 * Returns: %TRUE or %FALSE
 *
 * Sets the video track @stream_index.
 */
gboolean
gst_clapper_set_video_track (GstClapper * self, gint stream_index)
{
  GstClapperStreamInfo *info;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_CLAPPER (self), FALSE);

  /* check if stream_index exist in our internal media_info list */
  g_mutex_lock (&self->lock);
  info = gst_clapper_stream_info_find (self->media_info,
      GST_TYPE_CLAPPER_VIDEO_INFO, stream_index);
  g_mutex_unlock (&self->lock);
  if (!info) {
    GST_ERROR_OBJECT (self, "invalid video stream index %d", stream_index);
    return FALSE;
  }

  if (self->use_playbin3) {
    g_mutex_lock (&self->lock);
    g_free (self->video_sid);
    self->video_sid = g_strdup (info->stream_id);
    ret = gst_clapper_select_streams (self);
    g_mutex_unlock (&self->lock);
  } else {
    g_object_set (G_OBJECT (self->playbin), "current-video", stream_index,
        NULL);
  }

  GST_DEBUG_OBJECT (self, "set stream index '%d'", stream_index);
  return ret;
}

/**
 * gst_clapper_set_subtitle_track:
 * @clapper: #GstClapper instance
 * @stream_index: stream index
 *
 * Returns: %TRUE or %FALSE
 *
 * Sets the subtitle stack @stream_index.
 */
gboolean
gst_clapper_set_subtitle_track (GstClapper * self, gint stream_index)
{
  GstClapperStreamInfo *info;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_CLAPPER (self), FALSE);

  g_mutex_lock (&self->lock);
  info = gst_clapper_stream_info_find (self->media_info,
      GST_TYPE_CLAPPER_SUBTITLE_INFO, stream_index);
  g_mutex_unlock (&self->lock);
  if (!info) {
    GST_ERROR_OBJECT (self, "invalid subtitle stream index %d", stream_index);
    return FALSE;
  }

  if (self->use_playbin3) {
    g_mutex_lock (&self->lock);
    g_free (self->subtitle_sid);
    self->subtitle_sid = g_strdup (info->stream_id);
    ret = gst_clapper_select_streams (self);
    g_mutex_unlock (&self->lock);
  } else {
    g_object_set (G_OBJECT (self->playbin), "current-text", stream_index, NULL);
  }

  GST_DEBUG_OBJECT (self, "set stream index '%d'", stream_index);
  return ret;
}

/**
 * gst_clapper_set_audio_track_enabled:
 * @clapper: #GstClapper instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current audio track.
 */
void
gst_clapper_set_audio_track_enabled (GstClapper * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (enabled)
    clapper_set_flag (self, GST_PLAY_FLAG_AUDIO);
  else
    clapper_clear_flag (self, GST_PLAY_FLAG_AUDIO);

  GST_DEBUG_OBJECT (self, "track is '%s'", enabled ? "Enabled" : "Disabled");
}

/**
 * gst_clapper_set_video_track_enabled:
 * @clapper: #GstClapper instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current video track.
 */
void
gst_clapper_set_video_track_enabled (GstClapper * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (enabled)
    clapper_set_flag (self, GST_PLAY_FLAG_VIDEO);
  else
    clapper_clear_flag (self, GST_PLAY_FLAG_VIDEO);

  GST_DEBUG_OBJECT (self, "track is '%s'", enabled ? "Enabled" : "Disabled");
}

/**
 * gst_clapper_set_subtitle_track_enabled:
 * @clapper: #GstClapper instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current subtitle track.
 */
void
gst_clapper_set_subtitle_track_enabled (GstClapper * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (enabled)
    clapper_set_flag (self, GST_PLAY_FLAG_SUBTITLE);
  else
    clapper_clear_flag (self, GST_PLAY_FLAG_SUBTITLE);

  GST_DEBUG_OBJECT (self, "track is '%s'", enabled ? "Enabled" : "Disabled");
}

/**
 * gst_clapper_set_visualization:
 * @clapper: #GstClapper instance
 * @name: visualization element obtained from
 * #gst_clapper_visualizations_get()
 *
 * Returns: %TRUE if the visualizations was set correctly. Otherwise,
 * %FALSE.
 */
gboolean
gst_clapper_set_visualization (GstClapper * self, const gchar * name)
{
  g_return_val_if_fail (GST_IS_CLAPPER (self), FALSE);

  g_mutex_lock (&self->lock);
  if (self->current_vis_element) {
    gst_object_unref (self->current_vis_element);
    self->current_vis_element = NULL;
  }

  if (name) {
    self->current_vis_element = gst_element_factory_make (name, NULL);
    if (!self->current_vis_element)
      goto error_no_element;
    gst_object_ref_sink (self->current_vis_element);
  }
  g_object_set (self->playbin, "vis-plugin", self->current_vis_element, NULL);

  g_mutex_unlock (&self->lock);
  GST_DEBUG_OBJECT (self, "set vis-plugin to '%s'", name);

  return TRUE;

error_no_element:
  g_mutex_unlock (&self->lock);
  GST_WARNING_OBJECT (self, "could not find visualization '%s'", name);
  return FALSE;
}

/**
 * gst_clapper_get_current_visualization:
 * @clapper: #GstClapper instance
 *
 * Returns: (transfer full): Name of the currently enabled visualization.
 *   g_free() after usage.
 */
gchar *
gst_clapper_get_current_visualization (GstClapper * self)
{
  gchar *name = NULL;
  GstElement *vis_plugin = NULL;

  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  if (!clapper_get_has_flag (self, GST_PLAY_FLAG_VIS))
    return NULL;

  g_object_get (self->playbin, "vis-plugin", &vis_plugin, NULL);

  if (vis_plugin) {
    GstElementFactory *factory = gst_element_get_factory (vis_plugin);
    if (factory)
      name = g_strdup (gst_plugin_feature_get_name (factory));
    gst_object_unref (vis_plugin);
  }

  GST_DEBUG_OBJECT (self, "vis-plugin '%s' %p", name, vis_plugin);

  return name;
}

/**
 * gst_clapper_set_visualization_enabled:
 * @clapper: #GstClapper instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the visualization.
 */
void
gst_clapper_set_visualization_enabled (GstClapper * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  if (enabled)
    clapper_set_flag (self, GST_PLAY_FLAG_VIS);
  else
    clapper_clear_flag (self, GST_PLAY_FLAG_VIS);

  GST_DEBUG_OBJECT (self, "visualization is '%s'",
      enabled ? "Enabled" : "Disabled");
}

struct CBChannelMap
{
  const gchar *label;           /* channel label name */
  const gchar *name;            /* get_name () */
};

static const struct CBChannelMap cb_channel_map[] = {
  /* GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS */ {"BRIGHTNESS", "brightness"},
  /* GST_CLAPPER_COLOR_BALANCE_CONTRAST   */ {"CONTRAST", "contrast"},
  /* GST_CLAPPER_COLOR_BALANCE_SATURATION */ {"SATURATION", "saturation"},
  /* GST_CLAPPER_COLOR_BALANCE_HUE        */ {"HUE", "hue"},
};

static GstColorBalanceChannel *
gst_clapper_color_balance_find_channel (GstClapper * self,
    GstClapperColorBalanceType type)
{
  GstColorBalanceChannel *channel;
  const GList *l, *channels;

  if (type < GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS ||
      type > GST_CLAPPER_COLOR_BALANCE_HUE)
    return NULL;

  channels =
      gst_color_balance_list_channels (GST_COLOR_BALANCE (self->playbin));
  for (l = channels; l; l = l->next) {
    channel = l->data;
    if (g_strrstr (channel->label, cb_channel_map[type].label))
      return channel;
  }

  return NULL;
}

/**
 * gst_clapper_has_color_balance:
 * @clapper:#GstClapper instance
 *
 * Checks whether the @clapper has color balance support available.
 *
 * Returns: %TRUE if @clapper has color balance support. Otherwise,
 *   %FALSE.
 */
gboolean
gst_clapper_has_color_balance (GstClapper * self)
{
  const GList *channels;

  g_return_val_if_fail (GST_IS_CLAPPER (self), FALSE);

  if (!GST_IS_COLOR_BALANCE (self->playbin))
    return FALSE;

  channels =
      gst_color_balance_list_channels (GST_COLOR_BALANCE (self->playbin));
  return (channels != NULL);
}

/**
 * gst_clapper_set_color_balance:
 * @clapper: #GstClapper instance
 * @type: #GstClapperColorBalanceType
 * @value: The new value for the @type, ranged [0,1]
 *
 * Sets the current value of the indicated channel @type to the passed
 * value.
 */
void
gst_clapper_set_color_balance (GstClapper * self, GstClapperColorBalanceType type,
    gdouble value)
{
  GstColorBalanceChannel *channel;
  gdouble new_val;

  g_return_if_fail (GST_IS_CLAPPER (self));
  g_return_if_fail (value >= 0.0 && value <= 1.0);

  if (!GST_IS_COLOR_BALANCE (self->playbin))
    return;

  channel = gst_clapper_color_balance_find_channel (self, type);
  if (!channel)
    return;

  value = CLAMP (value, 0.0, 1.0);

  /* Convert to channel range */
  new_val = channel->min_value + value * ((gdouble) channel->max_value -
      (gdouble) channel->min_value);

  gst_color_balance_set_value (GST_COLOR_BALANCE (self->playbin), channel,
      new_val);
}

/**
 * gst_clapper_get_color_balance:
 * @clapper: #GstClapper instance
 * @type: #GstClapperColorBalanceType
 *
 * Retrieve the current value of the indicated @type.
 *
 * Returns: The current value of @type, between [0,1]. In case of
 *   error -1 is returned.
 */
gdouble
gst_clapper_get_color_balance (GstClapper * self, GstClapperColorBalanceType type)
{
  GstColorBalanceChannel *channel;
  gint value;

  g_return_val_if_fail (GST_IS_CLAPPER (self), -1);

  if (!GST_IS_COLOR_BALANCE (self->playbin))
    return -1;

  channel = gst_clapper_color_balance_find_channel (self, type);
  if (!channel)
    return -1;

  value = gst_color_balance_get_value (GST_COLOR_BALANCE (self->playbin),
      channel);

  return ((gdouble) value -
      (gdouble) channel->min_value) / ((gdouble) channel->max_value -
      (gdouble) channel->min_value);
}

/**
 * gst_clapper_get_multiview_mode:
 * @clapper: #GstClapper instance
 *
 * Retrieve the current value of the indicated @type.
 *
 * Returns: The current value of @type, Default: -1 "none"
 */
GstVideoMultiviewFramePacking
gst_clapper_get_multiview_mode (GstClapper * self)
{
  GstVideoMultiviewFramePacking val = GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE;

  g_return_val_if_fail (GST_IS_CLAPPER (self),
      GST_VIDEO_MULTIVIEW_FRAME_PACKING_NONE);

  g_object_get (self, "video-multiview-mode", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_multiview_mode:
 * @clapper: #GstClapper instance
 * @mode: The new value for the @type
 *
 * Sets the current value of the indicated mode @type to the passed
 * value.
 */
void
gst_clapper_set_multiview_mode (GstClapper * self,
    GstVideoMultiviewFramePacking mode)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "video-multiview-mode", mode, NULL);
}

/**
 * gst_clapper_get_multiview_flags:
 * @clapper: #GstClapper instance
 *
 * Retrieve the current value of the indicated @type.
 *
 * Returns: The current value of @type, Default: 0x00000000 "none
 */
GstVideoMultiviewFlags
gst_clapper_get_multiview_flags (GstClapper * self)
{
  GstVideoMultiviewFlags val = GST_VIDEO_MULTIVIEW_FLAGS_NONE;

  g_return_val_if_fail (GST_IS_CLAPPER (self), val);

  g_object_get (self, "video-multiview-flags", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_multiview_flags:
 * @clapper: #GstClapper instance
 * @flags: The new value for the @type
 *
 * Sets the current value of the indicated mode @type to the passed
 * value.
 */
void
gst_clapper_set_multiview_flags (GstClapper * self, GstVideoMultiviewFlags flags)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "video-multiview-flags", flags, NULL);
}

/**
 * gst_clapper_get_audio_video_offset:
 * @clapper: #GstClapper instance
 *
 * Retrieve the current value of audio-video-offset property
 *
 * Returns: The current value of audio-video-offset in nanoseconds
 */
gint64
gst_clapper_get_audio_video_offset (GstClapper * self)
{
  gint64 val = 0;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_AUDIO_VIDEO_OFFSET);

  g_object_get (self, "audio-video-offset", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_audio_video_offset:
 * @clapper: #GstClapper instance
 * @offset: #gint64 in nanoseconds
 *
 * Sets audio-video-offset property by value of @offset
 */
void
gst_clapper_set_audio_video_offset (GstClapper * self, gint64 offset)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "audio-video-offset", offset, NULL);
}

/**
 * gst_clapper_get_subtitle_video_offset:
 * @clapper: #GstClapper instance
 *
 * Retrieve the current value of subtitle-video-offset property
 *
 * Returns: The current value of subtitle-video-offset in nanoseconds
 */
gint64
gst_clapper_get_subtitle_video_offset (GstClapper * self)
{
  gint64 val = 0;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_SUBTITLE_VIDEO_OFFSET);

  g_object_get (self, "subtitle-video-offset", &val, NULL);

  return val;
}

/**
 * gst_clapper_set_subtitle_video_offset:
 * @clapper: #GstClapper instance
 * @offset: #gint64 in nanoseconds
 *
 * Sets subtitle-video-offset property by value of @offset
 */
void
gst_clapper_set_subtitle_video_offset (GstClapper * self, gint64 offset)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "subtitle-video-offset", offset, NULL);
}


#define C_ENUM(v) ((gint) v)
#define C_FLAGS(v) ((guint) v)

GType
gst_clapper_color_balance_type_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_CLAPPER_COLOR_BALANCE_HUE), "GST_CLAPPER_COLOR_BALANCE_HUE",
        "hue"},
    {C_ENUM (GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS),
        "GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS", "brightness"},
    {C_ENUM (GST_CLAPPER_COLOR_BALANCE_SATURATION),
        "GST_CLAPPER_COLOR_BALANCE_SATURATION", "saturation"},
    {C_ENUM (GST_CLAPPER_COLOR_BALANCE_CONTRAST),
        "GST_CLAPPER_COLOR_BALANCE_CONTRAST", "contrast"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstClapperColorBalanceType", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_clapper_color_balance_type_get_name:
 * @type: a #GstClapperColorBalanceType
 *
 * Gets a string representing the given color balance type.
 *
 * Returns: (transfer none): a string with the name of the color
 * balance type.
 */
const gchar *
gst_clapper_color_balance_type_get_name (GstClapperColorBalanceType type)
{
  g_return_val_if_fail (type >= GST_CLAPPER_COLOR_BALANCE_BRIGHTNESS &&
      type <= GST_CLAPPER_COLOR_BALANCE_HUE, NULL);

  return cb_channel_map[type].name;
}

GType
gst_clapper_state_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_CLAPPER_STATE_STOPPED), "GST_CLAPPER_STATE_STOPPED", "stopped"},
    {C_ENUM (GST_CLAPPER_STATE_BUFFERING), "GST_CLAPPER_STATE_BUFFERING",
        "buffering"},
    {C_ENUM (GST_CLAPPER_STATE_PAUSED), "GST_CLAPPER_STATE_PAUSED", "paused"},
    {C_ENUM (GST_CLAPPER_STATE_PLAYING), "GST_CLAPPER_STATE_PLAYING", "playing"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstClapperState", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_clapper_state_get_name:
 * @state: a #GstClapperState
 *
 * Gets a string representing the given state.
 *
 * Returns: (transfer none): a string with the name of the state.
 */
const gchar *
gst_clapper_state_get_name (GstClapperState state)
{
  switch (state) {
    case GST_CLAPPER_STATE_STOPPED:
      return "stopped";
    case GST_CLAPPER_STATE_BUFFERING:
      return "buffering";
    case GST_CLAPPER_STATE_PAUSED:
      return "paused";
    case GST_CLAPPER_STATE_PLAYING:
      return "playing";
  }

  g_assert_not_reached ();
  return NULL;
}

GType
gst_clapper_error_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_CLAPPER_ERROR_FAILED), "GST_CLAPPER_ERROR_FAILED", "failed"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstClapperError", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_clapper_error_get_name:
 * @error: a #GstClapperError
 *
 * Gets a string representing the given error.
 *
 * Returns: (transfer none): a string with the given error.
 */
const gchar *
gst_clapper_error_get_name (GstClapperError error)
{
  switch (error) {
    case GST_CLAPPER_ERROR_FAILED:
      return "failed";
  }

  g_assert_not_reached ();
  return NULL;
}

GType
gst_clapper_seek_mode_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_CLAPPER_SEEK_MODE_DEFAULT), "GST_CLAPPER_SEEK_MODE_DEFAULT",
        "default"},
    {C_ENUM (GST_CLAPPER_SEEK_MODE_ACCURATE), "GST_CLAPPER_SEEK_MODE_ACCURATE",
        "accurate"},
    {C_ENUM (GST_CLAPPER_SEEK_MODE_FAST), "GST_CLAPPER_SEEK_MODE_FAST", "fast"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstClapperSeekMode", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

/**
 * gst_clapper_get_seek_mode:
 * @clapper: #GstClapper instance
 *
 * Returns: The currently used seek mode, Default: 0 "default"
 */
GstClapperSeekMode
gst_clapper_get_seek_mode (GstClapper * self)
{
  GstClapperSeekMode mode;

  g_return_val_if_fail (GST_IS_CLAPPER (self), DEFAULT_SEEK_MODE);

  g_object_get (self, "seek-mode", &mode, NULL);

  return mode;
}

/**
 * gst_clapper_set_seek_mode:
 * @clapper: #GstClapper instance
 * @mode: #GstClapperSeekMode
 *
 * Changes currently used clapper seek mode to the one of @mode
 */
void
gst_clapper_set_seek_mode (GstClapper * self, GstClapperSeekMode mode)
{
  g_return_if_fail (GST_IS_CLAPPER (self));

  g_object_set (self, "seek-mode", mode, NULL);
}

/**
 * gst_clapper_get_video_snapshot:
 * @clapper: #GstClapper instance
 * @format: output format of the video snapshot
 * @config: (allow-none): Additional configuration
 *
 * Get a snapshot of the currently selected video stream, if any. The format can be
 * selected with @format and optional configuration is possible with @config
 * Currently supported settings are:
 * - width, height of type G_TYPE_INT
 * - pixel-aspect-ratio of type GST_TYPE_FRACTION
 *  Except for GST_CLAPPER_THUMBNAIL_RAW_NATIVE format, if no config is set, pixel-aspect-ratio would be 1/1
 *
 * Returns: (transfer full):  Current video snapshot sample or %NULL on failure
 */
GstSample *
gst_clapper_get_video_snapshot (GstClapper * self,
    GstClapperSnapshotFormat format, const GstStructure * config)
{
  gint video_tracks = 0;
  GstSample *sample = NULL;
  GstCaps *caps = NULL;
  gint width = -1;
  gint height = -1;
  gint par_n = 1;
  gint par_d = 1;
  g_return_val_if_fail (GST_IS_CLAPPER (self), NULL);

  g_object_get (self->playbin, "n-video", &video_tracks, NULL);
  if (video_tracks == 0) {
    GST_DEBUG_OBJECT (self, "total video track num is 0");
    return NULL;
  }

  switch (format) {
    case GST_CLAPPER_THUMBNAIL_RAW_xRGB:
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "xRGB", NULL);
      break;
    case GST_CLAPPER_THUMBNAIL_RAW_BGRx:
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "BGRx", NULL);
      break;
    case GST_CLAPPER_THUMBNAIL_JPG:
      caps = gst_caps_new_empty_simple ("image/jpeg");
      break;
    case GST_CLAPPER_THUMBNAIL_PNG:
      caps = gst_caps_new_empty_simple ("image/png");
      break;
    case GST_CLAPPER_THUMBNAIL_RAW_NATIVE:
    default:
      caps = gst_caps_new_empty_simple ("video/x-raw");
      break;
  }

  if (NULL != config) {
    if (!gst_structure_get_int (config, "width", &width))
      width = -1;
    if (!gst_structure_get_int (config, "height", &height))
      height = -1;
    if (!gst_structure_get_fraction (config, "pixel-aspect-ratio", &par_n,
            &par_d)) {
      if (format != GST_CLAPPER_THUMBNAIL_RAW_NATIVE) {
        par_n = 1;
        par_d = 1;
      } else {
        par_n = 0;
        par_d = 0;
      }
    }
  }

  if (width > 0 && height > 0) {
    gst_caps_set_simple (caps, "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, NULL);
  }

  if (format != GST_CLAPPER_THUMBNAIL_RAW_NATIVE) {
    gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
        par_n, par_d, NULL);
  } else if (NULL != config && par_n != 0 && par_d != 0) {
    gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
        par_n, par_d, NULL);
  }

  g_signal_emit_by_name (self->playbin, "convert-sample", caps, &sample);
  gst_caps_unref (caps);
  if (!sample) {
    GST_WARNING_OBJECT (self, "Failed to retrieve or convert video frame");
    return NULL;
  }

  return sample;
}
