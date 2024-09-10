/* Clapper Tube Library
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

#include <clapper-tube/clapper-tube.h>

#define __CLAPPER_TUBE_INTERNAL_INSIDE__

#include "clapper-tube/clapper-tube-internal-visibility.h"
#include "clapper-tube/clapper-tube-client-private.h"

G_BEGIN_DECLS

CLAPPER_TUBE_INTERNAL_API
gboolean clapper_tube_init_check (void);

CLAPPER_TUBE_INTERNAL_API
const gchar *const * clapper_tube_get_supported_schemes (void);

G_END_DECLS

#undef __CLAPPER_TUBE_INTERNAL_INSIDE__
