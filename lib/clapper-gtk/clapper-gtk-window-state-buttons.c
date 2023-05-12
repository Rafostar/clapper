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

/**
 * SECTION:clapper-gtk-window-state-buttons
 * @title: ClapperGtkWindowStateButtons
 * @short_description: A #GtkBox with window state control buttons (minimize, maximize, close).
 */

#include <clapper/clapper.h>

#include "clapper-gtk-window-state-buttons.h"

#define GST_CAT_DEFAULT clapper_gtk_window_state_buttons_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkWindowStateButtons
{
  GtkBox parent;

  GtkWidget *minimize_button;
  GtkWidget *maximize_button;
  GtkWidget *close_button;

  GtkSettings *settings;
};

#define parent_class clapper_gtk_window_state_buttons_parent_class
G_DEFINE_TYPE (ClapperGtkWindowStateButtons, clapper_gtk_window_state_buttons, GTK_TYPE_BOX)

static void
minimize_button_clicked_cb (GtkButton *button, ClapperGtkWindowStateButtons *self)
{
  GST_INFO_OBJECT (self, "Minimize button clicked");
  gtk_widget_activate_action (GTK_WIDGET (self), "window.minimize", NULL);
}

static void
maximize_button_clicked_cb (GtkButton *button, ClapperGtkWindowStateButtons *self)
{
  GST_INFO_OBJECT (self, "Maximize button clicked");
  gtk_widget_activate_action (GTK_WIDGET (self), "window.toggle-maximized", NULL);
}

static void
close_button_clicked_cb (GtkButton *button, ClapperGtkWindowStateButtons *self)
{
  GST_INFO_OBJECT (self, "Close button clicked");
  gtk_widget_activate_action (GTK_WIDGET (self), "window.close", NULL);
}

static void
clapper_gtk_window_state_buttons_update_layout (ClapperGtkWindowStateButtons *self)
{
  gchar *org_layout = NULL;
  const gchar *layout;
  gboolean minimize_visible = FALSE;
  gboolean maximize_visible = FALSE;
  gboolean close_visible = FALSE;
  GtkWidget *last_widget = NULL;
  guint i;

  GST_DEBUG_OBJECT (self, "Buttons layout update");

  g_object_get (self->settings, "gtk-decoration-layout", &org_layout, NULL);

  if (G_UNLIKELY (org_layout == NULL))
    return;

  layout = org_layout;

  for (i = 0; layout[i]; ++i) {
    GtkWidget *widget = NULL;
    const gchar *next = layout + i + 1;

    if (next[0] != '\0' && next[0] != ',' && next[0] != ':')
      continue;

    GST_TRACE_OBJECT (self, "Remaining layout: %s", layout);

    if (g_str_has_prefix (layout, "minimize")) {
      widget = self->minimize_button;
      minimize_visible = TRUE;
    } else if (g_str_has_prefix (layout, "maximize")) {
      widget = self->maximize_button;
      maximize_visible = TRUE;
    } else if (g_str_has_prefix (layout, "close")) {
      widget = self->close_button;
      close_visible = TRUE;
    }

    if (widget) {
      gtk_box_reorder_child_after (GTK_BOX (self), widget, last_widget);
      last_widget = widget;
    }

    if (next[0] == '\0')
      break;

    layout = next + 1;
    i = 0;
  }

  gtk_widget_set_visible (self->minimize_button, minimize_visible);
  gtk_widget_set_visible (self->maximize_button, maximize_visible);
  gtk_widget_set_visible (self->close_button, close_visible);

  GST_DEBUG_OBJECT (self, "Buttons layout updated");

  g_free (org_layout);
}

static void
clapper_gtk_window_state_buttons_update_maximize_icon (ClapperGtkWindowStateButtons *self,
    GdkToplevel *toplevel)
{
  GdkToplevelState state = gdk_toplevel_get_state (toplevel);
  gboolean is_maximized = (state & GDK_TOPLEVEL_STATE_MAXIMIZED);

  gtk_button_set_icon_name (GTK_BUTTON (self->maximize_button), (is_maximized)
      ? "window-restore-symbolic" : "window-maximize-symbolic");
}

