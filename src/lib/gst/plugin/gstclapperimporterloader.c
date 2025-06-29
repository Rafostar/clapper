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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapperimporterloader.h"
#include "gstclapperimporter.h"
#include "gstclappercontexthandler.h"

#ifdef G_OS_WIN32
#include <windows.h>
static HMODULE _importer_dll_handle = NULL;
#endif

#define GST_CAT_DEFAULT gst_clapper_importer_loader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_importer_loader_parent_class
G_DEFINE_TYPE (GstClapperImporterLoader, gst_clapper_importer_loader, GST_TYPE_OBJECT);

typedef GstClapperImporter* (* MakeImporter) (GPtrArray *context_handlers);
typedef GstCaps* (* MakeCaps) (gboolean is_template, GstRank *rank, GPtrArray *context_handlers);

typedef struct
{
  const gchar *loader;
  GstCaps *caps;
  GstRank rank;
  MakeImporter make_importer;
} GstClapperImporterData;

static void
gst_clapper_importer_data_free (GstClapperImporterData *data)
{
  GST_TRACE ("Freeing importer data for %s: %" GST_PTR_FORMAT, data->loader, data->caps);

  gst_clear_caps (&data->caps);
  g_free (data);
}

static GstClapperImporterData *
_obtain_importer_data (const gchar *name, MakeCaps make_caps, MakeImporter make_importer, gboolean is_template, GPtrArray *context_handlers)
{
  GstClapperImporterData *data;

  GST_DEBUG ("Found importer: %s", name);

  data = g_new0 (GstClapperImporterData, 1);
  data->loader = name;
  data->caps = make_caps (is_template, &data->rank, context_handlers);
  data->make_importer = make_importer;

  GST_TRACE ("Created importer data for %s: %" GST_PTR_FORMAT, data->loader, data->caps);

  if (G_UNLIKELY (!data->caps)) {
    if (!is_template) {
      GST_ERROR ("Invalid importer without caps: %s", name);
    } else {
      /* When importer cannot be actually used, due to e.g. unsupported HW */
      GST_DEBUG ("No actual caps returned from importer");
    }
    gst_clapper_importer_data_free (data);

    return NULL;
  }

  GST_DEBUG ("Importer caps: %" GST_PTR_FORMAT, data->caps);

  return data;
}

static gint
_sort_importers_cb (gconstpointer a, gconstpointer b)
{
  GstClapperImporterData *data_a, *data_b;

  data_a = *((GstClapperImporterData **) a);
  data_b = *((GstClapperImporterData **) b);

  return (data_b->rank - data_a->rank);
}

static GPtrArray *
_obtain_importers (gboolean is_template, GPtrArray *context_handlers)
{
  GPtrArray *importers;

  GST_DEBUG ("Checking %s importers",
      (is_template) ? "available" : "usable");

  importers = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_clapper_importer_data_free);

#define _append_importer_data(importer) \
{ \
  GstClapperImporterData *data;  \
  extern GstClapperImporter* gst_clapper_##importer##_make_importer (GPtrArray *context_handlers); \
  extern GstCaps* gst_clapper_##importer##_make_caps (gboolean is_template, GstRank *rank, GPtrArray *context_handlers); \
  data = _obtain_importer_data (#importer, gst_clapper_##importer##_make_caps, gst_clapper_##importer##_make_importer, is_template, context_handlers); \
  if (data) \
    g_ptr_array_add (importers, data); \
}

#ifdef CLAPPER_GST_HAS_GLIMPORTER
  _append_importer_data (glimporter)
#endif
#ifdef CLAPPER_GST_HAS_GLUPLOADER
  _append_importer_data (gluploader)
#endif
#ifdef CLAPPER_GST_HAS_RAWIMPORTER
  _append_importer_data (rawimporter)
#endif

  g_ptr_array_sort (importers, (GCompareFunc) _sort_importers_cb);

  GST_DEBUG ("Found %i %s importers", importers->len,
      (is_template) ? "available" : "usable");

  return importers;
}

GstClapperImporterLoader *
gst_clapper_importer_loader_new (void)
{
  return g_object_new (GST_TYPE_CLAPPER_IMPORTER_LOADER, NULL);
}

