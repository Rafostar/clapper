/* Clapper GTK Integration Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "clapper-gtk-version.h"

/**
 * clapper_gtk_get_major_version:
 *
 * ClapperGtk runtime major version component
 *
 * This returns the ClapperGtk library version your code is
 * running against unlike [const@ClapperGtk.MAJOR_VERSION]
 * which represents compile time version.
 *
 * Returns: the major version number of the ClapperGtk library
 *
 * Since: 0.10
 */
guint
clapper_gtk_get_major_version (void)
{
  return CLAPPER_GTK_MAJOR_VERSION;
}

/**
 * clapper_gtk_get_minor_version:
 *
 * ClapperGtk runtime minor version component
 *
 * This returns the ClapperGtk library version your code is
 * running against unlike [const@ClapperGtk.MINOR_VERSION]
 * which represents compile time version.
 *
 * Returns: the minor version number of the ClapperGtk library
 *
 * Since: 0.10
 */
guint
clapper_gtk_get_minor_version (void)
{
  return CLAPPER_GTK_MINOR_VERSION;
}

/**
 * clapper_gtk_get_micro_version:
 *
 * ClapperGtk runtime micro version component
 *
 * This returns the ClapperGtk library version your code is
 * running against unlike [const@ClapperGtk.MICRO_VERSION]
 * which represents compile time version.
 *
 * Returns: the micro version number of the ClapperGtk library
 *
 * Since: 0.10
 */
guint
clapper_gtk_get_micro_version (void)
{
  return CLAPPER_GTK_MICRO_VERSION;
}

/**
 * clapper_gtk_get_version_s:
 *
 * ClapperGtk runtime version as string
 *
 * This returns the ClapperGtk library version your code is
 * running against unlike [const@ClapperGtk.VERSION_S]
 * which represents compile time version.
 *
 * Returns: the version of the ClapperGtk library as string
 *
 * Since: 0.10
 */
const gchar *
clapper_gtk_get_version_s (void)
{
  return CLAPPER_GTK_VERSION_S;
}
