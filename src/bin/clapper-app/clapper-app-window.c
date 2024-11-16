/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <math.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <adwaita.h>
#include <clapper/clapper.h>
#include <clapper-gtk/clapper-gtk.h>

#include "clapper-app-window.h"
#include "clapper-app-file-dialog.h"
#include "clapper-app-utils.h"

#define MIN_WINDOW_WIDTH 352
#define MIN_WINDOW_HEIGHT 198

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 576

#define N_PROGRESSION_MODES 5

#define CLAPPER_APP_SEEK_UNIT_SECOND 0
#define CLAPPER_APP_SEEK_UNIT_MINUTE 1
#define CLAPPER_APP_SEEK_UNIT_PERCENTAGE 2

#define PERCENTAGE_ROUND(a) (round ((gdouble) a / 0.01) * 0.01)
#define AXIS_WINS_OVER(a,b) ((a > 0 && a - 0.3 > b) || (a < 0 && a + 0.3 < b))

#define MIN_STEP_DELAY 12000

#define GST_CAT_DEFAULT clapper_app_window_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppWindow
{
  GtkApplicationWindow parent;

  GtkWidget *video;
  ClapperGtkBillboard *billboard;
  ClapperGtkSimpleControls *simple_controls;

  GtkDropTarget *drop_target;
  GtkCssProvider *provider;

  ClapperMediaItem *current_item;

  GSettings *settings;

  guint seek_timeout;
  guint resize_tick_id;

  gboolean key_held;
  gboolean scrolling;
  gboolean seeking;

  gboolean was_playing;
  gdouble pending_position;
  gdouble current_duration;

  gdouble last_volume;
};

#define parent_class clapper_app_window_parent_class
G_DEFINE_TYPE (ClapperAppWindow, clapper_app_window, GTK_TYPE_APPLICATION_WINDOW)

typedef struct
{
  gint dest_width, dest_height;
  gint64 last_tick;
} ClapperAppWindowResizeData;

#if CLAPPER_HAVE_MPRIS
static guint16 instance_count = 0;
#endif

static inline GQuark
clapper_app_window_extra_options_get_quark (void)
{
  return g_quark_from_static_string ("clapper-app-window-extra-options-quark");
}

static void
clapper_app_window_extra_options_free (ClapperAppWindowExtraOptions *extra_opts)
{
  GST_TRACE ("Freeing window extra options: %p", extra_opts);

  g_free (extra_opts->video_filter);
  g_free (extra_opts->audio_filter);

  g_free (extra_opts->video_sink);
  g_free (extra_opts->audio_sink);

  g_free (extra_opts);
}

static void
_media_item_title_changed_cb (ClapperMediaItem *item,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindow *self)
{
  gchar *title;

  if ((title = clapper_media_item_get_title (item))) {
    gtk_window_set_title (GTK_WINDOW (self), title);
    g_free (title);
  } else {
    gtk_window_set_title (GTK_WINDOW (self), CLAPPER_APP_NAME);
  }
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindow *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  /* Disconnect signal from old item */
  if (self->current_item) {
    g_signal_handlers_disconnect_by_func (self->current_item,
        _media_item_title_changed_cb, self);
  }

  gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item));
  GST_DEBUG_OBJECT (self, "Current item changed to: %" GST_PTR_FORMAT, self->current_item);

  /* Reconnect signal to new item */
  if (self->current_item) {
    g_signal_connect (self->current_item, "notify::title",
        G_CALLBACK (_media_item_title_changed_cb), self);
    _media_item_title_changed_cb (self->current_item, NULL, self);
  } else {
    gtk_window_set_title (GTK_WINDOW (self), CLAPPER_APP_NAME);
  }

  gst_clear_object (&current_item);
}

static void
_player_adaptive_bandwidth_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, gpointer *user_data G_GNUC_UNUSED)
{
  /* Do not take whole bandwidth */
  clapper_player_set_adaptive_start_bitrate (player,
      clapper_player_get_adaptive_bandwidth (player) * 0.8);
}

static gboolean
_get_seek_method_mapping (GValue *value,
    GVariant *variant, gpointer user_data G_GNUC_UNUSED)
{
  ClapperPlayerSeekMethod seek_method;

  seek_method = (ClapperPlayerSeekMethod) g_variant_get_int32 (variant);
  g_value_set_enum (value, seek_method);

  return TRUE;
}

static GtkWidget *
_pick_pointer_widget (ClapperAppWindow *self)
{
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self));
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (self));
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GtkWidget *widget = NULL;

  if (G_LIKELY (seat != NULL)) {
    GdkDevice *device = gdk_seat_get_pointer (seat);
    gdouble px = 0, py = 0, native_x = 0, native_y = 0;

    if (G_LIKELY (device != NULL))
      gdk_surface_get_device_position (surface, device, &px, &py, NULL);

    gtk_native_get_surface_transform (GTK_NATIVE (self), &native_x, &native_y);

    widget = gtk_widget_pick (GTK_WIDGET (self),
        px - native_x, py - native_y, GTK_PICK_DEFAULT);
  }

  return widget;
}

static void
_player_volume_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindow *self)
{
  gdouble volume = PERCENTAGE_ROUND (clapper_player_get_volume (player));

  /* Only notify when volume changes at least 1%. Remembering last volume
   * also prevents us from showing volume when it is restored on startup. */
  if (volume != self->last_volume) {
    clapper_gtk_billboard_announce_volume (self->billboard);
    self->last_volume = volume;
  }
}

