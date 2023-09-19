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
 * ClapperGtkTitleHeader:
 *
 * A header panel widget that displays current media title.
 *
 * #ClapperGtkTitleHeader is a simple, ready to be used header widget that
 * displays current media title. It can be placed as-is as a [class@ClapperGtk.Video]
 * overlay (either fading or not).
 */

#include <clapper/clapper.h>

#include "clapper-gtk-title-header.h"
#include "clapper-gtk-utils.h"

#define GST_CAT_DEFAULT clapper_gtk_title_header_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkTitleHeader
{
  ClapperGtkLeadContainer parent;

  GtkWidget *title_label;
  GBinding *title_binding;
};

#define parent_class clapper_gtk_title_header_parent_class
G_DEFINE_TYPE (ClapperGtkTitleHeader, clapper_gtk_title_header, CLAPPER_GTK_TYPE_LEAD_CONTAINER)

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkTitleHeader *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  g_clear_pointer (&self->title_binding, g_binding_unbind);

  if (current_item) {
    self->title_binding = g_object_bind_property (current_item, "title",
        self->title_label, "label", G_BINDING_SYNC_CREATE);
    gst_object_unref (current_item);
  } else {
    gtk_label_set_label (GTK_LABEL (self->title_label), "No media");
  }
}

/**
 * clapper_gtk_title_header_new:
 *
 * Creates a new #GtkWidget with title header.
 *
 * Returns: (transfer full): a new #GtkWidget instance.
 */
GtkWidget *
clapper_gtk_title_header_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_TITLE_HEADER, NULL);
}

static void
clapper_gtk_title_header_map (GtkWidget *widget)
{
  ClapperGtkTitleHeader *self = CLAPPER_GTK_TITLE_HEADER_CAST (widget);
  ClapperPlayer *player;

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (player);

    g_signal_connect (queue, "notify::current-item",
        G_CALLBACK (_queue_current_item_changed_cb), self);
    _queue_current_item_changed_cb (queue, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_title_header_unmap (GtkWidget *widget)
{
  ClapperGtkTitleHeader *self = CLAPPER_GTK_TITLE_HEADER_CAST (widget);
  ClapperPlayer *player;

  g_clear_pointer (&self->title_binding, g_binding_unbind);

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (player);

    g_signal_handlers_disconnect_by_func (queue, _queue_current_item_changed_cb, self);
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_title_header_init (ClapperGtkTitleHeader *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clapper_gtk_title_header_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_TITLE_HEADER);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_title_header_class_init (ClapperGtkTitleHeaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtktitleheader", 0,
      "Clapper GTK Title Header");

  gobject_class->dispose = clapper_gtk_title_header_dispose;

  widget_class->map = clapper_gtk_title_header_map;
  widget_class->unmap = clapper_gtk_title_header_unmap;

  gtk_widget_class_set_template_from_resource (widget_class,
      "/com/github/rafostar/Clapper/clapper-gtk/ui/clapper-gtk-title-header.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkTitleHeader, title_label);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-title-header");
}
