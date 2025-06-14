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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_LIMITED_LAYOUT (clapper_gtk_limited_layout_get_type())
#define CLAPPER_GTK_LIMITED_LAYOUT_CAST(obj) ((ClapperGtkLimitedLayout *)(obj))

G_DECLARE_FINAL_TYPE (ClapperGtkLimitedLayout, clapper_gtk_limited_layout, CLAPPER_GTK, LIMITED_LAYOUT, GtkLayoutManager)

G_GNUC_INTERNAL
void clapper_gtk_limited_layout_set_max_width (ClapperGtkLimitedLayout *layout, gint max_width);

G_GNUC_INTERNAL
gint clapper_gtk_limited_layout_get_max_width (ClapperGtkLimitedLayout *layout);

G_GNUC_INTERNAL
void clapper_gtk_limited_layout_set_max_height (ClapperGtkLimitedLayout *layout, gint max_height);

G_GNUC_INTERNAL
gint clapper_gtk_limited_layout_get_max_height (ClapperGtkLimitedLayout *layout);

G_GNUC_INTERNAL
void clapper_gtk_limited_layout_set_adaptive_width (ClapperGtkLimitedLayout *layout, gint width);

G_GNUC_INTERNAL
gint clapper_gtk_limited_layout_get_adaptive_width (ClapperGtkLimitedLayout *layout);

G_GNUC_INTERNAL
void clapper_gtk_limited_layout_set_adaptive_height (ClapperGtkLimitedLayout *layout, gint height);

G_GNUC_INTERNAL
gint clapper_gtk_limited_layout_get_adaptive_height (ClapperGtkLimitedLayout *layout);

G_END_DECLS
