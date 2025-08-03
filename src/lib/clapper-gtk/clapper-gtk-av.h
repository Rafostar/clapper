/* Clapper GTK Integration Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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
#include <clapper/clapper.h>

#include <clapper-gtk/clapper-gtk-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_AV (clapper_gtk_av_get_type())
#define CLAPPER_GTK_AV_CAST(obj) ((ClapperGtkAv *)(obj))

CLAPPER_GTK_API
G_DECLARE_DERIVABLE_TYPE (ClapperGtkAv, clapper_gtk_av, CLAPPER_GTK, AV, GtkWidget)

struct _ClapperGtkAvClass
{
  GtkWidgetClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

CLAPPER_GTK_API
ClapperPlayer * clapper_gtk_av_get_player (ClapperGtkAv *av);

CLAPPER_GTK_API
void clapper_gtk_av_set_auto_inhibit (ClapperGtkAv *av, gboolean inhibit);

CLAPPER_GTK_API
gboolean clapper_gtk_av_get_auto_inhibit (ClapperGtkAv *av);

CLAPPER_GTK_API
gboolean clapper_gtk_av_get_inhibited (ClapperGtkAv *av);

G_END_DECLS
