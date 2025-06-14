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
#include <gst/gst.h>

#include "clapper-timeline.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
ClapperTimeline * clapper_timeline_new (void);

G_GNUC_INTERNAL
gboolean clapper_timeline_set_toc (ClapperTimeline *timeline, GstToc *toc, gboolean updated);

G_GNUC_INTERNAL
void clapper_timeline_refresh (ClapperTimeline *timeline);

G_END_DECLS
