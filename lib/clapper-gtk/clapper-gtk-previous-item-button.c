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
 * ClapperGtkPreviousItemButton:
 *
 * A #GtkButton for selecting previous queue item.
 */

#include <clapper/clapper.h>

#include "clapper-gtk-previous-item-button.h"

struct _ClapperGtkPreviousItemButton
{
  ClapperGtkSkipItemButton parent;
};

G_DEFINE_TYPE (ClapperGtkPreviousItemButton, clapper_gtk_previous_item_button, CLAPPER_GTK_TYPE_SKIP_ITEM_BUTTON)

static gboolean
clapper_gtk_previous_item_button_can_skip (ClapperGtkSkipItemButton *skip_button, ClapperQueue *queue)
{
  guint current_index = clapper_queue_get_current_index (queue);

  return (current_index != CLAPPER_QUEUE_INVALID_POSITION && current_index > 0);
}

static void
clapper_gtk_previous_item_button_skip_item (ClapperGtkSkipItemButton *skip_button, ClapperQueue *queue)
{
  clapper_queue_select_previous_item (queue);
}

/**
 * clapper_gtk_previous_item_button_new:
 *
 * Creates a new #GtkWidget with button to play previous #ClapperMediaItem.
 *
 * Returns: (transfer full): a new #GtkWidget instance.
 */
GtkWidget *
clapper_gtk_previous_item_button_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_PREVIOUS_ITEM_BUTTON, NULL);
}

static void
clapper_gtk_previous_item_button_init (ClapperGtkPreviousItemButton *self)
{
  gtk_button_set_icon_name (GTK_BUTTON (self), "media-skip-backward-symbolic");
}

static void
clapper_gtk_previous_item_button_class_init (ClapperGtkPreviousItemButtonClass *klass)
{
  ClapperGtkSkipItemButtonClass *skip_class = (ClapperGtkSkipItemButtonClass *) klass;

  skip_class->can_skip = clapper_gtk_previous_item_button_can_skip;
  skip_class->skip_item = clapper_gtk_previous_item_button_skip_item;
}
