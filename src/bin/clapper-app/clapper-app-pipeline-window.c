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

#include "config.h"

#include "clapper-app-pipeline-window.h"
#include "clapper-app-pipeline-viewer.h"

#define GST_CAT_DEFAULT clapper_app_pipeline_window_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppPipelineWindow
{
  AdwWindow parent;

  ClapperAppPipelineViewer *pipeline_viewer;
};

#define parent_class clapper_app_pipeline_window_parent_class
G_DEFINE_TYPE (ClapperAppPipelineWindow, clapper_app_pipeline_window, ADW_TYPE_WINDOW);

static gboolean
close_cb (GtkWidget *widget, GVariant *args G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED)
{
  gtk_window_close (GTK_WINDOW (widget));

  return TRUE;
}

GtkWidget *
clapper_app_pipeline_window_new (GtkApplication *gtk_app, ClapperPlayer *player)
{
  ClapperAppPipelineWindow *window;

  window = g_object_new (CLAPPER_APP_TYPE_PIPELINE_WINDOW,
      "application", gtk_app,
      "transient-for", gtk_application_get_active_window (gtk_app),
      NULL);
  clapper_app_pipeline_viewer_set_player (window->pipeline_viewer, player);

  return GTK_WIDGET (window);
}

static void
clapper_app_pipeline_window_init (ClapperAppPipelineWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clapper_app_pipeline_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_PIPELINE_WINDOW);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_pipeline_window_finalize (GObject *object)
{
  ClapperAppPipelineWindow *self = CLAPPER_APP_PIPELINE_WINDOW_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_pipeline_window_class_init (ClapperAppPipelineWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperapppipelinewindow", 0,
      "Clapper App Pipeline Window");

  gobject_class->dispose = clapper_app_pipeline_window_dispose;
  gobject_class->finalize = clapper_app_pipeline_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-pipeline-window.ui");

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Escape, 0, close_cb, NULL);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppPipelineWindow, pipeline_viewer);

  //gtk_widget_class_bind_template_callback (widget_class, refresh_button_clicked_cb);
}
