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

#define __CLAPPER_INSIDE__

#include <clapper/clapper-visibility.h>

#include <clapper/clapper-enums.h>
#include <clapper/clapper-version.h>

#include <clapper/clapper-audio-stream.h>
#include <clapper/clapper-basic-functions.h>
#include <clapper/clapper-enhancer-proxy.h>
#include <clapper/clapper-enhancer-proxy-list.h>
#include <clapper/clapper-feature.h>
#include <clapper/clapper-harvest.h>
#include <clapper/clapper-marker.h>
#include <clapper/clapper-media-item.h>
#include <clapper/clapper-player.h>
#include <clapper/clapper-queue.h>
#include <clapper/clapper-stream.h>
#include <clapper/clapper-stream-list.h>
#include <clapper/clapper-subtitle-stream.h>
#include <clapper/clapper-threaded-object.h>
#include <clapper/clapper-timeline.h>
#include <clapper/clapper-utils.h>
#include <clapper/clapper-video-stream.h>

#include <clapper/clapper-extractable.h>
#include <clapper/clapper-reactable.h>

#include <clapper/clapper-functionalities-availability.h>
#include <clapper/features/clapper-features-availability.h>

#if CLAPPER_HAVE_DISCOVERER
#include <clapper/features/discoverer/clapper-discoverer.h>
#endif
#if CLAPPER_HAVE_MPRIS
#include <clapper/features/mpris/clapper-mpris.h>
#endif
#if CLAPPER_HAVE_SERVER
#include <clapper/features/server/clapper-server.h>
#endif

#undef __CLAPPER_INSIDE__