static void
_player_speed_changed_cb (ClapperPlayer *player G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindow *self)
{
  clapper_gtk_billboard_announce_speed (self->billboard);
}

static void
video_toggle_fullscreen_cb (ClapperGtkVideo *video, ClapperAppWindow *self)
{
  GtkWindow *window = GTK_WINDOW (self);

  g_object_set (window, "fullscreened", !gtk_window_is_fullscreen (window), NULL);
}

static void
video_map_cb (GtkWidget *widget, ClapperAppWindow *self)
{
  ClapperPlayer *player;
  gdouble speed;

  GST_TRACE_OBJECT (self, "Video map");

  player = clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (self->video));

  g_signal_connect (player, "notify::volume",
      G_CALLBACK (_player_volume_changed_cb), self);
  g_signal_connect (player, "notify::speed",
      G_CALLBACK (_player_speed_changed_cb), self);

  speed = clapper_player_get_speed (player);

  /* If we are starting with non-1x speed, notify user about it */
  if (!G_APPROX_VALUE (speed, 1.0, FLT_EPSILON))
    clapper_gtk_billboard_announce_speed (self->billboard);
}

static void
video_unmap_cb (GtkWidget *widget, ClapperAppWindow *self)
{
  ClapperPlayer *player;

  GST_TRACE_OBJECT (self, "Video unmap");

  player = clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (self->video));

  g_signal_handlers_disconnect_by_func (player, _player_volume_changed_cb, self);
  g_signal_handlers_disconnect_by_func (player, _player_speed_changed_cb, self);
}

static void
_open_subtitles_cb (ClapperGtkExtraMenuButton *button G_GNUC_UNUSED,
    ClapperMediaItem *item, ClapperAppWindow *self)
{
  GtkApplication *gtk_app = gtk_window_get_application (GTK_WINDOW (self));
  clapper_app_file_dialog_open_subtitles (gtk_app, item);
}

static void
click_pressed_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperAppWindow *self)
{
  GdkCursor *cursor;
  const gchar *cursor_name = NULL;

  if (gtk_gesture_single_get_current_button (
      GTK_GESTURE_SINGLE (click)) != GDK_BUTTON_SECONDARY)
    return;

  GST_LOG_OBJECT (self, "Right click pressed");

  if ((cursor = gtk_widget_get_cursor (self->video)))
    cursor_name = gdk_cursor_get_name (cursor);

  /* Restore cursor if faded on video */
  if (g_strcmp0 (cursor_name, "none") == 0) {
    GdkCursor *new_cursor = gdk_cursor_new_from_name ("default", NULL);

    gtk_widget_set_cursor (self->video, new_cursor);
    g_object_unref (new_cursor);
  }
}

static gboolean
_resize_tick (GtkWidget *widget, GdkFrameClock *frame_clock,
    ClapperAppWindowResizeData *resize_data)
{
  gint64 now = gdk_frame_clock_get_frame_time (frame_clock);

  if (now - resize_data->last_tick >= MIN_STEP_DELAY) {
    ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (widget);
    gint win_width, win_height;

    GST_LOG_OBJECT (self, "Resize step, last: %" G_GINT64_FORMAT
        ", now: %" G_GINT64_FORMAT, resize_data->last_tick, now);

    gtk_window_get_default_size (GTK_WINDOW (self), &win_width, &win_height);

    if (win_width != resize_data->dest_width) {
      gint width_diff = ABS (win_width - resize_data->dest_width);
      gint step_size = (width_diff > 180) ? 120 : MAX (width_diff / 4, 1);

      win_width += (win_width > resize_data->dest_width) ? -step_size : step_size;
    }
    if (win_height != resize_data->dest_height) {
      gint height_diff = ABS (win_height - resize_data->dest_height);
      gint step_size = (height_diff > 180) ? 120 : MAX (height_diff / 4, 1);

      win_height += (win_height > resize_data->dest_height) ? -step_size : step_size;
    }

    gtk_window_set_default_size (GTK_WINDOW (self), win_width, win_height);

    if (win_width == resize_data->dest_width
        && win_height == resize_data->dest_height) {
      GST_DEBUG_OBJECT (self, "Window resize finish");
      self->resize_tick_id = 0;

      return G_SOURCE_REMOVE;
    }

    resize_data->last_tick = now;
  }

  return G_SOURCE_CONTINUE;
}

static void
_calculate_win_resize (gint win_w, gint win_h,
    gint vid_w, gint vid_h, gint *dest_w, gint *dest_h)
{
  gdouble win_aspect = (gdouble) win_w / win_h;
  gdouble vid_aspect = (gdouble) vid_w / vid_h;

  if (win_aspect < vid_aspect) {
    while (!G_APPROX_VALUE (fmod (win_w, vid_aspect), 0, FLT_EPSILON))
      win_w++;

    win_h = round ((gdouble) win_w / vid_aspect);

    if (win_h < MIN_WINDOW_HEIGHT) {
      _calculate_win_resize (G_MAXINT, MIN_WINDOW_HEIGHT, vid_w, vid_h, dest_w, dest_h);
      return;
    }
  } else {
    while (!G_APPROX_VALUE (fmod (win_h * vid_aspect, 1.0), 0, FLT_EPSILON))
      win_h++;

    win_w = round ((gdouble) win_h * vid_aspect);

    if (win_w < MIN_WINDOW_WIDTH) {
      _calculate_win_resize (MIN_WINDOW_WIDTH, G_MAXINT, vid_w, vid_h, dest_w, dest_h);
      return;
    }
  }

  *dest_w = win_w;
  *dest_h = win_h;
}

