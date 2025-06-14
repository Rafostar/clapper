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

#pragma once

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <gst/gst.h>

G_BEGIN_DECLS

/**
 * CLAPPER_TIME_FORMAT: (skip):
 *
 * A string that can be used in printf-like format to display
 * e.g. position or duration in `hh:mm:ss` format. Meant to be
 * used together with [func@Clapper.TIME_ARGS].
 *
 * Example:
 *
 * ```c
 * gchar *str = g_strdup_printf ("%" CLAPPER_TIME_FORMAT, CLAPPER_TIME_ARGS (time));
 * ```
 */
#define CLAPPER_TIME_FORMAT "02u:%02u:%02u"

/**
 * CLAPPER_TIME_ARGS: (skip):
 * @t: time value in seconds
 *
 * Formats @t for the [const@Clapper.TIME_FORMAT] format string.
 */
#define CLAPPER_TIME_ARGS(t) \
    (guint) (((GstClockTime)(t)) / 3600),      \
    (guint) ((((GstClockTime)(t)) / 60) % 60), \
    (guint) (((GstClockTime)(t)) % 60)

/**
 * CLAPPER_TIME_MS_FORMAT: (skip):
 *
 * Same as [const@Clapper.TIME_FORMAT], but also displays milliseconds.
 * Meant to be used together with [func@Clapper.TIME_MS_ARGS].
 *
 * Example:
 *
 * ```c
 * gchar *str = g_strdup_printf ("%" CLAPPER_TIME_MS_FORMAT, CLAPPER_TIME_MS_ARGS (time));
 * ```
 */
#define CLAPPER_TIME_MS_FORMAT "02u:%02u:%02u.%03u"

/**
 * CLAPPER_TIME_MS_ARGS: (skip):
 * @t: time value in seconds
 *
 * Formats @t for the [const@Clapper.TIME_MS_FORMAT] format string.
 */
#define CLAPPER_TIME_MS_ARGS(t) \
    CLAPPER_TIME_ARGS(t),       \
    (guint) (((GstClockTime)(t * 1000)) % 1000)

G_END_DECLS
