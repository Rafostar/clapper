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

typedef GstClapperImporter* (* MakeImporter) (void);
typedef GstCaps* (* MakeCaps) (GstRank *rank, GStrv *context_types);

typedef struct
{
  gchar *module_path;
  GModule *open_module;
  GstCaps *caps;
  GstRank rank;
  GStrv context_types;
} GstClapperImporterData;

static void
gst_clapper_importer_data_free (GstClapperImporterData *data)
{
  g_free (data->module_path);

  if (data->open_module)
    g_module_close (data->open_module);

  gst_clear_caps (&data->caps);
  g_strfreev (data->context_types);
  g_free (data);
}

static gboolean
_open_importer (GstClapperImporterData *data)
{
  g_return_val_if_fail (data && data->module_path, FALSE);

  /* Already open */
  if (data->open_module)
    return TRUE;

  GST_DEBUG ("Opening module: %s", data->module_path);
  data->open_module = g_module_open (data->module_path, G_MODULE_BIND_LAZY);

  if (!data->open_module) {
    GST_WARNING ("Could not load importer: %s, reason: %s",
        data->module_path, g_module_error ());
    return FALSE;
  }
  GST_DEBUG ("Opened importer module");

  /* Make sure module stays loaded. Seems to be needed for
   * reusing exported symbols from the same module again */
  g_module_make_resident (data->open_module);

  return TRUE;
}

static void
_close_importer (GstClapperImporterData *data)
{
  if (!data || !data->open_module)
    return;

  if (G_LIKELY (g_module_close (data->open_module)))
    GST_DEBUG ("Closed module: %s", data->module_path);
  else
    GST_WARNING ("Could not close importer module");

  data->open_module = NULL;
}

static GstClapperImporter *
_obtain_importer_internal (GstClapperImporterData *data)
{
  MakeImporter make_importer;
  GstClapperImporter *importer = NULL;

  if (!_open_importer (data))
    goto finish;

  if (!g_module_symbol (data->open_module, "make_importer", (gpointer *) &make_importer)
      || make_importer == NULL) {
    GST_WARNING ("Make function missing in importer");
    goto fail;
  }

  /* Do not close the module, we are gonna continue using it */
  if ((importer = make_importer ()))
    goto finish;

fail:
  _close_importer (data);

finish:
  return importer;
}

static GstClapperImporterData *
_fill_importer_data (const gchar *module_path)
{
  MakeCaps make_caps;
  GstClapperImporterData *data;

  data = g_new0 (GstClapperImporterData, 1);
  data->module_path = g_strdup (module_path);
  data->open_module = g_module_open (data->module_path, G_MODULE_BIND_LAZY);

  if (!data->open_module)
    goto fail;

  if (!g_module_symbol (data->open_module, "make_caps", (gpointer *) &make_caps)
      || make_caps == NULL) {
    GST_WARNING ("Make caps function missing in importer");
    goto fail;
  }

  data->caps = make_caps (&data->rank, &data->context_types);
  GST_DEBUG ("Caps reading %ssuccessful", data->caps ? "" : "un");

  if (!data->caps)
    goto fail;

  /* Once we obtain importer data, close module afterwards */
  _close_importer (data);

  return data;

fail:
  gst_clapper_importer_data_free (data);

  return NULL;
}

static gint
_sort_importers_cb (gconstpointer a, gconstpointer b)
{
  GstClapperImporterData *data_a, *data_b;

  data_a = *((GstClapperImporterData **) a);
  data_b = *((GstClapperImporterData **) b);

  return (data_b->rank - data_a->rank);
}

static gpointer
_obtain_available_importers (G_GNUC_UNUSED gpointer data)
{
  GPtrArray *importers;
  GFile *dir;
  GFileEnumerator *dir_enum;
  GError *error = NULL;

  importers = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_clapper_importer_data_free);

  GST_INFO ("Checking available clapper sink importers");

  dir = g_file_new_for_path (CLAPPER_SINK_IMPORTER_PATH);

  if ((dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error))) {
    while (TRUE) {
      GFileInfo *info = NULL;
      GstClapperImporterData *data;
      gchar *module_path;
      const gchar *module_name;

      if (!g_file_enumerator_iterate (dir_enum, &info,
          NULL, NULL, &error) || !info)
        break;

      module_name = g_file_info_get_name (info);

      if (!g_str_has_suffix (module_name, G_MODULE_SUFFIX))
        continue;

      module_path = g_module_build_path (CLAPPER_SINK_IMPORTER_PATH, module_name);
      data = _fill_importer_data (module_path);
      g_free (module_path);

      if (!data) {
        GST_WARNING ("Could not read importer data: %s", module_name);
        continue;
      }

      GST_INFO ("Found importer: %s, caps: %" GST_PTR_FORMAT, module_name, data->caps);
      g_ptr_array_add (importers, data);
    }

    g_object_unref (dir_enum);
  }

  g_object_unref (dir);

  if (error) {
    GST_ERROR ("Could not load importer, reason: %s",
        (error->message) ? error->message : "unknown");
    g_error_free (error);
  }

  g_ptr_array_sort (importers, (GCompareFunc) _sort_importers_cb);

  return importers;
}

