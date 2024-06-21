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

#include <gio/gio.h>

#include "clapper-app-file-dialog.h"
#include "clapper-app-utils.h"

static inline void
_open_files_from_model (GtkApplication *gtk_app, GListModel *files_model)
{
  GFile **files = NULL;
  gint n_files = 0;

  if (clapper_app_utils_files_from_list_model (files_model, &files, &n_files)) {
    g_application_open (G_APPLICATION (gtk_app), files, n_files, "add-only");
    clapper_app_utils_files_free (files);
  }
}

static void
_open_files_cb (GtkFileDialog *dialog, GAsyncResult *result, GtkApplication *gtk_app)
{
  GError *error = NULL;
  GListModel *files_model = gtk_file_dialog_open_multiple_finish (dialog, result, &error);

  if (G_LIKELY (error == NULL)) {
    _open_files_from_model (gtk_app, files_model);
  } else {
    if (error->domain != GTK_DIALOG_ERROR || error->code != GTK_DIALOG_ERROR_DISMISSED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not open file dialog");
    }
    g_error_free (error);
  }
  g_clear_object (&files_model);
}

static void
_open_subtitles_cb (GtkFileDialog *dialog, GAsyncResult *result, ClapperMediaItem *item)
{
  GError *error = NULL;
  GFile *file = gtk_file_dialog_open_finish (dialog, result, &error);

  if (G_LIKELY (error == NULL)) {
    gchar *suburi = g_file_get_uri (file);

    clapper_media_item_set_suburi (item, suburi);
    g_free (suburi);
  } else {
    if (error->domain != GTK_DIALOG_ERROR || error->code != GTK_DIALOG_ERROR_DISMISSED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not open file dialog");
    }
    g_error_free (error);
  }
  g_clear_object (&file);
  gst_object_unref (item); // Borrowed reference
}

static void
_dialog_add_mime_types (GtkFileDialog *dialog, const gchar *filter_name,
    const gchar *const *mime_types)
{
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  GtkFileFilter *filter = gtk_file_filter_new ();
  guint i;

  /* XXX: Windows does not support mime-types file
   * filters, so use file extensions instead */
  for (i = 0; mime_types[i]; ++i) {
#ifndef G_OS_WIN32
    gtk_file_filter_add_mime_type (filter, mime_types[i]);
#else
    gtk_file_filter_add_suffix (filter, mime_types[i]);
#endif
  }

  gtk_file_filter_set_name (filter, filter_name);
  g_list_store_append (filters, filter);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  g_object_unref (filters);
  g_object_unref (filter);
}

void
clapper_app_file_dialog_open_files (GtkApplication *gtk_app)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  _dialog_add_mime_types (dialog, "Media Files",
#ifndef G_OS_WIN32
      clapper_app_utils_get_mime_types ());
#else
      clapper_app_utils_get_extensions ());
#endif

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, "Add Files");

  gtk_file_dialog_open_multiple (dialog, window, NULL,
      (GAsyncReadyCallback) _open_files_cb,
      gtk_app);

  g_object_unref (dialog);
}

void
clapper_app_file_dialog_open_subtitles (GtkApplication *gtk_app, ClapperMediaItem *item)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  _dialog_add_mime_types (dialog, "Subtitles",
#ifndef G_OS_WIN32
      clapper_app_utils_get_subtitles_mime_types ());
#else
      clapper_app_utils_get_subtitles_extensions ());
#endif

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, "Open Subtitles");

  gtk_file_dialog_open (dialog, window, NULL,
      (GAsyncReadyCallback) _open_subtitles_cb,
      gst_object_ref (item));

  g_object_unref (dialog);
}
