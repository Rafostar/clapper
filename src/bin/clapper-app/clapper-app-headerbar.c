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

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "clapper-app-headerbar.h"
#include "clapper-app-utils.h"

#define GST_CAT_DEFAULT clapper_app_headerbar_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppHeaderbar
{
  ClapperGtkContainer parent;

  GtkWidget *queue_revealer;
  GtkWidget *previous_item_revealer;
  GtkWidget *next_item_revealer;
  GtkWidget *win_buttons_revealer;

  GtkDropTarget *drop_target;

  gboolean adapt;
};

#define parent_class clapper_app_headerbar_parent_class
G_DEFINE_TYPE (ClapperAppHeaderbar, clapper_app_headerbar, CLAPPER_GTK_TYPE_CONTAINER);

static void
_determine_win_buttons_reveal (ClapperAppHeaderbar *self)
{
  gboolean queue_reveal = gtk_revealer_get_reveal_child (GTK_REVEALER (self->queue_revealer));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->win_buttons_revealer),
      (queue_reveal) ? !self->adapt : TRUE);
}

static void
container_adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperAppHeaderbar *self)
{
  GST_DEBUG_OBJECT (self, "Width adapted: %s", (adapt) ? "yes" : "no");
  self->adapt = adapt;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->previous_item_revealer), !adapt);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->next_item_revealer), !adapt);

  _determine_win_buttons_reveal (self);
}

static void
queue_reveal_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppHeaderbar *self)
{
  _determine_win_buttons_reveal (self);
}

static void
reveal_queue_button_clicked_cb (GtkButton *button, ClapperAppHeaderbar *self)
{
  gboolean reveal;

  GST_INFO_OBJECT (self, "Reveal queue button clicked");

  reveal = gtk_revealer_get_reveal_child (GTK_REVEALER (self->queue_revealer));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->queue_revealer), !reveal);
}

static void
drop_value_notify_cb (GtkDropTarget *drop_target,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppHeaderbar *self)
{
  const GValue *value = gtk_drop_target_get_value (drop_target);

  if (value && !clapper_app_utils_value_for_item_is_valid (value))
    gtk_drop_target_reject (drop_target);
}

typedef struct {
  ClapperAppHeaderbar *headerbar;
  GtkApplication *app;
} HeaderbarDropCallbackData;

static void
_headerbar_drop_expand_cb (GObject *source G_GNUC_UNUSED, GAsyncResult *res, gpointer user_data)
{
  HeaderbarDropCallbackData *cb_data = (HeaderbarDropCallbackData *) user_data;
  ClapperAppHeaderbar *self = cb_data->headerbar;
  GFile **expanded_files = NULL;
  gint n_expanded = 0;
  GError *error = NULL;

  if (clapper_app_utils_expand_files_finish (res, &expanded_files, &n_expanded, &error)) {
    ClapperPlayer *player;

    if ((player = clapper_gtk_get_player_from_ancestor (GTK_WIDGET (self)))) {
      ClapperQueue *queue = clapper_player_get_queue (player);
      gint i;

      for (i = 0; i < n_expanded; ++i) {
        ClapperMediaItem *item = clapper_media_item_new_from_file (expanded_files[i]);

        clapper_queue_add_item (queue, item);
        if (i == 0) // Select first added item for playback
          clapper_queue_select_item (queue, item);

        gst_object_unref (item);
      }
    }

    clapper_app_utils_files_free (expanded_files);
  } else {
    g_warning ("Could not expand dropped files on headerbar: %s", error ? error->message : "Unknown error");
    g_clear_error (&error);
  }

  if (cb_data->app)
    g_application_unmark_busy (G_APPLICATION (cb_data->app));

  g_object_unref (self);
  g_clear_object (&cb_data->app);
  g_free (cb_data);
}

static gboolean
drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, ClapperAppHeaderbar *self)
{
  GFile **files = NULL;
  gint n_files = 0;
  gboolean success = FALSE;

  if (clapper_app_utils_files_from_value (value, &files, &n_files)) {
    GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
    GtkApplication *app = root ? gtk_window_get_application (GTK_WINDOW (root)) : NULL;
    HeaderbarDropCallbackData *cb_data = g_new0 (HeaderbarDropCallbackData, 1);
    cb_data->headerbar = g_object_ref (self);
    cb_data->app = app ? g_object_ref (app) : NULL;

    if (cb_data->app)
      g_application_mark_busy (G_APPLICATION (cb_data->app));

    clapper_app_utils_expand_files_async (files, n_files, NULL, _headerbar_drop_expand_cb, cb_data);
    clapper_app_utils_files_free (files);
    success = TRUE;
  }

  return success;
}

static void
clapper_app_headerbar_init (ClapperAppHeaderbar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_drop_target_set_gtypes (self->drop_target,
      (GType[3]) { GDK_TYPE_FILE_LIST, G_TYPE_FILE, G_TYPE_STRING }, 3);
}

static void
clapper_app_headerbar_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_HEADERBAR);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_headerbar_finalize (GObject *object)
{
  ClapperAppHeaderbar *self = CLAPPER_APP_HEADERBAR_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_headerbar_class_init (ClapperAppHeaderbarClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappheaderbar", 0,
      "Clapper App Headerbar");

  gobject_class->dispose = clapper_app_headerbar_dispose;
  gobject_class->finalize = clapper_app_headerbar_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-headerbar.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, queue_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, previous_item_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, next_item_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, win_buttons_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppHeaderbar, drop_target);

  gtk_widget_class_bind_template_callback (widget_class, container_adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, reveal_queue_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, queue_reveal_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_value_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-app-headerbar");
}
