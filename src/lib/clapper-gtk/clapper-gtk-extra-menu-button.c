/* Clapper GTK Integration Library
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

/**
 * ClapperGtkExtraMenuButton:
 *
 * A menu button with extra options.
 */

#include "config.h"

#include <math.h>
#include <glib/gi18n-lib.h>
#include <clapper/clapper.h>

#include "clapper-gtk-extra-menu-button.h"
#include "clapper-gtk-stream-check-button-private.h"
#include "clapper-gtk-utils-private.h"

#define PERCENTAGE_ROUND(a) (round ((gdouble) a / 0.01) * 0.01)

#define DEFAULT_CAN_OPEN_SUBTITLES FALSE

#define GST_CAT_DEFAULT clapper_gtk_extra_menu_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkExtraMenuButton
{
  GtkWidget parent;

  GtkWidget *menu_button;

  GtkWidget *volume_box;
  GtkWidget *volume_button;
  GtkWidget *volume_spin;

  GtkWidget *speed_box;
  GtkWidget *speed_button;
  GtkWidget *speed_spin;

  GtkWidget *top_separator;

  GtkWidget *video_list_view;
  GtkScrolledWindow *video_sw;

  GtkWidget *audio_list_view;
  GtkScrolledWindow *audio_sw;

  GtkWidget *subtitle_list_view;
  GtkScrolledWindow *subtitle_sw;

  ClapperPlayer *player;
  ClapperMediaItem *current_item;

  GSimpleActionGroup *action_group;

  gboolean mute;

  GBinding *volume_binding;
  GBinding *speed_binding;

  GBinding *video_binding;
  GBinding *audio_binding;
  GBinding *subtitle_binding;

  gboolean can_open_subtitles;
};

#define parent_class clapper_gtk_extra_menu_button_parent_class
G_DEFINE_TYPE (ClapperGtkExtraMenuButton, clapper_gtk_extra_menu_button, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_VOLUME_VISIBLE,
  PROP_SPEED_VISIBLE,
  PROP_CAN_OPEN_SUBTITLES,
  PROP_LAST
};

enum
{
  SIGNAL_OPEN_SUBTITLES,
  SIGNAL_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

static void
_set_action_enabled (ClapperGtkExtraMenuButton *self, const gchar *name, gboolean enabled)
{
  GAction *action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), name);
  gboolean was_enabled = g_action_get_enabled (action);

  if (was_enabled == enabled)
    return;

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static gboolean
_double_transform_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, gpointer user_data G_GNUC_UNUSED)
{
  gdouble val_dbl = g_value_get_double (from_value);

  g_value_set_double (to_value, PERCENTAGE_ROUND (val_dbl));
  return TRUE;
}

static gint
volume_spin_input_cb (GtkSpinButton *spin_button, gdouble *value, ClapperGtkExtraMenuButton *self)
{
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  gdouble volume = g_strtod (text, NULL);

  if (volume < 0)
    volume = 0;
  else if (volume > 200)
    volume = 200;

  volume /= 100.0;

  if (volume > 0.99 && volume < 1.01)
    volume = 1.0;

  *value = volume;

  return TRUE;
}

static gboolean
volume_spin_output_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble volume = gtk_adjustment_get_value (adjustment);
  gchar *volume_str;

  volume_str = g_strdup_printf ("%.0lf%%", volume * 100);
  gtk_editable_set_text (GTK_EDITABLE (spin_button), volume_str);
  g_free (volume_str);

  return TRUE;
}

static void
volume_spin_changed_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble volume = gtk_adjustment_get_value (adjustment);

  gtk_button_set_icon_name (GTK_BUTTON (self->volume_button),
      clapper_gtk_get_icon_name_for_volume ((!self->mute) ? volume : 0));
}

static gint
speed_spin_input_cb (GtkSpinButton *spin_button, gdouble *value, ClapperGtkExtraMenuButton *self)
{
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  gdouble speed = g_strtod (text, NULL);

  if (speed < 0.05)
    speed = 0.05;
  else if (speed > 2.0)
    speed = 2.0;

  if (speed > 0.99 && speed < 1.01)
    speed = 1.0;

  *value = speed;

  return TRUE;
}

static gboolean
speed_spin_output_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble speed = gtk_adjustment_get_value (adjustment);
  gchar *speed_str;

  speed_str = g_strdup_printf ("%.2lfx", speed);
  gtk_editable_set_text (GTK_EDITABLE (spin_button), speed_str);
  g_free (speed_str);

  return TRUE;
}

