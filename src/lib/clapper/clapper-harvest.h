/* Clapper Playback Library
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

#if !defined(__CLAPPER_INSIDE__) && !defined(CLAPPER_COMPILATION)
#error "Only <clapper/clapper.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <clapper/clapper-visibility.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_HARVEST (clapper_harvest_get_type())
#define CLAPPER_HARVEST_CAST(obj) ((ClapperHarvest *)(obj))

G_DECLARE_FINAL_TYPE (ClapperHarvest, clapper_harvest, CLAPPER, HARVEST, GstObject)

CLAPPER_API
gboolean clapper_harvest_fill (ClapperHarvest *harvest, const gchar *media_type, gpointer data, gsize size);

CLAPPER_API
gboolean clapper_harvest_fill_with_text (ClapperHarvest *harvest, const gchar *media_type, gchar *text);

CLAPPER_API
gboolean clapper_harvest_fill_with_bytes (ClapperHarvest *harvest, const gchar *media_type, GBytes *bytes);

CLAPPER_API
void clapper_harvest_tags_add (ClapperHarvest *harvest, const gchar *tag, ...) G_GNUC_NULL_TERMINATED;

CLAPPER_API
void clapper_harvest_tags_add_value (ClapperHarvest *harvest, const gchar *tag, const GValue *value);

CLAPPER_API
void clapper_harvest_toc_add (ClapperHarvest *harvest, GstTocEntryType type, const gchar *title, gdouble start, gdouble end);

CLAPPER_API
void clapper_harvest_headers_set (ClapperHarvest *harvest, const gchar *key, ...) G_GNUC_NULL_TERMINATED;

CLAPPER_API
void clapper_harvest_headers_set_value (ClapperHarvest *harvest, const gchar *key, const GValue *value);

G_END_DECLS
