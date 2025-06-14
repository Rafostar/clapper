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

#include "clapper-gtk-container.h"

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_STATUS (clapper_gtk_status_get_type())
#define CLAPPER_GTK_STATUS_CAST(obj) ((ClapperGtkStatus *)(obj))

G_DECLARE_FINAL_TYPE (ClapperGtkStatus, clapper_gtk_status, CLAPPER_GTK, STATUS, ClapperGtkContainer)

G_GNUC_INTERNAL
void clapper_gtk_status_set_error (ClapperGtkStatus *status, const GError *error);

G_GNUC_INTERNAL
void clapper_gtk_status_set_missing_plugin (ClapperGtkStatus *status, const gchar *name);

G_GNUC_INTERNAL
void clapper_gtk_status_clear (ClapperGtkStatus *status);

G_END_DECLS
