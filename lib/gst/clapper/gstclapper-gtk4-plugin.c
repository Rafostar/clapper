/* GStreamer
 *
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

/**
 * SECTION:gstclapper-gtk4plugin
 * @title: GstClapperGtk4Plugin
 * @short_description: Clapper GTK4 plugin
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapper-gtk4-plugin.h"
#include "gtk4/gstclapperglsink.h"

enum
{
  PROP_0,
  PROP_VIDEO_SINK,
  PROP_LAST
};

#define parent_class gst_clapper_gtk4_plugin_parent_class
G_DEFINE_TYPE_WITH_CODE (GstClapperGtk4Plugin, gst_clapper_gtk4_plugin,
    G_TYPE_OBJECT, NULL);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_clapper_gtk4_plugin_constructed (GObject * object);
static void gst_clapper_gtk4_plugin_finalize (GObject * object);
static void gst_clapper_gtk4_plugin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_clapper_gtk4_plugin_init
    (G_GNUC_UNUSED GstClapperGtk4Plugin * self)
{
}

static void gst_clapper_gtk4_plugin_class_init
    (G_GNUC_UNUSED GstClapperGtk4PluginClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_clapper_gtk4_plugin_constructed;
  gobject_class->get_property = gst_clapper_gtk4_plugin_get_property;
  gobject_class->finalize = gst_clapper_gtk4_plugin_finalize;

  param_specs[PROP_VIDEO_SINK] =
      g_param_spec_object ("video-sink",
      "Video Sink", "Video sink to use with video renderer",
      GST_TYPE_ELEMENT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gst_clapper_gtk4_plugin_constructed (GObject * object)
{
  GstClapperGtk4Plugin *self = GST_CLAPPER_GTK4_PLUGIN (object);

  self->video_sink = g_object_new (GST_TYPE_CLAPPER_GL_SINK, NULL);
  gst_object_ref_sink (self->video_sink);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_clapper_gtk4_plugin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstClapperGtk4Plugin *self = GST_CLAPPER_GTK4_PLUGIN (object);

  switch (prop_id) {
    case PROP_VIDEO_SINK:
      g_value_set_object (value, self->video_sink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_gtk4_plugin_finalize (GObject * object)
{
  GstClapperGtk4Plugin *self = GST_CLAPPER_GTK4_PLUGIN (object);

  gst_object_unref (self->video_sink);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gst_clapper_gtk4_plugin_new:
 *
 * Creates a new GTK4 plugin.
 *
 * Returns: (transfer full): the new GstClapperGtk4Plugin
 */
GstClapperGtk4Plugin *
gst_clapper_gtk4_plugin_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_GTK4_PLUGIN, NULL);
}
