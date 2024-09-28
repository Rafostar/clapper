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
 * ClapperExtractable:
 *
 * An interface for creating enhancers that resolve given URI into something playable.
 *
 * Since: 0.8
 */

#include <gst/gst.h>

#include "clapper-extractable-private.h"
#include "clapper-harvest-private.h"

G_DEFINE_INTERFACE (ClapperExtractable, clapper_extractable, G_TYPE_OBJECT);

static gboolean
clapper_extractable_default_extract (ClapperExtractable *self, GUri *uri,
    ClapperHarvest *harvest, GCancellable *cancellable, GError **error)
{
  if (*error == NULL) {
    g_set_error (error, GST_CORE_ERROR,
        GST_CORE_ERROR_NOT_IMPLEMENTED,
        "Extractable object did not implement extract function");
  }

  return FALSE;
}

static void
clapper_extractable_default_init (ClapperExtractableInterface *iface)
{
  iface->extract = clapper_extractable_default_extract;
}

gboolean
clapper_extractable_extract (ClapperExtractable *self, GUri *uri,
    ClapperHarvest *harvest, GCancellable *cancellable, GError **error)
{
  ClapperExtractableInterface *iface = CLAPPER_EXTRACTABLE_GET_IFACE (self);

  return iface->extract (self, uri, harvest, cancellable, error);
}
