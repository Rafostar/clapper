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
 * ClapperGtkTogglePlayButton:
 *
 * A #GtkButton for toggling play/pause of playback.
 */

#include <clapper/clapper.h>

#include "clapper-gtk-toggle-play-button.h"
#include "clapper-gtk-utils.h"

#define PLAY_ICON_NAME "media-playback-start-symbolic"
#define PAUSE_ICON_NAME "media-playback-pause-symbolic"

#define GST_CAT_DEFAULT clapper_gtk_toggle_play_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkTogglePlayButton
{
  GtkButton parent;

  GBinding *state_binding;
};

#define parent_class clapper_gtk_toggle_play_button_parent_class
G_DEFINE_TYPE (ClapperGtkTogglePlayButton, clapper_gtk_toggle_play_button, GTK_TYPE_BUTTON)

static gboolean
_transform_state_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, ClapperGtkTogglePlayButton *self)
{
  ClapperPlayerState state = g_value_get_enum (from_value);

  GST_DEBUG_OBJECT (self, "Reflecting player state change, now: %i", state);

  switch (state) {
    case CLAPPER_PLAYER_STATE_STOPPED:
    case CLAPPER_PLAYER_STATE_PAUSED:
      g_value_set_string (to_value, PLAY_ICON_NAME);
      break;
    case CLAPPER_PLAYER_STATE_PLAYING:
      g_value_set_string (to_value, PAUSE_ICON_NAME);
      break;
    default:
      return FALSE; // no change
  }

  return TRUE;
}

/**
 * clapper_gtk_toggle_play_button_new:
 *
 * Creates a new #ClapperGtkTogglePlayButton instance.
 *
 * Returns: a new toggle play button #GtkWidget.
 */
GtkWidget *
clapper_gtk_toggle_play_button_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_TOGGLE_PLAY_BUTTON, NULL);
}

static void
clapper_gtk_toggle_play_button_init (ClapperGtkTogglePlayButton *self)
{
  gtk_button_set_icon_name (GTK_BUTTON (self), PLAY_ICON_NAME);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self), "av.toggle-play");
}

static void
clapper_gtk_toggle_play_button_map (GtkWidget *widget)
{
  ClapperGtkTogglePlayButton *self = CLAPPER_GTK_TOGGLE_PLAY_BUTTON_CAST (widget);
  ClapperPlayer *player;

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    self->state_binding = g_object_bind_property_full (player, "state",
        self, "icon-name", G_BINDING_SYNC_CREATE,
        (GBindingTransformFunc) _transform_state_func,
        NULL, self, NULL);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_toggle_play_button_unmap (GtkWidget *widget)
{
  ClapperGtkTogglePlayButton *self = CLAPPER_GTK_TOGGLE_PLAY_BUTTON_CAST (widget);

  g_clear_pointer (&self->state_binding, g_binding_unbind);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_toggle_play_button_class_init (ClapperGtkTogglePlayButtonClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtktoggleplaybutton", 0,
      "Clapper GTK Toggle Play Button");

  widget_class->map = clapper_gtk_toggle_play_button_map;
  widget_class->unmap = clapper_gtk_toggle_play_button_unmap;
}