static GstCaps *
_make_caps_for_importers (const GPtrArray *importers)
{
  GstCaps *caps = gst_caps_new_empty ();
  guint i;

  for (i = 0; i < importers->len; i++) {
    GstClapperImporterData *data = g_ptr_array_index (importers, i);

    gst_caps_append (caps, gst_caps_ref (data->caps));
  }

  return caps;
}

GstPadTemplate *
gst_clapper_importer_loader_make_sink_pad_template (void)
{
  GPtrArray *importers;
  GstCaps *caps;
  GstPadTemplate *templ;

  /* This is only called once from sink class init function */
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperimporterloader", 0,
      "Clapper Importer Loader");

  GST_DEBUG ("Making sink pad template");

  importers = _obtain_importers (TRUE, NULL);
  caps = _make_caps_for_importers (importers);
  g_ptr_array_unref (importers);

  if (G_UNLIKELY (gst_caps_is_empty (caps)))
    gst_caps_append (caps, gst_caps_new_any ());

  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
  gst_caps_unref (caps);

  GST_TRACE ("Created sink pad template");

  return templ;
}

GstCaps *
gst_clapper_importer_loader_make_actual_caps (GstClapperImporterLoader *self)
{
  return _make_caps_for_importers (self->importers);
}

gboolean
gst_clapper_importer_loader_handle_context_query (GstClapperImporterLoader *self,
    GstBaseSink *bsink, GstQuery *query)
{
  guint i;

  for (i = 0; i < self->context_handlers->len; i++) {
    GstClapperContextHandler *handler = g_ptr_array_index (self->context_handlers, i);

    if (gst_clapper_context_handler_handle_context_query (handler, bsink, query))
      return TRUE;
  }

  return FALSE;
}

static const GstClapperImporterData *
_get_importer_data_for_caps (const GPtrArray *importers, const GstCaps *caps)
{
  guint i;

  for (i = 0; i < importers->len; i++) {
    GstClapperImporterData *data = g_ptr_array_index (importers, i);

    if (!gst_caps_is_always_compatible (caps, data->caps))
      continue;

    return data;
  }

  return NULL;
}

gboolean
gst_clapper_importer_loader_find_importer_for_caps (GstClapperImporterLoader *self,
    GstCaps *caps, GstClapperImporter **importer)
{
  const GstClapperImporterData *data = NULL;
  GstClapperImporter *found_importer = NULL;

  GST_OBJECT_LOCK (self);

  GST_DEBUG_OBJECT (self, "Requested importer for caps: %" GST_PTR_FORMAT, caps);
  data = _get_importer_data_for_caps (self->importers, caps);

  GST_LOG_OBJECT (self, "Old importer: %s, new: %s",
      self->last_loader ? self->last_loader : NULL,
      data ? data->loader : NULL);

  if (G_UNLIKELY (!data)) {
    gst_clear_object (importer);
    goto finish;
  }

  if (*importer && (self->last_loader == data->loader)) {
    GST_DEBUG_OBJECT (self, "No importer change");

    gst_clapper_importer_set_caps (*importer, caps);
    goto finish;
  }

  found_importer = data->make_importer (self->context_handlers);
  gst_clear_object (importer);

  if (!found_importer)
    goto finish;

  gst_clapper_importer_set_caps (found_importer, caps);

  *importer = found_importer;

finish:
  self->last_loader = (*importer && data)
      ? data->loader
      : NULL;

  GST_OBJECT_UNLOCK (self);

  return (*importer != NULL);
}

static void
gst_clapper_importer_loader_init (GstClapperImporterLoader *self)
{
  self->context_handlers = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_object_unref);
  self->importers = _obtain_importers (FALSE, self->context_handlers);
}

static void
gst_clapper_importer_loader_finalize (GObject *object)
{
  GstClapperImporterLoader *self = GST_CLAPPER_IMPORTER_LOADER_CAST (object);

  GST_TRACE ("Finalize");

  if (self->importers)
    g_ptr_array_unref (self->importers);

  g_ptr_array_unref (self->context_handlers);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_importer_loader_class_init (GstClapperImporterLoaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_clapper_importer_loader_finalize;
}
