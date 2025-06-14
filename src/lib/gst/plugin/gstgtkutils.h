/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <gst/video/video.h>

#include "gst/plugin/clapper-gst-visibility.h"

G_BEGIN_DECLS

CLAPPER_GST_API
gpointer        gst_gtk_invoke_on_main                (GThreadFunc func, gpointer data);

CLAPPER_GST_API
GdkTexture *    gst_video_frame_into_gdk_texture      (GstVideoFrame *frame);

void            gst_gtk_get_width_height_for_rotation (gint width, gint height,
                                                       gint *out_width, gint *out_height,
                                                       GstVideoOrientationMethod rotation);

G_END_DECLS
