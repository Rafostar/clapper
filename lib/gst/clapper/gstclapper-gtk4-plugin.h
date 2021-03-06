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

#ifndef __GST_CLAPPER_GTK4_PLUGIN_H__
#define __GST_CLAPPER_GTK4_PLUGIN_H__

#include <gst/gst.h>
#include <gst/clapper/clapper-prelude.h>

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_GTK4_PLUGIN             (gst_clapper_gtk4_plugin_get_type ())
#define GST_IS_CLAPPER_GTK4_PLUGIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_GTK4_PLUGIN))
#define GST_IS_CLAPPER_GTK4_PLUGIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_GTK4_PLUGIN))
#define GST_CLAPPER_GTK4_PLUGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_GTK4_PLUGIN, GstClapperGtk4PluginClass))
#define GST_CLAPPER_GTK4_PLUGIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_GTK4_PLUGIN, GstClapperGtk4Plugin))
#define GST_CLAPPER_GTK4_PLUGIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_GTK4_PLUGIN, GstClapperGtk4PluginClass))
#define GST_CLAPPER_GTK4_PLUGIN_CAST(obj)        ((GstClapperGtk4Plugin*)(obj))

typedef struct _GstClapperGtk4Plugin GstClapperGtk4Plugin;
typedef struct _GstClapperGtk4PluginClass GstClapperGtk4PluginClass;

/**
 * GstClapperGtk4Plugin:
 *
 * Opaque #GstClapperGtk4Plugin object
 */
struct _GstClapperGtk4Plugin
{
  /* <private> */
  GObject parent;

  GstElement *video_sink;
};

/**
 * GstClapperGtk4PluginClass:
 *
 * The #GstClapperGtk4PluginClass struct only contains private data
 */
struct _GstClapperGtk4PluginClass
{
  /* <private> */
  GstElementClass parent_class;
};

GST_CLAPPER_API
GType gst_clapper_gtk4_plugin_get_type             (void);

GST_CLAPPER_API
GstClapperGtk4Plugin * gst_clapper_gtk4_plugin_new (void);

G_END_DECLS

#endif /* __GST_CLAPPER_GTK4_PLUGIN__ */
