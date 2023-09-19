/*
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

#if !defined(__CLAPPER_GTK_INSIDE__) && !defined(CLAPPER_GTK_COMPILATION)
#error "Only <clapper-gtk/clapper-gtk.h> can be included directly."
#endif

#include <clapper-gtk/clapper-gtk-enum-types.h>

G_BEGIN_DECLS

/**
 * ClapperGtkVideoActionMask:
 * @CLAPPER_GTK_VIDEO_ACTION_NONE: no action
 * @CLAPPER_GTK_VIDEO_ACTION_TOGGLE_PLAY: toggle playback
 * @CLAPPER_GTK_VIDEO_ACTION_TOGGLE_FULLSCREEN: toggle fullscreen
 * @CLAPPER_GTK_VIDEO_ACTION_REVEAL_OVERLAYS: reveal fading overlays
 * @CLAPPER_GTK_VIDEO_ACTION_ANY: all of the above
 */
typedef enum
{
  CLAPPER_GTK_VIDEO_ACTION_NONE = 0,
  CLAPPER_GTK_VIDEO_ACTION_TOGGLE_PLAY = 1 << 0,
  CLAPPER_GTK_VIDEO_ACTION_TOGGLE_FULLSCREEN = 1 << 1,
  CLAPPER_GTK_VIDEO_ACTION_REVEAL_OVERLAYS = 1 << 2,
  CLAPPER_GTK_VIDEO_ACTION_ANY = 0x3FFFFFF
} ClapperGtkVideoActionMask;

G_END_DECLS
