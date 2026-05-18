/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "clapper-app-utils.h"
#include "clapper-app-media-item-box.h"

#ifdef HAVE_GRAPHVIZ
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#ifdef HAVE_WIN_PROCESS_THREADS_API
#include <processthreadsapi.h>
#endif
#ifdef HAVE_WIN_TIME_API
#include <timeapi.h>
#endif
#endif

#define GST_CAT_DEFAULT clapper_app_utils_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

void
clapper_app_utils_debug_init (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperapputils", 0,
      "Clapper App Utils");
}

/* Windows specific functions */
#ifdef G_OS_WIN32

/*
 * clapper_app_utils_win_enforce_hi_res_clock:
 *
 * Enforce high resolution clock by explicitly disabling Windows
 * timer resolution power throttling. When disabled, system remembers
 * and honors any previous timer resolution request by the process.
 *
 * By default, Windows 11 may automatically ignore the timer
 * resolution requests in certain scenarios.
 */
void
clapper_app_utils_win_enforce_hi_res_clock (void)
{
#ifdef HAVE_WIN_PROCESS_THREADS_API
  PROCESS_POWER_THROTTLING_STATE PowerThrottling;
  gboolean success;

  RtlZeroMemory (&PowerThrottling, sizeof (PowerThrottling));

  PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
  PowerThrottling.StateMask = 0; // Always honor timer resolution requests

  success = (gboolean) SetProcessInformation(
      GetCurrentProcess (),
      ProcessPowerThrottling,
      &PowerThrottling,
      sizeof (PowerThrottling));

  /* Not an error. Older Windows does not have this functionality, but
   * also honor hi-res clock by default anyway, so do not print then. */
  GST_INFO ("Windows hi-res clock support is %senforced",
      (success) ? "" : "NOT ");
#endif
}

/*
 * clapper_app_utils_win_hi_res_clock_start:
 *
 * Start Windows high resolution clock which will improve
 * accuracy of various Windows timer APIs and precision
 * of #GstSystemClock during playback.
 *
 * On Windows 10 version 2004 (and older), this function affects
 * a global Windows setting. On any other (newer) version this
 * will only affect a single process.
 *
 * Returns: Timer resolution period value.
 */
guint
clapper_app_utils_win_hi_res_clock_start (void)
{
  guint resolution = 0;

#ifdef HAVE_WIN_TIME_API
  TIMECAPS time_caps;
  MMRESULT res;

  if ((res = timeGetDevCaps (&time_caps, sizeof (TIMECAPS))) != TIMERR_NOERROR) {
    GST_WARNING ("Could not query timer resolution, code: %u", res);
    return 0;
  }

  resolution = MIN (MAX (time_caps.wPeriodMin, 1), time_caps.wPeriodMax);

  if ((res = timeBeginPeriod (resolution)) != TIMERR_NOERROR) {
    GST_WARNING ("Could not request timer resolution, code: %u", res);
    return 0;
  }

  GST_INFO ("Started Windows hi-res clock, precision: %ums", resolution);
#endif

  return resolution;
}

/*
 * clapper_app_utils_win_hi_res_clock_stop:
 * @resolution: started resolution value (non-zero)
 *
 * Stop previously started Microsoft Windows high resolution clock.
 */
void
clapper_app_utils_win_hi_res_clock_stop (guint resolution)
{
#ifdef HAVE_WIN_TIME_API
  MMRESULT res;

  if ((res = timeEndPeriod (resolution)) == TIMERR_NOERROR)
    GST_INFO ("Stopped Windows hi-res clock");
  else
    GST_ERROR ("Could not stop hi-res clock, code: %u", res);
#endif
}

/* Extensions are used only on Windows */
const gchar *const *
clapper_app_utils_get_extensions (void)
{
  static const gchar *const all_extensions[] = {
    "avi", "claps", "m2ts", "mkv", "mov",
    "mp4", "webm", "wmv", NULL
  };

  return all_extensions;
}

