/* Clapper GTK Integration Library
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
 * ClapperGtkVideo:
 *
 * A ready to be used GTK video widget implementing Clapper API.
 *
 * #ClapperGtkVideo is the main widget exposed by `ClapperGtk` API. It both displays
 * videos played by [class@Clapper.Player] (exposed as its property) and manages
 * revealing and fading of any additional widgets overlaid on top of it.
 *
 * Other widgets provided by `ClapperGtk` library, once placed anywhere on video
 * (including nesting within another widget like [class@Gtk.Box]) will automatically
 * control #ClapperGtkVideo they were overlaid on top of. This allows to freely create
 * custom playback control panels best suited for specific application. Additionally,
 * pre-made widgets such as [class@ClapperGtk.SimpleControls] are also available.
 *
 * # Basic usage
 *
 * A typical use case is to embed video widget as part of your app where video playback
 * is needed. Get the [class@Clapper.Player] belonging to the video widget and start adding
 * new [class@Clapper.MediaItem] items to the [class@Clapper.Queue] for playback.
 * For more information please refer to the Clapper playback library documentation.
 *
 * #ClapperGtkVideo can automatically take care of revealing and later fading overlaid
 * content when interacting with the video. To do this, simply add your widgets with
 * [method@ClapperGtk.Video.add_fading_overlay]. If you want to display some static content
 * on top of video (or take care of visibility within overlaid widget itself) you can add
 * it to the video as a normal overlay with [method@ClapperGtk.Video.add_overlay].
 *
 * # Actions
 *
 * #ClapperGtkVideo defines a set of built-in actions:
 *
 * ```yaml
 * - "video.toggle-play": toggle play/pause
 * - "video.play": start/resume playback
 * - "video.pause": pause playback
 * - "video.stop": stop playback
 * - "video.seek": seek to position (variant "d")
 * - "video.seek-custom": seek to position using seek method (variant "(di)")
 * - "video.toggle-mute": toggle mute state
 * - "video.set-mute": set mute state (variant "b")
 * - "video.volume-up": increase volume by 2%
 * - "video.volume-down": decrease volume by 2%
 * - "video.set-volume": set volume to specified value (variant "d")
 * - "video.speed-up": increase speed (from 0.05x - 2x range to nearest quarter)
 * - "video.speed-down": decrease speed (from 0.05x - 2x range to nearest quarter)
 * - "video.set-speed": set speed to specified value (variant "d")
 * - "video.previous-item": select previous item in queue
 * - "video.next-item": select next item in queue
 * - "video.select-item": select item at specified index in queue (variant "u")
 * ```
 *
 * # ClapperGtkVideo as GtkBuildable
 *
 * #ClapperGtkVideo implementation of the [iface@Gtk.Buildable] interface supports
 * placing children as either normal overlay by specifying `overlay` or a fading
 * one by specifying `fading-overlay` as the `type` attribute of a `<child>` element.
 * Position of overlaid content is determined by `valign/halign` properties.
 *
 * ```xml
 * <object class="ClapperGtkVideo" id="video">
 *   <child type="fading-overlay">
 *     <object class="ClapperGtkTitleHeader">
 *       <property name="valign">start</property>
 *     </object>
 *   </child>
 *   <child type="fading-overlay">
 *     <object class="ClapperGtkSimpleControls">
 *       <property name="valign">end</property>
 *     </object>
 *   </child>
 * </object>
 * ```
 */

#include "config.h"

#include <math.h>

#include "clapper-gtk-enums.h"
#include "clapper-gtk-video.h"
#include "clapper-gtk-lead-container.h"
#include "clapper-gtk-status-private.h"
#include "clapper-gtk-buffering-animation-private.h"
#include "clapper-gtk-video-placeholder-private.h"

#define PERCENTAGE_ROUND(a) (round ((gdouble) a / 0.01) * 0.01)

#define DEFAULT_FADE_DELAY 3000
#define DEFAULT_TOUCH_FADE_DELAY 5000
#define DEFAULT_AUTO_INHIBIT FALSE

#define MIN_MOTION_DELAY 100000

#define GST_CAT_DEFAULT clapper_gtk_video_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkVideo
{
  GtkWidget parent;

  GtkWidget *overlay;
  GtkWidget *status;
  GtkWidget *buffering_animation;

  GtkGesture *touch_gesture;
  GtkGesture *click_gesture;

  /* Props */
  ClapperPlayer *player;
  guint fade_delay;
  guint touch_fade_delay;
  gboolean auto_inhibit;

  GPtrArray *overlays;
  GPtrArray *fading_overlays;

  gboolean buffering;
  gboolean showing_status;

  gulong notify_revealed_id;
  guint fade_timeout;
  gboolean reveal, revealed;

  guint inhibit_cookie;

  /* Current pointer coords and type */
  gdouble x, y;
  gboolean is_touch;
  gboolean touching;
  gint64 last_motion_time;
  gboolean pending_toggle_play;
};

static void
clapper_gtk_video_add_child (GtkBuildable *buildable,
    GtkBuilder *builder, GObject *child, const char *type)
{
  if (GTK_IS_WIDGET (child)) {
    if (g_strcmp0 (type, "overlay") == 0)
      clapper_gtk_video_add_overlay (CLAPPER_GTK_VIDEO (buildable), GTK_WIDGET (child));
    else if (g_strcmp0 (type, "fading-overlay") == 0)
      clapper_gtk_video_add_fading_overlay (CLAPPER_GTK_VIDEO (buildable), GTK_WIDGET (child));
    else
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
  } else {
    GtkBuildableIface *parent_iface = g_type_interface_peek_parent (GTK_BUILDABLE_GET_IFACE (buildable));
    parent_iface->add_child (buildable, builder, child, type);
  }
}

static void
_buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = clapper_gtk_video_add_child;
}

#define parent_class clapper_gtk_video_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperGtkVideo, clapper_gtk_video, GTK_TYPE_WIDGET,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, _buildable_iface_init))