static void
_decoration_layout_changed_cb (GtkSettings *settings,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkWindowStateButtons *self)
{
  clapper_gtk_window_state_buttons_update_layout (self);
}

static void
_surface_state_changed_cb (GdkSurface *surface,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkWindowStateButtons *self)
{
  clapper_gtk_window_state_buttons_update_maximize_icon (self, GDK_TOPLEVEL (surface));
}

/**
 * clapper_gtk_window_state_buttons_new:
 *
 * Creates a new #GtkWidget with window state control buttons.
 *
 * Returns: (transfer full): a new #GtkWidget instance.
 */
GtkWidget *
clapper_gtk_window_state_buttons_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_WINDOW_STATE_BUTTONS, NULL);
}

static void
clapper_gtk_window_state_buttons_init (ClapperGtkWindowStateButtons *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
_clear_stored_settings (ClapperGtkWindowStateButtons *self)
{
  if (self->settings) {
    g_signal_handlers_disconnect_by_func (self->settings,
        _decoration_layout_changed_cb, self);
    g_clear_object (&self->settings);
  }
}

static void
clapper_gtk_window_state_buttons_realize (GtkWidget *widget)
{
  ClapperGtkWindowStateButtons *self = CLAPPER_GTK_WINDOW_STATE_BUTTONS_CAST (widget);
  GtkSettings *settings;
  GtkRoot *root;
  GdkSurface *surface;

  GST_TRACE_OBJECT (self, "Realize");

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  settings = gtk_settings_get_for_display (gtk_widget_get_display (widget));

  if (settings != self->settings) {
    _clear_stored_settings (self);
    self->settings = g_object_ref (settings);

    g_signal_connect (self->settings, "notify::gtk-decoration-layout",
        G_CALLBACK (_decoration_layout_changed_cb), self);

    clapper_gtk_window_state_buttons_update_layout (self);
  }

  root = gtk_widget_get_root (widget);
  surface = gtk_native_get_surface (GTK_NATIVE (root));

  g_signal_connect (surface, "notify::state",
      G_CALLBACK (_surface_state_changed_cb), self);

  clapper_gtk_window_state_buttons_update_maximize_icon (self,
      GDK_TOPLEVEL (surface));
}

static void
clapper_gtk_window_state_buttons_unrealize (GtkWidget *widget)
{
  ClapperGtkWindowStateButtons *self = CLAPPER_GTK_WINDOW_STATE_BUTTONS_CAST (widget);
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
clapper_gtk_window_state_buttons_dispose (GObject *object)
{
  ClapperGtkWindowStateButtons *self = CLAPPER_GTK_WINDOW_STATE_BUTTONS_CAST (object);

  _clear_stored_settings (self);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_WINDOW_STATE_BUTTONS);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_window_state_buttons_finalize (GObject *object)
{
  /* FIXME: Do something here */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_gtk_window_state_buttons_class_init (ClapperGtkWindowStateButtonsClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkwindowstatebuttons", 0,
      "Clapper GTK Window State Buttons");

  gobject_class->dispose = clapper_gtk_window_state_buttons_dispose;
  gobject_class->finalize = clapper_gtk_window_state_buttons_finalize;

  widget_class->realize = clapper_gtk_window_state_buttons_realize;
  widget_class->unrealize = clapper_gtk_window_state_buttons_unrealize;

  gtk_widget_class_set_template_from_resource (widget_class,
      "/com/github/rafostar/Clapper/clapper-gtk/ui/clapper-gtk-window-state-buttons.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkWindowStateButtons, minimize_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkWindowStateButtons, maximize_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkWindowStateButtons, close_button);

  gtk_widget_class_bind_template_callback (widget_class, minimize_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, maximize_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-window-state-buttons");
}