const gchar *const *
clapper_app_utils_get_subtitles_extensions (void)
{
  static const gchar *const subs_extensions[] = {
    "srt", "vtt", NULL
  };

  return subs_extensions;
}
#endif // G_OS_WIN32

const gchar *const *
clapper_app_utils_get_mime_types (void)
{
  static const gchar *const all_mime_types[] = {
    "video/*",
    "audio/*",
    "application/claps",
    "application/x-subrip",
    "text/x-ssa",
    NULL
  };

  return all_mime_types;
}

const gchar *const *
clapper_app_utils_get_subtitles_mime_types (void)
{
  static const gchar *const subs_mime_types[] = {
    "application/x-subrip",
    "text/x-ssa",
    NULL
  };

  return subs_mime_types;
}

void
clapper_app_utils_parse_progression (ClapperQueueProgressionMode mode,
    const gchar **icon, const gchar **label)
{
  const gchar *const icon_names[] = {
    "action-unavailable-symbolic",
    "media-playlist-consecutive-symbolic",
    "media-playlist-repeat-song-symbolic",
    "media-playlist-repeat-symbolic",
    "media-playlist-shuffle-symbolic",
    NULL
  };
  const gchar *const labels[] = {
    _("No progression"),
    _("Consecutive"),
    _("Repeat item"),
    _("Carousel"),
    _("Shuffle"),
    NULL
  };

  *icon = icon_names[mode];
  *label = labels[mode];
}