enum
{
  PROP_0,
  PROP_PLAYER,
  PROP_FADE_DELAY,
  PROP_TOUCH_FADE_DELAY,
  PROP_AUTO_INHIBIT,
  PROP_INHIBITED,
  PROP_LAST
};

enum
{
  SIGNAL_TOGGLE_FULLSCREEN,
  SIGNAL_SEEK_REQUEST,
  SIGNAL_LAST
};

static gboolean provider_added = FALSE;
static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

static void
toggle_play_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  switch (clapper_player_get_state (player)) {
    case CLAPPER_PLAYER_STATE_PLAYING:
      clapper_player_pause (player);
      break;
    case CLAPPER_PLAYER_STATE_STOPPED:
    case CLAPPER_PLAYER_STATE_PAUSED:
      clapper_player_play (player);
      break;
    default:
      break;
  }
}

static void
play_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  clapper_player_play (player);
}

static void
pause_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  clapper_player_pause (player);
}

static void
stop_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  clapper_player_stop (player);
}

static void
seek_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble position = g_variant_get_double (parameter);

  clapper_player_seek (player, position);
}

static void
seek_custom_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  ClapperPlayerSeekMethod method = CLAPPER_PLAYER_SEEK_METHOD_NORMAL;
  gdouble position = 0;

  g_variant_get (parameter, "(di)", &position, &method);
  clapper_player_seek_custom (player, position, method);
}

static void
toggle_mute_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  clapper_player_set_mute (player, !clapper_player_get_mute (player));
}

static void
set_mute_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gboolean mute = g_variant_get_boolean (parameter);

  clapper_player_set_mute (player, mute);
}

static void
volume_up_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble volume = (clapper_player_get_volume (player) + 0.02);

  if (volume > 2.0)
    volume = 2.0;

  clapper_player_set_volume (player, PERCENTAGE_ROUND (volume));
}

static void
volume_down_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble volume = (clapper_player_get_volume (player) - 0.02);

  if (volume < 0)
    volume = 0;

  clapper_player_set_volume (player, PERCENTAGE_ROUND (volume));
}

static void
set_volume_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble volume = g_variant_get_double (parameter);

  clapper_player_set_volume (player, volume);
}

static void
speed_up_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble dest, speed = clapper_player_get_speed (player);

  if (speed >= 2.0)
    return;

  dest = 0.25;
  while (speed >= dest)
    dest += 0.25;

  if (dest > 2.0)
    dest = 2.0;

  clapper_player_set_speed (player, dest);
}

static void
speed_down_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble dest, speed = clapper_player_get_speed (player);

  if (speed <= 0.05)
    return;

  dest = 2.0;
  while (speed <= dest)
    dest -= 0.25;

  if (dest < 0.05)
    dest = 0.05;

  clapper_player_set_speed (player, dest);
}

static void
set_speed_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  gdouble speed = g_variant_get_double (parameter);

  clapper_player_set_speed (player, speed);
}

static void
previous_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  clapper_queue_select_previous_item (clapper_player_get_queue (player));
}

static void
next_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);

  clapper_queue_select_next_item (clapper_player_get_queue (player));
}

static void
select_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  ClapperPlayer *player = clapper_gtk_video_get_player (self);
  guint index = g_variant_get_uint32 (parameter);

  clapper_queue_select_index (clapper_player_get_queue (player), index);
}

static void
_set_reveal_fading_overlays (ClapperGtkVideo *self, gboolean reveal)
{
  GdkCursor *cursor = gdk_cursor_new_from_name ((reveal) ? "default" : "none", NULL);
  guint i;

  self->reveal = reveal;
  GST_LOG_OBJECT (self, "%s requested", (self->reveal) ? "Reveal" : "Fade");

  gtk_widget_set_cursor (GTK_WIDGET (self), cursor);
  g_object_unref (cursor);

  for (i = 0; i < self->fading_overlays->len; ++i) {
    GtkRevealer *revealer = (GtkRevealer *) g_ptr_array_index (self->fading_overlays, i);

    if (reveal)
      gtk_widget_set_visible ((GtkWidget *) revealer, TRUE);

    gtk_revealer_set_reveal_child (revealer, reveal);
  }
}

static inline gboolean
_is_on_leading_overlay (ClapperGtkVideo *self, ClapperGtkVideoActionMask blocked_action)
{
  GtkWidget *video = (GtkWidget *) self;
  GtkWidget *tmp_widget = gtk_widget_pick (video, self->x, self->y, GTK_PICK_DEFAULT);
  gboolean is_leading = FALSE;

  GST_LOG_OBJECT (self, "Checking if is on leading overlay...");

  while (tmp_widget && tmp_widget != video) {
    if (CLAPPER_GTK_IS_LEAD_CONTAINER (tmp_widget)) {
      ClapperGtkLeadContainer *lead_container = CLAPPER_GTK_LEAD_CONTAINER_CAST (tmp_widget);

      if (clapper_gtk_lead_container_get_leading (lead_container)
          && (clapper_gtk_lead_container_get_blocked_actions (lead_container) & blocked_action)) {
        is_leading = TRUE;
        break;
      }
    }
    tmp_widget = gtk_widget_get_parent (tmp_widget);
  }

  GST_LOG_OBJECT (self, "Is on leading overlay: %s", (is_leading) ? "yes" : "no");

  return is_leading;
}

