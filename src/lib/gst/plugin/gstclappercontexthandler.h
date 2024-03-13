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

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_CLAPPER_CONTEXT_HANDLER               (gst_clapper_context_handler_get_type())
#define GST_IS_CLAPPER_CONTEXT_HANDLER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_CONTEXT_HANDLER))
#define GST_IS_CLAPPER_CONTEXT_HANDLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_CONTEXT_HANDLER))
#define GST_CLAPPER_CONTEXT_HANDLER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_CONTEXT_HANDLER, GstClapperContextHandlerClass))
#define GST_CLAPPER_CONTEXT_HANDLER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_CONTEXT_HANDLER, GstClapperContextHandler))
#define GST_CLAPPER_CONTEXT_HANDLER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_CONTEXT_HANDLER, GstClapperContextHandlerClass))
#define GST_CLAPPER_CONTEXT_HANDLER_CAST(obj)          ((GstClapperContextHandler *)(obj))

typedef struct _GstClapperContextHandler GstClapperContextHandler;
typedef struct _GstClapperContextHandlerClass GstClapperContextHandlerClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstClapperContextHandler, gst_object_unref)
#endif

struct _GstClapperContextHandler
{
  GstObject parent;
};

struct _GstClapperContextHandlerClass
{
  GstObjectClass parent_class;

  gboolean (* handle_context_query) (GstClapperContextHandler *handler,
                                     GstBaseSink              *bsink,
                                     GstQuery                 *query);
};

GType                      gst_clapper_context_handler_get_type              (void);

gboolean                   gst_clapper_context_handler_handle_context_query  (GstClapperContextHandler *handler, GstBaseSink *bsink, GstQuery *query);

GstClapperContextHandler * gst_clapper_context_handler_obtain_with_type      (GPtrArray *context_handlers, GType type);

G_END_DECLS
