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

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define CLAPPER_APP_TYPE_PREFERENCES_WINDOW (clapper_app_preferences_window_get_type())
#define CLAPPER_APP_PREFERENCES_WINDOW_CAST(obj) ((ClapperAppPreferencesWindow *)(obj))

G_DECLARE_FINAL_TYPE (ClapperAppPreferencesWindow, clapper_app_preferences_window, CLAPPER_APP, PREFERENCES_WINDOW, AdwPreferencesWindow)

G_GNUC_INTERNAL
GtkWidget * clapper_app_preferences_window_new (GtkApplication *gtk_app);

G_END_DECLS