gboolean
clapper_app_utils_is_subtitles_file (GFile *file)
{
  GFileInfo *info;
  gboolean is_subs = FALSE;

  if ((info = g_file_query_info (file,
      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
      G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
      G_FILE_QUERY_INFO_NONE,
      NULL, NULL))) {
    const gchar *content_type = NULL;

    if (g_file_info_has_attribute (info,
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)) {
      content_type = g_file_info_get_content_type (info);
    } else if (g_file_info_has_attribute (info,
        G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE)) {
      content_type = g_file_info_get_attribute_string (info,
          G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
    }

    is_subs = (content_type && g_strv_contains (
        clapper_app_utils_get_subtitles_mime_types (),
        content_type));

    g_object_unref (info);
  }

  return is_subs;
}

gboolean
clapper_app_utils_is_media_file (GFile *file)
{
  GFileInfo *info;
  gboolean is_media = FALSE;

  if ((info = g_file_query_info (file,
      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
      G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
      G_FILE_QUERY_INFO_NONE,
      NULL, NULL))) {
    const gchar *content_type = NULL;

    if (g_file_info_has_attribute (info,
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)) {
      content_type = g_file_info_get_content_type (info);
    } else if (g_file_info_has_attribute (info,
        G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE)) {
      content_type = g_file_info_get_attribute_string (info,
          G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
    }

    if (content_type) {
      if (g_str_has_prefix (content_type, "video/") ||
          g_str_has_prefix (content_type, "audio/") ||
          g_strcmp0 (content_type, "application/claps") == 0) {
        is_media = TRUE;
      }
    }

    g_object_unref (info);
  }

  return is_media;
}

gboolean
clapper_app_utils_has_directory (GFile **files, gint n_files)
{
  gint i;
  for (i = 0; i < n_files; ++i) {
    if (files[i]) {
      GFileType type = g_file_query_file_type (files[i], G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);
      if (type == G_FILE_TYPE_DIRECTORY) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

typedef struct {
  GFile **files;
  gint n_files;
} ExpandData;

static void
expand_data_free (ExpandData *data)
{
  if (data->files) {
    for (gint i = 0; i < data->n_files; ++i) {
      g_clear_object (&data->files[i]);
    }
    g_free (data->files);
  }
  g_free (data);
}

static gint
_natural_compare (const gchar *s1, const gchar *s2)
{
  if (!s1 && !s2) return 0;
  if (!s1) return -1;
  if (!s2) return 1;

  while (*s1 && *s2) {
    if (g_ascii_isdigit (*s1) && g_ascii_isdigit (*s2)) {
      /* Skip leading zeros but keep track of them */
      const gchar *start1 = s1;
      const gchar *start2 = s2;
      const gchar *digits1;
      const gchar *digits2;
      gint len1;
      gint len2;
      const gchar *p1;
      const gchar *p2;
      gint zeros1;
      gint zeros2;

      while (*s1 == '0') s1++;
      while (*s2 == '0') s2++;

      /* Count number of remaining digits */
      digits1 = s1;
      digits2 = s2;
      while (g_ascii_isdigit (*s1)) s1++;
      while (g_ascii_isdigit (*s2)) s2++;

      len1 = s1 - digits1;
      len2 = s2 - digits2;

      /* If lengths are different, the one with more digits is larger */
      if (len1 != len2) {
        return (len1 < len2) ? -1 : 1;
      }

      /* If lengths are the same, compare them lexicographically (digit by digit) */
      p1 = digits1;
      p2 = digits2;
      while (p1 < s1) {
        if (*p1 != *p2) {
          return (*p1 < *p2) ? -1 : 1;
        }
        p1++;
        p2++;
      }

      /* If the numeric parts are equal, we compare the number of leading zeros.
       * The one with fewer leading zeros is sorted first. */
      zeros1 = digits1 - start1;
      zeros2 = digits2 - start2;
      if (zeros1 != zeros2) {
        return (zeros1 < zeros2) ? -1 : 1;
      }
    } else {
      gunichar c1 = g_utf8_get_char (s1);
      gunichar c2 = g_utf8_get_char (s2);

      if (c1 != c2) {
        gunichar fold1 = g_unichar_tolower (c1);
        gunichar fold2 = g_unichar_tolower (c2);
        if (fold1 != fold2) {
          return (fold1 < fold2) ? -1 : 1;
        }
      }
      s1 = g_utf8_next_char (s1);
      s2 = g_utf8_next_char (s2);
    }
  }

  if (*s1) return 1;
  if (*s2) return -1;
  return 0;
}

static gint
_compare_files_list (gconstpointer a, gconstpointer b)
{
  GFile *file_a = G_FILE (a);
  GFile *file_b = G_FILE (b);
  gchar *name_a = g_file_get_basename (file_a);
  gchar *name_b = g_file_get_basename (file_b);
  gint res = 0;

  if (name_a && name_b) {
    res = _natural_compare (name_a, name_b);
  } else if (name_a) {
    res = 1;
  } else if (name_b) {
    res = -1;
  }

  g_free (name_a);
  g_free (name_b);

  return res;
}

static void
_scan_directory_recursive (GFile *dir, GPtrArray *out_files, GHashTable *seen_uris, GCancellable *cancellable)
{
  GFileEnumerator *enumerator;
  GError *error = NULL;

  if (g_cancellable_is_cancelled (cancellable))
    return;

  enumerator = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME ","
      G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
      cancellable, &error);

  if (error) {
    g_warning ("Failed to enumerate directory: %s", error->message);
    g_error_free (error);
    return;
  }

  if (enumerator) {
    GList *dir_files = NULL;
    GFileInfo *info;

    while ((info = g_file_enumerator_next_file (enumerator, cancellable, NULL)) != NULL) {
      GFile *child = g_file_enumerator_get_child (enumerator, info);
      GFileType type = g_file_info_get_file_type (info);

      if (type == G_FILE_TYPE_DIRECTORY) {
        _scan_directory_recursive (child, out_files, seen_uris, cancellable);
        g_object_unref (child);
      } else {
        dir_files = g_list_prepend (dir_files, child);
      }
      g_object_unref (info);
    }
    g_object_unref (enumerator);

    /* Sort the files found in this specific directory naturally */
    dir_files = g_list_sort (dir_files, (GCompareFunc) _compare_files_list);

    for (GList *l = dir_files; l != NULL; l = l->next) {
      GFile *file = G_FILE (l->data);
      gchar *uri = g_file_get_uri (file);

      if (uri && !g_hash_table_contains (seen_uris, uri)) {
        if (clapper_app_utils_is_media_file (file)) {
          g_hash_table_add (seen_uris, g_strdup (uri));
          g_ptr_array_add (out_files, g_object_ref (file));
        }
      }
      g_free (uri);
    }
    g_list_free_full (dir_files, g_object_unref);
  }
}

static void
_expand_files_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
  ExpandData *data = (ExpandData *) task_data;
  GPtrArray *out_files = g_ptr_array_new_with_free_func (g_object_unref);
  GHashTable *seen_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  gint i;

  for (i = 0; i < data->n_files; ++i) {
    GFile *file;
    GFileType type;

    if (g_cancellable_is_cancelled (cancellable))
      break;

    file = data->files[i];
    if (!file)
      continue;

    type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable);
    if (type == G_FILE_TYPE_DIRECTORY) {
      _scan_directory_recursive (file, out_files, seen_uris, cancellable);
    } else {
      gchar *uri = g_file_get_uri (file);
      if (uri && !g_hash_table_contains (seen_uris, uri)) {
        if (clapper_app_utils_is_media_file (file) || clapper_app_utils_is_subtitles_file (file)) {
          g_hash_table_add (seen_uris, g_strdup (uri));
          g_ptr_array_add (out_files, g_object_ref (file));
        }
      }
      g_free (uri);
    }
  }

  g_hash_table_destroy (seen_uris);

  if (g_cancellable_is_cancelled (cancellable)) {
    g_ptr_array_free (out_files, TRUE);
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "File expansion cancelled");
  } else {
    g_task_return_pointer (task, out_files, (GDestroyNotify) g_ptr_array_unref);
  }
}

void
clapper_app_utils_expand_files_async (GFile **files, gint n_files,
    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  ExpandData *data = g_new0 (ExpandData, 1);
  gint i;

  data->n_files = n_files;
  data->files = g_new (GFile *, n_files);
  for (i = 0; i < n_files; ++i) {
    data->files[i] = g_object_ref (files[i]);
  }

  g_task_set_task_data (task, data, (GDestroyNotify) expand_data_free);
  g_task_run_in_thread (task, _expand_files_thread);
  g_object_unref (task);
}

gboolean
clapper_app_utils_expand_files_finish (GAsyncResult *result,
    GFile ***out_files, gint *out_n_files, GError **error)
{
  GTask *task = G_TASK (result);
  GPtrArray *arr = g_task_propagate_pointer (task, error);
  GFile **files;
  gint len;
  gint i;

  if (!arr) {
    if (out_files)
      *out_files = NULL;
    if (out_n_files)
      *out_n_files = 0;
    return FALSE;
  }

  len = arr->len;
  files = g_new (GFile *, len + 1);
  for (i = 0; i < len; ++i) {
    files[i] = g_object_ref (g_ptr_array_index (arr, i));
  }
  files[len] = NULL;

  g_ptr_array_unref (arr);

  if (out_files)
    *out_files = files;
  if (out_n_files)
    *out_n_files = len;

  return TRUE;
}

gboolean
clapper_app_utils_value_for_item_is_valid (const GValue *value)
{
  if (G_VALUE_HOLDS (value, GTK_TYPE_WIDGET))
    return CLAPPER_APP_IS_MEDIA_ITEM_BOX (g_value_get_object (value));

  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)
      || G_VALUE_HOLDS (value, G_TYPE_FILE))
    return TRUE;

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    return gst_uri_is_valid (g_value_get_string (value));

  return FALSE;
}

