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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "clapper.h"
#include "clapper-utils-private.h"
#include "clapper-playbin-bus-private.h"
#include "clapper-app-bus-private.h"
#include "clapper-features-bus-private.h"

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
 * terminating on failure it returns %FALSE with @error set.
 *
 * Returns: %TRUE if Clapper could be initialized, %FALSE otherwise.
 */
gboolean
clapper_init_check (int *argc, char **argv[])
{
  return clapper_init_check_internal (argc, argv);
}
