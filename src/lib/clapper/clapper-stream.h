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

#include <clapper/clapper-enums.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_STREAM (clapper_stream_get_type())
#define CLAPPER_STREAM_CAST(obj) ((ClapperStream *)(obj))

G_DECLARE_DERIVABLE_TYPE (ClapperStream, clapper_stream, CLAPPER, STREAM, GstObject)

struct _ClapperStreamClass
{
  GstObjectClass parent_class;

  /**
   * ClapperStreamClass::internal_stream_updated:
   * @stream: a #ClapperStream
   * @caps: (nullable): an updated #GstCaps if changed
   * @tags: (nullable): an updated #GstTagList if changed
   *
   * This function is called when internal #GstStream gets updated.
   * Meant for internal usage only. Used for subclasses to update
   * their properties accordingly.
   *
   * Note that this vfunc is called from different threads.
   */
  void (* internal_stream_updated) (ClapperStream *stream, GstCaps *caps, GstTagList *tags);

  /*< private >*/
  gpointer padding[4];
};

ClapperStreamType clapper_stream_get_stream_type (ClapperStream *stream);

gchar * clapper_stream_get_title (ClapperStream *stream);

G_END_DECLS
