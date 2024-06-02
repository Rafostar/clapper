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

#include <clapper-tube/clapper-tube-enum-types.h>

G_BEGIN_DECLS

/**
 * ClapperTubeExtractorError:
 * @CLAPPER_TUBE_EXTRACTOR_ERROR_FAILED: could not perform extraction.
 * @CLAPPER_TUBE_EXTRACTOR_ERROR_OTHER: some other error occured.
 */
typedef enum
{
  CLAPPER_TUBE_EXTRACTOR_ERROR_FAILED,
  CLAPPER_TUBE_EXTRACTOR_ERROR_OTHER,
} ClapperTubeExtractorError;

G_END_DECLS
