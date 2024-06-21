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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <clapper/clapper-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_THREADED_OBJECT (clapper_threaded_object_get_type())
#define CLAPPER_THREADED_OBJECT_CAST(obj) ((ClapperThreadedObject *)(obj))

CLAPPER_API
G_DECLARE_DERIVABLE_TYPE (ClapperThreadedObject, clapper_threaded_object, CLAPPER, THREADED_OBJECT, GstObject)

/**
 * ClapperThreadedObjectClass:
 * @parent_class: The object class structure.
 * @thread_start: Called right after thread started.
 * @thread_stop: Called when thread is going to stop.
 */
struct _ClapperThreadedObjectClass
{
  GstObjectClass parent_class;

  /**
   * ClapperThreadedObjectClass::thread_start:
   * @threaded_object: a #ClapperThreadedObject
   *
   * Called right after thread started.
   *
   * Useful for initializing objects that work within this new thread.
   */
  void (* thread_start) (ClapperThreadedObject *threaded_object);

  /**
   * ClapperThreadedObjectClass::thread_stop:
   * @threaded_object: a #ClapperThreadedObject
   *
   * Called when thread is going to stop.
   *
   * Useful for cleanup of things created on thread start.
   */
  void (* thread_stop) (ClapperThreadedObject *threaded_object);

  /*< private >*/
  gpointer padding[4];
};

CLAPPER_API
GMainContext * clapper_threaded_object_get_context (ClapperThreadedObject *threaded_object);

G_END_DECLS
