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

/**
 * ClapperGtkNextItemButton:
 *
 * A #GtkButton for selecting next queue item.
 */

#include <clapper/clapper.h>

#include "clapper-gtk-next-item-button.h"
#include "clapper-gtk-utils.h"

#define GST_CAT_DEFAULT clapper_gtk_next_item_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkNextItemButton
{
  GtkButton parent;

  GBinding *n_items_binding;
  GBinding *current_index_binding;
};

#define parent_class clapper_gtk_next_item_button_parent_class
G_DEFINE_TYPE (ClapperGtkNextItemButton, clapper_gtk_next_item_button, GTK_TYPE_BUTTON)

static gboolean
_transform_sensitive_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, ClapperGtkNextItemButton *self)
{
  ClapperQueue *queue = CLAPPER_QUEUE_CAST (g_binding_dup_source (binding));
  guint current_index;
  gboolean can_skip;

  if (G_UNLIKELY (queue == NULL))
    return FALSE;

  current_index = clapper_queue_get_current_index (queue);

  can_skip = (current_index != CLAPPER_QUEUE_INVALID_POSITION
      && current_index < clapper_queue_get_n_items (queue) - 1);
  gst_object_unref (queue);

  g_value_set_boolean (to_value, can_skip);
  GST_DEBUG_OBJECT (self, "Set sensitive: %s", (can_skip) ? "yes" : "no");

  return TRUE;
}

/**
 * clapper_gtk_next_item_button_new:
 *
 * Creates a new #ClapperGtkNextItemButton to play next #ClapperMediaItem.
 *
 * Returns: a new next item button #GtkWidget.
 */
GtkWidget *
clapper_gtk_next_item_button_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_NEXT_ITEM_BUTTON, NULL);
}

static void
clapper_gtk_next_item_button_init (ClapperGtkNextItemButton *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
  gtk_button_set_icon_name (GTK_BUTTON (self), "media-skip-forward-symbolic");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self), "video.next-item");
}

static void
clapper_gtk_next_item_button_map (GtkWidget *widget)
{
  ClapperGtkNextItemButton *self = CLAPPER_GTK_NEXT_ITEM_BUTTON_CAST (widget);
  ClapperPlayer *player;

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (player);

    self->n_items_binding = g_object_bind_property_full (queue, "n-items",
        self, "sensitive", G_BINDING_DEFAULT, // Since we sync below, no need to do it twice
        (GBindingTransformFunc) _transform_sensitive_func,
        NULL, self, NULL);
    self->current_index_binding = g_object_bind_property_full (queue, "current-index",
        self, "sensitive", G_BINDING_SYNC_CREATE,
        (GBindingTransformFunc) _transform_sensitive_func,
        NULL, self, NULL);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_next_item_button_unmap (GtkWidget *widget)
{
  ClapperGtkNextItemButton *self = CLAPPER_GTK_NEXT_ITEM_BUTTON_CAST (widget);

  g_clear_pointer (&self->n_items_binding, g_binding_unbind);
  g_clear_pointer (&self->current_index_binding, g_binding_unbind);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_next_item_button_class_init (ClapperGtkNextItemButtonClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtknextitembutton", 0,
      "Clapper GTK Next Item Button");

  widget_class->map = clapper_gtk_next_item_button_map;
  widget_class->unmap = clapper_gtk_next_item_button_unmap;
}