gboolean
clapper_app_utils_files_from_list_model (GListModel *files_model, GFile ***files, gint *n_files)
{
  guint i, len = g_list_model_get_n_items (files_model);

  if (G_UNLIKELY (len == 0 || len > G_MAXINT))
    return FALSE;

  *files = g_new (GFile *, len + 1);

  if (n_files)
    *n_files = (gint) len;

  for (i = 0; i < len; ++i) {
    (*files)[i] = g_list_model_get_item (files_model, i);
  }
  (*files)[i] = NULL;

  return TRUE;
}

gboolean
clapper_app_utils_files_from_slist (GSList *file_list, GFile ***files, gint *n_files)
{
  GSList *fl;
  guint len, i = 0;

  len = g_slist_length (file_list);

  if (G_UNLIKELY (len == 0 || len > G_MAXINT))
    return FALSE;

  *files = g_new (GFile *, len + 1);

  if (n_files)
    *n_files = (gint) len;

  for (fl = file_list; fl != NULL; fl = fl->next) {
    (*files)[i] = (GFile *) g_object_ref (fl->data);
    i++;
  }
  (*files)[i] = NULL;

  return TRUE;
}

gboolean
clapper_app_utils_files_from_string (const gchar *string, GFile ***files, gint *n_files)
{
  GSList *slist = NULL;
  gchar **uris = g_strsplit (string, "\n", 0);
  guint i;
  gboolean success;

  for (i = 0; uris[i]; ++i) {
    const gchar *uri = uris[i];

    if (!gst_uri_is_valid (uri))
      continue;

    slist = g_slist_append (slist, g_file_new_for_uri (uri));
  }

  g_strfreev (uris);

  if (!slist)
    return FALSE;

  success = clapper_app_utils_files_from_slist (slist, files, n_files);
  g_slist_free_full (slist, g_object_unref);

  return success;
}