static inline gboolean
_determine_can_fade (ClapperGtkVideo *self)
{
  GtkWidget *video = (GtkWidget *) self;
  GtkRoot *root;
  GtkNative *native, *child_native;
  GtkWidget *focus_child;
  gboolean in_fading_overlay = FALSE;

  GST_LOG_OBJECT (self, "Checking if overlays can fade...");

  if (self->is_touch) {
    if (self->touching) {
      GST_LOG_OBJECT (self, "Cannot fade while interacting with touchscreen");
      return FALSE;
    }
  } else if (self->x > 0 && self->y > 0) {
    GtkWidget *tmp_widget = gtk_widget_pick (video, self->x, self->y, GTK_PICK_DEFAULT);
    guint i;

    if (!tmp_widget) {
      GST_LOG_OBJECT (self, "Can fade, since no widget under pointer");
      return TRUE;
    }

    for (i = 0; i < self->fading_overlays->len; ++i) {
      GtkWidget *revealer = (GtkWidget *) g_ptr_array_index (self->fading_overlays, i);

      if (tmp_widget == revealer || gtk_widget_is_ancestor (tmp_widget, revealer)) {
        in_fading_overlay = TRUE;
        break;
      }
    }

    if (!in_fading_overlay) {
      GST_LOG_OBJECT (self, "Can fade, since pointer not within fading overlay");
      return TRUE;
    }

    while (tmp_widget && tmp_widget != video) {
      GtkStateFlags state_flags = gtk_widget_get_state_flags (tmp_widget);

      if (GTK_IS_ACTIONABLE (tmp_widget)
          && (state_flags & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE))) {
        GST_LOG_OBJECT (self, "Cannot fade while on activatable widget");
        return FALSE;
      }
      if ((state_flags & GTK_STATE_FLAG_DROP_ACTIVE)) {
        GST_LOG_OBJECT (self, "Cannot fade on drop-active widget");
        return FALSE;
      }
      if (GTK_IS_ACCESSIBLE (tmp_widget) && gtk_widget_get_can_target (tmp_widget)) {
        GtkAccessibleRole role = gtk_accessible_get_accessible_role ((GtkAccessible *) tmp_widget);

        switch (role) {
          case GTK_ACCESSIBLE_ROLE_LIST:
            GST_LOG_OBJECT (self, "Cannot fade while browsing list");
            return FALSE;
          case GTK_ACCESSIBLE_ROLE_SLIDER:
          case GTK_ACCESSIBLE_ROLE_SCROLLBAR:
            GST_LOG_OBJECT (self, "Cannot fade while on slider/scrollbar");
            return FALSE;
          default:
            break;
        }
      }

      tmp_widget = gtk_widget_get_parent (tmp_widget);
    };
  }

  root = gtk_widget_get_root (video);

  if (G_UNLIKELY (root == NULL))
    return FALSE;

  focus_child = gtk_root_get_focus (root);

  if (!focus_child
      || !gtk_widget_has_focus (focus_child)
      || !gtk_widget_is_ancestor (focus_child, video)) {
    GST_LOG_OBJECT (self, "Can fade, since no focused child in video");
    return TRUE;
  }

  native = gtk_widget_get_native (video);
  child_native = gtk_widget_get_native (focus_child);

  if (native != child_native) {
    GST_LOG_OBJECT (self, "Cannot fade while another surface is open");
    return FALSE;
  }

  GST_LOG_OBJECT (self, "Can fade");
  return TRUE;
}

static void
_fade_overlay_delay_cb (ClapperGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Fade handler reached");
  self->fade_timeout = 0;

  if (self->reveal) {
    gboolean can_fade = _determine_can_fade (self);

    GST_DEBUG_OBJECT (self, "Can fade overlays: %s", (can_fade) ? "yes" : "no");

    if (can_fade)
      _set_reveal_fading_overlays (self, FALSE);
  }
}

static void
_reset_fade_timeout (ClapperGtkVideo *self)
{
  GST_TRACE_OBJECT (self, "Fade timeout reset");

  g_clear_handle_id (&self->fade_timeout, g_source_remove);
  self->fade_timeout = g_timeout_add_once (
      (self->is_touch) ? self->touch_fade_delay : self->fade_delay,
      (GSourceOnceFunc) _fade_overlay_delay_cb, self);
}

static void
_window_is_active_cb (GtkWindow *window,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkVideo *self)
{
  gboolean active = gtk_window_is_active (window);

  GST_DEBUG_OBJECT (self, "Window is now %sactive",
      (active) ? "" : "in");

  if (!active) {
    /* Needs to set when drag starts during touch,
     * we do not get touch release then */
    self->touching = FALSE;

    /* Ensure our overlays will fade eventually */
    if (self->revealed && !self->fade_timeout)
      _reset_fade_timeout (self);
  }
}

static void
_handle_motion (ClapperGtkVideo *self, GtkEventController *controller, gdouble x, gdouble y)
{
  gint64 now;

  /* Start with points comparison as its faster,
   * otherwise we will check if threshold exceeded */
  if (self->x == x && self->y == y)
    return;

  now = g_get_monotonic_time ();

  /* We do not want to reset timeout too often
   * (especially on high refresh rate screens). */
  if (now - self->last_motion_time >= MIN_MOTION_DELAY) {
    GdkDevice *device = gtk_event_controller_get_current_event_device (controller);
    gboolean is_threshold = (ABS (self->x - x) > 1 || ABS (self->y - y) > 1);

    self->x = x;
    self->y = y;
    self->is_touch = (device && gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);

    if (is_threshold) {
      if (!self->reveal && !_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_REVEAL_OVERLAYS))
        _set_reveal_fading_overlays (self, TRUE);

      /* Extend time until fade */
      if (self->revealed)
        _reset_fade_timeout (self);
    }

    self->last_motion_time = now;
  }
}

static void
_handle_motion_leave (ClapperGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Motion leave");

  /* On leave we only reset coords to let overlays fade,
   * device is not expected to change here */
  self->x = -1;
  self->y = -1;

  /* Ensure our overlays will fade eventually */
  if (self->revealed && !self->fade_timeout)
    _reset_fade_timeout (self);
}

static void
motion_enter_cb (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  GdkDevice *device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (motion));

  /* XXX: We do not update x/y coords here in order to not mislead us
   * that we are not on non-fading overlay when another surface is open */

  self->is_touch = (device && gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);

  /* Tap to reveal is handled elsewhere */
  if (self->is_touch)
    return;

  if (!self->reveal && !_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_REVEAL_OVERLAYS))
    _set_reveal_fading_overlays (self, TRUE);

  /* Extend time until fade */
  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
motion_cb (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  _handle_motion (self, GTK_EVENT_CONTROLLER (motion), x, y);
}

