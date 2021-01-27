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

#include "gstclapper-signal-dispatcher.h"
#include "gstclapper-signal-dispatcher-private.h"

G_DEFINE_INTERFACE (GstClapperSignalDispatcher, gst_clapper_signal_dispatcher,
    G_TYPE_OBJECT);

static void
gst_clapper_signal_dispatcher_default_init (G_GNUC_UNUSED
    GstClapperSignalDispatcherInterface * iface)
{

}

void
gst_clapper_signal_dispatcher_dispatch (GstClapperSignalDispatcher * self,
    GstClapper * clapper, GstClapperSignalDispatcherFunc emitter, gpointer data,
    GDestroyNotify destroy)
{
  GstClapperSignalDispatcherInterface *iface;

  if (!self) {
    emitter (data);
    if (destroy)
      destroy (data);
    return;
  }

  g_return_if_fail (GST_IS_CLAPPER_SIGNAL_DISPATCHER (self));
  iface = GST_CLAPPER_SIGNAL_DISPATCHER_GET_INTERFACE (self);
  g_return_if_fail (iface->dispatch != NULL);

  iface->dispatch (self, clapper, emitter, data, destroy);
}
