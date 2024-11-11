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

#include <glib.h>
#include <gst/gst.h>
#include <gst/gstbin.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_URI_LIST_DEMUX (clapper_uri_list_demux_get_type())
#define CLAPPER_URI_LIST_DEMUX_CAST(obj) ((ClapperUriListDemux *)(obj))

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (ClapperUriListDemux, clapper_uri_list_demux, CLAPPER, URI_LIST_DEMUX, GstBin)

GST_ELEMENT_REGISTER_DECLARE (clapperurilistdemux)

G_END_DECLS
