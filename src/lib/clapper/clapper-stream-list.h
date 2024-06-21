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

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-stream.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_STREAM_LIST (clapper_stream_list_get_type())
#define CLAPPER_STREAM_LIST_CAST(obj) ((ClapperStreamList *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperStreamList, clapper_stream_list, CLAPPER, STREAM_LIST, GstObject)

/**
 * CLAPPER_STREAM_LIST_INVALID_POSITION:
 *
 * The value used to refer to an invalid position in a #ClapperStreamList
 */
#define CLAPPER_STREAM_LIST_INVALID_POSITION ((guint) 0xffffffff)

CLAPPER_API
gboolean clapper_stream_list_select_stream (ClapperStreamList *list, ClapperStream *stream);

CLAPPER_API
gboolean clapper_stream_list_select_index (ClapperStreamList *list, guint index);

CLAPPER_API
ClapperStream * clapper_stream_list_get_stream (ClapperStreamList *list, guint index);

CLAPPER_API
ClapperStream * clapper_stream_list_get_current_stream (ClapperStreamList *list);

CLAPPER_API
guint clapper_stream_list_get_current_index (ClapperStreamList *list);

CLAPPER_API
guint clapper_stream_list_get_n_streams (ClapperStreamList *list);

G_END_DECLS