static void
_resize_window (ClapperAppWindow *self)
{
  ClapperPlayer *player;
  ClapperStreamList *vstreams;
  ClapperVideoStream *vstream;
  GdkToplevelState toplevel_state, disallowed;

  if (self->resize_tick_id != 0)
    return;

  toplevel_state = gdk_toplevel_get_state (GDK_TOPLEVEL (
      gtk_native_get_surface (GTK_NATIVE (self))));
  disallowed = (GDK_TOPLEVEL_STATE_MINIMIZED
      | GDK_TOPLEVEL_STATE_MAXIMIZED
      | GDK_TOPLEVEL_STATE_FULLSCREEN
      | GDK_TOPLEVEL_STATE_TILED);

  if ((toplevel_state & disallowed) > 0) {
    GST_DEBUG_OBJECT (self, "Cannot resize window in disallowed state");
    return;
  }

  player = clapper_app_window_get_player (self);
  vstreams = clapper_player_get_video_streams (player);
  vstream = CLAPPER_VIDEO_STREAM_CAST (
      clapper_stream_list_get_current_stream (vstreams));

  if (vstream) {
    gint video_width = clapper_video_stream_get_width (vstream);
    gint video_height = clapper_video_stream_get_height (vstream);

    if (G_LIKELY (video_width > 0 && video_height > 0)) {
      gint win_width, win_height, dest_width, dest_height;

      gtk_window_get_default_size (GTK_WINDOW (self), &win_width, &win_height);

      _calculate_win_resize (win_width, win_height,
          video_width, video_height, &dest_width, &dest_height);

      /* Only begin resize when not already at perfect size */
      if (dest_width != win_width || dest_height != win_height) {
        ClapperAppWindowResizeData *resize_data;

        resize_data = g_new0 (ClapperAppWindowResizeData, 1);
        resize_data->dest_width = dest_width;
        resize_data->dest_height = dest_height;

        GST_DEBUG_OBJECT (self, "Window resize start, dest: %ix%i",
            resize_data->dest_width, resize_data->dest_height);

        self->resize_tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
            (GtkTickCallback) _resize_tick, resize_data, g_free);
      }
    }

    gst_object_unref (vstream);
  }
}

static void
_handle_middle_click (ClapperAppWindow *self, GtkGestureClick *click)
{
  _resize_window (self);
  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
_handle_right_click (ClapperAppWindow *self, GtkGestureClick *click)
{
  GdkSurface *surface;
  GdkEventSequence *sequence;
  GdkEvent *event;

  GST_LOG_OBJECT (self, "Right click released");

  surface = gtk_native_get_surface (GTK_NATIVE (self));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (click));
  event = gtk_gesture_get_last_event (GTK_GESTURE (click), sequence);

  if (G_UNLIKELY (event == NULL))
    return;

  if (!gdk_toplevel_show_window_menu (GDK_TOPLEVEL (surface), event))
    GST_FIXME_OBJECT (self, "Implement fallback context menu");

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
click_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperAppWindow *self)
{
  switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click))) {
    case GDK_BUTTON_MIDDLE:
      _handle_middle_click (self, click);
      break;
    case GDK_BUTTON_SECONDARY:
      _handle_right_click (self, click);
      break;
    default:
      break;
  }
}

static void
drag_begin_cb (GtkGestureDrag *drag,
    gdouble start_x, gdouble start_y, ClapperAppWindow *self)
{
  GtkWidget *widget, *pickup;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag));
  pickup = gtk_widget_pick (widget, start_x, start_y, GTK_PICK_DEFAULT);

  /* We do not want to cause drag on list view as it has
   * a GtkDragSource controller which acts on delay */
  if (GTK_IS_LIST_VIEW (pickup) || gtk_widget_get_ancestor (pickup, GTK_TYPE_LIST_VIEW)) {
    gtk_gesture_set_state (GTK_GESTURE (drag), GTK_EVENT_SEQUENCE_DENIED);
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (drag));

    GST_DEBUG_OBJECT (self, "Window drag denied");
  }
}

static void
drag_update_cb (GtkGestureDrag *drag,
    gdouble offset_x, gdouble offset_y, ClapperAppWindow *self)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (self));
  gint drag_threshold = 8; // initially set to default

  g_object_get (settings, "gtk-dnd-drag-threshold", &drag_threshold, NULL);

  if (ABS (offset_x) > drag_threshold || ABS (offset_y) > drag_threshold) {
    GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self));
    gdouble start_x = 0, start_y = 0, native_x = 0, native_y = 0;

    gtk_gesture_set_state (GTK_GESTURE (drag), GTK_EVENT_SEQUENCE_CLAIMED);
    gtk_gesture_drag_get_start_point (drag, &start_x, &start_y);

    gtk_native_get_surface_transform (GTK_NATIVE (self), &native_x, &native_y);

    gdk_toplevel_begin_move (GDK_TOPLEVEL (surface),
        gtk_gesture_get_device (GTK_GESTURE (drag)),
        GDK_BUTTON_PRIMARY,
        start_x + native_x,
        start_y + native_y,
        gtk_event_controller_get_current_event_time (GTK_EVENT_CONTROLLER (drag)));

    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (drag));
  }
}

