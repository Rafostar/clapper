/* ClapperTube Library
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
#include <gmodule.h>

static gboolean
plugin_init (GstPlugin *plugin)
{
  gboolean res = FALSE;

  if (!g_module_supported ())
    return FALSE;

  gst_plugin_add_dependency_simple (plugin,
      "CLAPPER_TUBE_PLUGIN_PATH", CLAPPER_TUBE_PLUGIN_PATH, NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_PATHS_ARE_DEFAULT_ONLY);

  //res |= GST_ELEMENT_REGISTER (clappertubesrc, plugin);
  //res |= GST_ELEMENT_REGISTER (gtuberuridemux, plugin);

  return res;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    clappertube, PLUGIN_DESC, plugin_init,
    PACKAGE_VERSION, PLUGIN_LICENSE, PACKAGE, PACKAGE_ORIGIN)
