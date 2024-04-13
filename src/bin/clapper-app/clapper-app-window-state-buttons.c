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

#include <clapper/clapper.h>

#include "clapper-app-window-state-buttons.h"

#define GST_CAT_DEFAULT clapper_app_window_state_buttons_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppWindowStateButtons
{
  GtkBox parent;

  GtkWidget *minimize_button;
  GtkWidget *maximize_button;
  GtkWidget *close_button;

  /* Props */
  GtkWidget *menu_button;
  GtkPositionType position;

  gboolean has_minimize;
  gboolean has_maximize;
  gboolean has_close;

  gboolean has_buttons;

  gboolean is_maximized;
  gboolean is_fullscreen;

  GtkSettings *settings;
};

enum
{
  PROP_0,
  PROP_POSITION,
  PROP_MENU_BUTTON,
  PROP_LAST
};

#define parent_class clapper_app_window_state_buttons_parent_class
G_DEFINE_TYPE (ClapperAppWindowStateButtons, clapper_app_window_state_buttons, GTK_TYPE_BOX)

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
minimize_button_clicked_cb (GtkButton *button, ClapperAppWindowStateButtons *self)
{
  GST_INFO_OBJECT (self, "Minimize button clicked");
  gtk_widget_activate_action (GTK_WIDGET (self), "window.minimize", NULL);
}

static void
maximize_button_clicked_cb (GtkButton *button, ClapperAppWindowStateButtons *self)
{
  GST_INFO_OBJECT (self, "Maximize button clicked");
  gtk_widget_activate_action (GTK_WIDGET (self), "window.toggle-maximized", NULL);
}

static void
close_button_clicked_cb (GtkButton *button, ClapperAppWindowStateButtons *self)
{
  GST_INFO_OBJECT (self, "Close button clicked");
  gtk_widget_activate_action (GTK_WIDGET (self), "window.close", NULL);
}

static void
_refresh_buttons_visibility (ClapperAppWindowStateButtons *self)
{
  gboolean show_minimize = (self->has_minimize && !self->is_fullscreen);
  gboolean show_maximize = (self->has_maximize && !self->is_fullscreen);
  gboolean has_buttons;

  gtk_widget_set_visible (self->minimize_button, show_minimize);
  gtk_widget_set_visible (self->maximize_button, show_maximize);
  gtk_widget_set_visible (self->close_button, self->has_close);

  has_buttons = (self->menu_button != NULL || show_minimize
      || show_maximize || self->has_close);

  if (self->has_buttons != has_buttons) {
    self->has_buttons = has_buttons;

    if (self->has_buttons)
      gtk_widget_add_css_class (GTK_WIDGET (self), "filled");
    else
      gtk_widget_remove_css_class (GTK_WIDGET (self), "filled");
  }
}

static void
clapper_app_window_state_buttons_parse_layout (ClapperAppWindowStateButtons *self)
{
  gchar *org_layout = NULL;
  gboolean has_minimize = FALSE;
  gboolean has_maximize = FALSE;
  gboolean has_close = FALSE;

  GST_DEBUG_OBJECT (self, "Buttons layout update");

  g_object_get (self->settings, "gtk-decoration-layout", &org_layout, NULL);

  if (G_LIKELY (org_layout != NULL)) {
    GtkWidget *last_widget = self->menu_button;
    gboolean can_parse = (self->position == GTK_POS_LEFT);
    gboolean had_sign = can_parse;
    guint i;

    for (i = 0; org_layout[i] != '\0'; ++i) {
      const gchar *layout = org_layout + i;

      if (layout[0] == ',') {
        had_sign = TRUE;
        continue;
      }

      if (layout[0] == ':') {
        if (self->position == GTK_POS_LEFT)
          break;
        else
          can_parse = TRUE;

        had_sign = TRUE;
        continue;
      }

      if (had_sign && can_parse) {
        GtkWidget *widget = NULL;

        GST_TRACE_OBJECT (self, "Remaining layout: %s", layout);

        if (g_str_has_prefix (layout, "minimize")) {
          widget = self->minimize_button;
          has_minimize = TRUE;
        } else if (g_str_has_prefix (layout, "maximize")) {
          widget = self->maximize_button;
          has_maximize = TRUE;
        } else if (g_str_has_prefix (layout, "close")) {
          widget = self->close_button;
          has_close = TRUE;
        }

        if (widget) {
          gtk_box_reorder_child_after (GTK_BOX (self), widget, last_widget);
          last_widget = widget;
        }

        had_sign = FALSE;
      }
    }
  }

  self->has_minimize = has_minimize;
  self->has_maximize = has_maximize;
  self->has_close = has_close;

  GST_DEBUG_OBJECT (self, "Buttons layout parsed");

  g_free (org_layout);
}

static void
_decoration_layout_changed_cb (GtkSettings *settings,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindowStateButtons *self)
{
  clapper_app_window_state_buttons_parse_layout (self);
  _refresh_buttons_visibility (self);
}

