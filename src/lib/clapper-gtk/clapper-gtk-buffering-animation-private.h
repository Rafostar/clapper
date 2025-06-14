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

#define CLAPPER_GTK_TYPE_BUFFERING_ANIMATION (clapper_gtk_buffering_animation_get_type())
#define CLAPPER_GTK_BUFFERING_ANIMATION_CAST(obj) ((ClapperGtkBufferingAnimation *)(obj))

G_DECLARE_FINAL_TYPE (ClapperGtkBufferingAnimation, clapper_gtk_buffering_animation, CLAPPER_GTK, BUFFERING_ANIMATION, ClapperGtkContainer)

G_GNUC_INTERNAL
void clapper_gtk_buffering_animation_start (ClapperGtkBufferingAnimation *animation);

G_GNUC_INTERNAL
void clapper_gtk_buffering_animation_stop (ClapperGtkBufferingAnimation *animation);

G_END_DECLS
