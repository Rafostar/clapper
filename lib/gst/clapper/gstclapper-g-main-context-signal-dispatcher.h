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

#ifndef __GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_H__
#define __GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_H__

#include <gst/clapper/gstclapper-types.h>
#include <gst/clapper/gstclapper-signal-dispatcher.h>

G_BEGIN_DECLS

typedef struct _GstClapperGMainContextSignalDispatcher
    GstClapperGMainContextSignalDispatcher;
typedef struct _GstClapperGMainContextSignalDispatcherClass
    GstClapperGMainContextSignalDispatcherClass;

#define GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER             (gst_clapper_g_main_context_signal_dispatcher_get_type ())
#define GST_IS_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER))
#define GST_IS_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER))
#define GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER, GstClapperGMainContextSignalDispatcherClass))
#define GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER, GstClapperGMainContextSignalDispatcher))
#define GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER, GstClapperGMainContextSignalDispatcherClass))
#define GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_CAST(obj)        ((GstClapperGMainContextSignalDispatcher*)(obj))

GST_CLAPPER_API
GType gst_clapper_g_main_context_signal_dispatcher_get_type (void);

GST_CLAPPER_API
GstClapperSignalDispatcher * gst_clapper_g_main_context_signal_dispatcher_new (GMainContext * application_context);

G_END_DECLS

#endif /* __GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER_H__ */
