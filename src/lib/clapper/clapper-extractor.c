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

/**
 * ClapperExtractor:
 *
 * An interface for creating extractor type of addons.
 *
 * Each extractor must implement at least [vfunc@Clapper.Extractor.query]
 * and [vfunc@Clapper.Extractor.extract] to be usable.
 *
 * Since: 0.8
 */

#include "clapper-extractor-private.h"
#include "clapper-harvest-private.h"

G_DEFINE_INTERFACE (ClapperExtractor, clapper_extractor, G_TYPE_OBJECT);

static void
clapper_extractor_default_init (ClapperExtractorInterface *iface)
{
}

gboolean
clapper_extractor_query (ClapperExtractor *self, GUri *uri)
{
  ClapperExtractorInterface *iface = CLAPPER_EXTRACTOR_GET_IFACE (self);
  gboolean supported = FALSE;

  if (G_LIKELY (iface->query))
    supported = iface->query (self, uri);

  return supported;
}

gboolean
clapper_extractor_extract (ClapperExtractor *self, ClapperHarvest *harvest,
    GCancellable *cancellable, GError **error)
{
  ClapperExtractorInterface *iface = CLAPPER_EXTRACTOR_GET_IFACE (self);
  gboolean success = FALSE;

  if (G_LIKELY (iface->extract))
    success = iface->extract (self, harvest, cancellable, error);

  return success;
}

guint
clapper_extractor_get_update_interval (ClapperExtractor *self)
{
  ClapperExtractorInterface *iface = CLAPPER_EXTRACTOR_GET_IFACE (self);
  guint interval = 0;

  /* No point scheduling update if it is not implemented, so check both */
  if (iface->get_update_interval && iface->update)
    interval = iface->get_update_interval (self);

  return interval;
}

gboolean
clapper_extractor_update (ClapperExtractor *self, ClapperHarvest *harvest,
    GCancellable *cancellable, GError **error)
{
  ClapperExtractorInterface *iface = CLAPPER_EXTRACTOR_GET_IFACE (self);
  gboolean success = FALSE;

  /* It is possible that extractor requests update manually
   * (without implementing interval), so a check is needed. */
  if (iface->update)
    success = iface->update (self, harvest, cancellable, error);

  return success;
}
