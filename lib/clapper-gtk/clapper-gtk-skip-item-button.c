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
 * ClapperGtkSkipItemButton:
 *
 * A base class for creating buttons that skip to previous/next queue item.
 *
 * Unless you are in need to implement custom logic here by yourself, you should probably
 * just use one of this class descending widgets that `ClapperGtk` provides, that is:
 * [class@ClapperGtk.NextItemButton] and [class@ClapperGtk.PreviousItemButton].
 */

#include <gst/gst.h>
#include <clapper/clapper.h>

#include "clapper-gtk-skip-item-button.h"
#include "clapper-gtk-utils.h"

#define GST_CAT_DEFAULT clapper_gtk_skip_item_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperGtkSkipItemButtonPrivate ClapperGtkSkipItemButtonPrivate;

struct _ClapperGtkSkipItemButtonPrivate
{
  GBinding *n_items_binding;
  GBinding *current_index_binding;
};

#define parent_class clapper_gtk_skip_item_button_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (ClapperGtkSkipItemButton, clapper_gtk_skip_item_button, GTK_TYPE_BUTTON)

static gboolean
_transform_sensitive_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, ClapperGtkSkipItemButton *self)
{
  ClapperQueue *queue = CLAPPER_QUEUE_CAST (g_binding_dup_source (binding));
  ClapperGtkSkipItemButtonClass *skip_class = CLAPPER_GTK_SKIP_ITEM_BUTTON_GET_CLASS (self);
  gboolean can_skip;

  if (G_UNLIKELY (queue == NULL))
    return FALSE;

  can_skip = skip_class->can_skip (self, queue);
  gst_object_unref (queue);

  g_value_set_boolean (to_value, can_skip);
  GST_DEBUG_OBJECT (self, "Set sensitive: %s", (can_skip) ? "yes" : "no");

  return TRUE;
}

static void
clapper_gtk_skip_item_button_init (ClapperGtkSkipItemButton *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
}

static void
clapper_gtk_skip_item_button_map (GtkWidget *widget)
{
  ClapperGtkSkipItemButton *self = CLAPPER_GTK_SKIP_ITEM_BUTTON_CAST (widget);
  ClapperGtkSkipItemButtonPrivate *priv = clapper_gtk_skip_item_button_get_instance_private (self);
  ClapperPlayer *player;

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (player);

    priv->n_items_binding = g_object_bind_property_full (queue, "n-items",
        self, "sensitive", G_BINDING_DEFAULT, // Since we sync below, no need to do it twice
        (GBindingTransformFunc) _transform_sensitive_func,
        NULL, self, NULL);
    priv->current_index_binding = g_object_bind_property_full (queue, "current-index",
        self, "sensitive", G_BINDING_SYNC_CREATE,
        (GBindingTransformFunc) _transform_sensitive_func,
        NULL, self, NULL);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_skip_item_button_unmap (GtkWidget *widget)
{
  ClapperGtkSkipItemButton *self = CLAPPER_GTK_SKIP_ITEM_BUTTON_CAST (widget);
  ClapperGtkSkipItemButtonPrivate *priv = clapper_gtk_skip_item_button_get_instance_private (self);

  g_clear_pointer (&priv->n_items_binding, g_binding_unbind);
  g_clear_pointer (&priv->current_index_binding, g_binding_unbind);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_skip_item_button_clicked (GtkButton* button)
{
  ClapperGtkSkipItemButton *self = CLAPPER_GTK_SKIP_ITEM_BUTTON_CAST (button);
  ClapperGtkSkipItemButtonClass *skip_class = CLAPPER_GTK_SKIP_ITEM_BUTTON_GET_CLASS (self);
  ClapperPlayer *player;

  GST_DEBUG_OBJECT (self, "Clicked");

  player = clapper_gtk_get_player_from_ancestor (GTK_WIDGET (button));
  if (G_UNLIKELY (player == NULL))
    return;

  skip_class->skip_item (self, clapper_player_get_queue (player));
}

static void
clapper_gtk_skip_item_button_class_init (ClapperGtkSkipItemButtonClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GtkButtonClass *button_class = (GtkButtonClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkskipitembutton", 0,
      "Clapper GTK Skip Item Button");

  widget_class->map = clapper_gtk_skip_item_button_map;
  widget_class->unmap = clapper_gtk_skip_item_button_unmap;

  button_class->clicked = clapper_gtk_skip_item_button_clicked;
}
