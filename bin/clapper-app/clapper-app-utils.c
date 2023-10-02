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

#include <gtk/gtk.h>

#include "clapper-app-utils.h"
#include "clapper-app-media-item-box.h"

gboolean
clapper_app_utils_uri_is_valid (const gchar *uri)
{
  /* FIXME: Improve validation */
  return gst_uri_is_valid (uri);
}

gboolean
clapper_app_utils_value_for_item_is_valid (const GValue *value)
{
  if (G_VALUE_HOLDS (value, GTK_TYPE_WIDGET))
    return CLAPPER_APP_IS_MEDIA_ITEM_BOX (g_value_get_object (value));

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    return TRUE;

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    return clapper_app_utils_uri_is_valid (g_value_get_string (value));

  return FALSE;
}

ClapperMediaItem *
clapper_app_utils_media_item_from_value (const GValue *value)
{
  ClapperMediaItem *item = NULL;

  if (G_VALUE_HOLDS (value, G_TYPE_FILE)) {
    GFile *file = g_value_get_object (value);

    item = clapper_media_item_new_from_file (file);
  } else if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
    const gchar *uri = g_value_get_string (value);

    if (clapper_app_utils_uri_is_valid (uri))
      item = clapper_media_item_new (uri);
  }

  return item;
}
