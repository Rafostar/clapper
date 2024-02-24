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

#include <adwaita.h>

#include "clapper-app-about-window.h"

GtkWidget *
clapper_app_about_window_new (GtkApplication *gtk_app)
{
  AdwAboutWindow *about;
  GtkWindow *window;

/* FIXME: Only use from appdata, once done */
#if ADW_CHECK_VERSION(1,4,0)
  about = ADW_ABOUT_WINDOW (adw_about_window_new_from_appdata (
      "/com/github/rafostar/Clapper/clapper-app/data/metainfo/com.github.rafostar.Clapper.metainfo.xml",
      NULL));
#else
  about = ADW_ABOUT_WINDOW (adw_about_window_new ());
#endif
  window = gtk_application_get_active_window (gtk_app);

  gtk_window_set_modal (GTK_WINDOW (about), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (about), window);

  //adw_about_window_set_translator_credits (about, _("translator-credits"));

  return GTK_WIDGET (about);
}