static inline void
_alter_volume (ClapperAppWindow *self, gdouble dy)
{
  ClapperPlayer *player = clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (self->video));
  gdouble volume = clapper_player_get_volume (player);

  /* We do not want for volume to change too suddenly */
  if (dy > 2.0)
    dy = 2.0;
  else if (dy < -2.0)
    dy = -2.0;

  volume -= dy * 0.02;

  /* Prevent going out of range and make it easier to set exactly 100% */
  if (volume > 2.0)
    volume = 2.0;
  else if (volume < 0.0)
    volume = 0.0;

  clapper_player_set_volume (player, PERCENTAGE_ROUND (volume));
}

static inline void
_alter_speed (ClapperAppWindow *self, gdouble dx)
{
  ClapperPlayer *player = clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (self->video));
  gdouble speed = clapper_player_get_speed (player);

  speed -= dx * 0.02;

  /* Prevent going out of range and make it easier to set exactly 1.0x */
  if (speed > 2.0)
    speed = 2.0;
  else if (speed < 0.05)
    speed = 0.05;

  clapper_player_set_speed (player, PERCENTAGE_ROUND (speed));
}

static gboolean
_begin_seek_operation (ClapperAppWindow *self)
{
  ClapperPlayer *player;
  ClapperQueue *queue;
  ClapperMediaItem *current_item;

  if (self->seeking)
    return FALSE;

  player = clapper_gtk_video_get_player (
      CLAPPER_GTK_VIDEO_CAST (self->video));
  queue = clapper_player_get_queue (player);
  current_item = clapper_queue_get_current_item (queue);

  self->current_duration = (current_item != NULL)
      ? clapper_media_item_get_duration (current_item)
      : 0;

  gst_clear_object (&current_item);

  /* Live content or not a video */
  if (self->current_duration == 0)
    return FALSE;

  if ((self->was_playing = (
      clapper_player_get_state (player) == CLAPPER_PLAYER_STATE_PLAYING)))
    clapper_player_pause (player);

  self->pending_position = clapper_player_get_position (player);
  self->seeking = TRUE;

  return TRUE;
}

static void
_end_seek_operation (ClapperAppWindow *self)
{
  if (self->seeking && self->current_duration > 0) {
    ClapperPlayer *player = clapper_gtk_video_get_player (
        CLAPPER_GTK_VIDEO_CAST (self->video));

    clapper_player_seek_custom (player, self->pending_position,
        g_settings_get_int (self->settings, "seek-method"));

    if (self->was_playing)
      clapper_player_play (player);
  }

  /* Reset */
  self->was_playing = FALSE;
  self->pending_position = 0;
  self->current_duration = 0;

  self->seeking = FALSE;
}

static void
_announce_current_seek_position (ClapperAppWindow *self, gboolean forward)
{
  gchar *position_str = g_strdup_printf (
      "%" CLAPPER_TIME_FORMAT " / %" CLAPPER_TIME_FORMAT,
      CLAPPER_TIME_ARGS (self->pending_position),
      CLAPPER_TIME_ARGS (self->current_duration));

  clapper_gtk_billboard_post_message (self->billboard,
      (forward) ? "media-seek-forward-symbolic" : "media-seek-backward-symbolic",
      position_str);

  g_free (position_str);
}

static inline void
_alter_position (ClapperAppWindow *self, gdouble dx)
{
  gboolean forward;

  /* This can only work on devices that
   * can detect scrolling begin and end */
  if (!self->scrolling
      || (!self->seeking && !_begin_seek_operation (self)))
    return;

  forward = (dx > 0);
  self->pending_position += dx;

  if (!forward) {
    if (self->pending_position < 0)
      self->pending_position = 0;
  } else {
    if (self->pending_position > self->current_duration)
      self->pending_position = self->current_duration;
  }

  _announce_current_seek_position (self, forward);
}

static void
scroll_begin_cb (GtkEventControllerScroll *scroll, ClapperAppWindow *self)
{
  GST_LOG_OBJECT (self, "Scroll begin");

  /* Assume that if device can begin, it can also end */
  self->scrolling = TRUE;
}

static gboolean
scroll_cb (GtkEventControllerScroll *scroll,
    gdouble dx, gdouble dy, ClapperAppWindow *self)
{
  GtkWidget *pickup;
  GdkDevice *device;
  gboolean handled;

  pickup = _pick_pointer_widget (self);

  /* We do not want to accidentally allow this controller to handle
   * scrolls when hovering over widgets that also handle scroll */
  while (pickup && !CLAPPER_GTK_IS_VIDEO (pickup)) {
    if (GTK_IS_SCROLLED_WINDOW (pickup) || GTK_IS_RANGE (pickup))
      return FALSE;

    pickup = gtk_widget_get_parent (pickup);
  }

  device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (scroll));

  switch (gdk_device_get_source (device)) {
    case GDK_SOURCE_TOUCHPAD:
    case GDK_SOURCE_TOUCHSCREEN:
      dx *= 0.4;
      dy *= 0.4;
      break;
    default:
      break;
  }

  if ((handled = AXIS_WINS_OVER (dy, dx)))
    _alter_volume (self, dy);
  else if ((handled = AXIS_WINS_OVER (dx, dy)))
    _alter_position (self, dx);

  return handled;
}

