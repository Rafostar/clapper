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
#include <gst/pbutils/pbutils.h>

#include "clapper.h"
#include "clapper-utils-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-app-bus-private.h"
#include "clapper-features-bus-private.h"
#include "gst/clapper-plugin-private.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include "clapper-enhancers-loader-private.h"
#endif

static gboolean is_initialized = FALSE;
static GMutex init_lock;

static gboolean
clapper_init_check_internal (int *argc, char **argv[])
{
  g_mutex_lock (&init_lock);

  if (is_initialized || !gst_init_check (argc, argv, NULL))
    goto finish;

  gst_pb_utils_init ();

  clapper_utils_initialize ();
  clapper_playbin_bus_initialize ();
  clapper_app_bus_initialize ();
  clapper_features_bus_initialize ();

#if CLAPPER_WITH_ENHANCERS_LOADER
  clapper_enhancers_loader_initialize ();
#endif

  gst_plugin_register_static (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      PACKAGE "internal",
      PLUGIN_DESC,
      (GstPluginInitFunc) clapper_gst_plugin_init,
      PACKAGE_VERSION,
      PLUGIN_LICENSE,
      PACKAGE,
      PACKAGE,
      PACKAGE_ORIGIN);

  is_initialized = TRUE;

finish:
  g_mutex_unlock (&init_lock);

  return is_initialized;
}

/**
 * clapper_init:
 * @argc: (inout) (nullable) (optional): pointer to application's argc
 * @argv: (inout) (array length=argc) (nullable) (optional): pointer to application's argv
 *
 * Initializes the Clapper library. Implementations must always call this
 * before using Clapper API.
 *
 * Because Clapper uses GStreamer internally, this function will also initialize
 * GStreamer before initializing Clapper itself for user convienience, so
 * application does not have to do so anymore.
 *
 * WARNING: This function will terminate your program if it was unable to
 * initialize for some reason. If you want to do some fallback logic,
 * use [func@Clapper.init_check] instead.
 */
void
clapper_init (int *argc, char **argv[])
{
  if (!clapper_init_check_internal (argc, argv)) {
    g_printerr ("Could not initialize Clapper library\n");
    exit (1);
  }
}

/**
 * clapper_init_check:
 * @argc: (inout) (nullable) (optional): pointer to application's argc
 * @argv: (inout) (array length=argc) (nullable) (optional): pointer to application's argv
 *
 * This function does the same thing as [func@Clapper.init], but instead of
 * terminating on failure it returns %FALSE.
 *
 * Returns: %TRUE if Clapper could be initialized, %FALSE otherwise.
 */
gboolean
clapper_init_check (int *argc, char **argv[])
{
  return clapper_init_check_internal (argc, argv);
}

/**
 * clapper_enhancer_check:
 * @iface_type: an interface #GType
 * @scheme: an URI scheme
 * @host: (nullable): an URI host
 * @name: (out) (optional) (transfer none): return location for found enhancer name
 *
 * Check if an enhancer of @type is available for given @scheme and @host.
 *
 * A check that compares requested capabilites of all available Clapper enhancers,
 * thus it is fast but does not guarantee that the found one will succeed. Please note
 * that this function will always return %FALSE if Clapper was built without enhancers
 * loader functionality. To check that, use [const@Clapper.WITH_ENHANCERS_LOADER].
 *
 * This function can be used to quickly determine early if Clapper will at least try to
 * handle URI and with one of its enhancers and which one.
 *
 * Example:
 *
 * ```c
 * gboolean supported = clapper_enhancer_check (CLAPPER_TYPE_EXTRACTABLE, "https", "example.com", NULL);
 * ```
 *
 * For self hosted services a custom URI @scheme without @host can be used. Enhancers should announce
 * support for such schemes by defining them in their plugin info files.
 *
 * ```c
 * gboolean supported = clapper_enhancer_check (CLAPPER_TYPE_EXTRACTABLE, "example", NULL, NULL);
 * ```
 *
 * Returns: whether a plausible enhancer was found.
 *
 * Since: 0.8
 */
gboolean
clapper_enhancer_check (GType iface_type, const gchar *scheme, const gchar *host, const gchar **name)
{
  gboolean success = FALSE;

  g_return_val_if_fail (G_TYPE_IS_INTERFACE (iface_type), FALSE);
  g_return_val_if_fail (scheme != NULL, FALSE);

#if CLAPPER_WITH_ENHANCERS_LOADER
  success = clapper_enhancers_loader_check (iface_type, scheme, host, name);
#endif

  return success;
}