static void
motion_leave_cb (GtkEventControllerMotion *motion, ClapperGtkVideo *self)
{
  _handle_motion_leave (self);
}

static void
drop_motion_cb (GtkDropControllerMotion *drop_motion,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  /* We do not actually support D&D here, just want to track
   * drop motion events from it and reveal overlays as one
   * or more widgets overlaid may support current drop */

  _handle_motion (self, GTK_EVENT_CONTROLLER (drop_motion), x, y);
}

static void
drop_motion_leave_cb (GtkDropControllerMotion *drop_motion, ClapperGtkVideo *self)
{
  _handle_motion_leave (self);
}

static void
left_click_pressed_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  GdkDevice *device;

  GST_LOG_OBJECT (self, "Left click pressed");

  /* Need to always clear click timeout,
   * so we will not pause after double click */
  self->pending_toggle_play = FALSE;

  device = gtk_gesture_get_device (GTK_GESTURE (click));

  self->x = x;
  self->y = y;
  self->is_touch = (device && gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);
}

static gboolean
_touch_in_lr_area (ClapperGtkVideo *self, gboolean *forward)
{
  gint video_w = gtk_widget_get_width (GTK_WIDGET (self));
  gdouble area_w = (video_w / 4.);
  gboolean in_area;

  if ((in_area = (self->x <= area_w))) {
    if (forward)
      *forward = FALSE;
  } else if ((in_area = (self->x >= video_w - area_w))) {
    if (forward)
      *forward = TRUE;
  }

  if (in_area && forward)
    *forward ^= (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

  GST_LOG_OBJECT (self, "Touch in area: %s (x: %.2lf, video_w: %i, area_w: %.0lf)",
      (in_area) ? "yes" : "no", self->x, video_w, area_w);

  return in_area;
}

static inline void
_handle_single_click (ClapperGtkVideo *self, GtkGestureClick *click)
{
  GdkDevice *device = gtk_gesture_get_device (GTK_GESTURE (click));

  /* FIXME: Try GstNavigation first and do below logic only when not handled
   * by upstream elements (maybe use sequence claiming for that?) */

  switch (gdk_device_get_source (device)) {
    case GDK_SOURCE_TOUCHSCREEN:
      /* First tap should only reveal overlays if fading/faded */
      if (!self->reveal && !_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_REVEAL_OVERLAYS)) {
        _set_reveal_fading_overlays (self, TRUE);
        gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
        break;
      }
      G_GNUC_FALLTHROUGH;
    default:
      if (!_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_TOGGLE_PLAY)) {
        self->pending_toggle_play = TRUE;
        gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
      }
      break;
  }
}

static inline void
_handle_double_click (ClapperGtkVideo *self, GtkGestureClick *click)
{
  gboolean handled = FALSE;

  if (self->is_touch) {
    gboolean forward = FALSE;

    if (_touch_in_lr_area (self, &forward)
        && !_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_SEEK_REQUEST)
        && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
           signals[SIGNAL_SEEK_REQUEST], 0, NULL, NULL, NULL) != 0) {
      g_signal_emit (self, signals[SIGNAL_SEEK_REQUEST], 0, forward);
      handled = TRUE;
    }
  }

  if (!handled) {
    if ((handled = !_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_TOGGLE_FULLSCREEN)))
      g_signal_emit (self, signals[SIGNAL_TOGGLE_FULLSCREEN], 0);
  }

  if (handled)
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static inline void
_handle_nth_click (ClapperGtkVideo *self, GtkGestureClick *click)
{
  gboolean forward = FALSE;

  if (_touch_in_lr_area (self, &forward)
      && !_is_on_leading_overlay (self, CLAPPER_GTK_VIDEO_ACTION_SEEK_REQUEST)) {
    g_signal_emit (self, signals[SIGNAL_SEEK_REQUEST], 0, forward);
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  }
}

static void
left_click_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Left click released");

  if (self->x < 0 || self->y < 0) {
    GST_LOG_OBJECT (self, "Ignoring click release outside of video");
    return;
  }

  self->x = x;
  self->y = y;

  switch (n_press) {
    case 1:
      _handle_single_click (self, click);
      break;
    case 2:
      _handle_double_click (self, click);
      break;
    default:
      _handle_nth_click (self, click);
      break;
  }

  /* Keep fading overlays revealed while clicking/tapping on video */
  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
left_click_stopped_cb (GtkGestureClick *click, ClapperGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Left click stopped");

  if (self->pending_toggle_play) {
    toggle_play_action_cb (GTK_WIDGET (self), NULL, NULL);
    self->pending_toggle_play = FALSE;
  }
}

static void
touch_pressed_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Touch pressed");

  self->is_touch = TRUE;
  self->touching = TRUE;

  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
touch_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Touch released");

  self->touching = FALSE;

  /* Ensure our overlays will fade eventually */
  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
_ensure_css_provider (void)
{
  GdkDisplay *display;

  if (provider_added)
    return;

  display = gdk_display_get_default ();

  if (G_LIKELY (display != NULL)) {
    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (provider,
        CLAPPER_GTK_RESOURCE_PREFIX "/css/styles.css");

    gtk_style_context_add_provider_for_display (display,
        (GtkStyleProvider *) provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 1);
    g_object_unref (provider);

    provider_added = TRUE;
  }
}

