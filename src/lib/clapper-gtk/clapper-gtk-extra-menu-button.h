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

#pragma once

#if !defined(__CLAPPER_GTK_INSIDE__) && !defined(CLAPPER_GTK_COMPILATION)
#error "Only <clapper-gtk/clapper-gtk.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <clapper-gtk/clapper-gtk-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_EXTRA_MENU_BUTTON (clapper_gtk_extra_menu_button_get_type())
#define CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST(obj) ((ClapperGtkExtraMenuButton *)(obj))

CLAPPER_GTK_API
G_DECLARE_FINAL_TYPE (ClapperGtkExtraMenuButton, clapper_gtk_extra_menu_button, CLAPPER_GTK, EXTRA_MENU_BUTTON, GtkWidget)

CLAPPER_GTK_API
GtkWidget * clapper_gtk_extra_menu_button_new (void);

CLAPPER_GTK_API
void clapper_gtk_extra_menu_button_set_volume_visible (ClapperGtkExtraMenuButton *button, gboolean visible);

CLAPPER_GTK_API
gboolean clapper_gtk_extra_menu_button_get_volume_visible (ClapperGtkExtraMenuButton *button);

CLAPPER_GTK_API
void clapper_gtk_extra_menu_button_set_speed_visible (ClapperGtkExtraMenuButton *button, gboolean visible);

CLAPPER_GTK_API
gboolean clapper_gtk_extra_menu_button_get_speed_visible (ClapperGtkExtraMenuButton *button);

CLAPPER_GTK_API
void clapper_gtk_extra_menu_button_set_can_open_subtitles (ClapperGtkExtraMenuButton *button, gboolean allowed);

CLAPPER_GTK_API
gboolean clapper_gtk_extra_menu_button_get_can_open_subtitles (ClapperGtkExtraMenuButton *button);

G_END_DECLS
