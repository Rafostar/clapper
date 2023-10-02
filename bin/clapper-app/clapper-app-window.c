/*
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

#include <adwaita.h>
#include <clapper/clapper.h>
#include <clapper-gtk/clapper-gtk.h>

#include "clapper-app-window.h"
#include "clapper-app-utils.h"

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 576

#define AXIS_WINS_OVER(a,b) ((a > 0 && a - 0.3 > b) || (a < 0 && a + 0.3 < b))

#define GST_CAT_DEFAULT clapper_app_window_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppWindow
{
  GtkApplicationWindow parent;

  GtkWidget *video;
  GtkWidget *billboard;
  GtkWidget *simple_controls;

  GtkCssProvider *provider;
};

#define parent_class clapper_app_window_parent_class
G_DEFINE_TYPE (ClapperAppWindow, clapper_app_window, GTK_TYPE_APPLICATION_WINDOW);

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
video_toggle_fullscreen_cb (ClapperGtkVideo *video, ClapperAppWindow *self)
{
  GtkWindow *window = GTK_WINDOW (self);

  g_object_set (window, "fullscreened", !gtk_window_is_fullscreen (window), NULL);
}

static void
right_click_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperAppWindow *self)
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
  gfloat volume = clapper_player_get_volume (player);

  /* We do not want for volume to change too suddenly */
  if (dy > 2.0)
    dy = 2.0;
  else if (dy < -2.0)
    dy = -2.0;

  volume -= dy * 0.02;

  /* Prevent going out of range and make it easier to set exactly 100% */
  if (volume > 2.0)
    volume = 2.0;
  else if (G_APPROX_VALUE (volume, 1.0, 0.02 - FLT_EPSILON))
    volume = 1.0;
  else if (volume < 0.0)
    volume = 0.0;

  clapper_player_set_volume (player, volume);
}

static inline void
_alter_speed (ClapperAppWindow *self, gdouble dx)
{
  ClapperPlayer *player = clapper_gtk_video_get_player (CLAPPER_GTK_VIDEO_CAST (self->video));
  gfloat speed = clapper_player_get_speed (player);

  speed -= dx * 0.02;

  /* Prevent going out of range and make it easier to set exactly 1.0x */
  if (speed > 2.0)
    speed = 2.0;
  else if (G_APPROX_VALUE (speed, 1.0, 0.02 - FLT_EPSILON))
    speed = 1.0;
  else if (speed < 0.01)
    speed = 0.01;

  clapper_player_set_speed (player, speed);
}

static inline void
_alter_position (ClapperAppWindow *self, gdouble dx)
{
  GST_FIXME_OBJECT (self, "Handle seek on vertical scroll");
}

static gboolean
scroll_cb (GtkEventControllerScroll *scroll,
    gdouble dx, gdouble dy, ClapperAppWindow *self)
{
  GtkWidget *pickup;
  GdkDevice *device;
  gboolean handled;

  pickup = _pick_pointer_widget (self);

  /* We do not want to accidentally allow this controller
   * to handle scrolls when hovering over scrolled window */
  if (pickup && (GTK_IS_SCROLLED_WINDOW (pickup)
      || gtk_widget_get_ancestor (pickup, GTK_TYPE_SCROLLED_WINDOW)))
    return FALSE;

  device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (scroll));

  switch (gdk_device_get_source (device)) {
    case GDK_SOURCE_TOUCHPAD:
    case GDK_SOURCE_TOUCHSCREEN:
      dx *= -0.5;
      dy *= -0.5;
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
drop_value_notify_cb (GtkDropTarget *drop_target,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindow *self)
{
  const GValue *value = gtk_drop_target_get_value (drop_target);

  if (!value) {
    clapper_gtk_billboard_unpin_pinned_message (
        CLAPPER_GTK_BILLBOARD_CAST (self->billboard));
    return;
  }

  if (!clapper_app_utils_value_for_item_is_valid (value)) {
    gtk_drop_target_reject (drop_target);
    return;
  }

  clapper_gtk_billboard_pin_message (
      CLAPPER_GTK_BILLBOARD_CAST (self->billboard),
      "insert-object-symbolic",
      "Drop on title bar to play now or anywhere else to enqueue.");
}

static gboolean
drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, ClapperAppWindow *self)
{
  GFile **files = NULL;
  gboolean success;

  if (G_VALUE_HOLDS (value, G_TYPE_FILE)) {
    files = g_new (GFile *, 1);
    files[0] = g_value_dup_object (value);
  } else if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
    const gchar *uri = g_value_get_string (value);

    if (clapper_app_utils_uri_is_valid (uri)) {
      files = g_new (GFile *, 1);
      files[0] = g_file_new_for_uri (g_value_get_string (value));
    }
  }

  if ((success = (files != NULL))) {
    GtkApplication *gtk_app;

    gtk_app = gtk_window_get_application (GTK_WINDOW (self));
    g_application_open (G_APPLICATION (gtk_app), files, 1, NULL);

    g_object_unref (files[0]);
    g_free (files);
  }

  return success;
}

