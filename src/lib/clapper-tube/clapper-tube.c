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

#include <gmodule.h>
#include <gst/gst.h>

#include "clapper-tube-private.h"
#include "clapper-tube-cache-private.h"
#include "clapper-tube-config-private.h"
#include "clapper-tube-loader-private.h"

#define GST_CAT_DEFAULT clapper_tube_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GMutex init_lock;
static gboolean init_done = FALSE; // init was run
static gboolean is_initialized = FALSE; // init was successful
static gchar **supported_schemes = NULL;

gboolean
clapper_tube_init_check (void)
{
  g_mutex_lock (&init_lock);

  if (!init_done) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertube", 0,
        "Clapper Tube");

    if (!g_module_supported ()) {
      GST_ERROR ("Dynamic loading of modules is not supported on this system");
    } else {
      GError *error = NULL;

      clapper_tube_config_init_internal ();
      clapper_tube_loader_init_internal ();

      if ((is_initialized = clapper_tube_cache_init (&error))) {
        supported_schemes = clapper_tube_cache_create_supported_schemes ();
      } else {
        GST_ERROR ("Could not initialize Clapper Tube, reason: %s",
            (error && error->message) ? error->message : "Unknown");
        g_error_free (error);
      }
    }

    init_done = TRUE;
  }

  g_mutex_unlock (&init_lock);

  return is_initialized;
}

const gchar *const *
clapper_tube_get_supported_schemes (void)
{
  return (const gchar *const *) supported_schemes;
}

gboolean
clapper_tube_has_extractor_for_uri (const gchar *uri)
{
  ClapperTubeExtractor *extractor;
  GUri *guri;
  gboolean found = FALSE;

  GST_DEBUG ("Checking URI support: %s", uri);

  if (!(guri = g_uri_parse (uri, G_URI_FLAGS_ENCODED, NULL))) {
    GST_DEBUG ("URI is invalid");
    return FALSE;
  }

  if ((extractor = clapper_tube_loader_get_extractor_for_uri (guri))) {
    gst_object_unref (extractor);
    found = TRUE;
  }
  GST_DEBUG ("URI supported: %s", (found) ? "yes" : "no");

  return found;
}
