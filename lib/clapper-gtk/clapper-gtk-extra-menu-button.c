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

/**
 * ClapperGtkExtraMenuButton:
 *
 * A menu button with extra options.
 */

#include <clapper/clapper.h>

#include "clapper-gtk-extra-menu-button.h"
#include "clapper-gtk-utils.h"

#define DEFAULT_VOLUME_VISIBLE TRUE

#define GST_CAT_DEFAULT clapper_gtk_extra_menu_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkExtraMenuButton
{
  GtkWidget parent;

  GtkWidget *menu_button;

  GtkWidget *volume_box;
  GtkWidget *volume_image;
  GtkWidget *volume_spin;

  GtkWidget *speed_box;
  GtkWidget *speed_image;
  GtkWidget *speed_spin;

  GtkWidget *top_separator;

  GtkWidget *video_list_view;
  GtkWidget *audio_list_view;
  GtkWidget *subtitle_list_view;

  ClapperPlayer *player;
  ClapperMediaItem *current_item;

  GSimpleActionGroup *action_group;

  GBinding *volume_binding;
  GBinding *speed_binding;
};

#define parent_class clapper_gtk_extra_menu_button_parent_class
G_DEFINE_TYPE (ClapperGtkExtraMenuButton, clapper_gtk_extra_menu_button, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_VOLUME_VISIBLE,
  PROP_SPEED_VISIBLE,
  PROP_OPEN_SUBTITLES_VISIBLE,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static gint
volume_spin_input_cb (GtkSpinButton *spin_button, gdouble *value, ClapperGtkExtraMenuButton *self)
{
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  gchar *sign = NULL;
  gdouble volume = g_ascii_strtod (text, &sign);

  if (volume < 0 || volume > 200
      || (sign && sign[0] != '\0' && sign[0] != '%'))
    return GTK_INPUT_ERROR;

  volume /= 100.f;

  if (volume > 0.99 && volume < 1.01)
    volume = 1.0f;

  *value = volume;

  return TRUE;
}

static gboolean
volume_spin_output_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble volume = gtk_adjustment_get_value (adjustment);
  gchar *volume_str;

  volume_str = g_strdup_printf ("%.0f%%", volume * 100);
  gtk_editable_set_text (GTK_EDITABLE (spin_button), volume_str);
  g_free (volume_str);

  return TRUE;
}

static void
volume_spin_changed_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gfloat volume = (gfloat) gtk_adjustment_get_value (adjustment);
  const gchar *icon_name;

  icon_name = (volume <= 0.0)
      ? "audio-volume-muted-symbolic"
      : (volume <= 0.3)
      ? "audio-volume-low-symbolic"
      : (volume <= 0.7)
      ? "audio-volume-medium-symbolic"
      : (volume <= 1.0)
      ? "audio-volume-high-symbolic"
      : "audio-volume-overamplified-symbolic";

  gtk_image_set_from_icon_name (GTK_IMAGE (self->volume_image), icon_name);
}

static gint
speed_spin_input_cb (GtkSpinButton *spin_button, gdouble *value, ClapperGtkExtraMenuButton *self)
{
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  gchar *sign = NULL;
  gdouble speed = g_ascii_strtod (text, &sign);

  if (speed < 0.01 || speed > 2.0
      || (sign && sign[0] != '\0' && sign[0] != 'x'))
    return GTK_INPUT_ERROR;

  if (speed > 0.99 && speed < 1.01)
    speed = 1.0f;

  *value = speed;

  return TRUE;
}

static gboolean
speed_spin_output_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gdouble speed = gtk_adjustment_get_value (adjustment);
  gchar *speed_str;

  speed_str = g_strdup_printf ("%.2fx", speed);
  gtk_editable_set_text (GTK_EDITABLE (spin_button), speed_str);
  g_free (speed_str);

  return TRUE;
}

