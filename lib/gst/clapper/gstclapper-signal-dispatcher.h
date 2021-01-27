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

#ifndef __GST_CLAPPER_SIGNAL_DISPATCHER_H__
#define __GST_CLAPPER_SIGNAL_DISPATCHER_H__

#include <gst/gst.h>
#include <gst/clapper/gstclapper-types.h>

G_BEGIN_DECLS

typedef struct _GstClapperSignalDispatcher GstClapperSignalDispatcher;
typedef struct _GstClapperSignalDispatcherInterface GstClapperSignalDispatcherInterface;

#define GST_TYPE_CLAPPER_SIGNAL_DISPATCHER                (gst_clapper_signal_dispatcher_get_type ())
#define GST_CLAPPER_SIGNAL_DISPATCHER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_SIGNAL_DISPATCHER, GstClapperSignalDispatcher))
#define GST_IS_CLAPPER_SIGNAL_DISPATCHER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_SIGNAL_DISPATCHER))
#define GST_CLAPPER_SIGNAL_DISPATCHER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_CLAPPER_SIGNAL_DISPATCHER, GstClapperSignalDispatcherInterface))

typedef void (*GstClapperSignalDispatcherFunc) (gpointer data);

struct _GstClapperSignalDispatcherInterface {
  GTypeInterface parent_iface;

  void (*dispatch) (GstClapperSignalDispatcher * self, GstClapper * clapper,
                    GstClapperSignalDispatcherFunc emitter, gpointer data,
                    GDestroyNotify destroy);
};

GST_CLAPPER_API
GType        gst_clapper_signal_dispatcher_get_type       (void);

G_END_DECLS

#endif /* __GST_CLAPPER_SIGNAL_DISPATCHER_H__ */
