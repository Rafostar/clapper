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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gst/gst.h>

#include "../clapper-basic-functions.h"
#include "../clapper-enhancer-proxy.h"
#include "../clapper-enhancer-proxy-list-private.h"
#include "../clapper-extractable.h"
#include "../clapper-playlistable.h"

#include "clapper-plugin-private.h"
#include "clapper-extractable-src-private.h"
#include "clapper-playlist-demux-private.h"
#include "clapper-uri-list-demux-private.h"

gboolean
clapper_gst_plugin_init (GstPlugin *plugin)
{
  gboolean res = TRUE;
  ClapperEnhancerProxyList *global_proxies;

  gst_plugin_add_dependency_simple (plugin,
      "CLAPPER_ENHANCERS_PATH", CLAPPER_ENHANCERS_PATH, NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_PATHS_ARE_DEFAULT_ONLY);

  global_proxies = clapper_get_global_enhancer_proxies ();

  /* Avoid registering an URI handler without schemes */
  if (clapper_enhancer_proxy_list_has_proxy_with_interface (global_proxies, CLAPPER_TYPE_EXTRACTABLE)) {
    res &= (GST_ELEMENT_REGISTER (clapperextractablesrc, plugin)
        && GST_ELEMENT_REGISTER (clapperurilistdemux, plugin));
  }

  /* Type find will only register if there are playlistable enhancers */
  if (GST_TYPE_FIND_REGISTER (clapperplaylistdemux, plugin))
    GST_ELEMENT_REGISTER (clapperplaylistdemux, plugin);

  return res;
}
