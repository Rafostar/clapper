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

#include <gmodule.h>

#include "gstclapperimporterloader.h"
#include "gstclapperimporter.h"

#define GST_CAT_DEFAULT gst_clapper_importer_loader_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_clapper_importer_loader_parent_class
G_DEFINE_TYPE (GstClapperImporterLoader, gst_clapper_importer_loader, GST_TYPE_OBJECT);

typedef GstClapperImporter* (* MakeImporter) (void);
typedef GstCaps* (* MakeCaps) (gboolean is_template, GstRank *rank, GStrv *context_types);

typedef struct
{
  GModule *module;
  GstCaps *caps;
  GstRank rank;
  GStrv context_types;
} GstClapperImporterData;

static void
gst_clapper_importer_data_free (GstClapperImporterData *data)
{
  GST_TRACE ("Freeing importer data: %" GST_PTR_FORMAT, data);

  gst_clear_caps (&data->caps);
  g_strfreev (data->context_types);
  g_free (data);
}

static GstClapperImporterData *
_obtain_importer_data (GModule *module, gboolean is_template)
{
  MakeCaps make_caps;
  GstClapperImporterData *data;

  if (!g_module_symbol (module, "make_caps", (gpointer *) &make_caps)
      || make_caps == NULL) {
    GST_WARNING ("Make caps function missing in importer");
    return NULL;
  }

  data = g_new0 (GstClapperImporterData, 1);
  data->module = module;
  data->caps = make_caps (is_template, &data->rank, &data->context_types);

  GST_TRACE ("Created importer data: %" GST_PTR_FORMAT, data);

  if (G_UNLIKELY (!data->caps)) {
    GST_ERROR ("Invalid importer without caps: %s",
        g_module_name (data->module));
    gst_clapper_importer_data_free (data);

    return NULL;
  }

  GST_DEBUG ("Found importer: %s, caps: %" GST_PTR_FORMAT,
      g_module_name (data->module), data->caps);

  return data;
}

static GstClapperImporter *
_obtain_importer_internal (GModule *module)
{
  MakeImporter make_importer;
  GstClapperImporter *importer;

  if (!g_module_symbol (module, "make_importer", (gpointer *) &make_importer)
      || make_importer == NULL) {
    GST_WARNING ("Make function missing in importer");
    return NULL;
  }

  importer = make_importer ();
  GST_TRACE ("Created importer: %" GST_PTR_FORMAT, importer);

  return importer;
}

static gpointer
_obtain_available_modules_once (G_GNUC_UNUSED gpointer data)
{
  GPtrArray *modules;
  GFile *dir;
  GFileEnumerator *dir_enum;
  GError *error = NULL;

  GST_INFO ("Preparing modules");

  modules = g_ptr_array_new ();
  dir = g_file_new_for_path (CLAPPER_SINK_IMPORTER_PATH);

  if ((dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error))) {
    while (TRUE) {
      GFileInfo *info = NULL;
      GModule *module;
      gchar *module_path;
      const gchar *module_name;

      if (!g_file_enumerator_iterate (dir_enum, &info,
          NULL, NULL, &error) || !info)
        break;

      module_name = g_file_info_get_name (info);

      if (!g_str_has_suffix (module_name, G_MODULE_SUFFIX))
        continue;

      module_path = g_module_build_path (CLAPPER_SINK_IMPORTER_PATH, module_name);
      module = g_module_open (module_path, G_MODULE_BIND_LAZY);
      g_free (module_path);

      if (!module) {
        GST_WARNING ("Could not read module: %s, reason: %s",
            module_name, g_module_error ());
        continue;
      }

      GST_INFO ("Found module: %s", module_name);
      g_ptr_array_add (modules, module);
    }

    g_object_unref (dir_enum);
  }

  g_object_unref (dir);

  if (error) {
    GST_ERROR ("Could not load module, reason: %s",
        (error->message) ? error->message : "unknown");
    g_error_free (error);
  }

  return modules;
}

