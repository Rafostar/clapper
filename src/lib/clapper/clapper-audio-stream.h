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

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-stream.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_AUDIO_STREAM (clapper_audio_stream_get_type())
#define CLAPPER_AUDIO_STREAM_CAST(obj) ((ClapperAudioStream *)(obj))

CLAPPER_API
G_DECLARE_FINAL_TYPE (ClapperAudioStream, clapper_audio_stream, CLAPPER, AUDIO_STREAM, ClapperStream)

CLAPPER_API
gchar * clapper_audio_stream_get_codec (ClapperAudioStream *stream);

CLAPPER_API
guint clapper_audio_stream_get_bitrate (ClapperAudioStream *stream);

CLAPPER_API
gchar * clapper_audio_stream_get_sample_format (ClapperAudioStream *stream);

CLAPPER_API
gint clapper_audio_stream_get_sample_rate (ClapperAudioStream *stream);

CLAPPER_API
gint clapper_audio_stream_get_channels (ClapperAudioStream *stream);

CLAPPER_API
gchar * clapper_audio_stream_get_lang_code (ClapperAudioStream *stream);

CLAPPER_API
gchar * clapper_audio_stream_get_lang_name (ClapperAudioStream *stream);

G_END_DECLS
