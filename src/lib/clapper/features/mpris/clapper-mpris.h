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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-feature.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_MPRIS (clapper_mpris_get_type())
#define CLAPPER_MPRIS_CAST(obj) ((ClapperMpris *)(obj))

CLAPPER_DEPRECATED
G_DECLARE_FINAL_TYPE (ClapperMpris, clapper_mpris, CLAPPER, MPRIS, ClapperFeature)

CLAPPER_DEPRECATED
ClapperMpris * clapper_mpris_new (const gchar *own_name, const gchar *identity, const gchar *desktop_entry);

CLAPPER_DEPRECATED
void clapper_mpris_set_queue_controllable (ClapperMpris *mpris, gboolean controllable);

CLAPPER_DEPRECATED
gboolean clapper_mpris_get_queue_controllable (ClapperMpris *mpris);

CLAPPER_DEPRECATED
void clapper_mpris_set_fallback_art_url (ClapperMpris *mpris, const gchar *art_url);

CLAPPER_DEPRECATED
gchar * clapper_mpris_get_fallback_art_url (ClapperMpris *mpris);

G_END_DECLS
