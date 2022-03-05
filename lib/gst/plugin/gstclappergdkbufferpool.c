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

#include "gstclappergdkbufferpool.h"

#define GST_CAT_DEFAULT gst_clapper_gdk_buffer_pool_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_gdk_buffer_pool_parent_class
G_DEFINE_TYPE (GstClapperGdkBufferPool, gst_clapper_gdk_buffer_pool, GST_TYPE_BUFFER_POOL);

static void
gst_clapper_gdk_buffer_pool_init (GstClapperGdkBufferPool *pool)
{
}

static const gchar **
gst_clapper_gdk_buffer_pool_get_options (GstBufferPool *pool)
{
  static const gchar *options[] = {
    GST_BUFFER_POOL_OPTION_VIDEO_META,
    NULL
  };

  return options;
}

static gboolean
gst_clapper_gdk_buffer_pool_set_config (GstBufferPool *pool, GstStructure *config)
{
  GstClapperGdkBufferPool *self = GST_CLAPPER_GDK_BUFFER_POOL_CAST (pool);
  GstCaps *caps = NULL;
  guint size, min_buffers, max_buffers;
  GstVideoInfo info;
  GstClapperGdkMemory *clapper_mem;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size,
      &min_buffers, &max_buffers)) {
    GST_WARNING_OBJECT (self, "Invalid buffer pool config");
    return FALSE;
  }

  if (!caps || !gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (pool, "Could not parse caps into video info");
    return FALSE;
  }

  gst_clear_object (&self->allocator);
  self->allocator = GST_CLAPPER_GDK_ALLOCATOR_CAST (
      gst_allocator_find (GST_CLAPPER_GDK_MEMORY_TYPE_NAME));

  if (G_UNLIKELY (!self->allocator)) {
    GST_ERROR_OBJECT (self, "ClapperGdkAllocator is unavailable");
    return FALSE;
  }

  clapper_mem = GST_CLAPPER_GDK_MEMORY_CAST (
      gst_clapper_gdk_allocator_alloc (self->allocator, &info));
  if (G_UNLIKELY (!clapper_mem)) {
    GST_ERROR_OBJECT (self, "Cannot create ClapperGdkMemory");
    return FALSE;
  }

  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (&clapper_mem->info), min_buffers, max_buffers);
  gst_memory_unref (GST_MEMORY_CAST (clapper_mem));

  self->info = info;

  GST_DEBUG_OBJECT (self, "Set buffer pool config: %" GST_PTR_FORMAT, config);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static GstFlowReturn
gst_clapper_gdk_buffer_pool_alloc (GstBufferPool *pool, GstBuffer **buffer,
    GstBufferPoolAcquireParams *params)
{
  GstClapperGdkBufferPool *self = GST_CLAPPER_GDK_BUFFER_POOL_CAST (pool);
  GstMemory *mem;
  GstClapperGdkMemory *clapper_mem;

  mem = gst_clapper_gdk_allocator_alloc (self->allocator, &self->info);
  if (G_UNLIKELY (!mem)) {
    GST_ERROR_OBJECT (self, "Cannot create ClapperGdkMemory");
    return GST_FLOW_ERROR;
  }

  clapper_mem = GST_CLAPPER_GDK_MEMORY_CAST (mem);

  *buffer = gst_buffer_new ();
  gst_buffer_append_memory (*buffer, mem);

  gst_buffer_add_video_meta_full (*buffer, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&self->info), GST_VIDEO_INFO_WIDTH (&self->info),
      GST_VIDEO_INFO_HEIGHT (&self->info), GST_VIDEO_INFO_N_PLANES (&self->info),
      clapper_mem->info.offset, clapper_mem->info.stride);

  GST_TRACE_OBJECT (self, "Allocated %" GST_PTR_FORMAT, *buffer);

  return GST_FLOW_OK;
}

static void
gst_clapper_gdk_buffer_reset_buffer (GstBufferPool *pool, GstBuffer *buffer)
{
  GstClapperGdkMemory *clapper_mem;

  GST_TRACE ("Reset %" GST_PTR_FORMAT, buffer);

  clapper_mem = GST_CLAPPER_GDK_MEMORY_CAST (gst_buffer_peek_memory (buffer, 0));
  g_clear_object (&clapper_mem->texture);

  return GST_BUFFER_POOL_CLASS (parent_class)->reset_buffer (pool, buffer);
}

static void
gst_clapper_gdk_buffer_pool_dispose (GObject *object)
{
  GstClapperGdkBufferPool *self = GST_CLAPPER_GDK_BUFFER_POOL_CAST (object);

  gst_clear_object (&self->allocator);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_clapper_gdk_buffer_pool_class_init (GstClapperGdkBufferPoolClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *bufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->dispose = gst_clapper_gdk_buffer_pool_dispose;

  bufferpool_class->get_options = gst_clapper_gdk_buffer_pool_get_options;
  bufferpool_class->set_config = gst_clapper_gdk_buffer_pool_set_config;
  bufferpool_class->alloc_buffer = gst_clapper_gdk_buffer_pool_alloc;
  bufferpool_class->reset_buffer = gst_clapper_gdk_buffer_reset_buffer;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergdkbufferpool", 0,
      "Clapper Gdk Buffer Pool");
}

GstBufferPool *
gst_clapper_gdk_buffer_pool_new (void)
{
  GstClapperGdkBufferPool *self;

  self = g_object_new (GST_TYPE_CLAPPER_GDK_BUFFER_POOL, NULL);
  gst_object_ref_sink (self);

  return GST_BUFFER_POOL_CAST (self);
}