static inline void
_set_inhibit_session (ClapperGtkVideo *self, gboolean inhibit)
{
  GtkRoot *root;
  GApplication *app;
  gboolean inhibited = (self->inhibit_cookie != 0);

  if (inhibited == inhibit)
    return;

  GST_DEBUG_OBJECT (self, "Trying to %sinhibit session...", (inhibit) ? "" : "un");

  root = gtk_widget_get_root (GTK_WIDGET (self));

  if (!root && !GTK_IS_WINDOW (root)) {
    GST_WARNING_OBJECT (self, "Cannot %sinhibit session "
        "without root window", (inhibit) ? "" : "un");
    return;
  }

  /* NOTE: Not using application from window prop,
   * as it goes away early when unrooting */
  app = g_application_get_default ();

  if (!app && !GTK_IS_APPLICATION (app)) {
    GST_WARNING_OBJECT (self, "Cannot %sinhibit session "
        "without window application set", (inhibit) ? "" : "un");
    return;
  }

  if (inhibited) {
    gtk_application_uninhibit (GTK_APPLICATION (app), self->inhibit_cookie);
    self->inhibit_cookie = 0;
  }
  if (inhibit) {
    self->inhibit_cookie = gtk_application_inhibit (GTK_APPLICATION (app),
        GTK_WINDOW (root), GTK_APPLICATION_INHIBIT_IDLE,
        "Video is playing");
  }

  GST_DEBUG_OBJECT (self, "Session %sinhibited", (inhibit) ? "" : "un");
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_INHIBITED]);
}

static inline void
_set_buffering_animation_enabled (ClapperGtkVideo *self, gboolean enabled)
{
  ClapperGtkBufferingAnimation *animation;

  if (self->buffering == enabled)
    return;

  animation = CLAPPER_GTK_BUFFERING_ANIMATION_CAST (self->buffering_animation);
  gtk_widget_set_visible (self->buffering_animation, enabled);

  if (enabled)
    clapper_gtk_buffering_animation_start (animation);
  else
    clapper_gtk_buffering_animation_stop (animation);

  self->buffering = enabled;
}

static void
_player_state_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkVideo *self)
{
  ClapperPlayerState state = clapper_player_get_state (player);

  if (self->auto_inhibit)
    _set_inhibit_session (self, state == CLAPPER_PLAYER_STATE_PLAYING);

  _set_buffering_animation_enabled (self, state == CLAPPER_PLAYER_STATE_BUFFERING);
}

static GtkWidget *
_get_widget_from_video_sink (GstElement *vsink)
{
  GtkWidget *widget = NULL;
  GParamSpec *pspec;

  if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (vsink), "widget"))
      && g_type_is_a (pspec->value_type, GTK_TYPE_WIDGET)) {
    GST_DEBUG ("Video sink provides a widget");
    g_object_get (vsink, "widget", &widget, NULL);
  } else if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (vsink), "paintable"))
      && g_type_is_a (pspec->value_type, G_TYPE_OBJECT)) {
    GObject *paintable = NULL;

    GST_DEBUG ("Video sink provides a paintable");
    g_object_get (vsink, "paintable", &paintable, NULL);

    if (G_LIKELY (paintable != NULL)) {
      if (GDK_IS_PAINTABLE (paintable)) {
        widget = g_object_ref_sink (gtk_picture_new ());
        gtk_picture_set_paintable (GTK_PICTURE (widget), GDK_PAINTABLE (paintable));
      }

      g_object_unref (paintable);
    }
  }

  return widget;
}

static void
_video_sink_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkVideo *self)
{
  GstElement *vsink = clapper_player_get_video_sink (player);
  GtkWidget *widget = NULL;

  GST_DEBUG_OBJECT (self, "Video sink changed to: %" GST_PTR_FORMAT, vsink);

  if (vsink) {
    widget = _get_widget_from_video_sink (vsink);

    if (!widget && GST_IS_BIN (vsink)) {
      GstIterator *iter;
      GValue value = G_VALUE_INIT;

      iter = gst_bin_iterate_recurse (GST_BIN_CAST (vsink));

      while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
        GstElement *element = g_value_get_object (&value);

        if (GST_OBJECT_FLAG_IS_SET (element, GST_ELEMENT_FLAG_SINK))
          widget = _get_widget_from_video_sink (element);

        g_value_unset (&value);

        if (widget)
          break;
      }

      gst_iterator_free (iter);
    }

    gst_object_unref (vsink);
  }

  if (!widget) {
    GST_DEBUG_OBJECT (self, "No widget from video sink, using placeholder");
    widget = g_object_ref_sink (clapper_gtk_video_placeholder_new ());
  }

  gtk_overlay_set_child (GTK_OVERLAY (self->overlay), widget);
  g_object_unref (widget);

  GST_DEBUG_OBJECT (self, "Set new video widget");
}

static void
_player_error_cb (ClapperPlayer *player, GError *error,
    const gchar *debug_info, ClapperGtkVideo *self)
{
  /* FIXME: Handle authentication error (pop dialog to set credentials and retry) */

  /* Buffering will not finish anymore if we were in middle of it */
  _set_buffering_animation_enabled (self, FALSE);

  if (!self->showing_status) {
    clapper_gtk_status_set_error (CLAPPER_GTK_STATUS_CAST (self->status), error);
    self->showing_status = TRUE;
  }
}

static void
_player_missing_plugin_cb (ClapperPlayer *player, const gchar *name,
    const gchar *installer_detail, ClapperGtkVideo *self)
{
  /* Some media files have custom/proprietary metadata,
   * it should be safe to simply ignore these */
  if (strstr (name, "meta/") != NULL)
    return;

  /* XXX: Playbin2 seems to not emit state change here,
   * so manually stop buffering animation just in case */
  _set_buffering_animation_enabled (self, FALSE);

  /* XXX: Some content can still be played partially (e.g. without audio),
   * but it should be better to stop and notify user that something is missing */
  clapper_player_stop (player);

  /* We might get "missing-plugin" followed by "error" signal. This boolean prevents
   * immediately overwriting status and lets user deal with problems in order. */
  if (!self->showing_status) {
    clapper_gtk_status_set_missing_plugin (CLAPPER_GTK_STATUS_CAST (self->status), name);
    self->showing_status = TRUE;
  }
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkVideo *self)
{
  clapper_gtk_status_clear (CLAPPER_GTK_STATUS_CAST (self->status));
  self->showing_status = FALSE;
}

static void
_fading_overlay_revealed_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkVideo *self)
{
  self->revealed = gtk_revealer_get_child_revealed (revealer);

  /* Start fade timeout once fully revealed */
  if (self->revealed)
    _reset_fade_timeout (self);
}

