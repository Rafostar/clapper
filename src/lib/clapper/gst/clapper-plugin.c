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

#include "clapper-plugin-private.h"
#include "../clapper-functionalities-availability.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include "clapper-enhancer-src-private.h"
#include "../clapper-extractable-private.h"
#include "../clapper-enhancers-loader-private.h"
#endif

#include "clapper-uri-list-demux-private.h"

gboolean
clapper_gst_plugin_init (GstPlugin *plugin)
{
  gboolean res = FALSE;

#if CLAPPER_WITH_ENHANCERS_LOADER
  gst_plugin_add_dependency_simple (plugin,
      "CLAPPER_ENHANCERS_PATH", CLAPPER_ENHANCERS_PATH, NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_PATHS_ARE_DEFAULT_ONLY);

  /* Avoid registering an URI handler without schemes */
  if (clapper_enhancers_loader_has_enhancers (CLAPPER_TYPE_EXTRACTABLE))
    res |= GST_ELEMENT_REGISTER (clapperenhancersrc, plugin);
#endif

  res |= GST_ELEMENT_REGISTER (clapperurilistdemux, plugin);

  return res;
}
