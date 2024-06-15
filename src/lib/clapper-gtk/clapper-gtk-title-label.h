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
#include <clapper/clapper.h>

#include <clapper-gtk/clapper-gtk-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_TITLE_LABEL (clapper_gtk_title_label_get_type())
#define CLAPPER_GTK_TITLE_LABEL_CAST(obj) ((ClapperGtkTitleLabel *)(obj))

CLAPPER_GTK_API
G_DECLARE_FINAL_TYPE (ClapperGtkTitleLabel, clapper_gtk_title_label, CLAPPER_GTK, TITLE_LABEL, GtkWidget)

CLAPPER_GTK_API
GtkWidget * clapper_gtk_title_label_new (void);

CLAPPER_GTK_API
void clapper_gtk_title_label_set_media_item (ClapperGtkTitleLabel *label, ClapperMediaItem *item);

CLAPPER_GTK_API
ClapperMediaItem * clapper_gtk_title_label_get_media_item (ClapperGtkTitleLabel *label);

CLAPPER_GTK_API
const gchar * clapper_gtk_title_label_get_current_title (ClapperGtkTitleLabel *label);

CLAPPER_GTK_API
void clapper_gtk_title_label_set_fallback_to_uri (ClapperGtkTitleLabel *label, gboolean enabled);

CLAPPER_GTK_API
gboolean clapper_gtk_title_label_get_fallback_to_uri (ClapperGtkTitleLabel *label);

G_END_DECLS
