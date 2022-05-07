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

#include "gstclappervaglimporter.h"
#include "gst/plugin/gstgdkformats.h"

#include <gst/va/gstva.h>

#define GST_CAT_DEFAULT gst_clapper_va_gl_importer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_va_gl_importer_parent_class
GST_CLAPPER_IMPORTER_DEFINE (GstClapperVAGLImporter, gst_clapper_va_gl_importer, GST_TYPE_CLAPPER_GL_BASE_IMPORTER);

static GdkTexture *
gst_clapper_va_gl_importer_generate_texture (GstClapperImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  return NULL;
}

static void
gst_clapper_va_gl_importer_init (GstClapperVAGLImporter *self)
{
}

static void
gst_clapper_va_gl_importer_class_init (GstClapperVAGLImporterClass *klass)
{
  GstClapperImporterClass *importer_class = (GstClapperImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappervaglimporter", 0,
      "Clapper VA GL Importer");

  importer_class->generate_texture = gst_clapper_va_gl_importer_generate_texture;
}

GstClapperImporter *
make_importer (void)
{
  return g_object_new (GST_TYPE_CLAPPER_VA_GL_IMPORTER, NULL);
}

GstCaps *
make_caps (GstRank *rank, GStrv *context_types)
{
  GstCaps *caps, *tmp;

  *rank = GST_RANK_PRIMARY;

  tmp = gst_caps_from_string (
      GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
          "{ " GST_GDK_GL_TEXTURE_FORMATS " }"));

  caps = gst_caps_copy (tmp);
  gst_caps_set_features_simple (caps, gst_caps_features_new (
      GST_CAPS_FEATURE_MEMORY_VA,
      GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, NULL));

  gst_caps_append (caps, tmp);

  return caps;
}
