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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * ClapperGtkToggleFullscreenButton:
 *
 * A #GtkButton for toggling fullscreen state.
 */

#include <gst/gst.h>

#include "clapper-gtk-toggle-fullscreen-button.h"
#include "clapper-gtk-video.h"

#define ENTER_FULLSCREEN_ICON_NAME "view-fullscreen-symbolic"
#define LEAVE_FULLSCREEN_ICON_NAME "view-restore-symbolic"

#define GST_CAT_DEFAULT clapper_gtk_toggle_fullscreen_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkToggleFullscreenButton
{
  GtkButton parent;

  gboolean is_fullscreen;
};

#define parent_class clapper_gtk_toggle_fullscreen_button_parent_class
G_DEFINE_TYPE (ClapperGtkToggleFullscreenButton, clapper_gtk_toggle_fullscreen_button, GTK_TYPE_BUTTON)

static void
_toplevel_state_changed_cb (GdkToplevel *toplevel,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkToggleFullscreenButton *self)
{
  GdkToplevelState state = gdk_toplevel_get_state (toplevel);
  gboolean is_fullscreen = (state & GDK_TOPLEVEL_STATE_FULLSCREEN);

  if (self->is_fullscreen == is_fullscreen)
    return;

  self->is_fullscreen = is_fullscreen;

  GST_DEBUG_OBJECT (self, "Toplevel state changed, fullscreen: %s",
      (self->is_fullscreen) ? "yes" : "no");

  gtk_button_set_icon_name (GTK_BUTTON (self),
      (!self->is_fullscreen) ? ENTER_FULLSCREEN_ICON_NAME : LEAVE_FULLSCREEN_ICON_NAME);
}

/**
 * clapper_gtk_toggle_fullscreen_button_new:
 *
 * Creates a new #ClapperGtkToggleFullscreenButton instance.
 *
 * Returns: a new toggle fullscreen button #GtkWidget.
 */
GtkWidget *
clapper_gtk_toggle_fullscreen_button_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_TOGGLE_FULLSCREEN_BUTTON, NULL);
}

static void
clapper_gtk_toggle_fullscreen_button_init (ClapperGtkToggleFullscreenButton *self)
{
  gtk_button_set_icon_name (GTK_BUTTON (self), ENTER_FULLSCREEN_ICON_NAME);
}

static void
clapper_gtk_toggle_fullscreen_button_map (GtkWidget *widget)
{
  ClapperGtkToggleFullscreenButton *self = CLAPPER_GTK_TOGGLE_FULLSCREEN_BUTTON_CAST (widget);
  GtkRoot *root;
  GdkSurface *surface;

  GST_TRACE_OBJECT (self, "Map");

  root = gtk_widget_get_root (widget);
  surface = gtk_native_get_surface (GTK_NATIVE (root));

  if (G_LIKELY (GDK_IS_TOPLEVEL (surface))) {
    GdkToplevel *toplevel = GDK_TOPLEVEL (surface);

    g_signal_connect (toplevel, "notify::state",
        G_CALLBACK (_toplevel_state_changed_cb), self);
    _toplevel_state_changed_cb (toplevel, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_toggle_fullscreen_button_unmap (GtkWidget *widget)
{
  ClapperGtkToggleFullscreenButton *self = CLAPPER_GTK_TOGGLE_FULLSCREEN_BUTTON_CAST (widget);
  GtkRoot *root;
  GdkSurface *surface;

  GST_TRACE_OBJECT (self, "Unmap");

  root = gtk_widget_get_root (widget);
  surface = gtk_native_get_surface (GTK_NATIVE (root));

  if (G_LIKELY (GDK_IS_TOPLEVEL (surface))) {
    GdkToplevel *toplevel = GDK_TOPLEVEL (surface);

    g_signal_handlers_disconnect_by_func (toplevel,
        _toplevel_state_changed_cb, self);
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_toggle_fullscreen_button_clicked (GtkButton* button)
{
  GtkWidget *video;

  GST_DEBUG_OBJECT (button, "Clicked");

  if ((video = gtk_widget_get_ancestor (GTK_WIDGET (button), CLAPPER_GTK_TYPE_VIDEO)))
    g_signal_emit_by_name (video, "toggle-fullscreen");
}

static void
clapper_gtk_toggle_fullscreen_button_class_init (ClapperGtkToggleFullscreenButtonClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GtkButtonClass *button_class = (GtkButtonClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtktogglefullscreenbutton", 0,
      "Clapper GTK Toggle Fullscreen Button");

  widget_class->map = clapper_gtk_toggle_fullscreen_button_map;
  widget_class->unmap = clapper_gtk_toggle_fullscreen_button_unmap;

  button_class->clicked = clapper_gtk_toggle_fullscreen_button_clicked;
}
