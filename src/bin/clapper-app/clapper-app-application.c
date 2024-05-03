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

#include <math.h>
#include <glib/gi18n.h>
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

#define PERCENTAGE_ROUND(a) (round ((gdouble) a / 0.01) * 0.01)

#define GST_CAT_DEFAULT clapper_app_application_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppApplication
{
  GtkApplication parent;

  GSettings *settings;

  gboolean need_init_state;
};

struct ClapperPluginFeatureData
{
  const gchar *name;
  GstRank rank;
};

struct ClapperPluginData
{
  const gchar *name;
  guint skip_version[3];
  struct ClapperPluginFeatureData features[10];
};

struct ClapperAppOptions
{
  gdouble volume;
  gdouble speed;

  gchar *video_filter;
  gchar *audio_filter;

  gchar *video_sink;
  gchar *audio_sink;
};

typedef struct
{
  const gchar *action;
  const gchar *accels[3];
} ClapperAppShortcut;

#define parent_class clapper_app_application_parent_class
G_DEFINE_TYPE (ClapperAppApplication, clapper_app_application, GTK_TYPE_APPLICATION);

static struct ClapperAppOptions app_opts = { 0, };

static inline void
_app_opts_free (void)
{
  g_free (app_opts.video_filter);
  g_free (app_opts.audio_filter);

  g_free (app_opts.video_sink);
  g_free (app_opts.audio_sink);
}

static inline void
_set_initial_plugin_feature_ranks (void)
{
  GstRegistry *registry = gst_registry_get ();
  guint i;

  const struct ClapperPluginData plugins_data[] = {
    {
      .name = "va",
      .skip_version = { 1, 24, 0 },
      .features = {
        { "vah264dec", GST_RANK_PRIMARY + 24 },
        { "vah265dec", GST_RANK_PRIMARY + 24 },
        { "vavp8dec", GST_RANK_PRIMARY + 24 },
        { "vavp9dec", GST_RANK_PRIMARY + 24 },
        { "vaav1dec", GST_RANK_PRIMARY + 24 },
        { NULL, 0 }
      }
    },
    {
      .name = "nvcodec",
      .skip_version = { 1, 24, 0 },
      .features = {
        { "nvh264dec", GST_RANK_PRIMARY + 28 },
        { "nvh265dec", GST_RANK_PRIMARY + 28 },
        { "nvvp8dec", GST_RANK_PRIMARY + 28 },
        { "nvvp9dec", GST_RANK_PRIMARY + 28 },
        { "nvav1dec", GST_RANK_PRIMARY + 28 },
        { NULL, 0 }
      }
    }
  };

  for (i = 0; i < G_N_ELEMENTS (plugins_data); ++i) {
    GList *features;

    if (!(features = gst_registry_get_feature_list_by_plugin (
        registry, plugins_data[i].name)))
      continue;

    if (g_list_length (features) > 0) {
      guint j;

      for (j = 0; G_N_ELEMENTS (plugins_data[i].features); ++j) {
        GstPluginFeature *feature;

        if (!plugins_data[i].features[j].name)
          break;

        if (!(feature = gst_registry_lookup_feature (registry,
            plugins_data[i].features[j].name)))
          continue;

        if (!gst_plugin_feature_check_version (feature,
            plugins_data[i].skip_version[0],
            plugins_data[i].skip_version[1],
            plugins_data[i].skip_version[2])) {
          gst_plugin_feature_set_rank (feature,
              plugins_data[i].features[j].rank);
          GST_DEBUG ("Initially set \"%s\" rank to: %i",
              plugins_data[i].features[j].name,
              plugins_data[i].features[j].rank);
        }
        gst_object_unref (feature);
      }
    }

    gst_plugin_feature_list_free (features);
  }
}

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
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-initial-state.ui");
  GtkWidget *initial_state = GTK_WIDGET (gtk_builder_get_object (builder, "initial_state"));

  gtk_stack_add_named (GTK_STACK (stack), initial_state, "initial_state");
  gtk_stack_set_visible_child (GTK_STACK (stack), initial_state);

  g_object_unref (builder);
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

  if (app_opts.video_filter)
    clapper_player_set_video_filter (player, clapper_app_utils_make_element (app_opts.video_filter));
  if (app_opts.audio_filter)
    clapper_player_set_audio_filter (player, clapper_app_utils_make_element (app_opts.audio_filter));
  if (app_opts.video_sink)
    clapper_player_set_video_sink (player, clapper_app_utils_make_element (app_opts.video_sink));
  if (app_opts.audio_sink)
    clapper_player_set_audio_sink (player, clapper_app_utils_make_element (app_opts.audio_sink));

  /* NOTE: Not using ternary operator to avoid accidental typecasting */
  if (app_opts.volume >= 0)
    clapper_player_set_volume (player, PERCENTAGE_ROUND (app_opts.volume));
  else
    clapper_player_set_volume (player, PERCENTAGE_ROUND (g_settings_get_double (self->settings, "volume")));

  clapper_player_set_mute (player, g_settings_get_boolean (self->settings, "mute"));

  if (app_opts.speed >= 0)
    clapper_player_set_speed (player, PERCENTAGE_ROUND (app_opts.speed));
  else
    clapper_player_set_speed (player, PERCENTAGE_ROUND (g_settings_get_double (self->settings, "speed")));

  clapper_player_set_subtitles_enabled (player, g_settings_get_boolean (self->settings, "subtitles-enabled"));
  clapper_queue_set_progression_mode (queue, g_settings_get_int (self->settings, "progression-mode"));

  if (g_settings_get_boolean (self->settings, "fullscreened"))
    gtk_window_fullscreen (GTK_WINDOW (app_window));
  else if (g_settings_get_boolean (self->settings, "maximized"))
    gtk_window_maximize (GTK_WINDOW (app_window));

  GST_DEBUG ("Configuration restored");
}

