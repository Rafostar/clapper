/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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
#include <gst/gst.h>
#include <clapper/clapper-media-item.h>

G_BEGIN_DECLS

#define CLAPPER_TYPE_DISCOVERER (clapper_discoverer_get_type())
#define CLAPPER_DISCOVERER_CAST(obj) ((ClapperDiscoverer *)(obj))

/**
 * ClapperDiscoverer:
 *
 * Clapper media scanner used to discover media item info.
 *
 * Once media is scanned, all extra information of it will be filled
 * within media item, this includes title, duration, chapters, etc.
 *
 * For performance reasons, #ClapperDiscoverer will create a thread pool
 * with an optimal number of threads for current hardware it is run in.
 * All discovery is then done asynchronously using said threads and queued
 * for discovery later if all threads are currently occupied.
 *
 * Please note that media items are also discovered during their playback
 * within using the player itself. Discoverer is useful in situations where
 * one wants to present to the user an updated media item before playback,
 * thus mainly to be applied into e.g. an UI with playback queue.
 */
G_DECLARE_FINAL_TYPE (ClapperDiscoverer, clapper_discoverer, CLAPPER, DISCOVERER, GstObject)

ClapperDiscoverer * clapper_discoverer_new (void);

void clapper_discoverer_discover_item (ClapperDiscoverer *discoverer, ClapperMediaItem *item);

G_END_DECLS
