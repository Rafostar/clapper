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

#include "clapper-stream.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
ClapperStream * clapper_stream_new (GstStream *gst_stream);

G_GNUC_INTERNAL
void clapper_stream_set_gst_stream (ClapperStream *stream, GstStream *gst_stream);

G_GNUC_INTERNAL
GstStream * clapper_stream_get_gst_stream (ClapperStream *stream);

G_GNUC_INTERNAL
void clapper_stream_take_string_prop (ClapperStream *stream, GParamSpec *pspec, gchar **ptr, gchar *value);

G_GNUC_INTERNAL
void clapper_stream_set_string_prop (ClapperStream *stream, GParamSpec *pspec, gchar **ptr, const gchar *value);

G_GNUC_INTERNAL
void clapper_stream_set_int_prop (ClapperStream *stream, GParamSpec *pspec, gint *ptr, gint value);

G_GNUC_INTERNAL
void clapper_stream_set_uint_prop (ClapperStream *stream, GParamSpec *pspec, guint *ptr, guint value);

G_GNUC_INTERNAL
void clapper_stream_set_double_prop (ClapperStream *stream, GParamSpec *pspec, gdouble *ptr, gdouble value);

G_END_DECLS
