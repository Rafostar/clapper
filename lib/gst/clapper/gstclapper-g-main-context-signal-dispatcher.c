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

/**
 * SECTION:gstclapper-gmaincontextsignaldispatcher
 * @title: GstClapperGMainContextSignalDispatcher
 * @short_description: Clapper GLib MainContext dispatcher
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapper-g-main-context-signal-dispatcher.h"

struct _GstClapperGMainContextSignalDispatcher
{
  GObject parent;
  GMainContext *application_context;
};

struct _GstClapperGMainContextSignalDispatcherClass
{
  GObjectClass parent_class;
};

static void
    gst_clapper_g_main_context_signal_dispatcher_interface_init
    (GstClapperSignalDispatcherInterface * iface);

enum
{
  G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_0,
  G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT,
  G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_LAST
};

G_DEFINE_TYPE_WITH_CODE (GstClapperGMainContextSignalDispatcher,
    gst_clapper_g_main_context_signal_dispatcher, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CLAPPER_SIGNAL_DISPATCHER,
        gst_clapper_g_main_context_signal_dispatcher_interface_init));

static GParamSpec
    * g_main_context_signal_dispatcher_param_specs
    [G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_LAST] = { NULL, };

static void
gst_clapper_g_main_context_signal_dispatcher_finalize (GObject * object)
{
  GstClapperGMainContextSignalDispatcher *self =
      GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (object);

  if (self->application_context)
    g_main_context_unref (self->application_context);

  G_OBJECT_CLASS
      (gst_clapper_g_main_context_signal_dispatcher_parent_class)->finalize
      (object);
}

static void
gst_clapper_g_main_context_signal_dispatcher_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstClapperGMainContextSignalDispatcher *self =
      GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (object);

  switch (prop_id) {
    case G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT:
      self->application_context = g_value_dup_boxed (value);
      if (!self->application_context)
        self->application_context = g_main_context_ref_thread_default ();
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_g_main_context_signal_dispatcher_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstClapperGMainContextSignalDispatcher *self =
      GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (object);

  switch (prop_id) {
    case G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT:
      g_value_set_boxed (value, self->application_context);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
    gst_clapper_g_main_context_signal_dispatcher_class_init
    (GstClapperGMainContextSignalDispatcherClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize =
      gst_clapper_g_main_context_signal_dispatcher_finalize;
  gobject_class->set_property =
      gst_clapper_g_main_context_signal_dispatcher_set_property;
  gobject_class->get_property =
      gst_clapper_g_main_context_signal_dispatcher_get_property;

  g_main_context_signal_dispatcher_param_specs
      [G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_APPLICATION_CONTEXT] =
      g_param_spec_boxed ("application-context", "Application Context",
      "Application GMainContext to dispatch signals to", G_TYPE_MAIN_CONTEXT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
      G_MAIN_CONTEXT_SIGNAL_DISPATCHER_PROP_LAST,
      g_main_context_signal_dispatcher_param_specs);
}

static void
    gst_clapper_g_main_context_signal_dispatcher_init
    (G_GNUC_UNUSED GstClapperGMainContextSignalDispatcher * self)
{
}

typedef struct
{
  void (*emitter) (gpointer data);
  gpointer data;
  GDestroyNotify destroy;
} GMainContextSignalDispatcherData;

static gboolean
g_main_context_signal_dispatcher_dispatch_gsourcefunc (gpointer user_data)
{
  GMainContextSignalDispatcherData *data = user_data;

  data->emitter (data->data);

  return G_SOURCE_REMOVE;
}

static void
g_main_context_signal_dispatcher_dispatch_destroy (gpointer user_data)
{
  GMainContextSignalDispatcherData *data = user_data;

  if (data->destroy)
    data->destroy (data->data);
  g_free (data);
}

static void
gst_clapper_g_main_context_signal_dispatcher_dispatch (GstClapperSignalDispatcher
    * iface, G_GNUC_UNUSED GstClapper * clapper, void (*emitter) (gpointer data),
    gpointer data, GDestroyNotify destroy)
{
  GstClapperGMainContextSignalDispatcher *self =
      GST_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER (iface);
  GMainContextSignalDispatcherData *gsourcefunc_data =
      g_new (GMainContextSignalDispatcherData, 1);

  gsourcefunc_data->emitter = emitter;
  gsourcefunc_data->data = data;
  gsourcefunc_data->destroy = destroy;

  g_main_context_invoke_full (self->application_context,
      G_PRIORITY_DEFAULT, g_main_context_signal_dispatcher_dispatch_gsourcefunc,
      gsourcefunc_data, g_main_context_signal_dispatcher_dispatch_destroy);
}

static void
    gst_clapper_g_main_context_signal_dispatcher_interface_init
    (GstClapperSignalDispatcherInterface * iface)
{
  iface->dispatch = gst_clapper_g_main_context_signal_dispatcher_dispatch;
}

/**
 * gst_clapper_g_main_context_signal_dispatcher_new:
 * @application_context: (allow-none): GMainContext to use or %NULL
 *
 * Creates a new GstClapperSignalDispatcher that uses @application_context,
 * or the thread default one if %NULL is used. See gst_clapper_new().
 *
 * Returns: (transfer full): the new GstClapperSignalDispatcher
 */
GstClapperSignalDispatcher *
gst_clapper_g_main_context_signal_dispatcher_new (GMainContext *
    application_context)
{
  return g_object_new (GST_TYPE_CLAPPER_G_MAIN_CONTEXT_SIGNAL_DISPATCHER,
      "application-context", application_context, NULL);
}
