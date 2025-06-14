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

#define CLAPPER_GTK_TYPE_STREAM_CHECK_BUTTON (clapper_gtk_stream_check_button_get_type())
#define CLAPPER_GTK_STREAM_CHECK_BUTTON_CAST(obj) ((ClapperGtkStreamCheckButton *)(obj))

G_DECLARE_FINAL_TYPE (ClapperGtkStreamCheckButton, clapper_gtk_stream_check_button, CLAPPER_GTK, STREAM_CHECK_BUTTON, GtkCheckButton)

G_END_DECLS
