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

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gst/gst.h>

#include "clapper-gtk-video-placeholder-private.h"
#include "clapper-gtk-utils-private.h"

#define NORMAL_SPACING 16
#define ADAPT_SPACING 8

#define GST_CAT_DEFAULT clapper_gtk_video_placeholder_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkVideoPlaceholder
{
  ClapperGtkContainer parent;

  GtkWidget *box;
  GtkWidget *title_label;

  ClapperPlayer *player;
};

#define parent_class clapper_gtk_video_placeholder_parent_class
G_DEFINE_TYPE (ClapperGtkVideoPlaceholder, clapper_gtk_video_placeholder, CLAPPER_GTK_TYPE_CONTAINER)

static void
adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperGtkVideoPlaceholder *self)
{
  GST_DEBUG_OBJECT (self, "Adapted: %s", (adapt) ? "yes" : "no");

  gtk_box_set_spacing (GTK_BOX (self->box), (adapt) ? ADAPT_SPACING : NORMAL_SPACING);

  if (adapt) {
    gtk_widget_add_css_class (GTK_WIDGET (self), "adapted");
    gtk_widget_add_css_class (GTK_WIDGET (self->title_label), "title-2");
  } else {
    gtk_widget_remove_css_class (GTK_WIDGET (self), "adapted");
    gtk_widget_remove_css_class (GTK_WIDGET (self->title_label), "title-2");
  }
}

static void
_player_state_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkVideoPlaceholder *self)
{
  gtk_widget_set_visible (self->box,
      clapper_player_get_state (player) > CLAPPER_PLAYER_STATE_STOPPED);
}

GtkWidget *
clapper_gtk_video_placeholder_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_VIDEO_PLACEHOLDER, NULL);
}

static void
clapper_gtk_video_placeholder_map (GtkWidget *widget)
{
  ClapperGtkVideoPlaceholder *self = CLAPPER_GTK_VIDEO_PLACEHOLDER_CAST (widget);

  if ((self->player = clapper_gtk_get_player_from_ancestor (widget))) {
    g_signal_connect (self->player, "notify::state",
        G_CALLBACK (_player_state_changed_cb), self);
    _player_state_changed_cb (self->player, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_video_placeholder_unmap (GtkWidget *widget)
{
  ClapperGtkVideoPlaceholder *self = CLAPPER_GTK_VIDEO_PLACEHOLDER_CAST (widget);

  if (self->player) {
    g_signal_handlers_disconnect_by_func (self->player, _player_state_changed_cb, self);
    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_video_placeholder_init (ClapperGtkVideoPlaceholder *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_box_set_spacing (GTK_BOX (self->box), NORMAL_SPACING);
}

static void
clapper_gtk_video_placeholder_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_VIDEO_PLACEHOLDER);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_video_placeholder_class_init (ClapperGtkVideoPlaceholderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkvideoplaceholder", 0,
      "Clapper GTK Video Placeholder");
  clapper_gtk_init_translations ();

  gobject_class->dispose = clapper_gtk_video_placeholder_dispose;

  widget_class->map = clapper_gtk_video_placeholder_map;
  widget_class->unmap = clapper_gtk_video_placeholder_unmap;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-video-placeholder.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkVideoPlaceholder, box);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkVideoPlaceholder, title_label);

  gtk_widget_class_bind_template_callback (widget_class, adapt_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-video-placeholder");
}