/**
 * clapper_gtk_video_new:
 *
 * Creates a new #ClapperGtkVideo instance.
 *
 * Newly created video widget will also set some default GStreamer elements
 * on its [class@Clapper.Player]. This includes Clapper own video sink and
 * a "scaletempo" element as audio filter. Both can still be changed after
 * construction by setting corresponding player properties.
 *
 * Returns: a new video #GtkWidget.
 */
GtkWidget *
clapper_gtk_video_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_VIDEO, NULL);
}

/**
 * clapper_gtk_video_add_overlay:
 * @video: a #ClapperGtkVideo
 * @widget: a #GtkWidget
 *
 * Add another #GtkWidget to be overlaid on top of video.
 *
 * The position at which @widget is placed is determined from
 * [property@Gtk.Widget:halign] and [property@Gtk.Widget:valign] properties.
 *
 * This function will overlay @widget as-is meaning that widget is responsible
 * for managing its own visablity if needed. If you want to add a #GtkWidget
 * that will reveal and fade itself automatically when interacting with @video
 * (e.g. controls panel) you can use clapper_gtk_video_add_fading_overlay()
 * function for convenience.
 */
void
clapper_gtk_video_add_overlay (ClapperGtkVideo *self, GtkWidget *widget)
{
  g_return_if_fail (CLAPPER_GTK_IS_VIDEO (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_ptr_array_add (self->overlays, widget);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), widget);
}

/**
 * clapper_gtk_video_add_fading_overlay:
 * @video: a #ClapperGtkVideo
 * @widget: a #GtkWidget
 *
 * Similiar as clapper_gtk_video_add_overlay() but will also automatically
 * add fading functionality to overlaid #GtkWidget for convenience. This will
 * make widget reveal itself when interacting with @video and fade otherwise.
 * Useful when placing widgets such as playback controls panels.
 */
void
clapper_gtk_video_add_fading_overlay (ClapperGtkVideo *self, GtkWidget *widget)
{
  GtkWidget *revealer;

  g_return_if_fail (CLAPPER_GTK_IS_VIDEO (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  revealer = gtk_revealer_new ();

  g_object_bind_property (revealer, "child-revealed", revealer, "visible", G_BINDING_DEFAULT);

  g_object_bind_property (widget, "halign", revealer, "halign", G_BINDING_SYNC_CREATE);
  g_object_bind_property (widget, "valign", revealer, "valign", G_BINDING_SYNC_CREATE);

  /* Since we reveal/fade all at once, one signal connection is enough */
  if (self->notify_revealed_id == 0) {
    self->notify_revealed_id = g_signal_connect (revealer, "notify::child-revealed",
        G_CALLBACK (_fading_overlay_revealed_cb), self);
  }

  gtk_widget_set_visible (revealer, self->reveal);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), self->reveal);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 800);
  gtk_revealer_set_child (GTK_REVEALER (revealer), widget);

  g_ptr_array_add (self->fading_overlays, revealer);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), revealer);
}

/**
 * clapper_gtk_video_get_player:
 * @video: a #ClapperGtkVideo
 *
 * Get #ClapperPlayer used by this #ClapperGtkVideo instance.
 *
 * Returns: (transfer none): a #ClapperPlayer used by video.
 */
ClapperPlayer *
clapper_gtk_video_get_player (ClapperGtkVideo *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_VIDEO (self), NULL);

  return self->player;
}

/**
 * clapper_gtk_video_set_fade_delay:
 * @video: a #ClapperGtkVideo
 * @delay: a fade delay
 *
 * Set time in milliseconds after which fading overlays should fade.
 */
void
clapper_gtk_video_set_fade_delay (ClapperGtkVideo *self, guint delay)
{
  g_return_if_fail (CLAPPER_GTK_IS_VIDEO (self));
  g_return_if_fail (delay >= 1000);

  self->fade_delay = delay;
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_FADE_DELAY]);
}

/**
 * clapper_gtk_video_get_fade_delay:
 * @video: a #ClapperGtkVideo
 *
 * Get time in milliseconds after which fading overlays should fade.
 *
 * Returns: currently set fade delay.
 */
guint
clapper_gtk_video_get_fade_delay (ClapperGtkVideo *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_VIDEO (self), 0);

  return self->fade_delay;
}

/**
 * clapper_gtk_video_set_touch_fade_delay:
 * @video: a #ClapperGtkVideo
 * @delay: a touch fade delay
 *
 * Set time in milliseconds after which fading overlays should fade
 * when using touchscreen.
 *
 * It is often useful to set this higher then normal fade delay property,
 * as in case of touch events user do not have a moving pointer that would
 * extend fade timeout, so he can have more time to decide what to press next.
 */
void
clapper_gtk_video_set_touch_fade_delay (ClapperGtkVideo *self, guint delay)
{
  g_return_if_fail (CLAPPER_GTK_IS_VIDEO (self));
  g_return_if_fail (delay >= 1);

  self->touch_fade_delay = delay;
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_TOUCH_FADE_DELAY]);
}

/**
 * clapper_gtk_video_get_touch_fade_delay:
 * @video: a #ClapperGtkVideo
 *
 * Get time in milliseconds after which fading overlays should fade
 * when revealed using touch device.
 *
 * Returns: currently set touch fade delay.
 */
guint
clapper_gtk_video_get_touch_fade_delay (ClapperGtkVideo *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_VIDEO (self), 0);

  return self->touch_fade_delay;
}

/**
 * clapper_gtk_video_set_auto_inhibit:
 * @video: a #ClapperGtkVideo
 * @inhibit: whether to enable automatic session inhibit
 *
 * Set whether video should try to automatically inhibit session
 * from idling (and possibly screen going black) when video is playing.
 */
void
clapper_gtk_video_set_auto_inhibit (ClapperGtkVideo *self, gboolean inhibit)
{
  g_return_if_fail (CLAPPER_GTK_IS_VIDEO (self));

  if (self->auto_inhibit != inhibit) {
    self->auto_inhibit = inhibit;

    /* Uninhibit if we were auto inhibited earlier */
    if (!self->auto_inhibit)
      _set_inhibit_session (self, FALSE);

    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_AUTO_INHIBIT]);
  }
}

