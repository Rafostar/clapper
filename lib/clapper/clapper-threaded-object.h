/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include <gst/gst.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_THREADED_OBJECT (clapper_threaded_object_get_type())
#define CLAPPER_THREADED_OBJECT_CAST(obj) ((ClapperThreadedObject *)(obj))

/**
 * ClapperThreadedObject:
 */
G_DECLARE_DERIVABLE_TYPE (ClapperThreadedObject, clapper_threaded_object, CLAPPER, THREADED_OBJECT, GstObject)

struct _ClapperThreadedObjectClass
{
  GstObjectClass parent_class;

  void (* thread_start) (ClapperThreadedObject *threaded_object);

  void (* thread_stop) (ClapperThreadedObject *threaded_object);
};

GMainContext * clapper_threaded_object_get_context (ClapperThreadedObject *threaded_object);

G_END_DECLS