static void
_surface_state_changed_cb (GdkSurface *surface,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppWindowStateButtons *self)
{
  GdkToplevelState state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));
  gboolean is_maximized, is_fullscreen;

  GST_LOG_OBJECT (self, "Surface state changed");

  is_maximized = (state & GDK_TOPLEVEL_STATE_MAXIMIZED);
  is_fullscreen = (state & GDK_TOPLEVEL_STATE_FULLSCREEN);

  if (self->is_maximized != is_maximized) {
    self->is_maximized = is_maximized;
    gtk_button_set_icon_name (GTK_BUTTON (self->maximize_button),
        (self->is_maximized) ? "window-restore-symbolic" : "window-maximize-symbolic");
  }
  if (self->is_fullscreen != is_fullscreen) {
    self->is_fullscreen = is_fullscreen;
    _refresh_buttons_visibility (self);
  }
}

static void
clapper_app_window_state_buttons_init (ClapperAppWindowStateButtons *self)
{
  self->position = GTK_POS_RIGHT;

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clapper_app_window_state_buttons_constructed (GObject *object)
{
  ClapperAppWindowStateButtons *self = CLAPPER_APP_WINDOW_STATE_BUTTONS_CAST (object);

  if (self->position == GTK_POS_RIGHT)
    gtk_widget_add_css_class (GTK_WIDGET (self), "right");
  else
    gtk_widget_add_css_class (GTK_WIDGET (self), "left");

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
_clear_stored_settings (ClapperAppWindowStateButtons *self)
{
  if (self->settings) {
    g_signal_handlers_disconnect_by_func (self->settings,
        _decoration_layout_changed_cb, self);
    g_clear_object (&self->settings);
  }
}

static void
clapper_app_window_state_buttons_realize (GtkWidget *widget)
{
  ClapperAppWindowStateButtons *self = CLAPPER_APP_WINDOW_STATE_BUTTONS_CAST (widget);
  GtkSettings *settings;
  GtkRoot *root;
  GdkSurface *surface;

  GST_TRACE_OBJECT (self, "Realize");

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  settings = gtk_settings_get_for_display (gtk_widget_get_display (widget));

  if (self->settings != settings) {
    _clear_stored_settings (self);
    self->settings = g_object_ref (settings);

    g_signal_connect (self->settings, "notify::gtk-decoration-layout",
        G_CALLBACK (_decoration_layout_changed_cb), self);
    _decoration_layout_changed_cb (self->settings, NULL, self);
  }

  root = gtk_widget_get_root (widget);
  surface = gtk_native_get_surface (GTK_NATIVE (root));

  g_signal_connect (surface, "notify::state",
      G_CALLBACK (_surface_state_changed_cb), self);
  _surface_state_changed_cb (surface, NULL, self);
}

static void
clapper_app_window_state_buttons_unrealize (GtkWidget *widget)
{
  ClapperAppWindowStateButtons *self = CLAPPER_APP_WINDOW_STATE_BUTTONS_CAST (widget);
  GtkRoot *root;
  GdkSurface *surface;

  GST_TRACE_OBJECT (self, "Unrealize");

  _clear_stored_settings (self);

  root = gtk_widget_get_root (widget);
  surface = gtk_native_get_surface (GTK_NATIVE (root));

  g_signal_handlers_disconnect_by_func (surface,
      _surface_state_changed_cb, self);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clapper_app_window_state_buttons_dispose (GObject *object)
{
  ClapperAppWindowStateButtons *self = CLAPPER_APP_WINDOW_STATE_BUTTONS_CAST (object);

  _clear_stored_settings (self);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_WINDOW_STATE_BUTTONS);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_window_state_buttons_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperAppWindowStateButtons *self = CLAPPER_APP_WINDOW_STATE_BUTTONS_CAST (object);

  switch (prop_id) {
    case PROP_POSITION:
      self->position = g_value_get_enum (value);
      break;
    case PROP_MENU_BUTTON:
      if ((self->menu_button = GTK_WIDGET (g_value_get_object (value)))) {
        gtk_box_prepend (GTK_BOX (self), self->menu_button);
        _refresh_buttons_visibility (self);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_window_state_buttons_class_init (ClapperAppWindowStateButtonsClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappwindowstatebuttons", 0,
      "Clapper App Window State Buttons");

  gobject_class->constructed = clapper_app_window_state_buttons_constructed;
  gobject_class->set_property = clapper_app_window_state_buttons_set_property;
  gobject_class->dispose = clapper_app_window_state_buttons_dispose;

  widget_class->realize = clapper_app_window_state_buttons_realize;
  widget_class->unrealize = clapper_app_window_state_buttons_unrealize;

  param_specs[PROP_POSITION] = g_param_spec_enum ("position",
      NULL, NULL, GTK_TYPE_POSITION_TYPE, GTK_POS_RIGHT,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MENU_BUTTON] = g_param_spec_object ("menu-button",
      NULL, NULL, GTK_TYPE_MENU_BUTTON,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-window-state-buttons.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindowStateButtons, minimize_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindowStateButtons, maximize_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppWindowStateButtons, close_button);

  gtk_widget_class_bind_template_callback (widget_class, minimize_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, maximize_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-app-window-state-buttons");
}
