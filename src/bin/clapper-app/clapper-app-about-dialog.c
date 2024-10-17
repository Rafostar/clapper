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
#include <gst/gst.h>
#include <clapper/clapper.h>
#include <adwaita.h>

#include "clapper-app-about-dialog.h"

GtkWidget *
clapper_app_about_dialog_new (void)
{
  AdwAboutDialog *about;
  GString *string;
  gchar *gst_ver, *debug_info;

  about = ADW_ABOUT_DIALOG (adw_about_dialog_new_from_appdata (
      CLAPPER_APP_RESOURCE_PREFIX "/data/metainfo/" CLAPPER_APP_ID ".metainfo.xml",
      NULL));

  /* Also show development versions */
  adw_about_dialog_set_version (about, CLAPPER_VERSION_S);

  /* TRANSLATORS: Put your name(s) here for credits or leave untranslated */
  adw_about_dialog_set_translator_credits (about, _("translator-credits"));

  string = g_string_new (NULL);

  g_string_append_printf (string, "GLib %u.%u.%u\n",
      glib_major_version,
      glib_minor_version,
      glib_micro_version);
  g_string_append_printf (string, "GTK %u.%u.%u\n",
      gtk_get_major_version (),
      gtk_get_minor_version (),
      gtk_get_micro_version ());
  g_string_append_printf (string, "Adwaita %u.%u.%u\n",
      adw_get_major_version (),
      adw_get_minor_version (),
      adw_get_micro_version ());

  gst_ver = gst_version_string ();
  g_string_append (string, gst_ver);
  g_free (gst_ver);

  debug_info = g_string_free_and_steal (string);
  adw_about_dialog_set_debug_info (about, debug_info);
  g_free (debug_info);

  return GTK_WIDGET (about);
}
