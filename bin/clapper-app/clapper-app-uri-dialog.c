/*
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <adwaita.h>
#include <gst/gst.h>

#include "clapper-app-uri-dialog.h"
#include "clapper-app-window.h"
#include "clapper-app-utils.h"

static void
_entry_text_changed_cb (GtkEntry *entry,
    GParamSpec *pspec G_GNUC_UNUSED, AdwMessageDialog *dialog)
{
  GtkEntryBuffer *buffer = gtk_entry_get_buffer (entry);
  guint text_length = gtk_entry_buffer_get_length (buffer);
  gboolean enabled = FALSE;

  if (text_length > 0) {
    const gchar *text = gtk_entry_buffer_get_text (buffer);
    enabled = (text && clapper_app_utils_uri_is_valid (text));
  }

  adw_message_dialog_set_response_enabled (dialog, "add", enabled);
}

static void
_open_uri_cb (AdwMessageDialog *dialog, GAsyncResult *result, GtkApplication *gtk_app)
{
  const gchar *response = adw_message_dialog_choose_finish (dialog, result);

  if (strcmp (response, "add") == 0) {
    GtkWidget *extra_child = adw_message_dialog_get_extra_child (dialog);
    GtkEntryBuffer *buffer = gtk_entry_get_buffer (GTK_ENTRY (extra_child));
    const gchar *text = gtk_entry_buffer_get_text (buffer);
    GFile **files;

    files = g_new (GFile *, 1);
    files[0] = g_file_new_for_uri (text);

    g_application_open (G_APPLICATION (gtk_app), files, 1, NULL);

    g_object_unref (files[0]);
    g_free (files);
  }
}

static void
_read_text_cb (GdkClipboard *clipboard, GAsyncResult *result, GtkWidget *extra_child)
{
  GtkEntry *entry = GTK_ENTRY (extra_child);
  GError *error = NULL;
  gchar *text = gdk_clipboard_read_text_finish (clipboard, result, &error);

  if (G_LIKELY (error == NULL)) {
    if (clapper_app_utils_uri_is_valid (text)) {
      gtk_editable_set_text (GTK_EDITABLE (entry), text);
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
    }
  } else {
    g_printerr ("Error: %s\n",
        (error->message) ? error->message : "Could not read clipboard");
    g_error_free (error);
  }

  g_free (text);
}

void
clapper_app_uri_dialog_open_uri (GtkApplication *gtk_app)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkBuilder *builder;
  AdwMessageDialog *dialog;
  GtkWidget *extra_child;
  GdkDisplay *display;

  builder = gtk_builder_new_from_resource (
      "/com/github/rafostar/Clapper/clapper-app/ui/clapper-app-uri-dialog.ui");

  dialog = ADW_MESSAGE_DIALOG (gtk_builder_get_object (builder, "dialog"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), window);

  extra_child = adw_message_dialog_get_extra_child (dialog);

  g_signal_connect (GTK_ENTRY (extra_child), "notify::text",
      G_CALLBACK (_entry_text_changed_cb), dialog);

  if ((display = gdk_display_get_default ())) {
    GdkClipboard *clipboard = gdk_display_get_clipboard (display);

    gdk_clipboard_read_text_async (clipboard, NULL,
        (GAsyncReadyCallback) _read_text_cb, extra_child);
  }

  /* NOTE: Dialog will automatically unref itself after response */
  adw_message_dialog_choose (dialog, NULL,
      (GAsyncReadyCallback) _open_uri_cb,
      gtk_app);

  g_object_unref (builder);
}
