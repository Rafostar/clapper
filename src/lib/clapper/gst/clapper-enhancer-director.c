/* Clapper Playback Library
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gst/gst.h>

#include "clapper-enhancer-director-private.h"
#include "../clapper-basic-functions.h"
#include "../clapper-cache-private.h"
#include "../clapper-enhancer-proxy-private.h"
#include "../clapper-extractable-private.h"
#include "../clapper-harvest-private.h"
#include "../clapper-utils.h"
#include "../../shared/clapper-shared-utils-private.h"

#include "../clapper-functionalities-availability.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include "../clapper-enhancers-loader-private.h"
#endif

#define CLEANUP_INTERVAL 10800 // once every 3 hours

#define GST_CAT_DEFAULT clapper_enhancer_director_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperEnhancerDirector
{
  ClapperThreadedObject parent;
};

#define parent_class clapper_enhancer_director_parent_class
G_DEFINE_TYPE (ClapperEnhancerDirector, clapper_enhancer_director, CLAPPER_TYPE_THREADED_OBJECT);

typedef struct
{
  ClapperEnhancerDirector *director;
  GList *filtered_proxies;
  GUri *uri;
  GCancellable *cancellable;
  GError **error;
} ClapperEnhancerDirectorData;

static gpointer
clapper_enhancer_director_extract_in_thread (ClapperEnhancerDirectorData *data)
{
  ClapperEnhancerDirector *self = data->director;
  GList *el;
  ClapperHarvest *harvest = NULL;
  gboolean success = FALSE;

  GST_DEBUG_OBJECT (self, "Extraction start");

  /* Cancelled during thread switching */
  if (g_cancellable_is_cancelled (data->cancellable))
    return NULL;

  GST_DEBUG_OBJECT (self, "Enhancer proxies for URI: %u",
      g_list_length (data->filtered_proxies));

  for (el = data->filtered_proxies; el; el = g_list_next (el)) {
    ClapperEnhancerProxy *proxy = CLAPPER_ENHANCER_PROXY_CAST (el->data);
    ClapperExtractable *extractable = NULL;
    GstStructure *config;

    harvest = clapper_harvest_new (); // fresh harvest for each iteration
    config = clapper_enhancer_proxy_make_current_config (proxy);

    if ((success = clapper_harvest_fill_from_cache (harvest, proxy, config, data->uri))
        || g_cancellable_is_cancelled (data->cancellable)) { // Check before extract
      gst_clear_structure (&config);
      break;
    }

#if CLAPPER_WITH_ENHANCERS_LOADER
    extractable = CLAPPER_EXTRACTABLE_CAST (
        clapper_enhancers_loader_create_enhancer (proxy, CLAPPER_TYPE_EXTRACTABLE));
#endif

    if (extractable) {
      if (config)
        clapper_enhancer_proxy_apply_config_to_enhancer (proxy, config, (GObject *) extractable);

      success = clapper_extractable_extract (extractable, data->uri,
          harvest, data->cancellable, data->error);
      gst_object_unref (extractable);

      /* We are done with extractable, but keep harvest and try to cache it */
      if (success) {
        if (!g_cancellable_is_cancelled (data->cancellable))
          clapper_harvest_export_to_cache (harvest, proxy, config, data->uri);

        gst_clear_structure (&config);
        break;
      }
    }

    /* Cleanup to try again with next enhancer */
    g_clear_object (&harvest);
    gst_clear_structure (&config);
  }

  /* Cancelled during extraction or exporting to cache */
  if (g_cancellable_is_cancelled (data->cancellable))
    success = FALSE;

  if (!success) {
    gst_clear_object (&harvest);

    /* Ensure we have some error set on failure */
    if (*data->error == NULL) {
      const gchar *err_msg = (g_cancellable_is_cancelled (data->cancellable))
          ? "Extraction was cancelled"
          : "Extraction failed";
      g_set_error (data->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_FAILED, "%s", err_msg);
    }
  }

  GST_DEBUG_OBJECT (self, "Extraction finish");

  return harvest;
}