static void
scroll_end_cb (GtkEventControllerScroll *scroll, ClapperAppWindow *self)
{
  GST_LOG_OBJECT (self, "Scroll end");

  self->scrolling = FALSE;

  if (self->seeking)
    _end_seek_operation (self);
}

static void
_handle_seek_key_press (ClapperAppWindow *self, gboolean forward)
{
  gint unit;
  gdouble offset;

  if (!self->seeking && !_begin_seek_operation (self))
    return;

  offset = (gdouble) g_settings_get_int (self->settings, "seek-value");
  unit = g_settings_get_int (self->settings, "seek-unit");

  switch (unit) {
    case CLAPPER_APP_SEEK_UNIT_SECOND:
      break;
    case CLAPPER_APP_SEEK_UNIT_MINUTE:
      offset *= 60;
      break;
    case CLAPPER_APP_SEEK_UNIT_PERCENTAGE:
      offset = (offset / 100.) * self->current_duration;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  forward ^= (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

  if (forward)
    self->pending_position += offset;
  else
    self->pending_position -= offset;

  if (!forward) {
    if (self->pending_position < 0)
      self->pending_position = 0;
  } else {
    if (self->pending_position > self->current_duration)
      self->pending_position = self->current_duration;
  }

  _announce_current_seek_position (self, forward);
}

static void
_handle_chapter_key_press (ClapperAppWindow *self, gboolean forward)
{
  ClapperPlayer *player = clapper_gtk_video_get_player (
      CLAPPER_GTK_VIDEO_CAST (self->video));
  ClapperQueue *queue = clapper_player_get_queue (player);
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);
  ClapperTimeline *timeline;
  ClapperMarker *dest_marker = NULL;
  gdouble position;
  guint i;
  gboolean is_rtl;

  if (!current_item)
    return;

  timeline = clapper_media_item_get_timeline (current_item);
  i = clapper_timeline_get_n_markers (timeline);

  /* No markers to iterate */
  if (i == 0) {
    gst_object_unref (current_item);
    return;
  }

  is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);
  forward ^= is_rtl;
  position = clapper_player_get_position (player);

  /* When going backwards give small tolerance, so we can
   * still go to previous one even when directly at/after marker */
  if (!forward)
    position -= 1.5;

  while (i--) {
    ClapperMarker *marker = clapper_timeline_get_marker (timeline, i);
    ClapperMarkerType marker_type = clapper_marker_get_marker_type (marker);
    gdouble start;
    gboolean found = FALSE;

    /* Ignore custom markers */
    if (marker_type >= CLAPPER_MARKER_TYPE_CUSTOM_1) {
      gst_object_unref (marker);
      continue;
    }

    start = clapper_marker_get_start (marker);
    found = (start <= position);

    if (found) {
      if (!forward)
        dest_marker = marker;
      else
        gst_object_unref (marker);

      break;
    }

    if (forward)
      gst_object_replace ((GstObject **) &dest_marker, GST_OBJECT_CAST (marker));

    gst_object_unref (marker);
  }

  if (dest_marker) {
    const gchar *title;
    gdouble start, duration;
    gchar *text;

    title = clapper_marker_get_title (dest_marker);
    start = clapper_marker_get_start (dest_marker);
    duration = clapper_media_item_get_duration (current_item);

    /* XXX: When RTL with mixed numbers and text, we have to
     * switch positions of start <-> duration ourselves */
    text = g_strdup_printf (
        "%s\n%" CLAPPER_TIME_FORMAT " / %" CLAPPER_TIME_FORMAT, title,
        CLAPPER_TIME_ARGS ((!is_rtl) ? start : duration),
        CLAPPER_TIME_ARGS ((!is_rtl) ? duration : start));

    clapper_gtk_billboard_post_message (self->billboard,
        "user-bookmarks-symbolic", text);
    clapper_player_seek (player, start);

    g_free (text);
    gst_object_unref (dest_marker);
  }

  gst_object_unref (current_item);
}