static inline void
_store_settings_from_window (ClapperAppApplication *self, ClapperAppWindow *app_window)
{
  ClapperPlayer *player = clapper_app_window_get_player (app_window);
  ClapperQueue *queue = clapper_player_get_queue (player);
  GtkWindow *window = GTK_WINDOW (app_window);

  GST_DEBUG ("Storing current configuration to GSettings");

  g_settings_set_double (self->settings, "volume", clapper_player_get_volume (player));
  g_settings_set_boolean (self->settings, "mute", clapper_player_get_mute (player));
  g_settings_set_double (self->settings, "speed", clapper_player_get_speed (player));
  g_settings_set_boolean (self->settings, "subtitles-enabled", clapper_player_get_subtitles_enabled (player));
  g_settings_set_int (self->settings, "progression-mode", clapper_queue_get_progression_mode (queue));

  g_settings_set_boolean (self->settings, "maximized", gtk_window_is_maximized (window));
  g_settings_set_boolean (self->settings, "fullscreened", gtk_window_is_fullscreen (window));

  GST_DEBUG ("Configuration stored");
}

GApplication *
clapper_app_application_new (void)
{
  return g_object_new (CLAPPER_APP_TYPE_APPLICATION,
      "application-id", CLAPPER_APP_ID,
      "flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_HANDLES_COMMAND_LINE,
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

  GST_INFO ("Activate");
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

  GST_INFO ("Handling local command line");

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

static gint
clapper_app_application_command_line (GApplication *app, GApplicationCommandLine *cmd_line)
{
  GVariantDict *options;
  GFile **files = NULL;
  gint n_files = 0;
  gboolean enqueue = FALSE;

  GST_INFO ("Handling command line");

  options = g_application_command_line_get_options_dict (cmd_line);

  /* Enqueue only makes sense from remote invocation */
  if (g_application_command_line_get_is_remote (cmd_line))
    enqueue = g_variant_dict_contains (options, "enqueue");

  if (clapper_app_utils_files_from_command_line (cmd_line, &files, &n_files)) {
    g_application_open (app, files, n_files, (enqueue) ? "add-only" : "");
    clapper_app_utils_files_free (files);
  } else {
    g_application_activate (app);
  }

  return EXIT_SUCCESS;
}

static gboolean
_is_claps_file (GFile *file)
{
  gchar *basename = g_file_get_basename (file);
  gboolean is_claps;

  is_claps = (basename && g_str_has_suffix (basename, ".claps"));
  g_free (basename);

  return is_claps;
}

static void
add_item_from_file (GFile *file, ClapperQueue *queue)
{
  ClapperMediaItem *item = clapper_media_item_new_from_file (file);

  GST_DEBUG ("Adding media item with URI: %s",
      clapper_media_item_get_uri (item));
  clapper_queue_add_item (queue, item);

  gst_object_unref (item);
}

static void
add_items_from_claps_file (GFile *file, ClapperQueue *queue)
{
  GDataInputStream *dstream = NULL;
  GFileInputStream *stream;
  GError *error = NULL;
  gchar *line;

  if (!(stream = g_file_read (file, NULL, &error)))
    goto finish;

  dstream = g_data_input_stream_new (G_INPUT_STREAM (stream));

  while ((line = g_data_input_stream_read_line (
      dstream, NULL, NULL, &error))) {
    g_strstrip (line);

    if (strlen (line) > 0) {
      GFile *tmp_file = gst_uri_is_valid (line)
          ? g_file_new_for_uri (line)
          : g_file_new_for_path (line);

      if (_is_claps_file (tmp_file))
        add_items_from_claps_file (tmp_file, queue);
      else
        add_item_from_file (tmp_file, queue);

      g_object_unref (tmp_file);
    }

    g_free (line);
  }

finish:
  if (error) {
    GST_ERROR ("Could not read \".claps\" file, reason: %s", error->message);
    g_error_free (error);
  }
  if (stream) {
    g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
    g_object_unref (stream);
  }
  g_clear_object (&dstream);
}

static void
add_item_with_subtitles (GFile *media_file,
    GFile *subs_file, ClapperQueue *queue)
{
  ClapperMediaItem *item = clapper_media_item_new_from_file (media_file);
  gchar *suburi = g_file_get_uri (subs_file);

  GST_DEBUG ("Adding media item with URI: %s, SUBURI: %s",
      clapper_media_item_get_uri (item), GST_STR_NULL (suburi));
  clapper_media_item_set_suburi (item, suburi);
  clapper_queue_add_item (queue, item);

  gst_object_unref (item);
  g_free (suburi);
}

static void
clapper_app_application_open (GApplication *app,
    GFile **files, gint n_files, const gchar *hint)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (app);
  GtkWindow *window;
  ClapperPlayer *player;
  ClapperQueue *queue;
  guint n_before;
  gboolean add_only, handled = FALSE;

  GST_INFO ("Open");

  /* Since we startup with media,
   * no need to show initial state */
  self->need_init_state = FALSE;

  g_application_activate (app);
  g_application_mark_busy (app);

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  while (window && !CLAPPER_APP_IS_WINDOW (window))
    window = gtk_window_get_transient_for (window);

  clapper_app_window_ensure_no_initial_state (CLAPPER_APP_WINDOW (window));

  player = clapper_app_window_get_player (CLAPPER_APP_WINDOW (window));
  queue = clapper_player_get_queue (player);

  n_before = clapper_queue_get_n_items (queue);

  /* Special path for opening video with subtitles at once */
  if (n_files == 2) {
    gboolean first_subs, second_subs;

    first_subs = clapper_app_utils_is_subtitles_file (files[0]);
    second_subs = clapper_app_utils_is_subtitles_file (files[1]);

    if ((handled = first_subs != second_subs)) {
      guint media_index, subs_index;

      media_index = (second_subs) ? 0 : 1;
      subs_index = (media_index + 1) % 2;

      add_item_with_subtitles (
          files[media_index], files[subs_index], queue);
    }
  }

  if (!handled) {
    gint i;

    for (i = 0; i < n_files; ++i) {
      if (_is_claps_file (files[i]))
        add_items_from_claps_file (files[i], queue);
      else
        add_item_from_file (files[i], queue);
    }
  }

  add_only = (g_strcmp0 (hint, "add-only") == 0);

  /* Select first thing from added item to play (behave like "open" should),
   * when queue was empty first item is automatically selected */
  if (!add_only && n_before > 0)
    clapper_queue_select_index (queue, n_before);

  g_application_unmark_busy (app);
}

static void
clapper_app_application_init (ClapperAppApplication *self)
{
  app_opts.volume = -1;
  app_opts.speed = -1;

  self->need_init_state = TRUE;
}

static void
clapper_app_application_constructed (GObject *object)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (object);
  GApplication *app = G_APPLICATION (self);
  guint i;

  const GOptionEntry app_options[] = {
    { "enqueue", 0, 0, G_OPTION_ARG_NONE, NULL, _("Add media to queue in primary application instance"), NULL },
    { "volume", 0, 0, G_OPTION_ARG_DOUBLE, &app_opts.volume, _("Audio volume to set (0 - 2.0 range)"), NULL },
    { "speed", 0, 0, G_OPTION_ARG_DOUBLE, &app_opts.speed, _("Playback speed to set (0.05 - 2.0 range)"), NULL },
    { "video-filter", 0, 0, G_OPTION_ARG_STRING, &app_opts.video_filter, _("Video filter to use (\"none\" to disable)"), NULL },
    { "audio-filter", 0, 0, G_OPTION_ARG_STRING, &app_opts.audio_filter, _("Audio filter to use (\"none\" to disable)"), NULL },
    { "video-sink", 0, 0, G_OPTION_ARG_STRING, &app_opts.video_sink, _("Video sink to use"), NULL },
    { "audio-sink", 0, 0, G_OPTION_ARG_STRING, &app_opts.audio_sink, _("Audio sink to use"), NULL },
    { NULL }
  };
  static const GActionEntry app_actions[] = {
    { "add-files", add_files, NULL, NULL, NULL },
    { "add-uri", add_uri, NULL, NULL, NULL },
    { "info", show_info, NULL, NULL, NULL },
    { "preferences", show_preferences, NULL, NULL, NULL },
    { "about", show_about, NULL, NULL, NULL },
  };
  static const ClapperAppShortcut app_shortcuts[] = {
    { "app.add-files", { "<Control>o", NULL, NULL }},
    { "app.add-uri", { "<Control>u", NULL, NULL }},
    { "app.info", { "<Control>i", NULL, NULL }},
    { "app.preferences", { "<Control>comma", NULL, NULL }},
    { "app.about", { "F1", NULL, NULL }},
    { "win.toggle-fullscreen", { "F11", "f", NULL }},
    { "win.show-help-overlay", { "<Control>question", NULL, NULL }},
    { "window.close", { "<Control>q", "q", NULL }},
  };

  /* Override initial ranks, they will be updated
   * from both stored settings and env below */
  _set_initial_plugin_feature_ranks ();

  self->settings = g_settings_new (CLAPPER_APP_ID);

  g_signal_connect (self->settings,
      "changed::plugin-feature-ranks",
      G_CALLBACK (plugin_feature_ranks_settings_changed_cb), self);
  plugin_feature_ranks_settings_changed_cb (self->settings, NULL, NULL);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
      app_actions, G_N_ELEMENTS (app_actions), app);

  for (i = 0; i < G_N_ELEMENTS (app_shortcuts); ++i)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), app_shortcuts[i].action, app_shortcuts[i].accels);

  g_application_add_main_option_entries (app, app_options);
  g_application_add_option_group (app, gst_init_get_option_group ());

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_app_application_finalize (GObject *object)
{
  ClapperAppApplication *self = CLAPPER_APP_APPLICATION_CAST (object);

  GST_TRACE ("Finalize");

  g_object_unref (self->settings);
  _app_opts_free ();

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
  application_class->command_line = clapper_app_application_command_line;
  application_class->open = clapper_app_application_open;
}
