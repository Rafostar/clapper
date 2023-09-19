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

#pragma once

#if !defined(__CLAPPER_GTK_INSIDE__) && !defined(CLAPPER_GTK_COMPILATION)
#error "Only <clapper-gtk/clapper-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <glib-object.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_SKIP_ITEM_BUTTON (clapper_gtk_skip_item_button_get_type())
#define CLAPPER_GTK_SKIP_ITEM_BUTTON_CAST(obj) ((ClapperGtkSkipItemButton *)(obj))

G_DECLARE_DERIVABLE_TYPE (ClapperGtkSkipItemButton, clapper_gtk_skip_item_button, CLAPPER_GTK, SKIP_ITEM_BUTTON, GtkButton)

struct _ClapperGtkSkipItemButtonClass
{
  GtkButtonClass parent_class;

  gboolean (* can_skip) (ClapperGtkSkipItemButton *skip_button, ClapperQueue *queue);

  void (* skip_item) (ClapperGtkSkipItemButton *skip_button, ClapperQueue *queue);
};

G_END_DECLS
