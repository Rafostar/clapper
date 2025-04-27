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

#include "config.h"

#include <gst/gst.h>

#include "../clapper.h"
#include "clapper-enhancer-src-private.h"

#include "clapper-plugin-private.h"
#include "clapper-uri-list-demux-private.h"

/*
 * clapper_gst_plugin_has_enhancers:
 * @iface_type: an interface #GType
 *
 * Check if any enhancer implementing given interface type is available.
 *
 * Returns: whether any enhancer was found.
 */
static gboolean
clapper_gst_plugin_has_enhancers (ClapperEnhancerProxyList *proxies, GType iface_type)
{
  guint i, n_proxies = clapper_enhancer_proxy_list_get_n_proxies (proxies);

  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (proxies, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, iface_type))
      return TRUE;
  }

  return FALSE;
}

gboolean
clapper_gst_plugin_init (GstPlugin *plugin)
{
  gboolean res = FALSE;
  ClapperEnhancerProxyList *global_proxies;

  gst_plugin_add_dependency_simple (plugin,
      "CLAPPER_ENHANCERS_PATH", CLAPPER_ENHANCERS_PATH, NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_PATHS_ARE_DEFAULT_ONLY);

  global_proxies = clapper_get_global_enhancer_proxies ();

  /* Avoid registering an URI handler without schemes */
  if (clapper_gst_plugin_has_enhancers (global_proxies, CLAPPER_TYPE_EXTRACTABLE))
    res |= GST_ELEMENT_REGISTER (clapperenhancersrc, plugin);

  res |= GST_ELEMENT_REGISTER (clapperurilistdemux, plugin);

  return res;
}
