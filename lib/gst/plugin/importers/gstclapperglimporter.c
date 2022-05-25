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
GST_CLAPPER_IMPORTER_DEFINE (GstClapperGLImporter, gst_clapper_gl_importer, GST_TYPE_CLAPPER_GL_BASE_IMPORTER);

static GdkTexture *
gst_clapper_gl_importer_generate_texture (GstClapperImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GstClapperGLBaseImporter *gl_bi = GST_CLAPPER_GL_BASE_IMPORTER_CAST (importer);

  return gst_clapper_gl_base_importer_make_gl_texture (gl_bi, buffer, v_info);
}

static void
gst_clapper_gl_importer_init (GstClapperGLImporter *self)
{
}

static void
gst_clapper_gl_importer_class_init (GstClapperGLImporterClass *klass)
{
  GstClapperImporterClass *importer_class = (GstClapperImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperglimporter", 0,
      "Clapper GL Importer");

  importer_class->generate_texture = gst_clapper_gl_importer_generate_texture;
}

GstClapperImporter *
make_importer (void)
{
  return g_object_new (GST_TYPE_CLAPPER_GL_IMPORTER, NULL);
}

GstCaps *
make_caps (gboolean is_template, GstRank *rank, GStrv *context_types)
{
  *rank = GST_RANK_SECONDARY;
  *context_types = gst_clapper_gl_base_importer_make_gl_context_types ();

  return gst_clapper_gl_base_importer_make_supported_gdk_gl_caps ();
}