static void
_handle_item_key_press (ClapperAppWindow *self, gboolean forward)
{
  ClapperPlayer *player = clapper_gtk_video_get_player (
      CLAPPER_GTK_VIDEO_CAST (self->video));
  ClapperQueue *queue = clapper_player_get_queue (player);
  guint prev_index, index;

  forward ^= (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

  prev_index = clapper_queue_get_current_index (queue);
  gtk_widget_activate_action (self->video,
      (forward) ? "video.next-item" : "video.previous-item", NULL);
  index = clapper_queue_get_current_index (queue);

  /* Notify only when changed */
  if (prev_index != index) {
    clapper_gtk_billboard_post_message (self->billboard,
        "applications-multimedia-symbolic",
        gtk_window_get_title (GTK_WINDOW (self)));
  }
}

static void
_handle_speed_key_press (ClapperAppWindow *self, gboolean forward)
{
  forward ^= (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

  gtk_widget_activate_action (self->video,
      (forward) ? "video.speed-up" : "video.speed-down", NULL);
}

static inline void
_handle_progression_key_press (ClapperAppWindow *self)
{
  ClapperPlayer *player = clapper_gtk_video_get_player (
      CLAPPER_GTK_VIDEO_CAST (self->video));
  ClapperQueue *queue = clapper_player_get_queue (player);
  ClapperQueueProgressionMode mode;
  const gchar *icon = NULL, *label = NULL;

  mode = ((clapper_queue_get_progression_mode (queue) + 1) % N_PROGRESSION_MODES);

  clapper_app_utils_parse_progression (mode, &icon, &label);
  clapper_queue_set_progression_mode (queue, mode);

  clapper_gtk_billboard_post_message (self->billboard, icon, label);
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller, guint keyval,
    guint keycode, GdkModifierType state, ClapperAppWindow *self)
{
  switch (keyval) {
    case GDK_KEY_Up:
      if ((state & GDK_MODIFIER_MASK) == 0)
        gtk_widget_activate_action (self->video, "video.volume-up", NULL);
      break;
    case GDK_KEY_Down:
      if ((state & GDK_MODIFIER_MASK) == 0)
        gtk_widget_activate_action (self->video, "video.volume-down", NULL);
      break;
    case GDK_KEY_Left:
      if ((state & GDK_MODIFIER_MASK) == 0) {
        _handle_seek_key_press (self, FALSE);
      } else if (!self->key_held && (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) {
        _handle_chapter_key_press (self, FALSE);
      } else if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK) {
        _handle_item_key_press (self, FALSE);
      }
      break;
    case GDK_KEY_j:
      if ((state & GDK_MODIFIER_MASK) == 0)
        _handle_seek_key_press (self, FALSE);
      break;
    case GDK_KEY_Right:
      if ((state & GDK_MODIFIER_MASK) == 0) {
        _handle_seek_key_press (self, TRUE);
      } else if (!self->key_held && (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) {
        _handle_chapter_key_press (self, TRUE);
      } else if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK) {
        _handle_item_key_press (self, TRUE);
      }
      break;
    case GDK_KEY_l:
      if ((state & GDK_MODIFIER_MASK) == 0)
        _handle_seek_key_press (self, TRUE);
      break;
    case GDK_KEY_space:
    case GDK_KEY_k:
      if (!self->key_held && (state & GDK_MODIFIER_MASK) == 0)
        gtk_widget_activate_action (self->video, "video.toggle-play", NULL);
      break;
    case GDK_KEY_less:
      if (!self->key_held) // Needs seek (action is slow)
        _handle_speed_key_press (self, FALSE);
      break;
    case GDK_KEY_greater:
      if (!self->key_held) // Needs seek (action is slow)
        _handle_speed_key_press (self, TRUE);
      break;
    case GDK_KEY_m:
      if (!self->key_held && (state & GDK_MODIFIER_MASK) == 0)
        gtk_widget_activate_action (self->video, "video.toggle-mute", NULL);
      break;
    case GDK_KEY_p:
      if (!self->key_held && (state & GDK_MODIFIER_MASK) == 0)
        _handle_progression_key_press (self);
      break;
    default:
      return FALSE;
  }

  self->key_held = TRUE;

  return TRUE;
}

static void
key_released_cb (GtkEventControllerKey *controller, guint keyval,
    guint keycode, GdkModifierType state, ClapperAppWindow *self)
{
  switch (keyval) {
    case GDK_KEY_Left:
    case GDK_KEY_j:
    case GDK_KEY_Right:
    case GDK_KEY_l:
      _end_seek_operation (self);
      break;
    default:
      break;
  }

  self->key_held = FALSE;
}

static void
_seek_delay_cb (ClapperAppWindow *self)
{
  GST_LOG_OBJECT (self, "Delayed seek handler reached");
  self->seek_timeout = 0;

  if (self->seeking)
    _end_seek_operation (self);
}

static void
video_seek_request_cb (ClapperGtkVideo *video, gboolean forward, ClapperAppWindow *self)
{
  g_clear_handle_id (&self->seek_timeout, g_source_remove);

  _handle_seek_key_press (self, forward);

  self->seek_timeout = g_timeout_add_once (500, (GSourceOnceFunc) _seek_delay_cb, self);
}

static void
drop_value_notify_cb (GtkDropTarget *drop_target,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindow *self)
{
  GtkWidget *stack;
  const GValue *value = gtk_drop_target_get_value (drop_target);

  if (!value) {
    clapper_gtk_billboard_unpin_pinned_message (self->billboard);
    return;
  }

  if (!clapper_app_utils_value_for_item_is_valid (value)) {
    gtk_drop_target_reject (drop_target);
    return;
  }

  stack = gtk_window_get_child (GTK_WINDOW (self));

  /* Do not pin message when still in initial state */
  if (gtk_stack_get_visible_child (GTK_STACK (stack)) == self->video) {
    clapper_gtk_billboard_pin_message (self->billboard,
        "insert-object-symbolic",
        _("Drop on title bar to play now or anywhere else to enqueue."));
  }
}

static gboolean
drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, ClapperAppWindow *self)
{
  GFile **files = NULL;
  gint n_files = 0;
  gboolean success = FALSE;

  if (clapper_app_utils_files_from_value (value, &files, &n_files)) {
    ClapperPlayer *player = clapper_app_window_get_player (self);
    ClapperQueue *queue = clapper_player_get_queue (player);
    gint i;

    clapper_app_window_ensure_no_initial_state (self);

    for (i = 0; i < n_files; ++i) {
      ClapperMediaItem *item = clapper_media_item_new_from_file (files[i]);

      clapper_queue_add_item (queue, item);
      gst_object_unref (item);
    }

    clapper_app_utils_files_free (files);
    success = TRUE;
  }

  return success;
}

static void
toggle_fullscreen (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (user_data);

  video_toggle_fullscreen_cb (CLAPPER_GTK_VIDEO_CAST (self->video), self);
}

static void
unfullscreen (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);

  if (gtk_window_is_fullscreen (window)) {
    ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (window);
    video_toggle_fullscreen_cb (CLAPPER_GTK_VIDEO_CAST (self->video), self);
  }
}

