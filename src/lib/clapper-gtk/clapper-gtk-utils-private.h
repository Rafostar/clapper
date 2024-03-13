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

#include "clapper-gtk-utils.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void clapper_gtk_init_translations (void);

G_GNUC_INTERNAL
const gchar * clapper_gtk_get_icon_name_for_volume (gfloat volume);

G_GNUC_INTERNAL
const gchar * clapper_gtk_get_icon_name_for_speed (gfloat speed);

G_END_DECLS
