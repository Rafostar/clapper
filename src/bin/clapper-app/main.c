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

#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <clapper/clapper.h>

#include "clapper-app-application.h"
#include "clapper-app-types.h"

gint
main (gint argc, gchar **argv)
{
  const gchar *clapper_ldir;
  GApplication *application;
  gint status;

  g_setenv ("GSK_RENDERER", "gl", FALSE);

  setlocale (LC_ALL, "");
  if (!(clapper_ldir = g_getenv ("CLAPPER_APP_OVERRIDE_LOCALEDIR")))
    clapper_ldir = LOCALEDIR;
  bindtextdomain (GETTEXT_PACKAGE, clapper_ldir);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  clapper_init (&argc, &argv);
  gtk_init ();
  adw_init ();

  clapper_app_types_init ();

  g_set_application_name ("Clapper");

  application = clapper_app_application_new ();

  status = g_application_run (application, argc, argv);
  g_object_unref (application);

  return status;
}
