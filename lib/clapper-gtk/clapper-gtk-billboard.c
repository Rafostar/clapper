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

/**
 * ClapperGtkBillboard:
 *
 * A layer where various messages can be displayed.
 *
 * #ClapperGtkBillboard widget is meant to be overlaid on top of
 * [class@ClapperGtk.Video] as a normal (non-fading) overlay. It takes
 * care of displaying and later fading individual messages on its own.
 */

#include <gst/gst.h>

#include "clapper-gtk-billboard.h"
#include "clapper-gtk-utils.h"

#define GST_CAT_DEFAULT clapper_gtk_billboard_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkBillboard
{
  ClapperGtkContainer parent;

  GtkWidget *side_revealer;
  GtkWidget *progress_revealer;
  GtkWidget *progress_box;
  GtkWidget *top_progress;
  GtkWidget *bottom_progress;
  GtkWidget *progress_image;
  GtkWidget *progress_label;

  GtkWidget *message_revealer;
  GtkWidget *message_image;
  GtkWidget *message_label;

  gboolean has_pinned;

  guint side_timeout;
  guint message_timeout;
};

#define parent_class clapper_gtk_billboard_parent_class
G_DEFINE_TYPE (ClapperGtkBillboard, clapper_gtk_billboard, CLAPPER_GTK_TYPE_CONTAINER)

static void
adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperGtkBillboard *self)
{
  GST_DEBUG_OBJECT (self, "Adapted: %s", (adapt) ? "yes" : "no");

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->progress_revealer), !adapt);
}

static void
revealer_revealed_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkBillboard *self)
{
  if (!gtk_revealer_get_child_revealed (revealer)) {
    GtkWidget *other_revealer = (GTK_WIDGET (revealer) == self->side_revealer)
        ? self->message_revealer
        : self->side_revealer;

    gtk_widget_set_visible (GTK_WIDGET (revealer), FALSE);

    /* We only hide here when nothing is posted on the board,
     * visiblity is set to TRUE when post is made */
    if (!gtk_revealer_get_child_revealed (GTK_REVEALER (other_revealer)))
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
  }
}

static void
_unreveal_side_delay_cb (ClapperGtkBillboard *self)
{
  GST_LOG_OBJECT (self, "Unreveal side handler reached");
  self->side_timeout = 0;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->side_revealer), FALSE);
}

static void
_unreveal_message_delay_cb (ClapperGtkBillboard *self)
{
  GST_LOG_OBJECT (self, "Unreveal message handler reached");
  self->message_timeout = 0;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->message_revealer), FALSE);
}

static void
reveal_side (ClapperGtkBillboard *self)
{
  g_clear_handle_id (&self->side_timeout, g_source_remove);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_widget_set_visible (self->side_revealer, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->side_revealer), TRUE);

  self->side_timeout = g_timeout_add_once (2000,
      (GSourceOnceFunc) _unreveal_side_delay_cb, self);
}

static void
reveal_message (ClapperGtkBillboard *self)
{
  g_clear_handle_id (&self->message_timeout, g_source_remove);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_widget_set_visible (self->message_revealer, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->message_revealer), TRUE);

  if (!self->has_pinned) {
    self->message_timeout = g_timeout_add_once (5000,
        (GSourceOnceFunc) _unreveal_message_delay_cb, self);
  }
}

static void
_post_message_internal (ClapperGtkBillboard *self,
    const gchar *icon_name, const gchar *message, gboolean pin)
{
  if (self->has_pinned)
    return;

  self->has_pinned = pin;

  gtk_image_set_from_icon_name (GTK_IMAGE (self->message_image), icon_name);
  gtk_label_set_label (GTK_LABEL (self->message_label), message);

  reveal_message (self);
}

static void
_player_volume_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkBillboard *self)
{
  gfloat volume = clapper_player_get_volume (player);

  clapper_gtk_billboard_announce_volume (self, volume);
}

static void
_player_mute_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkBillboard *self)
{
  gboolean mute = clapper_player_get_mute (player);

  if (mute)
    clapper_gtk_billboard_announce_volume (self, 0);
  else
    _player_volume_changed_cb (player, NULL, self);
}

static void
_player_speed_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkBillboard *self)
{
  gfloat speed = clapper_player_get_speed (player);

  clapper_gtk_billboard_announce_speed (self, speed);
}

/**
 * clapper_gtk_billboard_new:
 *
 * Creates a new #ClapperGtkBillboard instance.
 *
 * Returns: (transfer full): a new billboard #GtkWidget.
 */
GtkWidget *
clapper_gtk_billboard_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_BILLBOARD, NULL);
}

void
clapper_gtk_billboard_post_message (ClapperGtkBillboard *self,
    const gchar *icon_name, const gchar *message)
{
  _post_message_internal (self, icon_name, message, FALSE);
}

void
clapper_gtk_billboard_pin_message (ClapperGtkBillboard *self,
    const gchar *icon_name, const gchar *message)
{
  _post_message_internal (self, icon_name, message, TRUE);
}

