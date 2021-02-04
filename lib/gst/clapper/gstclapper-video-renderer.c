/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapper-video-renderer.h"
#include "gstclapper-video-renderer-private.h"

G_DEFINE_INTERFACE (GstClapperVideoRenderer, gst_clapper_video_renderer,
    G_TYPE_OBJECT);

static void
gst_clapper_video_renderer_default_init (G_GNUC_UNUSED
    GstClapperVideoRendererInterface * iface)
{

}

GstElement *
gst_clapper_video_renderer_create_video_sink (GstClapperVideoRenderer * self,
    GstClapper * clapper)
{
  GstClapperVideoRendererInterface *iface;

  g_return_val_if_fail (GST_IS_CLAPPER_VIDEO_RENDERER (self), NULL);
  iface = GST_CLAPPER_VIDEO_RENDERER_GET_INTERFACE (self);
  g_return_val_if_fail (iface->create_video_sink != NULL, NULL);

  return iface->create_video_sink (self, clapper);
}