static void
speed_spin_changed_cb (GtkSpinButton *spin_button, ClapperGtkExtraMenuButton *self)
{
  GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (spin_button);
  gfloat speed = (gfloat) gtk_adjustment_get_value (adjustment);
  const gchar *icon_name;

  icon_name = (speed < 1.0)
      ? "power-profile-power-saver-symbolic"
      : (speed == 1.0)
      ? "power-profile-balanced-symbolic"
      : "power-profile-performance-symbolic";

  gtk_image_set_from_icon_name (GTK_IMAGE (self->speed_image), icon_name);
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkExtraMenuButton *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  if (current_item) {
    ClapperStreamList *stream_list;
    GtkSingleSelection *selection;

    stream_list = clapper_media_item_get_video_streams (current_item);
    selection = gtk_single_selection_new (gst_object_ref (stream_list));

    gtk_list_view_set_model (GTK_LIST_VIEW (self->video_list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);

    stream_list = clapper_media_item_get_audio_streams (current_item);
    selection = gtk_single_selection_new (gst_object_ref (stream_list));

    gtk_list_view_set_model (GTK_LIST_VIEW (self->audio_list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);

    stream_list = clapper_media_item_get_subtitle_streams (current_item);
    selection = gtk_single_selection_new (gst_object_ref (stream_list));

    g_object_bind_property (stream_list, "current-index",
        selection, "selected", G_BINDING_SYNC_CREATE);

    gtk_list_view_set_model (GTK_LIST_VIEW (self->subtitle_list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);
  } else {
    gtk_list_view_set_model (GTK_LIST_VIEW (self->video_list_view), NULL);
    gtk_list_view_set_model (GTK_LIST_VIEW (self->audio_list_view), NULL);
    gtk_list_view_set_model (GTK_LIST_VIEW (self->subtitle_list_view), NULL);
  }

  gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item));
  gst_clear_object (&current_item);
}

static void
change_video_stream_enabled (GSimpleAction *action, GVariant *value, gpointer user_data)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (user_data);
/*
  if (G_LIKELY (self->player != NULL))
    clapper_player_set_video_enabled (self->player, g_variant_get_boolean (state));
*/
  g_simple_action_set_state (action, value);
}

static void
change_audio_stream_enabled (GSimpleAction *action, GVariant *value, gpointer user_data)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (user_data);
/*
  if (G_LIKELY (self->player != NULL))
    clapper_player_set_audio_enabled (self->player, g_variant_get_boolean (state));
*/
  g_simple_action_set_state (action, value);
}

static void
change_subtitle_stream_enabled (GSimpleAction *action, GVariant *value, gpointer user_data)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (user_data);
/*
  if (G_LIKELY (self->player != NULL))
    clapper_player_set_subtitles_enabled (self->player, g_variant_get_boolean (state));
*/
  g_simple_action_set_state (action, value);
}

static void
open_subtitle_stream (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (user_data);

  /* FIXME: Should we handle this here? Or simply tell user
   * to install app action handler for this by himself? */
  GST_FIXME_OBJECT (self, "Implement open subtitles file dialog");
}

static gboolean
_set_action_enabled (ClapperGtkExtraMenuButton *self, const gchar *name, gboolean enabled)
{
  GAction *action;
  gboolean was_enabled;

  action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), name);
  g_object_get (action, "enabled", &was_enabled, NULL);

  if (was_enabled == enabled)
    return FALSE;

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

  return TRUE;
}

static gboolean
_get_action_enabled (ClapperGtkExtraMenuButton *self, const gchar *name)
{
  GAction *action;
  gboolean enabled = FALSE;

  action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), name);
  g_object_get (action, "enabled", &enabled, NULL);

  return enabled;
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
 * Creates a new extra menu button #GtkWidget.
 *
 * Returns: (transfer full): a new #GtkWidget instance.
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
 * clapper_gtk_extra_menu_button_set_open_subtitles_visible:
 * @button: a #ClapperGtkExtraMenuButton
 * @visible: whether visible
 *
 * Set whether an option to open external subtitle stream should be visible.
 */
void
clapper_gtk_extra_menu_button_set_open_subtitles_visible (ClapperGtkExtraMenuButton *self, gboolean visible)
{
  g_return_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self));

  if (_set_action_enabled (self, "open-subtitle-stream", visible))
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_OPEN_SUBTITLES_VISIBLE]);
}

/**
 * clapper_gtk_extra_menu_button_get_open_subtitles_visible:
 * @button: a #ClapperGtkExtraMenuButton
 *
 * Get whether an option to open external subtitle stream inside popover is visible.
 *
 * Returns: %TRUE if open subtitles is visible, %FALSE otherwise.
 */