static const GPtrArray *
gst_clapper_importer_loader_get_available_modules (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, _obtain_available_modules_once, NULL);
  return (const GPtrArray *) once.retval;
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
_obtain_available_importers (gboolean is_template)
{
  const GPtrArray *modules;
  GPtrArray *importers;
  guint i;

  GST_DEBUG ("Checking available importers");

  modules = gst_clapper_importer_loader_get_available_modules ();
  importers = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_clapper_importer_data_free);

  for (i = 0; i < modules->len; i++) {
    GModule *module = g_ptr_array_index (modules, i);
    GstClapperImporterData *data;

    if ((data = _obtain_importer_data (module, is_template)))
      g_ptr_array_add (importers, data);
  }

  g_ptr_array_sort (importers, (GCompareFunc) _sort_importers_cb);

  GST_DEBUG ("Found %i available importers", importers->len);

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

  importers = _obtain_available_importers (TRUE);
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

static const GstClapperImporterData *
_get_importer_data_for_context_type (const GPtrArray *importers, const gchar *context_type)
{
  guint i;

  for (i = 0; i < importers->len; i++) {
    GstClapperImporterData *data = g_ptr_array_index (importers, i);
    guint j;

    if (!data->context_types)
      continue;

    for (j = 0; data->context_types[j]; j++) {
      if (strcmp (context_type, data->context_types[j]))
        continue;

      return data;
    }
  }

  return NULL;
}

static gboolean
_find_importer_internal (GstClapperImporterLoader *self,
    GstCaps *caps, GstQuery *query, GstClapperImporter **importer)
{
  const GstClapperImporterData *data = NULL;
  GstClapperImporter *found_importer = NULL;

  GST_OBJECT_LOCK (self);

  if (caps) {
    GST_DEBUG_OBJECT (self, "Requested importer for caps: %" GST_PTR_FORMAT, caps);
    data = _get_importer_data_for_caps (self->importers, caps);
  } else if (query) {
    const gchar *context_type;

    gst_query_parse_context_type (query, &context_type);

    GST_DEBUG_OBJECT (self, "Requested importer for context: %s", context_type);
    data = _get_importer_data_for_context_type (self->importers, context_type);
  }

  GST_LOG_OBJECT (self, "Old importer path: %s, new path: %s",
      (self->last_module) ? g_module_name (self->last_module) : NULL,
      (data) ? g_module_name (data->module) : NULL);

  if (!data) {
    /* In case of missing importer for context query, leave the old one.
     * We should allow some queries to go through unresponded */
    if (query)
      GST_DEBUG_OBJECT (self, "No importer for query, leaving old one");
    else
      gst_clear_object (importer);

    goto finish;
  }

  if (*importer && (self->last_module == data->module)) {
    GST_DEBUG_OBJECT (self, "No importer change");

    if (caps)
      gst_clapper_importer_set_caps (*importer, caps);

    goto finish;
  }

  found_importer = _obtain_importer_internal (data->module);

  if (*importer && found_importer)
    gst_clapper_importer_share_data (*importer, found_importer);

  gst_clear_object (importer);

  if (!found_importer || !gst_clapper_importer_prepare (found_importer)) {
    gst_clear_object (&found_importer);

    goto finish;
  }

  if (caps)
    gst_clapper_importer_set_caps (found_importer, caps);

  *importer = found_importer;

finish:
  self->last_module = (*importer && data)
      ? data->module
      : NULL;

  GST_OBJECT_UNLOCK (self);

  return (*importer != NULL);
}

gboolean
gst_clapper_importer_loader_find_importer_for_caps (GstClapperImporterLoader *self,
    GstCaps *caps, GstClapperImporter **importer)
{
  return _find_importer_internal (self, caps, NULL, importer);
}

gboolean
gst_clapper_importer_loader_find_importer_for_context_query (GstClapperImporterLoader *self,
    GstQuery *query, GstClapperImporter **importer)
{
  return _find_importer_internal (self, NULL, query, importer);
}

static void
gst_clapper_importer_loader_init (GstClapperImporterLoader *self)
{
}

static void
gst_clapper_importer_loader_constructed (GObject *object)
{
  GstClapperImporterLoader *self = GST_CLAPPER_IMPORTER_LOADER_CAST (object);

  self->importers = _obtain_available_importers (FALSE);

  GST_CALL_PARENT (G_OBJECT_CLASS, constructed, (object));
}

static void
gst_clapper_importer_loader_finalize (GObject *object)
{
  GstClapperImporterLoader *self = GST_CLAPPER_IMPORTER_LOADER_CAST (object);

  GST_TRACE ("Finalize");

  g_ptr_array_unref (self->importers);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_clapper_importer_loader_class_init (GstClapperImporterLoaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_clapper_importer_loader_constructed;
  gobject_class->finalize = gst_clapper_importer_loader_finalize;
}
