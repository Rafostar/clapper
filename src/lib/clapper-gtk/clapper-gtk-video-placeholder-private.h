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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "clapper-gtk-container.h"

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_VIDEO_PLACEHOLDER (clapper_gtk_video_placeholder_get_type())
#define CLAPPER_GTK_VIDEO_PLACEHOLDER_CAST(obj) ((ClapperGtkVideoPlaceholder *)(obj))

G_DECLARE_FINAL_TYPE (ClapperGtkVideoPlaceholder, clapper_gtk_video_placeholder, CLAPPER_GTK, VIDEO_PLACEHOLDER, ClapperGtkContainer)

G_GNUC_INTERNAL
GtkWidget * clapper_gtk_video_placeholder_new (void);

G_END_DECLS
