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

#include <gst/gst.h>
#include <clapper-gtk/clapper-gtk.h>

#include "clapper-app-application.h"
#include "clapper-app-window.h"
#include "clapper-app-file-dialog.h"
#include "clapper-app-uri-dialog.h"
#include "clapper-app-info-window.h"
#include "clapper-app-about-window.h"

#define GST_CAT_DEFAULT clapper_app_application_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppApplication
{
  GtkApplication parent;

  gboolean need_init_state;
};

typedef struct
{
  const gchar *action;
  const gchar *accels[3];
} ClapperAppShortcut;

#define parent_class clapper_app_application_parent_class
G_DEFINE_TYPE (ClapperAppApplication, clapper_app_application, GTK_TYPE_APPLICATION);

static void
_assemble_initial_state (GtkWindow *window)
{
  GtkWidget *stack = gtk_window_get_child (window);
  GtkBuilder *builder = gtk_builder_new_from_resource (
      "/com/github/rafostar/Clapper/clapper-app/ui/clapper-app-initial-state.ui");
  GtkWidget *initial_state = GTK_WIDGET (gtk_builder_get_object (builder, "initial_state"));

  gtk_stack_add_named (GTK_STACK (stack), initial_state, "initial_state");
  gtk_stack_set_visible_child (GTK_STACK (stack), initial_state);

  g_object_unref (builder);
}

static void
_ensure_no_initial_state (GtkWindow *window)
{
  GtkWidget *stack = gtk_window_get_child (window);
  const gchar *child_name = gtk_stack_get_visible_child_name (GTK_STACK (stack));

  if (g_strcmp0 (child_name, "initial_state") == 0) {
    gtk_stack_set_visible_child (GTK_STACK (stack),
        clapper_app_window_get_video (CLAPPER_APP_WINDOW (window)));
  }
}

static void
add_files (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  GtkApplication *gtk_app = GTK_APPLICATION (user_data);

  clapper_app_file_dialog_open_files (gtk_app);
}

static void
add_uri (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  GtkApplication *gtk_app = GTK_APPLICATION (user_data);

  clapper_app_uri_dialog_open_uri (gtk_app);
}

static void
show_info (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  GtkApplication *gtk_app = GTK_APPLICATION (user_data);
  GtkWidget *info_window;
  GtkWindow *window;
  ClapperPlayer *player;

  window = gtk_application_get_active_window (gtk_app);
  player = clapper_app_window_get_player (CLAPPER_APP_WINDOW (window));

  info_window = clapper_app_info_window_new (gtk_app, player);
  gtk_window_present (GTK_WINDOW (info_window));
}

static void
show_about (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  GtkApplication *gtk_app = GTK_APPLICATION (user_data);
  GtkWidget *about_window;

  about_window = clapper_app_about_window_new (gtk_app);
  gtk_window_present (GTK_WINDOW (about_window));
}

GApplication *
clapper_app_application_new (void)
{
  return g_object_new (CLAPPER_APP_TYPE_APPLICATION,
      "application-id", "com.github.rafostar.Clapper",
      "flags", G_APPLICATION_HANDLES_OPEN,
      NULL);
}

static void
clapper_app_application_constructed (GObject *object)
{
  GApplication *app = G_APPLICATION (object);
  guint i;

  static const GActionEntry app_entries[] = {
    { "add_files", add_files, NULL, NULL, NULL },
    { "add_uri", add_uri, NULL, NULL, NULL },
    //{ "preferences", show_preferences, NULL, NULL, NULL },
    { "media_info", show_info, NULL, NULL, NULL },
    { "about", show_about, NULL, NULL, NULL },
  };
  static const ClapperAppShortcut app_shortcuts[] = {
    { "app.add_files", { "<Control>o", NULL, NULL }},
    { "app.add_uri", { "<Control>u", NULL, NULL }},
    { "app.media_info", { "<Control>i", NULL, NULL }},
    { "app.about", { "F1", NULL, NULL }},
    { "win.toggle_fullscreen", { "F11", "f", NULL }},
  };

  g_action_map_add_action_entries (G_ACTION_MAP (app),
      app_entries, G_N_ELEMENTS (app_entries), app);

  for (i = 0; i < G_N_ELEMENTS (app_shortcuts); ++i)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), app_shortcuts[i].action, app_shortcuts[i].accels);

  g_application_add_option_group (app, gst_init_get_option_group ());

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_app_application_activate (GApplication *app)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (app);
  GtkWindow *window;

  GST_INFO_OBJECT (self, "Activate");
  G_APPLICATION_CLASS (parent_class)->activate (app);

  if (!(window = gtk_application_get_active_window (GTK_APPLICATION (app)))) {
    window = GTK_WINDOW (clapper_app_window_new (GTK_APPLICATION (app)));
  }

  if (self->need_init_state) {
    _assemble_initial_state (window);
    self->need_init_state = FALSE;
  }

  gtk_window_present (window);
}

static gboolean
clapper_app_application_local_command_line (GApplication *app,
    gchar ***arguments, gint *exit_status)
{
  gchar **argv = *arguments;
  guint i;

  /* NOTE: argv is never NULL, so no need to check */

  for (i = 0; argv[i]; ++i) {
    /* Handle "-" special case as URI */
    if (strlen (argv[i]) == 1 && argv[i][0] == '-') {
      g_free (argv[i]);
      argv[i] = g_strdup ("fd://0");
    }
  }

  return G_APPLICATION_CLASS (parent_class)->local_command_line (app, arguments, exit_status);
}

static void
clapper_app_application_open (GApplication *app,
    GFile **files, gint n_files, const gchar *hint)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (app);
  GtkWindow *window;
  ClapperPlayer *player;
  ClapperQueue *queue;
  guint i, n_before;

  GST_INFO_OBJECT (self, "Open");

  /* Since we startup with media,
   * no need to show initial state */
  self->need_init_state = FALSE;

  g_application_activate (app);

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  player = clapper_app_window_get_player (CLAPPER_APP_WINDOW (window));
  queue = clapper_player_get_queue (player);

  _ensure_no_initial_state (window);

  n_before = clapper_queue_get_n_items (queue);

  g_application_mark_busy (app);

  for (i = 0; i < n_files; ++i) {
    ClapperMediaItem *item = clapper_media_item_new_from_file (files[i]);

    GST_DEBUG_OBJECT (self, "Adding media item with URI: %s",
        clapper_media_item_get_uri (item));
    clapper_queue_add_item (queue, item);

    gst_object_unref (item);
  }

  g_application_unmark_busy (app);

  /* Autoplay */
  if (n_before == 0)
    clapper_player_play (player);
}

static void
clapper_app_application_init (ClapperAppApplication *self)
{
  self->need_init_state = TRUE;
}

static void
clapper_app_application_finalize (GObject *object)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_application_class_init (ClapperAppApplicationClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GApplicationClass *application_class = (GApplicationClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappapplication", 0,
      "Clapper App Application");

  gobject_class->constructed = clapper_app_application_constructed;
  gobject_class->finalize = clapper_app_application_finalize;

  application_class->activate = clapper_app_application_activate;
  application_class->local_command_line = clapper_app_application_local_command_line;
  application_class->open = clapper_app_application_open;
}
