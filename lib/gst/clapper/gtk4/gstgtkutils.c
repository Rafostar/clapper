/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>
#include "gstgtkutils.h"

struct invoke_context
{
  GThreadFunc func;
  gpointer data;
  GMutex lock;
  GCond cond;
  gboolean fired;

  gpointer res;
};

static gboolean
gst_gtk_invoke_func (struct invoke_context *info)
{
  g_mutex_lock (&info->lock);
  info->res = info->func (info->data);
  info->fired = TRUE;
  g_cond_signal (&info->cond);
  g_mutex_unlock (&info->lock);

  return G_SOURCE_REMOVE;
}

gpointer
gst_gtk_invoke_on_main (GThreadFunc func, gpointer data)
{
  GMainContext *main_context = g_main_context_default ();
  struct invoke_context info;

  g_mutex_init (&info.lock);
  g_cond_init (&info.cond);
  info.fired = FALSE;
  info.func = func;
  info.data = data;

  g_main_context_invoke (main_context, (GSourceFunc) gst_gtk_invoke_func,
      &info);

  g_mutex_lock (&info.lock);
  while (!info.fired)
    g_cond_wait (&info.cond, &info.lock);
  g_mutex_unlock (&info.lock);

  g_mutex_clear (&info.lock);
  g_cond_clear (&info.cond);

  return info.res;
}

void
gst_gtk_install_shared_properties (GObjectClass *gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          DEFAULT_FORCE_ASPECT_RATIO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", DEFAULT_PAR_N, DEFAULT_PAR_D,
          G_MAXINT, 1, 1, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KEEP_LAST_FRAME,
      g_param_spec_boolean ("keep-last-frame",
          "Keep last frame",
          "Keep showing last video frame after playback instead of black screen",
          DEFAULT_KEEP_LAST_FRAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}
