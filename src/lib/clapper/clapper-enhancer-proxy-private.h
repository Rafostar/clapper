/* Clapper Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "clapper-enhancer-proxy.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
ClapperEnhancerProxy * clapper_enhancer_proxy_new_global_take (GObject *peas_info); // Using parent type for building without libpeas

G_GNUC_INTERNAL
ClapperEnhancerProxy * clapper_enhancer_proxy_copy (ClapperEnhancerProxy *src_proxy, const gchar *copy_name);

G_GNUC_INTERNAL
gboolean clapper_enhancer_proxy_fill_from_cache (ClapperEnhancerProxy *proxy);

G_GNUC_INTERNAL
gboolean clapper_enhancer_proxy_fill_from_instance (ClapperEnhancerProxy *proxy, GObject *enhancer);

G_GNUC_INTERNAL
GObject * clapper_enhancer_proxy_get_peas_info (ClapperEnhancerProxy *proxy);

G_GNUC_INTERNAL
void clapper_enhancer_proxy_apply_current_config_to_enhancer (ClapperEnhancerProxy *proxy, GObject *enhancer);

G_END_DECLS
