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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/gstbin.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_URI_BASE_DEMUX (clapper_uri_base_demux_get_type())
#define CLAPPER_URI_BASE_DEMUX_CAST(obj) ((ClapperUriBaseDemux *)(obj))

G_GNUC_INTERNAL
G_DECLARE_DERIVABLE_TYPE (ClapperUriBaseDemux, clapper_uri_base_demux, CLAPPER, URI_BASE_DEMUX, GstBin)

struct _ClapperUriBaseDemuxClass
{
  GstBinClass parent_class;

  gboolean (* process_buffer) (ClapperUriBaseDemux *uri_bd, GstBuffer *buffer, GCancellable *cancellable);

  void (* handle_caps) (ClapperUriBaseDemux *uri_bd, GstCaps *caps);

  void (* handle_custom_event) (ClapperUriBaseDemux *uri_bd, GstEvent *event);

  gboolean (* handle_custom_query) (ClapperUriBaseDemux *uri_bd, GstQuery *query);
};

gboolean clapper_uri_base_demux_set_uri (ClapperUriBaseDemux *uri_bd, const gchar *uri, const gchar *blacklisted_el);

G_END_DECLS
