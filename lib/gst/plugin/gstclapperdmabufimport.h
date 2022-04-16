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

#include "gstclapperdmabufbaseimport.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_DMABUF_IMPORT (gst_clapper_dmabuf_import_get_type())
G_DECLARE_FINAL_TYPE (GstClapperDmabufImport, gst_clapper_dmabuf_import, GST, CLAPPER_DMABUF_IMPORT, GstClapperDmabufBaseImport)

#define GST_CLAPPER_DMABUF_IMPORT_CAST(obj)        ((GstClapperDmabufImport *)(obj))

struct _GstClapperDmabufImport
{
  GstClapperDmabufBaseImport parent;
};

GST_ELEMENT_REGISTER_DECLARE (clapperdmabufimport);

G_END_DECLS
