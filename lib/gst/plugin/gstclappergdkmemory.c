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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclappergdkmemory.h"
#include "gstgtkutils.h"

#define GST_CAT_DEFAULT gst_clapper_gdk_allocator_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstAllocator *_gst_clapper_gdk_allocator = NULL;

#define parent_class gst_clapper_gdk_allocator_parent_class
G_DEFINE_TYPE (GstClapperGdkAllocator, gst_clapper_gdk_allocator, GST_TYPE_ALLOCATOR);

static void
gst_clapper_gdk_allocator_free (GstAllocator *self, GstMemory *memory)
{
  GstClapperGdkMemory *mem = GST_CLAPPER_GDK_MEMORY_CAST (memory);

  GST_TRACE_OBJECT (self, "Freeing ClapperGdkMemory: %" GST_PTR_FORMAT, mem);

  g_clear_object (&mem->texture);
  g_free (mem);
}

static gpointer
gst_clapper_gdk_mem_map_full (GstMemory *memory, GstMapInfo *info, gsize maxsize)
{
  GstClapperGdkMemory *mem = GST_CLAPPER_GDK_MEMORY_CAST (memory);

  return &mem->texture;
}

static void
gst_clapper_gdk_mem_unmap_full (GstMemory *memory, GstMapInfo *info)
{
  /* NOOP */
}

static GstMemory *
gst_clapper_gdk_mem_copy (GstMemory *memory, gssize offset, gssize size)
{
  return NULL;
}

static GstMemory *
gst_clapper_gdk_mem_share (GstMemory *memory, gssize offset, gssize size)
{
  return NULL;
}

static gboolean
gst_clapper_gdk_mem_is_span (GstMemory *mem1, GstMemory *mem2, gsize *offset)
{
  return FALSE;
}

static void
gst_clapper_gdk_allocator_init (GstClapperGdkAllocator *self)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (self);

  alloc->mem_type = GST_CLAPPER_GDK_MEMORY_TYPE_NAME;
  alloc->mem_map_full = gst_clapper_gdk_mem_map_full;
  alloc->mem_unmap_full = gst_clapper_gdk_mem_unmap_full;
  alloc->mem_copy = gst_clapper_gdk_mem_copy;
  alloc->mem_share = gst_clapper_gdk_mem_share;
  alloc->mem_is_span = gst_clapper_gdk_mem_is_span;

  GST_OBJECT_FLAG_SET (self, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static void
gst_clapper_gdk_allocator_class_init (GstClapperGdkAllocatorClass *klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = NULL;
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_clapper_gdk_allocator_free);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergdkallocator", 0,
      "Clapper Gdk Allocator");
}

void
gst_clapper_gdk_memory_init_once (void)
{
  static gsize _alloc_init = 0;

  if (g_once_init_enter (&_alloc_init)) {
    _gst_clapper_gdk_allocator = GST_ALLOCATOR_CAST (
        g_object_new (GST_TYPE_CLAPPER_GDK_ALLOCATOR, NULL));
    gst_object_ref_sink (_gst_clapper_gdk_allocator);

    gst_allocator_register (GST_CLAPPER_GDK_MEMORY_TYPE_NAME, _gst_clapper_gdk_allocator);
    g_once_init_leave (&_alloc_init, 1);
  }
}

gboolean
gst_is_clapper_gdk_memory (GstMemory *memory)
{
  return (memory != NULL && memory->allocator != NULL
      && GST_IS_CLAPPER_GDK_ALLOCATOR (memory->allocator));
}

GstMemory *
gst_clapper_gdk_allocator_alloc (GstClapperGdkAllocator *self, const GstVideoInfo *info)
{
  GstClapperGdkMemory *mem;

  mem = g_new0 (GstClapperGdkMemory, 1);
  mem->info = *info;

  gst_memory_init (GST_MEMORY_CAST (mem), 0, GST_ALLOCATOR_CAST (self),
      NULL, GST_VIDEO_INFO_SIZE (info), 0, 0, GST_VIDEO_INFO_SIZE (info));

  GST_TRACE_OBJECT (self, "Allocated new ClapperGdkMemory: %" GST_PTR_FORMAT, mem);

  return GST_MEMORY_CAST (mem);
}
