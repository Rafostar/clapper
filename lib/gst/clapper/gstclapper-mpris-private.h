/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_CLAPPER_MPRIS_PRIVATE_H__
#define __GST_CLAPPER_MPRIS_PRIVATE_H__

#include <gst/clapper/gstclapper-mpris.h>
#include <gst/clapper/gstclapper.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
void gst_clapper_mpris_set_clapper                  (GstClapperMpris *self, GstClapper *clapper,
                                                        GstClapperSignalDispatcher *signal_dispatcher);

G_GNUC_INTERNAL
void gst_clapper_mpris_set_media_info               (GstClapperMpris *self, GstClapperMediaInfo *info);

G_GNUC_INTERNAL
void gst_clapper_mpris_set_playback_status          (GstClapperMpris *self, const gchar *status);

G_GNUC_INTERNAL
void gst_clapper_mpris_set_position                 (GstClapperMpris *self, gint64 position);

G_END_DECLS

#endif /* __GST_CLAPPER_MPRIS_PRIVATE_H__ */
