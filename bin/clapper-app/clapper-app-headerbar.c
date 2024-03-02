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

#include "clapper-app-headerbar.h"
#include "clapper-app-utils.h"

#define GST_CAT_DEFAULT clapper_app_headerbar_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppHeaderbar
{
  ClapperGtkContainer parent;

  GtkWidget *queue_revealer;
  GtkWidget *previous_item_revealer;
  GtkWidget *title_label;
  GtkWidget *next_item_revealer;
  GtkWidget *win_buttons_revealer;

  GBinding *title_binding;

  gboolean adapt;

  ClapperPlayer *player;
  ClapperMediaItem *current_item;
};

#define parent_class clapper_app_headerbar_parent_class
G_DEFINE_TYPE (ClapperAppHeaderbar, clapper_app_headerbar, CLAPPER_GTK_TYPE_CONTAINER);

static void
_determine_win_buttons_reveal (ClapperAppHeaderbar *self)
{
  gboolean queue_reveal = gtk_revealer_get_reveal_child (GTK_REVEALER (self->queue_revealer));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->win_buttons_revealer),
      (queue_reveal) ? !self->adapt : TRUE);
}

static void
container_adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperAppHeaderbar *self)
{
  GST_DEBUG_OBJECT (self, "Width adapted: %s", (adapt) ? "yes" : "no");
  self->adapt = adapt;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->previous_item_revealer), !adapt);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->next_item_revealer), !adapt);

  _determine_win_buttons_reveal (self);
}

static void
queue_reveal_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppHeaderbar *self)
{
  _determine_win_buttons_reveal (self);
}

static void
reveal_queue_button_clicked_cb (GtkButton *button, ClapperAppHeaderbar *self)
{
  gboolean reveal;

  GST_INFO_OBJECT (self, "Reveal queue button clicked");

  reveal = gtk_revealer_get_reveal_child (GTK_REVEALER (self->queue_revealer));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->queue_revealer), !reveal);
}

static void
drop_value_notify_cb (GtkDropTarget *drop_target,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppHeaderbar *self)
{
  const GValue *value = gtk_drop_target_get_value (drop_target);

  if (value && !clapper_app_utils_value_for_item_is_valid (value))
    gtk_drop_target_reject (drop_target);
}

static gboolean
drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, ClapperAppHeaderbar *self)
{
  ClapperMediaItem *item;
  gboolean success = FALSE;

  if ((item = clapper_app_utils_media_item_from_value (value))) {
    GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drop_target));
    ClapperPlayer *player;

    if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
      ClapperQueue *queue = clapper_player_get_queue (player);

      clapper_queue_add_item (queue, item);
      clapper_queue_select_item (queue, item);
      clapper_player_play (player);

      success = TRUE;
    }

    gst_object_unref (item);
  }

  return success;
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppHeaderbar *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  g_clear_pointer (&self->title_binding, g_binding_unbind);

  gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item));
  gst_clear_object (&current_item);

  if (self->current_item) {
    self->title_binding = g_object_bind_property (self->current_item, "title",
        self->title_label, "label", G_BINDING_SYNC_CREATE);
  } else {
    gtk_label_set_label (GTK_LABEL (self->title_label), NULL);
  }
}

GtkWidget *
clapper_app_headerbar_new (void)
{
  return g_object_new (CLAPPER_APP_TYPE_HEADERBAR, NULL);
}

static void
clapper_app_headerbar_map (GtkWidget *widget)
{
  ClapperAppHeaderbar *self = CLAPPER_APP_HEADERBAR_CAST (widget);

  if ((self->player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_signal_connect (queue, "notify::current-item",
        G_CALLBACK (_queue_current_item_changed_cb), self);

    _queue_current_item_changed_cb (queue, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_app_headerbar_unmap (GtkWidget *widget)
{
  ClapperAppHeaderbar *self = CLAPPER_APP_HEADERBAR_CAST (widget);

  g_clear_pointer (&self->title_binding, g_binding_unbind);

  if (self->player) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_signal_handlers_disconnect_by_func (queue, _queue_current_item_changed_cb, self);

    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_app_headerbar_init (ClapperAppHeaderbar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clapper_app_headerbar_dispose (GObject *object)
{
  ClapperAppHeaderbar *self = CLAPPER_APP_HEADERBAR_CAST (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_HEADERBAR);

  gst_clear_object (&self->current_item);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_headerbar_finalize (GObject *object)
{
  ClapperAppHeaderbar *self = CLAPPER_APP_HEADERBAR_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_headerbar_class_init (ClapperAppHeaderbarClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappheaderbar", 0,
      "Clapper App Headerbar");

  gobject_class->dispose = clapper_app_headerbar_dispose;
  gobject_class->finalize = clapper_app_headerbar_finalize;

  widget_class->map = clapper_app_headerbar_map;
  widget_class->unmap = clapper_app_headerbar_unmap;

  gtk_widget_class_set_template_from_resource (widget_class,
      "/com/github/rafostar/Clapper/clapper-app/ui/clapper-app-headerbar.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, queue_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, previous_item_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, title_label);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, next_item_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, win_buttons_revealer);

  gtk_widget_class_bind_template_callback (widget_class, container_adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, reveal_queue_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, queue_reveal_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_value_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-app-headerbar");
}
