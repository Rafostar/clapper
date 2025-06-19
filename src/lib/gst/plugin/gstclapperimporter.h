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

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gst/plugin/clapper-gst-visibility.h"

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_IMPORTER               (gst_clapper_importer_get_type())
#define GST_IS_CLAPPER_IMPORTER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_IMPORTER))
#define GST_IS_CLAPPER_IMPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_IMPORTER))
#define GST_CLAPPER_IMPORTER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_IMPORTER, GstClapperImporterClass))
#define GST_CLAPPER_IMPORTER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_IMPORTER, GstClapperImporter))
#define GST_CLAPPER_IMPORTER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_IMPORTER, GstClapperImporterClass))
#define GST_CLAPPER_IMPORTER_CAST(obj)          ((GstClapperImporter *)(obj))

#define GST_CLAPPER_IMPORTER_DEFINE(camel,lower,type)                            \
G_DEFINE_TYPE (camel, lower, type)                                               \
G_MODULE_EXPORT GstClapperImporter *make_importer (GPtrArray *context_handlers); \
G_MODULE_EXPORT GstCaps *make_caps (gboolean is_template,                        \
    GstRank *rank, GPtrArray *context_handlers);

typedef struct _GstClapperImporter GstClapperImporter;
typedef struct _GstClapperImporterClass GstClapperImporterClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperImporter, gst_object_unref)
#endif

struct _GstClapperImporter
{
  GstObject parent;

  GstCaps *pending_caps;
  GstBuffer *pending_buffer, *buffer;
  GPtrArray *pending_overlays, *overlays;
  GstVideoInfo pending_v_info, v_info;
  gboolean has_pending_v_info;

  GdkTexture *texture;

  GdkRGBA bg;
};

struct _GstClapperImporterClass
{
  GstObjectClass parent_class;

  void (* set_caps) (GstClapperImporter *importer,
                     GstCaps            *caps);

  GstBufferPool * (* create_pool) (GstClapperImporter *importer,
                                   GstStructure      **config);

  void (* add_allocation_metas) (GstClapperImporter *importer,
                                 GstQuery           *query);

  GdkTexture * (* generate_texture) (GstClapperImporter *importer,
                                     GstBuffer          *buffer,
                                     GstVideoInfo       *v_info);
};

CLAPPER_GST_API
GType           gst_clapper_importer_get_type                (void);

GstBufferPool * gst_clapper_importer_create_pool             (GstClapperImporter *importer, GstStructure **config);
void            gst_clapper_importer_add_allocation_metas    (GstClapperImporter *importer, GstQuery *query);

void            gst_clapper_importer_set_caps                (GstClapperImporter *importer, GstCaps *caps);
void            gst_clapper_importer_set_buffer              (GstClapperImporter *importer, GstBuffer *buffer);

void            gst_clapper_importer_snapshot                (GstClapperImporter *importer, GdkSnapshot *snapshot, gdouble width, gdouble height);

G_END_DECLS
