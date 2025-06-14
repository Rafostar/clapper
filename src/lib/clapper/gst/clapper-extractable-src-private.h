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
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_EXTRACTABLE_SRC (clapper_extractable_src_get_type())
#define CLAPPER_EXTRACTABLE_SRC_CAST(obj) ((ClapperExtractableSrc *)(obj))

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (ClapperExtractableSrc, clapper_extractable_src, CLAPPER, EXTRACTABLE_SRC, GstPushSrc)

GST_ELEMENT_REGISTER_DECLARE (clapperextractablesrc)

G_END_DECLS
