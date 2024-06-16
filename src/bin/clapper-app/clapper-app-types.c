/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "clapper-app-types.h"

#include "clapper-app-headerbar.h"
#include "clapper-app-media-item-box.h"
#include "clapper-app-property-row.h"
#include "clapper-app-queue-list.h"
#include "clapper-app-queue-progression-model.h"
#include "clapper-app-window-state-buttons.h"

/*
 * clapper_app_types_init:
 *
 * Ensure private types that appear in UI files in order for
 * GtkBuilder to be able to find them when building templates.
 */
inline void
clapper_app_types_init (void)
{
  g_type_ensure (CLAPPER_APP_TYPE_HEADERBAR);
  g_type_ensure (CLAPPER_APP_TYPE_MEDIA_ITEM_BOX);
  g_type_ensure (CLAPPER_APP_TYPE_PROPERTY_ROW);
  g_type_ensure (CLAPPER_APP_TYPE_QUEUE_LIST);
  g_type_ensure (CLAPPER_APP_TYPE_QUEUE_PROGRESSION_MODEL);
  g_type_ensure (CLAPPER_APP_TYPE_WINDOW_STATE_BUTTONS);
}
