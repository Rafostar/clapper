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

#define CLAPPER_TYPE_DISCOVERER (clapper_discoverer_get_type())
#define CLAPPER_DISCOVERER_CAST(obj) ((ClapperDiscoverer *)(obj))

CLAPPER_DEPRECATED
G_DECLARE_FINAL_TYPE (ClapperDiscoverer, clapper_discoverer, CLAPPER, DISCOVERER, ClapperFeature)

CLAPPER_DEPRECATED
ClapperDiscoverer * clapper_discoverer_new (void);

CLAPPER_DEPRECATED
void clapper_discoverer_set_discovery_mode (ClapperDiscoverer *discoverer, ClapperDiscovererDiscoveryMode mode);

CLAPPER_DEPRECATED
ClapperDiscovererDiscoveryMode clapper_discoverer_get_discovery_mode (ClapperDiscoverer *discoverer);

G_END_DECLS