/**
 * clapper_gtk_video_get_auto_inhibit:
 * @video: a #ClapperGtkVideo
 *
 * Get whether automatic session inhibit is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
clapper_gtk_video_get_auto_inhibit (ClapperGtkVideo *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_VIDEO (self), FALSE);

  return self->auto_inhibit;
}

/**
 * clapper_gtk_video_get_inhibited:
 * @video: a #ClapperGtkVideo
 *
 * Get whether session is currently inhibited by
 * [property@ClapperGtk.Video:auto-inhibit].
 *
 * Returns: %TRUE if inhibited, %FALSE otherwise.
 */
gboolean
clapper_gtk_video_get_inhibited (ClapperGtkVideo *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_VIDEO (self), FALSE);

  return (self->inhibit_cookie != 0);
}

static void
clapper_gtk_video_root (GtkWidget *widget)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  GtkRoot *root;

  _ensure_css_provider ();

  GTK_WIDGET_CLASS (parent_class)->root (widget);

  root = gtk_widget_get_root (widget);

  if (root && GTK_IS_WINDOW (root)) {
    GtkWindow *window = GTK_WINDOW (root);

    g_signal_connect (window, "notify::is-active",
        G_CALLBACK (_window_is_active_cb), self);
    _window_is_active_cb (window, NULL, self);
  }

  if (self->auto_inhibit) {
    ClapperPlayerState state = clapper_player_get_state (self->player);
    _set_inhibit_session (self, state == CLAPPER_PLAYER_STATE_PLAYING);
  }
}

static void
clapper_gtk_video_unroot (GtkWidget *widget)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (widget);
  GtkRoot *root = gtk_widget_get_root (widget);

  if (root && GTK_IS_WINDOW (root)) {
    g_signal_handlers_disconnect_by_func (GTK_WINDOW (root),
        _window_is_active_cb, self);
  }

  _set_inhibit_session (self, FALSE);

  GTK_WIDGET_CLASS (parent_class)->unroot (widget);
}