static const GPtrArray *
gst_clapper_importer_loader_get_available_importers (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, _obtain_available_importers, NULL);
  return (const GPtrArray *) once.retval;
}

static GstClapperImporterData *
_find_open_importer_data (const GPtrArray *importers)
{
  guint i;

  for (i = 0; i < importers->len; i++) {
    GstClapperImporterData *data = g_ptr_array_index (importers, i);

    if (data->open_module)
      return data;
  }

  return NULL;
}

static GstClapperImporterData *
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

static GstClapperImporterData *
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

void
gst_clapper_importer_loader_unload_all (void)
{
  const GPtrArray *importers;
  guint i;

  importers = gst_clapper_importer_loader_get_available_importers ();
  GST_TRACE ("Unloading all open modules");

  for (i = 0; i < importers->len; i++) {
    GstClapperImporterData *data = g_ptr_array_index (importers, i);

    _close_importer (data);
  }
}

GstPadTemplate *
gst_clapper_importer_loader_make_sink_pad_template (void)
{
  const GPtrArray *importers;
  GstCaps *sink_caps;
  GstPadTemplate *templ;
  guint i;

  /* This is only called once from sink class init function */
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperimporterloader", 0,
      "Clapper Importer Loader");

  importers = gst_clapper_importer_loader_get_available_importers ();
  sink_caps = gst_caps_new_empty ();

  for (i = 0; i < importers->len; i++) {
    GstClapperImporterData *data = g_ptr_array_index (importers, i);
    GstCaps *copied_caps;

    copied_caps = gst_caps_copy (data->caps);
    gst_caps_append (sink_caps, copied_caps);
  }

  if (G_UNLIKELY (gst_caps_is_empty (sink_caps)))
    gst_caps_append (sink_caps, gst_caps_new_any ());

  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, sink_caps);
  gst_caps_unref (sink_caps);

  return templ;
}

static gboolean
_find_importer_internal (GstCaps *caps, GstQuery *query, GstClapperImporter **importer)
{
  const GPtrArray *importers;
  GstClapperImporterData *old_data = NULL, *new_data = NULL;
  GstClapperImporter *found_importer = NULL;

  importers = gst_clapper_importer_loader_get_available_importers ();
  old_data = _find_open_importer_data (importers);

  if (caps) {
    GST_DEBUG ("Requested importer for caps: %" GST_PTR_FORMAT, caps);
    new_data = _get_importer_data_for_caps (importers, caps);
  } else if (query) {
    const gchar *context_type;

    gst_query_parse_context_type (query, &context_type);

    GST_DEBUG ("Requested importer for context: %s", context_type);
    new_data = _get_importer_data_for_context_type (importers, context_type);

    /* In case missing importer for context query, leave the old one.
     * We should allow some queries to go through unresponded */
    if (!new_data)
      new_data = old_data;
  }
  GST_LOG ("Old importer path: %s, new path: %s",
      (old_data != NULL) ? old_data->module_path : NULL,
      (new_data != NULL) ? new_data->module_path : NULL);

  if (old_data == new_data) {
    GST_DEBUG ("No importer change");

    if (*importer && caps)
      gst_clapper_importer_set_caps (*importer, caps);

    return (*importer != NULL);
  }

  if (new_data) {
    found_importer = _obtain_importer_internal (new_data);

    if (*importer && found_importer)
      gst_clapper_importer_share_data (*importer, found_importer);
  }

  gst_clear_object (importer);
  _close_importer (old_data);

  if (found_importer && gst_clapper_importer_prepare (found_importer)) {
    if (caps)
      gst_clapper_importer_set_caps (found_importer, caps);

    *importer = found_importer;
    return TRUE;
  }

  gst_clear_object (&found_importer);
  _close_importer (new_data);

  return FALSE;
}

gboolean
gst_clapper_importer_loader_find_importer_for_caps (GstCaps *caps, GstClapperImporter **importer)
{
  return _find_importer_internal (caps, NULL, importer);
}

gboolean
gst_clapper_importer_loader_find_importer_for_context_query (GstQuery *query, GstClapperImporter **importer)
{
  return _find_importer_internal (NULL, query, importer);
}