void
clapper_gtk_billboard_unpin_pinned_message (ClapperGtkBillboard *self)
{
  if (!self->has_pinned)
    return;

  _unreveal_message_delay_cb (self);
  self->has_pinned = FALSE;
}

void
clapper_gtk_billboard_announce_volume (ClapperGtkBillboard *self, gfloat volume)
{
  const gchar *icon_name;
  gchar *percent_str;
  gboolean has_overamp;

  /* Revert popup_speed changes */
  gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (self->bottom_progress), TRUE);

  icon_name = (volume <= 0.0)
      ? "audio-volume-muted-symbolic"
      : (volume <= 0.3)
      ? "audio-volume-low-symbolic"
      : (volume <= 0.7)
      ? "audio-volume-medium-symbolic"
      : (volume <= 1.0)
      ? "audio-volume-high-symbolic"
      : "audio-volume-overamplified-symbolic";

  has_overamp = gtk_widget_has_css_class (self->progress_box, "overamp");
  percent_str = g_strdup_printf ("%.0f%%", volume * 100);

  if (volume <= 1.0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), 0.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), volume);

    if (has_overamp)
      gtk_widget_remove_css_class (self->progress_box, "overamp");
  } else {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), volume - 1.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), 1.0);

    if (!has_overamp)
      gtk_widget_add_css_class (self->progress_box, "overamp");
  }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->progress_image), icon_name);
  gtk_label_set_label (GTK_LABEL (self->progress_label), percent_str);

  g_free (percent_str);

  reveal_side (self);
}

void
clapper_gtk_billboard_announce_speed (ClapperGtkBillboard *self, gfloat speed)
{
  const gchar *icon_name;
  gchar *speed_str;

  /* Revert popup_volume changes */
  if (gtk_widget_has_css_class (self->progress_box, "overamp"))
    gtk_widget_remove_css_class (self->progress_box, "overamp");

  gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (self->bottom_progress), FALSE);

  /* FIXME: Are these really the only icons we have? */
  icon_name = (speed < 1.0)
      ? "power-profile-power-saver-symbolic"
      : (speed == 1.0)
      ? "power-profile-balanced-symbolic"
      : "power-profile-performance-symbolic";

  speed_str = g_strdup_printf ("%.2fx", speed);

  if (speed <= 1.0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), 0.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), 1.0 - speed);
  } else {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), speed - 1.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), 0.0);
  }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->progress_image), icon_name);
  gtk_label_set_label (GTK_LABEL (self->progress_label), speed_str);

  g_free (speed_str);

  reveal_side (self);
}

static void
clapper_gtk_billboard_root (GtkWidget *widget)
{
  ClapperGtkBillboard *self = CLAPPER_GTK_BILLBOARD_CAST (widget);
  ClapperPlayer *player;

  GTK_WIDGET_CLASS (parent_class)->root (widget);

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    g_signal_connect (player, "notify::volume",
        G_CALLBACK (_player_volume_changed_cb), self);
    g_signal_connect (player, "notify::mute",
        G_CALLBACK (_player_mute_changed_cb), self);
    g_signal_connect (player, "notify::speed",
        G_CALLBACK (_player_speed_changed_cb), self);
  }
}

static void
clapper_gtk_billboard_unroot (GtkWidget *widget)
{
  ClapperGtkBillboard *self = CLAPPER_GTK_BILLBOARD_CAST (widget);
  ClapperPlayer *player;

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    g_signal_handlers_disconnect_by_func (player, _player_volume_changed_cb, self);
    g_signal_handlers_disconnect_by_func (player, _player_mute_changed_cb, self);
    g_signal_handlers_disconnect_by_func (player, _player_speed_changed_cb, self);
  }

  GTK_WIDGET_CLASS (parent_class)->unroot (widget);
}

static void
clapper_gtk_billboard_init (ClapperGtkBillboard *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clapper_gtk_billboard_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_BILLBOARD);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_billboard_finalize (GObject *object)
{
  ClapperGtkBillboard *self = CLAPPER_GTK_BILLBOARD_CAST (object);

  g_clear_handle_id (&self->side_timeout, g_source_remove);
  g_clear_handle_id (&self->message_timeout, g_source_remove);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_gtk_billboard_class_init (ClapperGtkBillboardClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkbillboard", 0,
      "Clapper GTK Billboard");

  gobject_class->dispose = clapper_gtk_billboard_dispose;
  gobject_class->finalize = clapper_gtk_billboard_finalize;

  /* Using root/unroot since initially invisible (unrealized)
   * and we want for signals to stay connected as long as parented */
  widget_class->root = clapper_gtk_billboard_root;
  widget_class->unroot = clapper_gtk_billboard_unroot;

  gtk_widget_class_set_template_from_resource (widget_class,
      "/com/github/rafostar/Clapper/clapper-gtk/ui/clapper-gtk-billboard.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, side_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, progress_box);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, progress_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, top_progress);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, bottom_progress);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, progress_image);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, progress_label);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, message_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, message_image);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkBillboard, message_label);

  gtk_widget_class_bind_template_callback (widget_class, adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, revealer_revealed_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-billboard");
}
