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

#include <clapper-gtk/clapper-gtk-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_CONTAINER (clapper_gtk_container_get_type())
#define CLAPPER_GTK_CONTAINER_CAST(obj) ((ClapperGtkContainer *)(obj))

CLAPPER_GTK_API
G_DECLARE_DERIVABLE_TYPE (ClapperGtkContainer, clapper_gtk_container, CLAPPER_GTK, CONTAINER, GtkWidget)

struct _ClapperGtkContainerClass
{
  GtkWidgetClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

CLAPPER_GTK_API
GtkWidget * clapper_gtk_container_new (void);

CLAPPER_GTK_API
void clapper_gtk_container_set_child (ClapperGtkContainer *container, GtkWidget *child);

CLAPPER_GTK_API
GtkWidget * clapper_gtk_container_get_child (ClapperGtkContainer *container);

CLAPPER_GTK_API
void clapper_gtk_container_set_width_target (ClapperGtkContainer *container, gint width);

CLAPPER_GTK_API
gint clapper_gtk_container_get_width_target (ClapperGtkContainer *container);

CLAPPER_GTK_API
void clapper_gtk_container_set_height_target (ClapperGtkContainer *container, gint height);

CLAPPER_GTK_API
gint clapper_gtk_container_get_height_target (ClapperGtkContainer *container);

CLAPPER_GTK_API
void clapper_gtk_container_set_adaptive_width (ClapperGtkContainer *container, gint width);

CLAPPER_GTK_API
gint clapper_gtk_container_get_adaptive_width (ClapperGtkContainer *container);

CLAPPER_GTK_API
void clapper_gtk_container_set_adaptive_height (ClapperGtkContainer *container, gint height);

CLAPPER_GTK_API
gint clapper_gtk_container_get_adaptive_height (ClapperGtkContainer *container);

G_END_DECLS
