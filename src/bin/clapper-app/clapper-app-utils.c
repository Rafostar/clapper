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
