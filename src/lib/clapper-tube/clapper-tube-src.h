/* ClapperTube Library
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

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define CLAPPER_TUBE_TYPE_SRC (clapper_tube_src_get_type())
#define CLAPPER_TUBE_SRC_CAST(obj) ((ClapperTubeSrc *)(obj))

G_DECLARE_FINAL_TYPE (ClapperTubeSrc, clapper_tube_src, CLAPPER_TUBE, SRC, GstPushSrc)

struct _ClapperTubeSrc
{
  GstPushSrc parent;

  GMutex client_lock;
  GCond client_cond;
  GThread *client_thread;

  GCancellable *cancellable;
  gsize buf_size;

  ClapperTubeMediaInfo *info;

  /* < properties > */
  gchar *location;
};

G_END_DECLS
