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
#include "clapper-app-preferences-window.h"
#include "clapper-app-about-window.h"
#include "clapper-app-utils.h"

#define CLAPPER_APP_ID "com.github.rafostar.Clapper"

#define GST_CAT_DEFAULT clapper_app_application_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppApplication
{
  GtkApplication parent;

  GSettings *settings;

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
_iter_ranks_func (const gchar *feature_name, GstRank rank,
    gboolean from_env, gpointer user_data G_GNUC_UNUSED)
{
  GstPluginFeature *feature;

  if ((feature = gst_registry_find_feature (gst_registry_get (),
      feature_name, GST_TYPE_ELEMENT_FACTORY))) {
    gst_plugin_feature_set_rank (feature, rank);
    GST_INFO ("Set \"%s\" rank to: %i", feature_name, rank);

    gst_object_unref (feature);
  }
}

static void
plugin_feature_ranks_settings_changed_cb (GSettings *settings,
    gchar *key G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  clapper_app_utils_iterate_plugin_feature_ranks (settings,
      (ClapperAppUtilsIterRanks) _iter_ranks_func, NULL);
}

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
show_preferences (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  GtkApplication *gtk_app = GTK_APPLICATION (user_data);
  GtkWidget *preferences_window;

  preferences_window = clapper_app_preferences_window_new (gtk_app);
  gtk_window_present (GTK_WINDOW (preferences_window));
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

static inline void
_restore_settings_to_window (ClapperAppApplication *self, ClapperAppWindow *app_window)
{
  ClapperPlayer *player = clapper_app_window_get_player (app_window);
  ClapperQueue *queue = clapper_player_get_queue (player);

  GST_DEBUG ("Restoring saved GSettings values to: %" GST_PTR_FORMAT, app_window);

  clapper_player_set_volume (player, g_settings_get_double (self->settings, "volume"));
  clapper_player_set_speed (player, g_settings_get_double (self->settings, "speed"));
  clapper_player_set_subtitles_enabled (player, g_settings_get_boolean (self->settings, "subtitles-enabled"));
  clapper_queue_set_progression_mode (queue, g_settings_get_int (self->settings, "progression-mode"));

  GST_DEBUG ("Configuration restored");
}

static inline void
_store_settings_from_window (ClapperAppApplication *self, ClapperAppWindow *app_window)
{
  ClapperPlayer *player = clapper_app_window_get_player (app_window);
  ClapperQueue *queue = clapper_player_get_queue (player);

  GST_DEBUG ("Storing current configuration to GSettings");

  g_settings_set_double (self->settings, "volume", clapper_player_get_volume (player));
  g_settings_set_double (self->settings, "speed", clapper_player_get_speed (player));
  g_settings_set_boolean (self->settings, "subtitles-enabled", clapper_player_get_subtitles_enabled (player));
  g_settings_set_int (self->settings, "progression-mode", clapper_queue_get_progression_mode (queue));

  GST_DEBUG ("Configuration stored");
}

GApplication *
clapper_app_application_new (void)
{
  return g_object_new (CLAPPER_APP_TYPE_APPLICATION,
      "application-id", CLAPPER_APP_ID,
      "flags", G_APPLICATION_HANDLES_OPEN,
      NULL);
}

static void
clapper_app_application_window_removed (GtkApplication *gtk_app, GtkWindow *window)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (gtk_app);

  if (CLAPPER_APP_IS_WINDOW (window)) {
    GList *win, *windows = gtk_application_get_windows (gtk_app);
    gboolean has_player_windows = FALSE;

    for (win = windows; win != NULL; win = win->next) {
      GtkWindow *rem_window = GTK_WINDOW (win->data);

      if ((has_player_windows = (rem_window != window
          && CLAPPER_APP_IS_WINDOW (rem_window))))
        break;
    }

    /* Last player window is closing, time to store settings */
    if (!has_player_windows)
      _store_settings_from_window (self, CLAPPER_APP_WINDOW_CAST (window));
  }

  GTK_APPLICATION_CLASS (parent_class)->window_removed (gtk_app, window);
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
    _restore_settings_to_window (self, CLAPPER_APP_WINDOW_CAST (window));
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
clapper_app_application_constructed (GObject *object)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (object);
  GApplication *app = G_APPLICATION (self);
  guint i;

  static const GActionEntry app_entries[] = {
    { "add_files", add_files, NULL, NULL, NULL },
    { "add_uri", add_uri, NULL, NULL, NULL },
    { "info", show_info, NULL, NULL, NULL },
    { "preferences", show_preferences, NULL, NULL, NULL },
    { "about", show_about, NULL, NULL, NULL },
  };
  static const ClapperAppShortcut app_shortcuts[] = {
    { "app.add_files", { "<Control>o", NULL, NULL }},
    { "app.add_uri", { "<Control>u", NULL, NULL }},
    { "app.info", { "<Control>i", NULL, NULL }},
    { "app.preferences", { "<Control>comma", NULL, NULL }},
    { "app.about", { "F1", NULL, NULL }},
    { "win.toggle_fullscreen", { "F11", "f", NULL }},
  };

  self->settings = g_settings_new (CLAPPER_APP_ID);

  g_signal_connect (self->settings,
      "changed::plugin-feature-ranks",
      G_CALLBACK (plugin_feature_ranks_settings_changed_cb), self);
  plugin_feature_ranks_settings_changed_cb (self->settings, NULL, NULL);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
      app_entries, G_N_ELEMENTS (app_entries), app);

  for (i = 0; i < G_N_ELEMENTS (app_shortcuts); ++i)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), app_shortcuts[i].action, app_shortcuts[i].accels);

  g_application_add_option_group (app, gst_init_get_option_group ());

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_app_application_finalize (GObject *object)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_object_unref (self->settings);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_application_class_init (ClapperAppApplicationClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GApplicationClass *application_class = (GApplicationClass *) klass;
  GtkApplicationClass *gtk_application_class = (GtkApplicationClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappapplication", 0,
      "Clapper App Application");

  gobject_class->constructed = clapper_app_application_constructed;
  gobject_class->finalize = clapper_app_application_finalize;

  gtk_application_class->window_removed = clapper_app_application_window_removed;

  application_class->activate = clapper_app_application_activate;
  application_class->local_command_line = clapper_app_application_local_command_line;
  application_class->open = clapper_app_application_open;
}
