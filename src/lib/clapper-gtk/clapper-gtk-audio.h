/* Clapper GTK Integration Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined(__CLAPPER_GTK_INSIDE__) && !defined(CLAPPER_GTK_COMPILATION)
#error "Only <clapper-gtk/clapper-gtk.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <clapper-gtk/clapper-gtk-av.h>
#include <clapper-gtk/clapper-gtk-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_GTK_TYPE_AUDIO (clapper_gtk_audio_get_type())
#define CLAPPER_GTK_AUDIO_CAST(obj) ((ClapperGtkAudio *)(obj))

CLAPPER_GTK_API
G_DECLARE_FINAL_TYPE (ClapperGtkAudio, clapper_gtk_audio, CLAPPER_GTK, AUDIO, ClapperGtkAv)

CLAPPER_GTK_API
GtkWidget * clapper_gtk_audio_new (void);

CLAPPER_GTK_API
void clapper_gtk_audio_set_child (ClapperGtkAudio *audio, GtkWidget *child);

CLAPPER_GTK_API
GtkWidget * clapper_gtk_audio_get_child (ClapperGtkAudio *audio);

G_END_DECLS