static inline void
_harvest_delete_if_expired (ClapperEnhancerDirector *self,
    ClapperEnhancerProxy *proxy, GFile *file, const gint64 epoch_now)
{
  GMappedFile *mapped_file;
  const gchar *data;
  gchar *filename;
  GError *error = NULL;
  gboolean delete = TRUE;

  filename = g_file_get_path (file);

  if ((mapped_file = clapper_cache_open (filename, &data, &error))) {
    /* Do not delete if versions match and not expired */
    if (g_strcmp0 (clapper_cache_read_string (&data),
        clapper_enhancer_proxy_get_version (proxy)) == 0
        && clapper_cache_read_int64 (&data) > epoch_now) {
      delete = FALSE;
    }
    g_mapped_file_unref (mapped_file);
  } else if (error) {
    if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT)
      GST_DEBUG_OBJECT (self, "No cached harvest file found");
    else
      GST_ERROR_OBJECT (self, "Could not read cached harvest file, reason: %s", error->message);

    g_clear_error (&error);
  }

  if (delete) {
    if (G_LIKELY (g_file_delete (file, NULL, &error))) {
      GST_TRACE_OBJECT (self, "Deleted cached harvest: \"%s\"", filename);
    } else {
      GST_ERROR_OBJECT (self, "Could not delete harvest: \"%s\", reason: %s",
          filename, GST_STR_NULL (error->message));
      g_error_free (error);
    }
  }

  g_free (filename);
}

static inline void
_cache_proxy_harvests_cleanup (ClapperEnhancerDirector *self,
    ClapperEnhancerProxy *proxy, const gint64 epoch_now)
{
  GFile *dir;
  GFileEnumerator *dir_enum;
  GError *error = NULL;

  dir = g_file_new_build_filename (g_get_user_cache_dir (), CLAPPER_API_NAME,
      "enhancers", clapper_enhancer_proxy_get_module_name (proxy),
      "harvests", NULL);

  if ((dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error))) {
    while (TRUE) {
      GFileInfo *info = NULL;
      GFile *child = NULL;

      if (!g_file_enumerator_iterate (dir_enum, &info,
          &child, NULL, &error) || !info)
        break;

      if (G_LIKELY (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
          && g_str_has_suffix (g_file_info_get_name (info), ".bin")))
        _harvest_delete_if_expired (self, proxy, child, epoch_now);
    }

    g_object_unref (dir_enum);
  }

  if (error) {
    if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_NOT_FOUND) {
      gchar *path = g_file_get_path (dir);

      GST_ERROR_OBJECT (self, "Could not cleanup in dir: \"%s\", reason: %s",
          path, GST_STR_NULL (error->message));
      g_free (path);
    }

    g_error_free (error);
  }

  g_object_unref (dir);
}

