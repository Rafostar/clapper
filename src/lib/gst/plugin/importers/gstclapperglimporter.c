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

#include "gstclapperglimporter.h"

#define GST_CAT_DEFAULT gst_clapper_gl_importer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_gl_importer_parent_class
GST_CLAPPER_IMPORTER_DEFINE (GstClapperGLImporter, gst_clapper_gl_importer, GST_TYPE_CLAPPER_IMPORTER);

static GstBufferPool *
gst_clapper_gl_importer_create_pool (GstClapperImporter *importer, GstStructure **config)
{
  GstClapperGLImporter *self = GST_CLAPPER_GL_IMPORTER_CAST (importer);
  GstBufferPool *pool;

  GST_DEBUG_OBJECT (self, "Creating new GL buffer pool");

  pool = gst_gl_buffer_pool_new (self->gl_handler->gst_context);
  *config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_add_option (*config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (*config, GST_BUFFER_POOL_OPTION_GL_SYNC_META);

  return pool;
}

static void
gst_clapper_gl_importer_add_allocation_metas (GstClapperImporter *importer, GstQuery *query)
{
  GstClapperGLImporter *self = GST_CLAPPER_GL_IMPORTER_CAST (importer);

  /* We can support GL sync meta */
  if (self->gl_handler->gst_context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL);

  /* Also add base importer class supported meta */
  GST_CLAPPER_IMPORTER_CLASS (parent_class)->add_allocation_metas (importer, query);
}

static GdkTexture *
gst_clapper_gl_importer_generate_texture (GstClapperImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GstClapperGLImporter *self = GST_CLAPPER_GL_IMPORTER_CAST (importer);

  return gst_clapper_gl_context_handler_make_gl_texture (self->gl_handler, buffer, v_info);
}

static void
gst_clapper_gl_importer_init (GstClapperGLImporter *self)
{
}

static void
gst_clapper_gl_importer_finalize (GObject *object)
{
  GstClapperGLImporter *self = GST_CLAPPER_GL_IMPORTER_CAST (object);

  gst_clear_object (&self->gl_handler);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_gl_importer_class_init (GstClapperGLImporterClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstClapperImporterClass *importer_class = (GstClapperImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperglimporter", 0,
      "Clapper GL Importer");

  gobject_class->finalize = gst_clapper_gl_importer_finalize;

  importer_class->create_pool = gst_clapper_gl_importer_create_pool;
  importer_class->add_allocation_metas = gst_clapper_gl_importer_add_allocation_metas;
  importer_class->generate_texture = gst_clapper_gl_importer_generate_texture;
}

GstClapperImporter *
make_importer (GPtrArray *context_handlers)
{
  GstClapperGLImporter *self;
  GstClapperContextHandler *handler;

  handler = gst_clapper_context_handler_obtain_with_type (context_handlers,
      GST_TYPE_CLAPPER_GL_CONTEXT_HANDLER);

  if (G_UNLIKELY (!handler))
    return NULL;

  self = g_object_new (GST_TYPE_CLAPPER_GL_IMPORTER, NULL);
  self->gl_handler = GST_CLAPPER_GL_CONTEXT_HANDLER_CAST (handler);

  return GST_CLAPPER_IMPORTER_CAST (self);
}

GstCaps *
make_caps (gboolean is_template, GstRank *rank, GPtrArray *context_handlers)
{
  *rank = GST_RANK_SECONDARY;

  if (!is_template && context_handlers)
    gst_clapper_gl_context_handler_add_handler (context_handlers);

  return gst_clapper_gl_context_handler_make_gdk_gl_caps (
      GST_CAPS_FEATURE_MEMORY_GL_MEMORY, TRUE);
}
