/*
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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-feature.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_SERVER (clapper_server_get_type())
#define CLAPPER_SERVER_CAST(obj) ((ClapperServer *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperServer, clapper_server, CLAPPER, SERVER, ClapperFeature)

CLAPPER_API
ClapperServer * clapper_server_new (void);

CLAPPER_API
void clapper_server_set_enabled (ClapperServer *server, gboolean enabled);

CLAPPER_API
gboolean clapper_server_get_enabled (ClapperServer *server);

CLAPPER_API
gboolean clapper_server_get_running (ClapperServer *server);

CLAPPER_API
void clapper_server_set_port (ClapperServer *server, guint port);

CLAPPER_API
guint clapper_server_get_port (ClapperServer *server);

CLAPPER_API
guint clapper_server_get_current_port (ClapperServer *server);

CLAPPER_API
void clapper_server_set_queue_controllable (ClapperServer *server, gboolean controllable);

CLAPPER_API
gboolean clapper_server_get_queue_controllable (ClapperServer *server);

G_END_DECLS
