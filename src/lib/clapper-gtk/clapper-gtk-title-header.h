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

#pragma once

#if !defined(__CLAPPER_GTK_INSIDE__) && !defined(CLAPPER_GTK_COMPILATION)
#error "Only <clapper-gtk/clapper-gtk.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <clapper-gtk/clapper-gtk-lead-container.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_TITLE_HEADER (clapper_gtk_title_header_get_type())
#define CLAPPER_GTK_TITLE_HEADER_CAST(obj) ((ClapperGtkTitleHeader *)(obj))

G_DECLARE_FINAL_TYPE (ClapperGtkTitleHeader, clapper_gtk_title_header, CLAPPER_GTK, TITLE_HEADER, ClapperGtkLeadContainer)

GtkWidget * clapper_gtk_title_header_new (void);

const gchar * clapper_gtk_title_header_get_current_title (ClapperGtkTitleHeader *header);

void clapper_gtk_title_header_set_fallback_to_uri (ClapperGtkTitleHeader *header, gboolean enabled);

gboolean clapper_gtk_title_header_get_fallback_to_uri (ClapperGtkTitleHeader *header);

G_END_DECLS
