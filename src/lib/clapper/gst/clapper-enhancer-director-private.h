/* Clapper Playback Library
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
#include <gio/gio.h>

#include "../clapper-threaded-object.h"
#include "../clapper-harvest.h"

G_BEGIN_DECLS

#define CLAPPER_TYPE_ENHANCER_DIRECTOR (clapper_enhancer_director_get_type())
#define CLAPPER_ENHANCER_DIRECTOR_CAST(obj) ((ClapperEnhancerDirector *)(obj))

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (ClapperEnhancerDirector, clapper_enhancer_director, CLAPPER, ENHANCER_DIRECTOR, ClapperThreadedObject)

G_GNUC_INTERNAL
ClapperEnhancerDirector * clapper_enhancer_director_new (void);

G_GNUC_INTERNAL
ClapperHarvest * clapper_enhancer_director_extract (ClapperEnhancerDirector *director, GUri *uri, GCancellable *cancellable, GError **error);

G_END_DECLS
