/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <clapper/clapper-feature.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_MPRIS (clapper_mpris_get_type())
#define CLAPPER_MPRIS_CAST(obj) ((ClapperMpris *)(obj))

/**
 * ClapperMpris:
 *
 * Represents a MPRIS feature.
 */
G_DECLARE_FINAL_TYPE (ClapperMpris, clapper_mpris, CLAPPER, MPRIS, ClapperFeature)

ClapperMpris * clapper_mpris_new (const gchar *own_name, const gchar *identity, const gchar *desktop_entry);

void clapper_mpris_set_art_url (ClapperMpris *mpris, const gchar *art_url);

gchar * clapper_mpris_get_art_url (ClapperMpris *mpris);

void clapper_mpris_set_fallback_art_url (ClapperMpris *mpris, const gchar *art_url);

gchar * clapper_mpris_get_fallback_art_url (ClapperMpris *mpris);

G_END_DECLS