static gboolean
_cache_cleanup_func (ClapperEnhancerDirector *self)
{
  GMappedFile *mapped_file;
  GDateTime *date;
  GError *error = NULL;
  gchar *filename;
  const gchar *data;
  gint64 since_cleanup, epoch_now, epoch_last = 0;

  date = g_date_time_new_now_utc ();
  epoch_now = g_date_time_to_unix (date);
  g_date_time_unref (date);

  filename = g_build_filename (g_get_user_cache_dir (), CLAPPER_API_NAME,
      "enhancers", "cleanup.bin", NULL);

  if ((mapped_file = clapper_cache_open (filename, &data, &error))) {
    epoch_last = clapper_cache_read_int64 (&data);
    g_mapped_file_unref (mapped_file);
  } else if (error) {
    if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT)
      GST_DEBUG_OBJECT (self, "No cache cleanup file found");
    else
      GST_ERROR_OBJECT (self, "Could not read cache cleanup file, reason: %s", error->message);

    g_clear_error (&error);
  }

  since_cleanup = epoch_now - epoch_last;

  if (since_cleanup >= CLEANUP_INTERVAL) {
    ClapperEnhancerProxyList *proxies;
    guint i, n_proxies;
    GByteArray *bytes;

    GST_TRACE_OBJECT (self, "Time for cache cleanup, last was %"
        CLAPPER_TIME_FORMAT " ago", CLAPPER_TIME_ARGS (since_cleanup));

    /* Start with writing to cache cleanup time,
     * so other directors can find it earlier */
    if ((bytes = clapper_cache_create ())) {
      clapper_cache_store_int64 (bytes, epoch_now);

      if (clapper_cache_write (filename, bytes, &error)) {
        GST_TRACE_OBJECT (self, "Written data to cache cleanup file, cleanup time: %"
            G_GINT64_FORMAT, epoch_now);
      } else if (error) {
        GST_ERROR_OBJECT (self, "Could not write cache cleanup data, reason: %s", error->message);
        g_clear_error (&error);
      }

      g_byte_array_free (bytes, TRUE);
    }

    /* Now do cleanup */
    proxies = clapper_get_global_enhancer_proxies ();
    n_proxies = clapper_enhancer_proxy_list_get_n_proxies (proxies);

    for (i = 0; i < n_proxies; ++i) {
      ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (proxies, i);

      if (!clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_EXTRACTABLE))
        continue;

      _cache_proxy_harvests_cleanup (self, proxy, epoch_now);
    }
  } else {
    GST_TRACE_OBJECT (self, "No cache cleanup yet, last was %"
        CLAPPER_TIME_FORMAT " ago", CLAPPER_TIME_ARGS (since_cleanup));
  }

  g_free (filename);

  return G_SOURCE_REMOVE;
}

/*
 * clapper_enhancer_director_new:
 *
 * Returns: (transfer full): a new #ClapperEnhancerDirector instance.
 */
ClapperEnhancerDirector *
clapper_enhancer_director_new (void)
{
  ClapperEnhancerDirector *director;

  director = g_object_new (CLAPPER_TYPE_ENHANCER_DIRECTOR, NULL);
  gst_object_ref_sink (director);

  return director;
}

ClapperHarvest *
clapper_enhancer_director_extract (ClapperEnhancerDirector *self,
    GList *filtered_proxies, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  ClapperEnhancerDirectorData *data = g_new (ClapperEnhancerDirectorData, 1);
  GMainContext *context;
  ClapperHarvest *harvest;

  data->director = self;
  data->filtered_proxies = filtered_proxies;
  data->uri = uri;
  data->cancellable = cancellable;
  data->error = error;

  context = clapper_threaded_object_get_context (CLAPPER_THREADED_OBJECT_CAST (self));

  harvest = CLAPPER_HARVEST_CAST (clapper_shared_utils_context_invoke_sync_full (context,
      (GThreadFunc) clapper_enhancer_director_extract_in_thread,
      data, (GDestroyNotify) g_free));

  /* Run cleanup async. Since context belongs to "self", do not ref it.
   * This ensures clean shutdown with thread stop function called. */
  if (!g_cancellable_is_cancelled (cancellable) && !clapper_cache_is_disabled ())
    g_main_context_invoke (context, (GSourceFunc) _cache_cleanup_func, self);

  return harvest;
}

static void
clapper_enhancer_director_thread_start (ClapperThreadedObject *threaded_object)
{
  GST_TRACE_OBJECT (threaded_object, "Enhancer director thread start");
}

static void
clapper_enhancer_director_thread_stop (ClapperThreadedObject *threaded_object)
{
  GST_TRACE_OBJECT (threaded_object, "Enhancer director thread stop");
}

static void
clapper_enhancer_director_init (ClapperEnhancerDirector *self)
{
}

static void
clapper_enhancer_director_finalize (GObject *object)
{
  GST_TRACE_OBJECT (object, "Finalize");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_enhancer_director_class_init (ClapperEnhancerDirectorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancerdirector", 0,
      "Clapper Enhancer Director");

  gobject_class->finalize = clapper_enhancer_director_finalize;

  threaded_object->thread_start = clapper_enhancer_director_thread_start;
  threaded_object->thread_stop = clapper_enhancer_director_thread_stop;
}
