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
#include <clapper-gtk/clapper-gtk-enums.h>
#include <clapper-gtk/clapper-gtk-container.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_LEAD_CONTAINER (clapper_gtk_lead_container_get_type())
#define CLAPPER_GTK_LEAD_CONTAINER_CAST(obj) ((ClapperGtkLeadContainer *)(obj))

CLAPPER_GTK_API
G_DECLARE_DERIVABLE_TYPE (ClapperGtkLeadContainer, clapper_gtk_lead_container, CLAPPER_GTK, LEAD_CONTAINER, ClapperGtkContainer)

struct _ClapperGtkLeadContainerClass
{
  ClapperGtkContainerClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

CLAPPER_GTK_API
GtkWidget * clapper_gtk_lead_container_new (void);

CLAPPER_GTK_API
void clapper_gtk_lead_container_set_leading (ClapperGtkLeadContainer *lead_container, gboolean leading);

CLAPPER_GTK_API
gboolean clapper_gtk_lead_container_get_leading (ClapperGtkLeadContainer *lead_container);

CLAPPER_GTK_API
void clapper_gtk_lead_container_set_blocked_actions (ClapperGtkLeadContainer *lead_container, ClapperGtkVideoActionMask actions);

CLAPPER_GTK_API
ClapperGtkVideoActionMask clapper_gtk_lead_container_get_blocked_actions (ClapperGtkLeadContainer *lead_container);

G_END_DECLS