gboolean
clapper_gtk_extra_menu_button_get_open_subtitles_visible (ClapperGtkExtraMenuButton *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_EXTRA_MENU_BUTTON (self), FALSE);

  return _get_action_enabled (self, "open-subtitle-stream");
}

static void
clapper_gtk_extra_menu_button_init (ClapperGtkExtraMenuButton *self)
{
  static GActionEntry action_entries[] = {
    { "video-stream-enabled", NULL, NULL, "true", change_video_stream_enabled },
    { "audio-stream-enabled", NULL, NULL, "true", change_audio_stream_enabled },
    { "subtitle-stream-enabled", NULL, NULL, "true", change_subtitle_stream_enabled },
    { "open-subtitle-stream", open_subtitle_stream, NULL, NULL, NULL },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  self->action_group = g_simple_action_group_new ();

  g_object_bind_property (self, "css-classes", self->menu_button,
      "css-classes", G_BINDING_DEFAULT);

  g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
      action_entries, G_N_ELEMENTS (action_entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
      "clappergtk", G_ACTION_GROUP (self->action_group));

  /* Set default visibilty */
  clapper_gtk_extra_menu_button_set_open_subtitles_visible (self, FALSE);
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
clapper_gtk_extra_menu_button_map (GtkWidget *widget)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (widget);

  if ((self->player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    self->volume_binding = g_object_bind_property (self->volume_spin, "value",
        self->player, "volume", G_BINDING_BIDIRECTIONAL);
    self->speed_binding = g_object_bind_property (self->speed_spin, "value",
        self->player, "speed", G_BINDING_BIDIRECTIONAL);

    g_signal_connect (queue, "notify::current-item",
        G_CALLBACK (_queue_current_item_changed_cb), self);
    _queue_current_item_changed_cb (queue, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_extra_menu_button_unmap (GtkWidget *widget)
{
  ClapperGtkExtraMenuButton *self = CLAPPER_GTK_EXTRA_MENU_BUTTON_CAST (widget);

  if (self->player) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_clear_pointer (&self->volume_binding, g_binding_unbind);
    g_clear_pointer (&self->speed_binding, g_binding_unbind);

    g_signal_handlers_disconnect_by_func (queue, _queue_current_item_changed_cb, self);

    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
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
    case PROP_OPEN_SUBTITLES_VISIBLE:
      g_value_set_boolean (value, clapper_gtk_extra_menu_button_get_open_subtitles_visible (self));
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
    case PROP_OPEN_SUBTITLES_VISIBLE:
      clapper_gtk_extra_menu_button_set_open_subtitles_visible (self, g_value_get_boolean (value));
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

  gobject_class->get_property = clapper_gtk_extra_menu_button_get_property;
  gobject_class->set_property = clapper_gtk_extra_menu_button_set_property;
  gobject_class->dispose = clapper_gtk_extra_menu_button_dispose;
  gobject_class->finalize = clapper_gtk_extra_menu_button_finalize;

  widget_class->compute_expand = clapper_gtk_extra_menu_button_compute_expand;
  widget_class->map = clapper_gtk_extra_menu_button_map;
  widget_class->unmap = clapper_gtk_extra_menu_button_unmap;

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
   * ClapperGtkExtraMenuButton:open-subtitles-visible:
   *
   * Visibility of open subtitles option inside popover.
   */
  param_specs[PROP_OPEN_SUBTITLES_VISIBLE] = g_param_spec_boolean ("open-subtitles-visible",
      NULL, NULL, FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      "/com/github/rafostar/Clapper/clapper-gtk/ui/clapper-gtk-extra-menu-button.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, menu_button);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, volume_box);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, volume_image);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, volume_spin);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, speed_box);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, speed_image);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, speed_spin);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, top_separator);

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, video_list_view);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, audio_list_view);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkExtraMenuButton, subtitle_list_view);

  gtk_widget_class_bind_template_callback (widget_class, volume_spin_input_cb);
  gtk_widget_class_bind_template_callback (widget_class, volume_spin_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, volume_spin_changed_cb);

  gtk_widget_class_bind_template_callback (widget_class, speed_spin_input_cb);
  gtk_widget_class_bind_template_callback (widget_class, speed_spin_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, speed_spin_changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-extra-menu-button");
}
