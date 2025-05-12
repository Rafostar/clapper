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
#include "clapper-cache-private.h"
#include "clapper-utils-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-app-bus-private.h"
#include "clapper-features-bus-private.h"
#include "clapper-enhancer-proxy-list-private.h"
#include "gst/clapper-plugin-private.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include "clapper-enhancers-loader-private.h"
#endif

static ClapperEnhancerProxyList *_proxies = NULL;
static gboolean is_initialized = FALSE;
static GMutex init_lock;

static gboolean
clapper_init_check_internal (int *argc, char **argv[])
{
  g_mutex_lock (&init_lock);

  if (is_initialized || !gst_init_check (argc, argv, NULL))
    goto finish;

  gst_pb_utils_init ();

  clapper_cache_initialize ();
  clapper_utils_initialize ();
  clapper_playbin_bus_initialize ();
  clapper_app_bus_initialize ();
  clapper_features_bus_initialize ();

  _proxies = clapper_enhancer_proxy_list_new_named ("global-proxy-list");

#if CLAPPER_WITH_ENHANCERS_LOADER
  clapper_enhancers_loader_initialize (_proxies);
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
 *
 * Deprecated: 0.10: Use list of enhancer proxies from [func@Clapper.get_global_enhancer_proxies] or
 *   [property@Clapper.Player:enhancer-proxies] and check if any proxy matches your search criteria.
 */
gboolean
clapper_enhancer_check (GType iface_type, const gchar *scheme, const gchar *host, const gchar **name)
{
  gboolean is_https;
  guint i, n_proxies;

  g_return_val_if_fail (G_TYPE_IS_INTERFACE (iface_type), FALSE);
  g_return_val_if_fail (scheme != NULL, FALSE);

  if (host) {
    /* Strip common subdomains, so plugins do not
     * have to list all combinations */
    if (g_str_has_prefix (host, "www."))
      host += 4;
    else if (g_str_has_prefix (host, "m."))
      host += 2;
  }

  /* Whether "http(s)" scheme is used */
  is_https = (g_str_has_prefix (scheme, "http")
      && (scheme[4] == '\0' || (scheme[4] == 's' && scheme[5] == '\0')));

  if (!host && is_https)
    return FALSE;

  n_proxies = clapper_enhancer_proxy_list_get_n_proxies (_proxies);
  for (i = 0; i < n_proxies; ++i) {
    ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (_proxies, i);

    if (clapper_enhancer_proxy_target_has_interface (proxy, iface_type)
        && clapper_enhancer_proxy_extra_data_lists_value (proxy, "X-Schemes", scheme)
        && (!is_https || clapper_enhancer_proxy_extra_data_lists_value (proxy, "X-Hosts", host))) {
      if (name)
        *name = clapper_enhancer_proxy_get_friendly_name (proxy);

      return TRUE;
    }
  }

  return FALSE;
}

/**
 * clapper_get_global_enhancer_proxies:
 *
 * Get a list of available enhancers in the form of [class@Clapper.EnhancerProxy] objects.
 *
 * This returns a global list of enhancer proxy objects. You can use it to inspect
 * available enhancers without creating a new player instance.
 *
 * Remember to initialize Clapper library before using this function.
 *
 * Only enhancer properties with [flags@Clapper.EnhancerParamFlags.GLOBAL] flag can be
 * set on proxies in this list. These are meant to be set ONLY by users, not applications
 * as they carry over to all player instances (possibly including other apps). Applications
 * should instead be changing properties with [flags@Clapper.EnhancerParamFlags.LOCAL] flag
 * set from individual proxy lists from [property@Clapper.Player:enhancer-proxies] which
 * will affect only that single player instance given list belongs to.
 *
 * Returns: (transfer none): a global #ClapperEnhancerProxyList of enhancer proxies.
 *
 * Since: 0.10
 */
ClapperEnhancerProxyList *
clapper_get_global_enhancer_proxies (void)
{
  return _proxies;
}
