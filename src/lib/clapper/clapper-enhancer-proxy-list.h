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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-enhancer-proxy.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_ENHANCER_PROXY_LIST (clapper_enhancer_proxy_list_get_type())
#define CLAPPER_ENHANCER_PROXY_LIST_CAST(obj) ((ClapperEnhancerProxyList *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperEnhancerProxyList, clapper_enhancer_proxy_list, CLAPPER, ENHANCER_PROXY_LIST, GstObject)

CLAPPER_API
ClapperEnhancerProxy * clapper_enhancer_proxy_list_get_proxy (ClapperEnhancerProxyList *list, guint index);

CLAPPER_API
ClapperEnhancerProxy * clapper_enhancer_proxy_list_peek_proxy (ClapperEnhancerProxyList *list, guint index);

CLAPPER_API
ClapperEnhancerProxy * clapper_enhancer_proxy_list_get_proxy_by_module (ClapperEnhancerProxyList *list, const gchar *module_name);

CLAPPER_API
guint clapper_enhancer_proxy_list_get_n_proxies (ClapperEnhancerProxyList *list);

G_END_DECLS
