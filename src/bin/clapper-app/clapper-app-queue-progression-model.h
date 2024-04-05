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

#pragma once

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CLAPPER_APP_TYPE_QUEUE_PROGRESSION_MODEL (clapper_app_queue_progression_model_get_type())
#define CLAPPER_APP_QUEUE_PROGRESSION_MODEL_CAST(obj) ((ClapperAppQueueProgressionModel *)(obj))

G_DECLARE_FINAL_TYPE (ClapperAppQueueProgressionModel, clapper_app_queue_progression_model, CLAPPER_APP, QUEUE_PROGRESSION_MODEL, GObject)

G_END_DECLS