static void
auto_resize (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  _resize_window (CLAPPER_APP_WINDOW_CAST (user_data));
}

static void
show_help_overlay (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (user_data);
  GtkBuilder *builder;
  GtkWidget *help_overlay;

  builder = gtk_builder_new_from_resource (
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-help-overlay.ui");
  help_overlay = GTK_WIDGET (gtk_builder_get_object (builder, "help_overlay"));

  gtk_window_set_transient_for (GTK_WINDOW (help_overlay), GTK_WINDOW (self));
  gtk_window_present (GTK_WINDOW (help_overlay));

  g_object_unref (builder);
}

GtkWidget *
clapper_app_window_new (GtkApplication *application)
{
  return g_object_new (CLAPPER_APP_TYPE_WINDOW,
      "application", application,
      NULL);
}

GtkWidget *
clapper_app_window_get_video (ClapperAppWindow *self)
{
  return self->video;
}

ClapperPlayer *
clapper_app_window_get_player (ClapperAppWindow *self)
{
  return clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (self->video));
}

ClapperAppWindowExtraOptions *
clapper_app_window_get_extra_options (ClapperAppWindow *self)
{
  return g_object_get_qdata ((GObject *) self,
      clapper_app_window_extra_options_get_quark ());
}

void
clapper_app_window_ensure_no_initial_state (ClapperAppWindow *self)
{
  GtkWidget *stack = gtk_window_get_child (GTK_WINDOW (self));
  const gchar *child_name = gtk_stack_get_visible_child_name (GTK_STACK (stack));

  if (g_strcmp0 (child_name, "initial_state") == 0)
    gtk_stack_set_visible_child (GTK_STACK (stack), self->video);
}

static gboolean
clapper_app_window_close_request (GtkWindow *window)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (window);
/* FIXME: Have GSettings again to store these values
  GSettings *settings = g_settings_new (CLAPPER_APP_ID);
  gint width = DEFAULT_WINDOW_WIDTH, height = DEFAULT_WINDOW_HEIGHT;
*/
  GST_DEBUG_OBJECT (self, "Close request");
/*
  gtk_window_get_default_size (window, &width, &height);

  g_settings_set (settings, "window-size", "(ii)", width, height);
  g_settings_set_boolean (settings, "maximized", gtk_window_is_maximized (window));
  g_settings_set_boolean (settings, "fullscreen", gtk_window_is_fullscreen (window));

  g_object_unref (settings);
*/

  return GTK_WINDOW_CLASS (parent_class)->close_request (window);
}

