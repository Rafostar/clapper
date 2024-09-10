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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <clapper/clapper.h>

#include "clapper-tube-internal-visibility.h"
#include "clapper-tube-harvest.h"

G_BEGIN_DECLS

#define CLAPPER_TUBE_TYPE_CLIENT (clapper_tube_client_get_type())
#define CLAPPER_TUBE_CLIENT_CAST(obj) ((ClapperTubeClient *)(obj))

CLAPPER_TUBE_INTERNAL_API
G_DECLARE_FINAL_TYPE (ClapperTubeClient, clapper_tube_client, CLAPPER_TUBE, CLIENT, ClapperThreadedObject)

CLAPPER_TUBE_INTERNAL_API
ClapperTubeHarvest * clapper_tube_client_run (ClapperTubeClient *client, GUri *uri, GCancellable *cancellable, GError **error);

CLAPPER_TUBE_INTERNAL_API
void clapper_tube_client_stop (ClapperTubeClient *client);

G_END_DECLS