static void
speed_spin_changed_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble speed = gtk_adjustment_get_value (adjustment);

  gtk_button_set_icon_name (GTK_BUTTON (self->speed_button),
      clapper_gtk_get_icon_name_for_speed (speed));
}

static void
visible_submenu_changed_cb (GtkPopoverMenu *popover_menu,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkExtraMenuButton *self)
{
  gchar *name = NULL;
  gboolean in_video, in_audio, in_subtitles;

  g_object_get (popover_menu, "visible-submenu", &name, NULL);

  /* TODO: Check if we have to compare translated strings here */
  GST_DEBUG ("Visible submenu changed to: \"%s\"", name);

  in_video = (g_strcmp0 (name, _("Video")) == 0);
  in_audio = (g_strcmp0 (name, _("Audio")) == 0);
  in_subtitles = (g_strcmp0 (name, _("Subtitles")) == 0);

  /* XXX: This works around the issue where popover does not adapt its
   * width when navigating submenus making spin buttons unnecesary centered */
  gtk_scrolled_window_set_propagate_natural_width (self->video_sw, in_video);
  gtk_scrolled_window_set_propagate_natural_width (self->audio_sw, in_audio);
  gtk_scrolled_window_set_propagate_natural_width (self->subtitle_sw, in_subtitles);

  g_free (name);
}

static void
_subtitles_enabled_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkExtraMenuButton *self)
{
  GAction *action = g_action_map_lookup_action (
      G_ACTION_MAP (self->action_group), "subtitle-stream-enabled");
  GVariant *variant = g_action_get_state (action);
  gboolean was_enabled, enabled;

  was_enabled = g_variant_get_boolean (variant);
  enabled = clapper_player_get_subtitles_enabled (player);

  g_variant_unref (variant);

  if (was_enabled == enabled)
    return;

  variant = g_variant_ref_sink (g_variant_new_boolean (enabled));

  g_simple_action_set_state (G_SIMPLE_ACTION (action), variant);
  g_variant_unref (variant);
}

static void
_player_mute_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkExtraMenuButton *self)
{
  self->mute = clapper_player_get_mute (player);

  volume_spin_changed_cb (GTK_SPIN_BUTTON (self->volume_spin), self);
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkExtraMenuButton *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  /* NOTE: This is also called after popover "map" signal */

  if (gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item))) {
    _set_action_enabled (self, "open-subtitle-stream",
        (self->can_open_subtitles && self->current_item != NULL));
  }

  gst_clear_object (&current_item);
}

static void
change_subtitle_stream_enabled (GSimpleAction *action, GVariant *value, gpointer user_data)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (user_data);
  gboolean enable = g_variant_get_boolean (value);

  if (G_LIKELY (self->player != NULL))
    clapper_player_set_subtitles_enabled (self->player, enable);

  g_simple_action_set_state (action, value);
}

static void
open_subtitle_stream (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (user_data);

  /* We should not be here otherwise */
  if (G_LIKELY (self->can_open_subtitles && self->current_item != NULL))
    g_signal_emit (self, signals[SIGNAL_OPEN_SUBTITLES], 0, self->current_item);
}

static void
_determine_top_separator_visibility (ClapperGtkExtraMenuButton *self)
{
  gboolean visible = gtk_widget_get_visible (self->volume_box)
      || gtk_widget_get_visible (self->speed_box);

  gtk_widget_set_visible (self->top_separator, visible);
}

/**
 * clapper_gtk_extra_menu_button_new:
 *
 * Creates a new #ClapperGtkExtraMenuButton instance.
 *
 * Returns: a new extra menu button #GtkWidget.
 */
GtkWidget *
clapper_gtk_extra_menu_button_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_EXTRA_MENU_BUTTON, NULL);
}

/**
 * clapper_gtk_extra_menu_button_set_volume_visible:
 * @button: a #ClapperGtkExtraMenuButton
 * @visible: whether visible
 *
 * Set whether volume control inside popover should be visible.
 */
void
clapper_gtk_extra_menu_button_set_volume_visible (ClapperGtkExtraMenuButton *self, gboolean visible)
{
  g_return_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self));

  if (gtk_widget_get_visible (self->volume_box) != visible) {
    gtk_widget_set_visible (self->volume_box, visible);
    _determine_top_separator_visibility (self);

    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_VOLUME_VISIBLE]);
  }
}

/**
 * clapper_gtk_extra_menu_button_get_volume_visible:
 * @button: a #ClapperGtkExtraMenuButton
 *
 * Get whether volume control inside popover is visible.
 *
 * Returns: TRUE if volume control is visible, %FALSE otherwise.
 */
