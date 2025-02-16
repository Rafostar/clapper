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

#include <gio/gio.h>
#include <adwaita.h>
#include <gst/gst.h>

#include "clapper-app-uri-dialog.h"
#include "clapper-app-utils.h"

static void
_entry_text_changed_cb (AdwEntryRow *entry,
    GParamSpec *pspec G_GNUC_UNUSED, AdwAlertDialog *dialog)
{
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry));
  gboolean enabled = FALSE;

  if (text && *text != '\0') {
    enabled = gst_uri_is_valid (text);
  }

  adw_alert_dialog_set_response_enabled (dialog, "add", enabled);
}

static void
_open_uri_cb (AdwAlertDialog *dialog, GAsyncResult *result, GtkApplication *gtk_app)
{
  const gchar *response = adw_alert_dialog_choose_finish (dialog, result);

  if (strcmp (response, "add") == 0) {
    GtkWidget *extra_child = adw_alert_dialog_get_extra_child (dialog);
    AdwEntryRow *entry_row = ADW_ENTRY_ROW (gtk_list_box_get_row_at_index (GTK_LIST_BOX (extra_child), 0));
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry_row));
    GFile **files = NULL;
    gint n_files = 0;

    if (clapper_app_utils_files_from_string (text, &files, &n_files)) {
      g_application_open (G_APPLICATION (gtk_app), files, n_files, "add-only");
      clapper_app_utils_files_free (files);
    }
  }
}

static void
_read_text_cb (GdkClipboard *clipboard, GAsyncResult *result, GtkWidget *entry_row)
{
  GError *error = NULL;
  gchar *text = gdk_clipboard_read_text_finish (clipboard, result, &error);

  if (G_LIKELY (error == NULL)) {
    if (gst_uri_is_valid (text)) {
      gtk_editable_set_text (GTK_EDITABLE (entry_row), text);
      gtk_editable_select_region (GTK_EDITABLE (entry_row), 0, -1);
    }
  } else {
    /* Common error when clipboard is empty or has unsupported content.
     * This we can safely ignore and not notify user. */
    if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_NOT_SUPPORTED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not read clipboard");
    }
    g_error_free (error);
  }

  g_free (text);
}

void
clapper_app_uri_dialog_open_uri (GtkApplication *gtk_app)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkBuilder *builder;
  AdwAlertDialog *dialog;
  GtkWidget *entry_row;
  GdkDisplay *display;

  builder = gtk_builder_new_from_resource (
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-uri-dialog.ui");

  dialog = ADW_ALERT_DIALOG (gtk_builder_get_object (builder, "dialog"));
  entry_row = GTK_WIDGET (gtk_builder_get_object (builder, "entry_row"));

  g_signal_connect (GTK_EDITABLE (entry_row), "notify::text",
      G_CALLBACK (_entry_text_changed_cb), dialog);

  if ((display = gdk_display_get_default ())) {
    GdkClipboard *clipboard = gdk_display_get_clipboard (display);

    gdk_clipboard_read_text_async (clipboard, NULL,
        (GAsyncReadyCallback) _read_text_cb, entry_row);
  }

  /* NOTE: Dialog will automatically unref itself after response */
  adw_alert_dialog_choose (dialog, GTK_WIDGET (window), NULL,
      (GAsyncReadyCallback) _open_uri_cb,
      gtk_app);

  g_object_unref (builder);
}
