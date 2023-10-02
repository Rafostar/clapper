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

#include <gtk/gtk.h>
#include <adwaita.h>

#include <clapper/clapper.h>
#include <clapper-gtk/clapper-gtk.h>

#include "clapper-app-application.h"

gint
main (gint argc, gchar **argv)
{
  GApplication *application;
  gint status;

  clapper_init (&argc, &argv);
  gtk_init ();
  adw_init ();

  g_set_application_name ("Clapper");

  application = clapper_app_application_new ();

  status = g_application_run (application, argc, argv);
  g_object_unref (application);

  return status;
}
