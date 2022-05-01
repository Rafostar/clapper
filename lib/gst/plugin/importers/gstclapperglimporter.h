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

#include "gstclapperglbaseimporter.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_GL_IMPORTER (gst_clapper_gl_importer_get_type())
G_DECLARE_FINAL_TYPE (GstClapperGLImporter, gst_clapper_gl_importer, GST, CLAPPER_GL_IMPORTER, GstClapperGLBaseImporter)

#define GST_CLAPPER_GL_IMPORTER_CAST(obj)        ((GstClapperGLImporter *)(obj))

struct _GstClapperGLImporter
{
  GstClapperGLBaseImporter parent;
};

G_END_DECLS
