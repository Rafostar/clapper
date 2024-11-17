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
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <clapper/clapper.h>

G_BEGIN_DECLS

#define CLAPPER_APP_TYPE_PIPELINE_VIEWER (clapper_app_pipeline_viewer_get_type())
#define CLAPPER_APP_PIPELINE_VIEWER_CAST(obj) ((ClapperAppPipelineViewer *)(obj))

G_DECLARE_FINAL_TYPE (ClapperAppPipelineViewer, clapper_app_pipeline_viewer, CLAPPER_APP, PIPELINE_VIEWER, GtkWidget)

void clapper_app_pipeline_viewer_set_player (ClapperAppPipelineViewer *pipeline_viewer, ClapperPlayer *player);

gboolean clapper_app_pipeline_viewer_focus (ClapperAppPipelineViewer *pipeline_viewer, const graphene_rect_t *viewport, gdouble zoom);

void clapper_app_pipeline_viewer_invalidate_viewport (ClapperAppPipelineViewer *pipeline_viewer);

gdouble clapper_app_pipeline_viewer_get_zoom (ClapperAppPipelineViewer *pipeline_viewer);

G_END_DECLS
