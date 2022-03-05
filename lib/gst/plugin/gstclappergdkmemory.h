/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gtk/gtk.h>
#include <gst/gstmemory.h>
#include <gst/gstallocator.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_GDK_ALLOCATOR (gst_clapper_gdk_allocator_get_type())
G_DECLARE_FINAL_TYPE (GstClapperGdkAllocator, gst_clapper_gdk_allocator, GST, CLAPPER_GDK_ALLOCATOR, GstAllocator)

#define GST_CLAPPER_GDK_ALLOCATOR_CAST(obj)      ((GstClapperGdkAllocator *)(obj))
#define GST_CLAPPER_GDK_MEMORY_CAST(mem)         ((GstClapperGdkMemory *)(mem))

#define GST_CLAPPER_GDK_MEMORY_TYPE_NAME         "gst.clapper.gdk.memory"

#define GST_CAPS_FEATURE_CLAPPER_GDK_MEMORY      "memory:ClapperGdkMemory"

#define GST_CLAPPER_GDK_MEMORY_FORMATS           \
    "RGBA64_LE, RGBA64_BE, ABGR, BGRA, "         \
    "ARGB, RGBA, BGRx, RGBx, BGR, RGB"           \

/* Formats that `GdkGLTexture` supports */
#define GST_CLAPPER_GDK_GL_TEXTURE_FORMATS       \
    "RGBA64_LE, RGBA64_BE, RGBA, RGBx, RGB"      \

typedef struct _GstClapperGdkMemory GstClapperGdkMemory;
struct _GstClapperGdkMemory
{
  GstMemory mem;

  GdkTexture *texture;
  GstVideoInfo info;
};

struct _GstClapperGdkAllocator
{
  GstAllocator parent;
};

void        gst_clapper_gdk_memory_init_once     (void);

gboolean    gst_is_clapper_gdk_memory            (GstMemory *memory);

GstMemory * gst_clapper_gdk_allocator_alloc      (GstClapperGdkAllocator *allocator, const GstVideoInfo *info);

G_END_DECLS
