/* Clapper Tube Library
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

#if !defined(__CLAPPER_TUBE_INSIDE__) && !defined(CLAPPER_TUBE_COMPILATION)
#error "Only <clapper-tube/clapper-tube.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#include <clapper-tube/clapper-tube-visibility.h>
#include <clapper-tube/clapper-tube-enums.h>
#include <clapper-tube/clapper-tube-extractor.h>
#include <clapper-tube/clapper-tube-harvest.h>
#include <clapper-tube/clapper-tube-cache.h>
#include <clapper-tube/clapper-tube-config.h>

G_BEGIN_DECLS

#define CLAPPER_TUBE_TYPE_WEB_EXTRACTOR (clapper_tube_web_extractor_get_type())
#define CLAPPER_TUBE_WEB_EXTRACTOR_CAST(obj) ((ClapperTubeWebExtractor *)(obj))

CLAPPER_TUBE_API
G_DECLARE_DERIVABLE_TYPE (ClapperTubeWebExtractor, clapper_tube_web_extractor, CLAPPER_TUBE, WEB_EXTRACTOR, ClapperTubeExtractor)

/**
 * ClapperTubeWebExtractorClass:
 * @parent_class: The object class structure.
 * @create_request: Create a #SoupMessage to send.
 * @read_response: Read response of prevoiusly sent #SoupMessage.
 */
struct _ClapperTubeWebExtractorClass
{
  ClapperTubeExtractorClass parent_class;

  /**
   * ClapperTubeWebExtractorClass::create_session:
   * @web_extractor: a #ClapperTubeWebExtractor
   *
   * Create a custom #SoupSession to use (optional).
   *
   * Returns: (transfer full): a #SoupSession.
   */
  SoupSession * (*create_session) (ClapperTubeWebExtractor *web_extractor);

  /**
   * ClapperTubeWebExtractorClass::create_request:
   * @web_extractor: a #ClapperTubeWebExtractor
   * @msg: (out) (optional) (transfer full): return location for #SoupMessage
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Create a #SoupMessage to send.
   *
   * Returns: a #ClapperTubeFlow of message creation.
   */
  ClapperTubeFlow (* create_request) (ClapperTubeWebExtractor *web_extractor, SoupMessage **msg, GCancellable *cancellable, GError **error);

  /**
   * ClapperTubeWebExtractorClass::read_response:
   * @web_extractor: a #ClapperTubeWebExtractor
   * @msg: a #SoupMessage
   * @stream: a #GInputStream
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Read response of prevoiusly sent #SoupMessage.
   *
   * Returns: a #ClapperTubeFlow of @stream data extraction.
   */
  ClapperTubeFlow (* read_response) (ClapperTubeWebExtractor *web_extractor, SoupMessage *msg, GInputStream *stream, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[4];
};

G_END_DECLS
