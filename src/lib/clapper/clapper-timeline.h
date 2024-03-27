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

#include <clapper/clapper-marker.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_TIMELINE (clapper_timeline_get_type())
#define CLAPPER_TIMELINE_CAST(obj) ((ClapperTimeline *)(obj))

G_DECLARE_FINAL_TYPE (ClapperTimeline, clapper_timeline, CLAPPER, TIMELINE, GstObject)

gboolean clapper_timeline_insert_marker (ClapperTimeline *timeline, ClapperMarker *marker);

void clapper_timeline_remove_marker (ClapperTimeline *timeline, ClapperMarker *marker);

ClapperMarker * clapper_timeline_get_marker (ClapperTimeline *timeline, guint index);

guint clapper_timeline_get_n_markers (ClapperTimeline *timeline);

G_END_DECLS