gboolean
clapper_gtk_extra_menu_button_get_volume_visible (ClapperGtkExtraMenuButton *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self), FALSE);

  return gtk_widget_get_visible (self->volume_box);
}

/**
 * clapper_gtk_extra_menu_button_set_speed_visible:
 * @button: a #ClapperGtkExtraMenuButton
 * @visible: whether visible
 *
 * Set whether speed control inside popover should be visible.
 */
void
clapper_gtk_extra_menu_button_set_speed_visible (ClapperGtkExtraMenuButton *self, gboolean visible)
{
  g_return_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self));

  if (gtk_widget_get_visible (self->speed_box) != visible) {
    gtk_widget_set_visible (self->speed_box, visible);
    _determine_top_separator_visibility (self);

    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_SPEED_VISIBLE]);
  }
}

/**
 * clapper_gtk_extra_menu_button_get_speed_visible:
 * @button: a #ClapperGtkExtraMenuButton
 *
 * Get whether speed control inside popover is visible.
 *
 * Returns: %TRUE if speed control is visible, %FALSE otherwise.
 */
gboolean
clapper_gtk_extra_menu_button_get_speed_visible (ClapperGtkExtraMenuButton *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self), FALSE);

  return gtk_widget_get_visible (self->speed_box);
}

/**
 * clapper_gtk_extra_menu_button_set_can_open_subtitles:
 * @button: a #ClapperGtkExtraMenuButton
 * @allowed: whether opening subtitles should be allowed
 *
 * Set whether an option to open external subtitle stream should be allowed.
 *
 * Note that this [class@Gtk.Widget] can only add subtitles to currently playing
 * [class@Clapper.MediaItem]. When no media is selected, option to open subtitles
 * will not be shown regardless how this option is set.
 */
void
clapper_gtk_extra_menu_button_set_can_open_subtitles (ClapperGtkExtraMenuButton *self, gboolean allowed)
{
  gboolean changed;

  g_return_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self));

  if ((changed = self->can_open_subtitles != allowed)) {
    self->can_open_subtitles = allowed;

    _set_action_enabled (self, "open-subtitle-stream",
        (self->can_open_subtitles && self->current_item != NULL));

    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CAN_OPEN_SUBTITLES]);
  }
}

/**
 * clapper_gtk_extra_menu_button_get_can_open_subtitles:
 * @button: a #ClapperGtkExtraMenuButton
 *
 * Get whether an option to open external subtitle stream inside popover is visible.
 *
 * Returns: %TRUE if open subtitles is visible, %FALSE otherwise.
 */
gboolean
clapper_gtk_extra_menu_button_get_can_open_subtitles (ClapperGtkExtraMenuButton *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self), FALSE);

  return self->can_open_subtitles;
}

static void
clapper_gtk_extra_menu_button_init (ClapperGtkExtraMenuButton *self)
{
  static GActionEntry action_entries[] = {
    { "subtitle-stream-enabled", NULL, NULL, "true", change_subtitle_stream_enabled },
    { "open-subtitle-stream", open_subtitle_stream, NULL, NULL, NULL },
  };

  /* Ensure private types */
  g_type_ensure (CLAPPER_GTK_TYPE_STREAM_CHECK_BUTTON);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->action_group = g_simple_action_group_new ();

  g_object_bind_property (self, "css-classes", self->menu_button,
      "css-classes", G_BINDING_DEFAULT);

  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
      action_entries, G_N_ELEMENTS (action_entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
      "clappergtk", G_ACTION_GROUP (self->action_group));

  /* Set default values */
  self->can_open_subtitles = DEFAULT_CAN_OPEN_SUBTITLES;
  _set_action_enabled (self, "open-subtitle-stream", self->can_open_subtitles);
}

static void
clapper_gtk_extra_menu_button_compute_expand (GtkWidget *widget,
    gboolean *hexpand_p, gboolean *vexpand_p)
{
  GtkWidget *child;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  if ((child = gtk_widget_get_first_child (widget))) {
    hexpand = gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL);
    vexpand = gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL);
  }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