gboolean
clapper_app_utils_files_from_command_line (GApplicationCommandLine *cmd_line, GFile ***files, gint *n_files)
{
  GSList *slist = NULL;
  gchar **argv;
  gint i, argc = 0;
  gboolean success;

  argv = g_application_command_line_get_arguments (cmd_line, &argc);

  for (i = 1; i < argc; ++i)
    slist = g_slist_append (slist, g_application_command_line_create_file_for_arg (cmd_line, argv[i]));

  g_strfreev (argv);

  if (!slist)
    return FALSE;

  success = clapper_app_utils_files_from_slist (slist, files, n_files);
  g_slist_free_full (slist, g_object_unref);

  return success;
}

static inline gboolean
_files_from_file (GFile *file, GFile ***files, gint *n_files)
{
  *files = g_new (GFile *, 2);

  (*files)[0] = g_object_ref (file);
  (*files)[1] = NULL;

  if (n_files)
    *n_files = 1;

  return TRUE;
}

gboolean
clapper_app_utils_files_from_value (const GValue *value, GFile ***files, gint *n_files)
{
  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)) {
    return clapper_app_utils_files_from_slist (
        (GSList *) g_value_get_boxed (value), files, n_files);
  } else if (G_VALUE_HOLDS (value, G_TYPE_FILE)) {
    return _files_from_file (
        (GFile *) g_value_get_object (value), files, n_files);
  } else if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
    return clapper_app_utils_files_from_string (
        g_value_get_string (value), files, n_files);
  }

  return FALSE;
}

void
clapper_app_utils_files_free (GFile **files)
{
  gint i;

  for (i = 0; files[i]; ++i)
    g_object_unref (files[i]);

  g_free (files);
}

static inline gboolean
_parse_feature_name (gchar *str, const gchar **feature_name)
{
  if (!str)
    return FALSE;

  g_strstrip (str);

  if (str[0] == '\0')
    return FALSE;

  *feature_name = str;
  return TRUE;
}

