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
#include <glib-object.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-stream.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_VIDEO_STREAM (clapper_video_stream_get_type())
#define CLAPPER_VIDEO_STREAM_CAST(obj) ((ClapperVideoStream *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperVideoStream, clapper_video_stream, CLAPPER, VIDEO_STREAM, ClapperStream)

CLAPPER_API
gchar * clapper_video_stream_get_codec (ClapperVideoStream *stream);

CLAPPER_API
gint clapper_video_stream_get_width (ClapperVideoStream *stream);

CLAPPER_API
gint clapper_video_stream_get_height (ClapperVideoStream *stream);

CLAPPER_API
gdouble clapper_video_stream_get_fps (ClapperVideoStream *stream);

CLAPPER_API
guint clapper_video_stream_get_bitrate (ClapperVideoStream *stream);

CLAPPER_API
gchar * clapper_video_stream_get_pixel_format (ClapperVideoStream *stream);

G_END_DECLS
