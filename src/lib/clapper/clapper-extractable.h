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

#define CLAPPER_TYPE_EXTRACTABLE (clapper_extractable_get_type())
#define CLAPPER_EXTRACTABLE_CAST(obj) ((ClapperExtractable *)(obj))

CLAPPER_API
G_DECLARE_INTERFACE (ClapperExtractable, clapper_extractable, CLAPPER, EXTRACTABLE, GObject)

/**
 * ClapperExtractableInterface:
 * @parent_iface: The parent interface structure.
 * @extract: Extract data and fill harvest.
 */
struct _ClapperExtractableInterface
{
  GTypeInterface parent_iface;

  /**
   * ClapperExtractableInterface::extract:
   * @extractable: a #ClapperExtractable
   * @uri: a #GUri
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Extract data and fill harvest.
   *
   * Returns: whether extraction was successful.
   *
   * Since: 0.8
   */
  gboolean (* extract) (ClapperExtractable *extractable, GUri *uri, ClapperHarvest *harvest, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[8];
};

G_END_DECLS
