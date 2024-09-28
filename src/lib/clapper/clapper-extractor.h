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
#include <gio/gio.h>

#include <clapper/clapper-visibility.h>
#include <clapper/clapper-harvest.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_EXTRACTOR (clapper_extractor_get_type())
#define CLAPPER_EXTRACTOR_CAST(obj) ((ClapperExtractor *)(obj))
#define CLAPPER_EXTRACTOR_ERROR (clapper_extractor_error_quark ())

CLAPPER_API
G_DECLARE_INTERFACE (ClapperExtractor, clapper_extractor, CLAPPER, EXTRACTOR, GObject)

/**
 * ClapperExtractorInterface:
 * @parent_iface: The parent interface structure.
 * @query: Check compatibility with given URI (required).
 * @extract: Extract data and fill harvest (required).
 * @get_update_interval: Get time in milliseconds after which next update should be called (optional).
 * @update: Update harvested tags, toc or request-headers (optional).
 */
struct _ClapperExtractorInterface
{
  GTypeInterface parent_iface;

  /**
   * ClapperExtractorInterface::query:
   * @extractor: a #ClapperExtractor
   * @uri: a #GUri
   *
   * Check compatibility with given URI (required).
   *
   * An extractor query function will be called only if URI scheme and host is matched
   * (as defined in plugin file). Implementations should check if @uri as a whole can
   * be handled.
   *
   * At this stage extractor can take useful info from @uri (e.g. video ID).
   *
   * Returns: whether extractor supports given URI.
   */
  gboolean (* query) (ClapperExtractor *extractor, GUri *uri);

  /**
   * ClapperExtractorInterface::extract:
   * @extractor: a #ClapperExtractor
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Extract data and fill harvest (required).
   *
   * Returns: whether extraction was successful.
   */
  gboolean (* extract) (ClapperExtractor *extractor, ClapperHarvest *harvest, GCancellable *cancellable, GError **error);

  /**
   * ClapperExtractorInterface::get_update_interval:
   * @extractor: a #ClapperExtractor
   *
   * Get time in milliseconds after which next update should be called (optional).
   *
   * Implement this in order for [vfunc@Clapper.Extractor.update] method to be called periodically.
   * If this is not implemented or returns zero, no updates will be done automatically.
   *
   * Returns: update interval in milliseconds.
   */
  guint (* get_update_interval) (ClapperExtractor *extractor);

  /**
   * ClapperExtractorInterface::update:
   * @extractor: a #ClapperExtractor
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Update harvested tags, toc or request-headers (optional).
   *
   * Implement this in order to change harvested media info after initial extraction.
   * Useful for dealing with live sources where e.g. media title changes during playback.
   *
   * Returns: whether the harvest was updated.
   */
  gboolean (* update) (ClapperExtractor *extractor, ClapperHarvest *harvest, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[8];
};

G_END_DECLS