static inline gboolean
_parse_feature_rank (gchar *str, GstRank *rank)
{
  if (!str)
    return FALSE;

  g_strstrip (str);

  if (str[0] == '\0')
    return FALSE;

  if (g_ascii_isdigit (str[0])) {
    gulong l;
    gchar *endptr;

    l = strtoul (str, &endptr, 10);
    if (endptr > str && endptr[0] == 0) {
      *rank = (GstRank) l;
    } else {
      return FALSE;
    }
  } else if (g_ascii_strcasecmp (str, "NONE") == 0) {
    *rank = GST_RANK_NONE;
  } else if (g_ascii_strcasecmp (str, "MARGINAL") == 0) {
    *rank = GST_RANK_MARGINAL;
  } else if (g_ascii_strcasecmp (str, "SECONDARY") == 0) {
    *rank = GST_RANK_SECONDARY;
  } else if (g_ascii_strcasecmp (str, "PRIMARY") == 0) {
    *rank = GST_RANK_PRIMARY;
  } else if (g_ascii_strcasecmp (str, "MAX") == 0) {
    *rank = (GstRank) G_MAXINT;
  } else {
    return FALSE;
  }

  return TRUE;
}

void
clapper_app_utils_iterate_plugin_feature_ranks (GSettings *settings,
    ClapperAppUtilsIterRanks callback, gpointer user_data)
{
  gchar **split, **walk, *stored_overrides;
  const gchar *env_overrides;
  gboolean from_env = FALSE;

  stored_overrides = g_settings_get_string (settings, "plugin-feature-ranks");
  env_overrides = g_getenv ("GST_PLUGIN_FEATURE_RANK");

  /* Iterate from GSettings, then from ENV */
parse_overrides:
  split = g_strsplit ((from_env) ? env_overrides : stored_overrides, ",", 0);

  for (walk = split; *walk; walk++) {
    gchar **values;

    if (!strchr (*walk, ':'))
      continue;

    values = g_strsplit (*walk, ":", 2);

    if (g_strv_length (values) == 2) {
      GstRank rank;
      const gchar *feature_name;

      if (_parse_feature_name (values[0], &feature_name)
          && _parse_feature_rank (values[1], &rank))
        callback (feature_name, rank, from_env, user_data);
    }

    g_strfreev (values);
  }

  g_strfreev (split);

  if (!from_env && env_overrides) {
    from_env = TRUE;
    goto parse_overrides;
  }

  g_free (stored_overrides);
}

GstElement *
clapper_app_utils_make_element (const gchar *string)
{
  gchar *char_loc;

  if (strcmp (string, "none") == 0)
    return NULL;

  char_loc = strchr (string, ' ');

  if (char_loc) {
    GstElement *element;
    GError *error = NULL;

    element = gst_parse_bin_from_description (string, TRUE, &error);
    if (error) {
      GST_ERROR ("Bin parse error: \"%s\", reason: %s", string, error->message);
      g_error_free (error);
    }

    return element;
  }

  return gst_element_factory_make (string, NULL);
}

/*
 * _get_tmp_dir:
 * @subdir: (nullable): an optional subdirectory
 *
 * Returns: (transfer full): a newly constructed #GFile
 */
static inline GFile *
_get_tmp_dir (const gchar *subdir)
{
  /* XXX: System tmp directory does not work within containers such as Flatpak
   * for our usage with file launcher, so make our own temp in app data dir */
  return g_file_new_build_filename (
      g_get_user_data_dir (), CLAPPER_APP_ID, "tmp", subdir, NULL);
}

#ifdef HAVE_GRAPHVIZ
static GFile *
_create_tmp_subdir (const gchar *subdir, GCancellable *cancellable, GError **error)
{
  GFile *tmp_dir;
  GError *my_error = NULL;

  tmp_dir = _get_tmp_dir (subdir);

  if (!g_file_make_directory_with_parents (tmp_dir, cancellable, &my_error)) {
    if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_EXISTS) {
      *error = g_error_copy (my_error);
      g_clear_object (&tmp_dir); // return NULL
    }
    g_error_free (my_error);
  }

  return tmp_dir;
}
#endif

