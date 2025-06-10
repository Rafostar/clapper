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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include "gstclapperimporter.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_IMPORTER_LOADER (gst_clapper_importer_loader_get_type())
G_DECLARE_FINAL_TYPE (GstClapperImporterLoader, gst_clapper_importer_loader, GST, CLAPPER_IMPORTER_LOADER, GstObject)

#define GST_CLAPPER_IMPORTER_LOADER_CAST(obj)        ((GstClapperImporterLoader *)(obj))

struct _GstClapperImporterLoader
{
  GstObject parent;

  GModule *last_module;

  GPtrArray *importers;
  GPtrArray *context_handlers;
};

GstClapperImporterLoader * gst_clapper_importer_loader_new                             (void);

GstPadTemplate *           gst_clapper_importer_loader_make_sink_pad_template          (void);

GstCaps *                  gst_clapper_importer_loader_make_actual_caps                (GstClapperImporterLoader *loader);

gboolean                   gst_clapper_importer_loader_handle_context_query            (GstClapperImporterLoader *loader, GstBaseSink *bsink, GstQuery *query);

gboolean                   gst_clapper_importer_loader_find_importer_for_caps          (GstClapperImporterLoader *loader, GstCaps *caps, GstClapperImporter **importer);

G_END_DECLS