static void
toggle_fullscreen (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (user_data);

  video_toggle_fullscreen_cb (CLAPPER_GTK_VIDEO_CAST (self->video), self);
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

static gboolean
clapper_app_window_close_request (GtkWindow *window)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (window);
/* FIXME: Have GSettings again to store these values
  GSettings *settings = g_settings_new ("com.github.rafostar.Clapper");
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
  GtkWidget *dummy_titlebar;
  ClapperGtkExtraMenuButton *button;

  gtk_widget_init_template (GTK_WIDGET (self));

  dummy_titlebar = g_object_new (GTK_TYPE_BOX,
      "can_focus", FALSE,
      "focusable", FALSE,
      "visible", FALSE,
      NULL);
  gtk_window_set_titlebar (GTK_WINDOW (self), dummy_titlebar);

  button = clapper_gtk_simple_controls_get_extra_menu_button (
      CLAPPER_GTK_SIMPLE_CONTROLS_CAST (self->simple_controls));
  clapper_gtk_extra_menu_button_set_open_subtitles_visible (button, TRUE);

  /* Prevent GTK from redrawing background for each frame */
  gtk_widget_remove_css_class (GTK_WIDGET (self), "background");
}

static void
clapper_app_window_constructed (GObject *object)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (object);
  ClapperPlayer *player = clapper_app_window_get_player (self);
  AdwStyleManager *manager;

  static const GActionEntry win_entries[] = {
    { "toggle_fullscreen", toggle_fullscreen, NULL, NULL, NULL },
  };

#if (CLAPPER_HAVE_MPRIS || CLAPPER_HAVE_SERVER || CLAPPER_HAVE_DISCOVERER)
  ClapperFeature *feature = NULL;
#endif

#if CLAPPER_HAVE_MPRIS
  feature = CLAPPER_FEATURE (clapper_mpris_new (
      "org.mpris.MediaPlayer2.Clapper",
      "Clapper", "com.github.rafostar.Clapper"));
  clapper_mpris_set_queue_controllable (CLAPPER_MPRIS (feature), TRUE);
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif

#if CLAPPER_HAVE_SERVER
  feature = CLAPPER_FEATURE (clapper_server_new ());
  clapper_server_set_queue_controllable (CLAPPER_SERVER (feature), TRUE);
  clapper_server_set_enabled (CLAPPER_SERVER (feature), TRUE);
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif

#if CLAPPER_HAVE_DISCOVERER
  feature = CLAPPER_FEATURE (clapper_discoverer_new ());
  clapper_player_add_feature (player, feature);
  gst_object_unref (feature);
#endif

  clapper_player_set_autoplay (player, TRUE);

  manager = adw_style_manager_get_default ();
  adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_DARK);

  self->provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (self->provider,
      "/com/github/rafostar/Clapper/clapper-app/css/styles.css");

  g_action_map_add_action_entries (G_ACTION_MAP (self),
      win_entries, G_N_ELEMENTS (win_entries), self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_app_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_WINDOW);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_window_finalize (GObject *object)
{
  ClapperAppWindow *self = CLAPPER_APP_WINDOW_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

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
      "/com/github/rafostar/Clapper/clapper-app/ui/clapper-app-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, video);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, billboard);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindow, simple_controls);

  gtk_widget_class_bind_template_callback (widget_class, video_toggle_fullscreen_cb);
  gtk_widget_class_bind_template_callback (widget_class, right_click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_update_cb);
  gtk_widget_class_bind_template_callback (widget_class, scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_value_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_cb);
}