static void
_create_pipeline_svg_file_in_thread (GTask *task, GObject *source G_GNUC_UNUSED,
    ClapperPlayer *player, GCancellable *cancellable)
{
  GFile *tmp_file = NULL;
  GError *error = NULL;

#ifdef HAVE_GRAPHVIZ
  GFile *tmp_subdir;
  Agraph_t *graph;
  GVC_t *gvc;
  gchar *path, *template = NULL, *dot_data = NULL, *img_data = NULL;
  gint fd;
  gsize size = 0;

  if (!(tmp_subdir = _create_tmp_subdir ("pipelines", cancellable, &error)))
    goto finish;

  path = g_file_get_path (tmp_subdir);
  g_object_unref (tmp_subdir);

  template = g_build_filename (path, "pipeline-XXXXXX.svg", NULL);
  g_free (path);

  fd = g_mkstemp (template); // Modifies template to actual filename

  if (G_UNLIKELY (fd == -1)) {
    g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "Could not open temp file for writing");
    goto finish;
  }

  dot_data = clapper_player_make_pipeline_graph (player, GST_DEBUG_GRAPH_SHOW_ALL);

  if (g_cancellable_is_cancelled (cancellable))
    goto close_and_finish;

  graph = agmemread (dot_data);

  gvc = gvContext ();
  gvLayout (gvc, graph, "dot");

#ifdef HAVE_GVC_13
  gvRenderData (gvc, graph, "svg", &img_data, &size);
#else
  {
    guint tmp_size = 0; // Temporary uint to satisfy older API
    gvRenderData (gvc, graph, "svg", &img_data, &tmp_size);
    size = tmp_size;
  }
#endif

  agclose (graph);
  gvFreeContext (gvc);

  if (g_cancellable_is_cancelled (cancellable))
    goto close_and_finish;

  if (write (fd, img_data, size) != -1) {
    tmp_file = g_file_new_for_path (template);
  } else {
    g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "Could not write data to temp file");
  }

close_and_finish:
  /* Always close the file IO */
  if (G_UNLIKELY (close (fd) == -1))
    GST_ERROR ("Could not close temp file!");

finish:
  g_free (template);
  g_free (dot_data);
  g_free (img_data);
#else
  g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
      "Cannot create graph file when compiled without Graphviz");
#endif

  if (tmp_file)
    g_task_return_pointer (task, tmp_file, (GDestroyNotify) g_object_unref);
  else
    g_task_return_error (task, error);
}

void
clapper_app_utils_create_pipeline_svg_file_async (ClapperPlayer *player,
    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task;

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, gst_object_ref (player), (GDestroyNotify) gst_object_unref);
  g_task_run_in_thread (task, (GTaskThreadFunc) _create_pipeline_svg_file_in_thread);

  g_object_unref (task);
}

static gboolean
_delete_dir_recursive (GFile *dir, GError **error)
{
  GFileEnumerator *dir_enum;

  if ((dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error))) {
    while (TRUE) {
      GFileInfo *info = NULL;
      GFile *child = NULL;

      if (!g_file_enumerator_iterate (dir_enum, &info,
          &child, NULL, error) || !info)
        break;

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
        if (!_delete_dir_recursive (child, error))
          break;
      } else if (!g_file_delete (child, NULL, error)) {
        break;
      }
    }

    g_object_unref (dir_enum);
  }

  if (*error != NULL)
    return FALSE;

  return g_file_delete (dir, NULL, error);
}

void
clapper_app_utils_delete_tmp_dir (void)
{
  GFile *tmp_dir = _get_tmp_dir (NULL);
  GError *error = NULL;

  if (!_delete_dir_recursive (tmp_dir, &error)) {
    if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_NOT_FOUND) {
      GST_ERROR ("Could not remove temp dir, reason: %s",
          GST_STR_NULL (error->message));
    }
    g_error_free (error);
  }

  g_object_unref (tmp_dir);
}