static void
clapper_app_window_realize (GtkWidget *widget)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (widget);

  GST_TRACE_OBJECT (self, "Realize");

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  gtk_style_context_add_provider_for_display (gtk_widget_get_display (widget),
      (GtkStyleProvider *) self->provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
clapper_app_window_unrealize (GtkWidget *widget)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (widget);

  GST_TRACE_OBJECT (self, "Unrealize");

  gtk_style_context_remove_provider_for_display (gtk_widget_get_display (widget),
      (GtkStyleProvider *) self->provider);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clapper_app_window_init (ClapperAppWindow *self)
{
  ClapperAppWindowExtraOptions *extra_opts;
  GtkSettings *settings;
  GtkWidget *dummy_titlebar;
  gint distance = 0;

  gtk_widget_set_size_request (GTK_WIDGET (self),
      MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);

  extra_opts = g_new0 (ClapperAppWindowExtraOptions, 1);
  GST_TRACE ("Created window extra options: %p", extra_opts);

  g_object_set_qdata_full ((GObject *) self,
      clapper_app_window_extra_options_get_quark (),
      extra_opts, (GDestroyNotify) clapper_app_window_extra_options_free);

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Make double tap easier to perform */
  settings = gtk_widget_get_settings (self->video);
  g_object_get (settings, "gtk-double-click-distance", &distance, NULL);
  g_object_set (settings, "gtk-double-click-distance", MAX (distance, 32), NULL);

  dummy_titlebar = g_object_new (GTK_TYPE_BOX,
      "can_focus", FALSE,
      "focusable", FALSE,
      "visible", FALSE,
      NULL);
  gtk_window_set_titlebar (GTK_WINDOW (self), dummy_titlebar);
  gtk_window_set_title (GTK_WINDOW (self), CLAPPER_APP_NAME);

  /* Prevent GTK from redrawing background for each frame */
  gtk_widget_remove_css_class (GTK_WIDGET (self), "background");

  gtk_drop_target_set_gtypes (self->drop_target,
      (GType[3]) { GDK_TYPE_FILE_LIST, G_TYPE_FILE, G_TYPE_STRING }, 3);
}

static void
clapper_app_window_constructed (GObject *object)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (object);
  ClapperPlayer *player = clapper_app_window_get_player (self);
  ClapperQueue *queue = clapper_player_get_queue (player);
  ClapperGtkExtraMenuButton *button;
  AdwStyleManager *manager;

  static const GActionEntry win_entries[] = {
    { "toggle-fullscreen", toggle_fullscreen, NULL, NULL, NULL },
    { "unfullscreen", unfullscreen, NULL, NULL, NULL },
    { "auto-resize", auto_resize, NULL, NULL, NULL },
    { "show-help-overlay", show_help_overlay, NULL, NULL, NULL },
  };

#if (CLAPPER_HAVE_MPRIS || CLAPPER_HAVE_SERVER || CLAPPER_HAVE_DISCOVERER)
  ClapperFeature *feature = NULL;
#endif
#if CLAPPER_HAVE_MPRIS
  gchar mpris_name[45];
  g_snprintf (mpris_name, sizeof (mpris_name),
      "org.mpris.MediaPlayer2.Clapper.instance%" G_GUINT16_FORMAT, instance_count++);
#endif

  self->settings = g_settings_new (CLAPPER_APP_ID);
  self->last_volume = PERCENTAGE_ROUND (g_settings_get_double (self->settings, "volume"));

#if CLAPPER_HAVE_MPRIS
  feature = CLAPPER_FEATURE (clapper_mpris_new (
      mpris_name, CLAPPER_APP_NAME, CLAPPER_APP_ID));
  clapper_mpris_set_queue_controllable (CLAPPER_MPRIS (feature), TRUE);
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif

#if CLAPPER_HAVE_SERVER
  feature = CLAPPER_FEATURE (clapper_server_new ());
  clapper_server_set_queue_controllable (CLAPPER_SERVER (feature), TRUE);
  g_settings_bind (self->settings, "server-enabled",
      feature, "enabled", G_SETTINGS_BIND_GET);
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif

#if CLAPPER_HAVE_DISCOVERER
  feature = CLAPPER_FEATURE (clapper_discoverer_new ());
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif

  /* FIXME: Allow setting sink/filter elements from prefs window
   * (this should include parsing bin descriptions) */

  clapper_player_set_autoplay (player, TRUE);

  /* No need to also call these here, as they only change
   * after application window is contructed */
  g_signal_connect (queue, "notify::current-item",
      G_CALLBACK (_queue_current_item_changed_cb), self);
  g_signal_connect (player, "notify::adaptive-bandwidth",
      G_CALLBACK (_player_adaptive_bandwidth_changed_cb), NULL);

  g_settings_bind (self->settings, "audio-offset",
      player, "audio-offset", G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "subtitle-offset",
      player, "subtitle-offset", G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "subtitle-font-desc",
      player, "subtitle-font-desc", G_SETTINGS_BIND_GET);

  button = clapper_gtk_simple_controls_get_extra_menu_button (
      self->simple_controls);

  g_settings_bind_with_mapping (self->settings, "seek-method",
      self->simple_controls, "seek-method", G_SETTINGS_BIND_GET,
      (GSettingsBindGetMapping) _get_seek_method_mapping,
      NULL, NULL, NULL);
  g_signal_connect (button, "open-subtitles",
      G_CALLBACK (_open_subtitles_cb), self);
  clapper_gtk_extra_menu_button_set_can_open_subtitles (button, TRUE);

  manager = adw_style_manager_get_default ();
  adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_DARK);

  self->provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (self->provider,
      CLAPPER_APP_RESOURCE_PREFIX "/css/styles.css");

  g_action_map_add_action_entries (G_ACTION_MAP (self),
      win_entries, G_N_ELEMENTS (win_entries), self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_app_window_dispose (GObject *object)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (object);

  if (self->resize_tick_id != 0) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->resize_tick_id);
    self->resize_tick_id = 0;
  }

  g_clear_handle_id (&self->seek_timeout, g_source_remove);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_WINDOW);

  gst_clear_object (&self->current_item);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_window_finalize (GObject *object)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_object_unref (self->settings);
  g_object_unref (self->provider);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_window_class_init (ClapperAppWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GtkWindowClass *window_class = (GtkWindowClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappwindow", 0,
      "Clapper App Window");

  gobject_class->constructed = clapper_app_window_constructed;
  gobject_class->dispose = clapper_app_window_dispose;
  gobject_class->finalize = clapper_app_window_finalize;

  widget_class->realize = clapper_app_window_realize;
  widget_class->unrealize = clapper_app_window_unrealize;

  window_class->close_request = clapper_app_window_close_request;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, video);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, billboard);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, simple_controls);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, drop_target);

  gtk_widget_class_bind_template_callback (widget_class, video_toggle_fullscreen_cb);
  gtk_widget_class_bind_template_callback (widget_class, video_seek_request_cb);
  gtk_widget_class_bind_template_callback (widget_class, video_map_cb);
  gtk_widget_class_bind_template_callback (widget_class, video_unmap_cb);
  gtk_widget_class_bind_template_callback (widget_class, scroll_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, scroll_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, key_released_cb);

  gtk_widget_class_bind_template_callback (widget_class, click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_update_cb);

  gtk_widget_class_bind_template_callback (widget_class, drop_value_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_cb);
}
