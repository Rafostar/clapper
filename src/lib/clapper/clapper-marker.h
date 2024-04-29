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

#pragma once

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_MARKER (clapper_marker_get_type())
#define CLAPPER_MARKER_CAST(obj) ((ClapperMarker *)(obj))

/* NOTE: #ClapperMarker are immutable objects that cannot be derived,
 * otherwise #ClapperFeaturesManager would not be able to announce media
 * item changed caused by changes within them */
G_DECLARE_FINAL_TYPE (ClapperMarker, clapper_marker, CLAPPER, MARKER, GstObject)

/**
 * CLAPPER_MARKER_NO_END:
 *
 * The value used to indicate that marker does not have an ending time specified
 */
#define CLAPPER_MARKER_NO_END ((gdouble) -1) // Needs a cast from int, otherwise GIR is generated incorrectly

ClapperMarker * clapper_marker_new (ClapperMarkerType marker_type, const gchar *title, gdouble start, gdouble end);

ClapperMarkerType clapper_marker_get_marker_type (ClapperMarker *marker);

const gchar * clapper_marker_get_title (ClapperMarker *marker);

gdouble clapper_marker_get_start (ClapperMarker *marker);

gdouble clapper_marker_get_end (ClapperMarker *marker);

G_END_DECLS
