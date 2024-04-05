/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GST_GDK_MEMORY_ENDIAN_FORMATS      "RGBA64_LE"
#define GST_GDK_GL_TEXTURE_ENDIAN_FORMATS  "RGBA64_LE"
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define GST_GDK_MEMORY_ENDIAN_FORMATS      "RGBA64_BE"
#define GST_GDK_GL_TEXTURE_ENDIAN_FORMATS  "RGBA64_BE"
#endif

#define GST_GDK_MEMORY_FORMATS                       \
    GST_GDK_MEMORY_ENDIAN_FORMATS ", "               \
    "ABGR, BGRA, ARGB, RGBA, BGRx, RGBx, BGR, RGB"

/* Formats that `GdkGLTexture` supports */
#define GST_GDK_GL_TEXTURE_FORMATS                   \
    GST_GDK_GL_TEXTURE_ENDIAN_FORMATS ", "           \
    "RGBA, RGBx, RGB"