clapper_gtk_extra_menu_button_realize (GtkWidget *widget)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (widget);

  GST_TRACE_OBJECT (self, "Realize");

  if ((self->player = clapper_gtk_get_player_from_ancestor (GTK_WIDGET (self)))) {
    ClapperStreamList *stream_list;
    GtkSingleSelection *selection;

    g_signal_connect (self->player, "notify::mute",
        G_CALLBACK (_player_mute_changed_cb), self);
    self->mute = clapper_player_get_mute (self->player);

    stream_list = clapper_player_get_video_streams (self->player);
    selection = gtk_single_selection_new (gst_object_ref (stream_list));
    gtk_single_selection_set_autoselect (selection, FALSE);

    self->video_binding = g_object_bind_property (stream_list, "current-index",
        selection, "selected", G_BINDING_SYNC_CREATE);

    gtk_list_view_set_model (GTK_LIST_VIEW (self->video_list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);

    stream_list = clapper_player_get_audio_streams (self->player);
    selection = gtk_single_selection_new (gst_object_ref (stream_list));
    gtk_single_selection_set_autoselect (selection, FALSE);

    self->audio_binding = g_object_bind_property (stream_list, "current-index",
        selection, "selected", G_BINDING_SYNC_CREATE);

    gtk_list_view_set_model (GTK_LIST_VIEW (self->audio_list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);

    stream_list = clapper_player_get_subtitle_streams (self->player);
    selection = gtk_single_selection_new (gst_object_ref (stream_list));
    gtk_single_selection_set_autoselect (selection, FALSE);

    self->subtitle_binding = g_object_bind_property (stream_list, "current-index",
        selection, "selected", G_BINDING_SYNC_CREATE);

    gtk_list_view_set_model (GTK_LIST_VIEW (self->subtitle_list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);
  }

  GTK_WIDGET_CLASS (parent_class)->realize (widget);
}

static void
clapper_gtk_extra_menu_button_unrealize (GtkWidget *widget)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (widget);

  GST_TRACE_OBJECT (self, "Unrealize");

  g_clear_pointer (&self->video_binding, g_binding_unbind);
  g_clear_pointer (&self->audio_binding, g_binding_unbind);
  g_clear_pointer (&self->subtitle_binding, g_binding_unbind);

  gtk_list_view_set_model (GTK_LIST_VIEW (self->video_list_view), NULL);
  gtk_list_view_set_model (GTK_LIST_VIEW (self->audio_list_view), NULL);
  gtk_list_view_set_model (GTK_LIST_VIEW (self->subtitle_list_view), NULL);

  if (self->player) {
    g_signal_handlers_disconnect_by_func (self->player, _player_mute_changed_cb, self);

    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
popover_map_cb (GtkWidget *widget, ClapperGtkExtraMenuButton *self)
{
  ClapperQueue *queue;

  GST_TRACE_OBJECT (self, "Popover map");

  gtk_widget_set_can_focus (widget, TRUE);

  if (G_UNLIKELY (self->player == NULL))
    return;

  queue = clapper_player_get_queue (self->player);

  self->volume_binding = g_object_bind_property_full (self->player, "volume",
      self->volume_spin, "value",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
      (GBindingTransformFunc) _double_transform_func,
      (GBindingTransformFunc) _double_transform_func,
      NULL, NULL);
  self->speed_binding = g_object_bind_property_full (self->player, "speed",
      self->speed_spin, "value",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
      (GBindingTransformFunc) _double_transform_func,
      (GBindingTransformFunc) _double_transform_func,
      NULL, NULL);

  g_signal_connect (self->player, "notify::subtitles-enabled",
      G_CALLBACK (_subtitles_enabled_changed_cb), self);
  _subtitles_enabled_changed_cb (self->player, NULL, self);

  g_signal_connect (queue, "notify::current-item",
      G_CALLBACK (_queue_current_item_changed_cb), self);
  _queue_current_item_changed_cb (queue, NULL, self);
}

static void
popover_unmap_cb (GtkWidget *widget, ClapperGtkExtraMenuButton *self)
{
  ClapperQueue *queue;

  GST_TRACE_OBJECT (self, "Popover unmap");

  /* Drop focus after popover is closed. Fixes issue
   * with keyboard shortcuts not working when closed
   * while within submenu */
  gtk_widget_set_can_focus (widget, FALSE);

  if (G_UNLIKELY (self->player == NULL))
    return;

  queue = clapper_player_get_queue (self->player);

  g_clear_pointer (&self->volume_binding, g_binding_unbind);
  g_clear_pointer (&self->speed_binding, g_binding_unbind);

  g_signal_handlers_disconnect_by_func (self->player, _subtitles_enabled_changed_cb, self);
  g_signal_handlers_disconnect_by_func (queue, _queue_current_item_changed_cb, self);
}

static void
clapper_gtk_extra_menu_button_dispose (GObject *object)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_EXTRA_MENU_BUTTON);

  g_clear_pointer (&self->menu_button, gtk_widget_unparent);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_extra_menu_button_finalize (GObject *object)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&self->current_item);
  g_object_unref (self->action_group);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_gtk_extra_menu_button_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (object);

  switch (prop_id) {
    case PROP_VOLUME_VISIBLE:
      g_value_set_boolean (value, clapper_gtk_extra_menu_button_get_volume_visible (self));
      break;
    case PROP_SPEED_VISIBLE:
      g_value_set_boolean (value, clapper_gtk_extra_menu_button_get_speed_visible (self));
      break;
    case PROP_CAN_OPEN_SUBTITLES:
      g_value_set_boolean (value, clapper_gtk_extra_menu_button_get_can_open_subtitles (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_extra_menu_button_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (object);

  switch (prop_id) {
    case PROP_VOLUME_VISIBLE:
      clapper_gtk_extra_menu_button_set_volume_visible (self, g_value_get_boolean (value));
      break;
    case PROP_SPEED_VISIBLE:
      clapper_gtk_extra_menu_button_set_speed_visible (self, g_value_get_boolean (value));
      break;
    case PROP_CAN_OPEN_SUBTITLES:
      clapper_gtk_extra_menu_button_set_can_open_subtitles (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_extra_menu_button_class_init (ClapperGtkExtraMenuButtonClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkextramenubutton", 0,
      "Clapper GTK Extra Menu Button");
  clapper_gtk_init_translations ();

  gobject_class->get_property = clapper_gtk_extra_menu_button_get_property;
  gobject_class->set_property = clapper_gtk_extra_menu_button_set_property;
  gobject_class->dispose = clapper_gtk_extra_menu_button_dispose;
  gobject_class->finalize = clapper_gtk_extra_menu_button_finalize;

  widget_class->compute_expand = clapper_gtk_extra_menu_button_compute_expand;
  widget_class->realize = clapper_gtk_extra_menu_button_realize;
  widget_class->unrealize = clapper_gtk_extra_menu_button_unrealize;

  /**
   * ClapperGtkExtraMenuButton:volume-visible:
   *
   * Visibility of volume control inside popover.
   */
  param_specs[PROP_VOLUME_VISIBLE] = g_param_spec_boolean ("volume-visible",
      NULL, NULL, TRUE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkExtraMenuButton:speed-visible:
   *
   * Visibility of speed control inside popover.
   */
  param_specs[PROP_SPEED_VISIBLE] = g_param_spec_boolean ("speed-visible",
      NULL, NULL, TRUE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkExtraMenuButton:can-open-subtitles:
   *
   * Visibility of open subtitles option inside popover.
   */
  param_specs[PROP_CAN_OPEN_SUBTITLES] = g_param_spec_boolean ("can-open-subtitles",
      NULL, NULL, FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkExtraMenuButton::open-subtitles:
   * @button: a #ClapperGtkExtraMenuButton
   * @item: a #ClapperMediaItem
   *
   * A signal that user wants to open subtitles file.
   *
   * Implementation should add a way for user to select subtitles to open
   * such as by e.g. using [class@Gtk.FileDialog] and then add them to the
   * @item using [method@Clapper.MediaItem.set_suburi] method.
   *
   * This signal will pass the [class@Clapper.MediaItem] that was current when
   * user clicked the open button and subtitles should be added to this @item.
   * This avoids situations where another item starts playing before user selects
   * subtitles file to be opened. When using asynchronous operations to open file,
   * implementation should [method@GObject.Object.ref] the item to ensure that it
   * stays valid until finish.
   *
   * Note that this signal will not be emitted if open button is not visible by
   * setting [method@ClapperGtk.ExtraMenuButton.set_can_open_subtitles] to %TRUE,
   * so you do not have to implement handler for it otherwise.
   */
  signals[SIGNAL_OPEN_SUBTITLES] = g_signal_new ("open-subtitles",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, CLAPPER_TYPE_MEDIA_ITEM);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-extra-menu-button.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, menu_button);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, volume_box);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, volume_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, volume_spin);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, speed_box);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, speed_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, speed_spin);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, top_separator);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, video_list_view);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, video_sw);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, audio_list_view);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, audio_sw);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, subtitle_list_view);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, subtitle_sw);

  gtk_widget_class_bind_template_callback (widget_class, volume_spin_input_cb);
  gtk_widget_class_bind_template_callback (widget_class, volume_spin_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, volume_spin_changed_cb);

  gtk_widget_class_bind_template_callback (widget_class, speed_spin_input_cb);
  gtk_widget_class_bind_template_callback (widget_class, speed_spin_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, speed_spin_changed_cb);

  gtk_widget_class_bind_template_callback (widget_class, popover_map_cb);
  gtk_widget_class_bind_template_callback (widget_class, popover_unmap_cb);
  gtk_widget_class_bind_template_callback (widget_class, visible_submenu_changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-extra-menu-button");
}