static void
clapper_gtk_video_init (ClapperGtkVideo *self)
{
  self->overlay = gtk_overlay_new ();
  gtk_widget_set_overflow (self->overlay, GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_parent (self->overlay, GTK_WIDGET (self));

  self->overlays = g_ptr_array_new ();
  self->fading_overlays = g_ptr_array_new ();

  self->fade_delay = DEFAULT_FADE_DELAY;
  self->touch_fade_delay = DEFAULT_TOUCH_FADE_DELAY;
  self->auto_inhibit = DEFAULT_AUTO_INHIBIT;

  /* Ensure private types */
  g_type_ensure (CLAPPER_GTK_TYPE_STATUS);
  g_type_ensure (CLAPPER_GTK_TYPE_BUFFERING_ANIMATION);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_gesture_group (self->touch_gesture, self->click_gesture);
}

static void
clapper_gtk_video_constructed (GObject *object)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (object);
  GstElement *afilter, *vsink;
  ClapperQueue *queue;

  self->player = clapper_player_new ();
  queue = clapper_player_get_queue (self->player);

  g_signal_connect (self->player, "notify::state",
      G_CALLBACK (_player_state_changed_cb), self);
  g_signal_connect (self->player, "notify::video-sink",
      G_CALLBACK (_video_sink_changed_cb), self);

  vsink = gst_element_factory_make ("clappersink", NULL);

  /* FIXME: This is a temporary workaround for lack
   * of DMA_DRM negotiation support in sink itself */
  if (G_LIKELY (vsink != NULL)) {
    guint major = 0, minor = 0, micro = 0, nano = 0;

    gst_version (&major, &minor, &micro, &nano);
    if (major == 1 && minor >= 24) {
      GstElement *bin;

      if ((bin = gst_element_factory_make ("glsinkbin", NULL))) {
        g_object_set (bin, "sink", vsink, NULL);
        vsink = bin;
      }
    }

    clapper_player_set_video_sink (self->player, vsink);
  }

  afilter = gst_element_factory_make ("scaletempo", NULL);
  if (G_LIKELY (afilter != NULL))
    clapper_player_set_audio_filter (self->player, afilter);

  g_signal_connect (self->player, "error",
      G_CALLBACK (_player_error_cb), self);
  g_signal_connect (self->player, "missing-plugin",
      G_CALLBACK (_player_missing_plugin_cb), self);

  g_signal_connect (queue, "notify::current-item",
      G_CALLBACK (_queue_current_item_changed_cb), self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_gtk_video_dispose (GObject *object)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (object);

  if (self->notify_revealed_id != 0) {
    GtkRevealer *revealer = GTK_REVEALER (g_ptr_array_index (self->fading_overlays, 0));

    g_signal_handler_disconnect (revealer, self->notify_revealed_id);
    self->notify_revealed_id = 0;
  }

  g_clear_handle_id (&self->fade_timeout, g_source_remove);

  /* Something else might still be holding a reference on the player,
   * thus we should disconnect everything before disposing template */
  if (self->player) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_signal_handlers_disconnect_by_func (self->player,
        _player_state_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->player,
        _video_sink_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->player,
        _player_error_cb, self);
    g_signal_handlers_disconnect_by_func (self->player,
        _player_missing_plugin_cb, self);

    g_signal_handlers_disconnect_by_func (queue,
        _queue_current_item_changed_cb, self);
  }

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_VIDEO);

  g_clear_pointer (&self->overlay, gtk_widget_unparent);
  gst_clear_object (&self->player);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_video_finalize (GObject *object)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (object);

  g_ptr_array_unref (self->overlays);
  g_ptr_array_unref (self->fading_overlays);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_gtk_video_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (object);

  switch (prop_id) {
    case PROP_PLAYER:
      g_value_set_object (value, clapper_gtk_video_get_player (self));
      break;
    case PROP_FADE_DELAY:
      g_value_set_uint (value, clapper_gtk_video_get_fade_delay (self));
      break;
    case PROP_TOUCH_FADE_DELAY:
      g_value_set_uint (value, clapper_gtk_video_get_touch_fade_delay (self));
      break;
    case PROP_AUTO_INHIBIT:
      g_value_set_boolean (value, clapper_gtk_video_get_auto_inhibit (self));
      break;
    case PROP_INHIBITED:
      g_value_set_boolean (value, clapper_gtk_video_get_inhibited (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_video_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkVideo *self = CLAPPER_GTK_VIDEO_CAST (object);

  switch (prop_id) {
    case PROP_FADE_DELAY:
      clapper_gtk_video_set_fade_delay (self, g_value_get_uint (value));
      break;
    case PROP_TOUCH_FADE_DELAY:
      clapper_gtk_video_set_touch_fade_delay (self, g_value_get_uint (value));
      break;
    case PROP_AUTO_INHIBIT:
      clapper_gtk_video_set_auto_inhibit (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_video_class_init (ClapperGtkVideoClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkvideo", GST_DEBUG_FG_MAGENTA,
      "Clapper GTK Video");

  widget_class->root = clapper_gtk_video_root;
  widget_class->unroot = clapper_gtk_video_unroot;

  gobject_class->constructed = clapper_gtk_video_constructed;
  gobject_class->get_property = clapper_gtk_video_get_property;
  gobject_class->set_property = clapper_gtk_video_set_property;
  gobject_class->dispose = clapper_gtk_video_dispose;
  gobject_class->finalize = clapper_gtk_video_finalize;

  /**
   * ClapperGtkVideo:player:
   *
   * A #ClapperPlayer used by video.
   */
  param_specs[PROP_PLAYER] = g_param_spec_object ("player",
      NULL, NULL, CLAPPER_TYPE_PLAYER,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkVideo:fade-delay:
   *
   * A delay in milliseconds before trying to fade all fading overlays.
   */
  param_specs[PROP_FADE_DELAY] = g_param_spec_uint ("fade-delay",
      NULL, NULL, 1, G_MAXUINT, DEFAULT_FADE_DELAY,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkVideo:touch-fade-delay:
   *
   * A delay in milliseconds before trying to fade all fading overlays
   * after revealed using touchscreen.
   */
  param_specs[PROP_TOUCH_FADE_DELAY] = g_param_spec_uint ("touch-fade-delay",
      NULL, NULL, 1, G_MAXUINT, DEFAULT_TOUCH_FADE_DELAY,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkVideo:auto-inhibit:
   *
   * Try to automatically inhibit session when video is playing.
   */
  param_specs[PROP_AUTO_INHIBIT] = g_param_spec_boolean ("auto-inhibit",
      NULL, NULL, DEFAULT_AUTO_INHIBIT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkVideo:inhibited:
   *
   * Get whether session is currently inhibited by the video.
   */
  param_specs[PROP_INHIBITED] = g_param_spec_boolean ("inhibited",
      NULL, NULL, FALSE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkVideo::toggle-fullscreen:
   * @video: a #ClapperGtkVideo
   *
   * A signal that user requested a change in fullscreen state of the video.
   *
   * Note that when going fullscreen from this signal, user will expect
   * for only video to be fullscreened and not the whole app window.
   * It is up to implementation to decide how to handle that.
   */
  signals[SIGNAL_TOGGLE_FULLSCREEN] = g_signal_new ("toggle-fullscreen",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * ClapperGtkVideo::seek-request:
   * @video: a #ClapperGtkVideo
   * @forward: %TRUE if seek should be forward, %FALSE if backward
   *
   * A helper signal for implementing common seeking by double tap
   * on screen side for touchscreen devices.
   *
   * Note that @forward already takes into account RTL direction,
   * so implementation does not have to check.
   */
  signals[SIGNAL_SEEK_REQUEST] = g_signal_new ("seek-request",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_install_action (widget_class, "video.toggle-play", NULL, toggle_play_action_cb);
  gtk_widget_class_install_action (widget_class, "video.play", NULL, play_action_cb);
  gtk_widget_class_install_action (widget_class, "video.pause", NULL, pause_action_cb);
  gtk_widget_class_install_action (widget_class, "video.stop", NULL, stop_action_cb);
  gtk_widget_class_install_action (widget_class, "video.seek", "d", seek_action_cb);
  gtk_widget_class_install_action (widget_class, "video.seek-custom", "(di)", seek_custom_action_cb);
  gtk_widget_class_install_action (widget_class, "video.toggle-mute", NULL, toggle_mute_action_cb);
  gtk_widget_class_install_action (widget_class, "video.set-mute", "b", set_mute_action_cb);
  gtk_widget_class_install_action (widget_class, "video.volume-up", NULL, volume_up_action_cb);
  gtk_widget_class_install_action (widget_class, "video.volume-down", NULL, volume_down_action_cb);
  gtk_widget_class_install_action (widget_class, "video.set-volume", "d", set_volume_action_cb);
  gtk_widget_class_install_action (widget_class, "video.speed-up", NULL, speed_up_action_cb);
  gtk_widget_class_install_action (widget_class, "video.speed-down", NULL, speed_down_action_cb);
  gtk_widget_class_install_action (widget_class, "video.set-speed", "d", set_speed_action_cb);
  gtk_widget_class_install_action (widget_class, "video.previous-item", NULL, previous_item_action_cb);
  gtk_widget_class_install_action (widget_class, "video.next-item", NULL, next_item_action_cb);
  gtk_widget_class_install_action (widget_class, "video.select-item", "u", select_item_action_cb);

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-video.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkVideo, status);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkVideo, buffering_animation);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkVideo, touch_gesture);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkVideo, click_gesture);

  gtk_widget_class_bind_template_callback (widget_class, left_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, left_click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, left_click_stopped_cb);
  gtk_widget_class_bind_template_callback (widget_class, touch_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, touch_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_motion_leave_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-video");
}
