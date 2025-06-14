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
#include <clapper-gtk/clapper-gtk-container.h>
#include <clapper-gtk/clapper-gtk-extra-menu-button.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_SIMPLE_CONTROLS (clapper_gtk_simple_controls_get_type())
#define CLAPPER_GTK_SIMPLE_CONTROLS_CAST(obj) ((ClapperGtkSimpleControls *)(obj))

CLAPPER_GTK_API
G_DECLARE_FINAL_TYPE (ClapperGtkSimpleControls, clapper_gtk_simple_controls, CLAPPER_GTK, SIMPLE_CONTROLS, ClapperGtkContainer)

CLAPPER_GTK_API
GtkWidget * clapper_gtk_simple_controls_new (void);

CLAPPER_GTK_API
void clapper_gtk_simple_controls_set_fullscreenable (ClapperGtkSimpleControls *controls, gboolean fullscreenable);

CLAPPER_GTK_API
gboolean clapper_gtk_simple_controls_get_fullscreenable (ClapperGtkSimpleControls *controls);

CLAPPER_GTK_API
void clapper_gtk_simple_controls_set_seek_method (ClapperGtkSimpleControls *controls, ClapperPlayerSeekMethod method);

CLAPPER_GTK_API
ClapperPlayerSeekMethod clapper_gtk_simple_controls_get_seek_method (ClapperGtkSimpleControls *controls);

CLAPPER_GTK_API
ClapperGtkExtraMenuButton * clapper_gtk_simple_controls_get_extra_menu_button (ClapperGtkSimpleControls *controls);

G_END_DECLS
