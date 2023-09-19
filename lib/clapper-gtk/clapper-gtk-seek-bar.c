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
 * ClapperGtkSeekBar:
 *
 * A bar for seeking and displaying playback position.
 */

#include <clapper/clapper.h>

#include "clapper-gtk-seek-bar.h"
#include "clapper-gtk-container.h"
#include "clapper-gtk-utils.h"

#define DEFAULT_REVEAL_LABELS TRUE
#define DEFAULT_SEEK_METHOD CLAPPER_PLAYER_SEEK_METHOD_NORMAL

#define GST_CAT_DEFAULT clapper_gtk_seek_bar_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkSeekBar
{
  GtkWidget parent;

  GtkWidget *position_revealer;
  GtkWidget *position_label;

  GtkWidget *scale;

  GtkWidget *duration_revealer;
  GtkWidget *duration_label;

  gboolean has_hours;

  gboolean dragging;
  guint position_int;

  gboolean reveal_labels;
  ClapperPlayerSeekMethod seek_method;

  ClapperPlayer *player;
  ClapperMediaItem *current_item;
};

static void
clapper_gtk_seek_bar_add_child (GtkBuildable *buildable,
    GtkBuilder *builder, GObject *child, const char *type)
{
  if (GTK_IS_WIDGET (child)) {
    gtk_widget_insert_before (GTK_WIDGET (child), GTK_WIDGET (buildable), NULL);
  } else {
    GtkBuildableIface *parent_iface = g_type_interface_peek_parent (GTK_BUILDABLE_GET_IFACE (buildable));
    parent_iface->add_child (buildable, builder, child, type);
  }
}

static void
_buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = clapper_gtk_seek_bar_add_child;
}

#define parent_class clapper_gtk_seek_bar_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperGtkSeekBar, clapper_gtk_seek_bar, GTK_TYPE_WIDGET,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, _buildable_iface_init))

enum
{
  PROP_0,
  PROP_REVEAL_LABELS,
  PROP_SEEK_METHOD,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_scale_value_changed_cb (GtkRange *range, ClapperGtkSeekBar *self)
{
  gdouble value = gtk_range_get_value (range);
  gchar *position_str = g_strdup_printf ("%" CLAPPER_TIME_FORMAT, CLAPPER_TIME_ARGS (value));

  gtk_label_set_label (GTK_LABEL (self->position_label),
      (self->has_hours) ? position_str : position_str + 3);
  g_free (position_str);
}

static void
_player_position_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  GtkAdjustment *adjustment;
  gfloat position;

  if (self->dragging)
    return;

  position = clapper_player_get_position (player);

  if (ABS (self->position_int - position) < 1)
    return;

  GST_LOG_OBJECT (self, "Position changed: %f", position);

  self->position_int = (guint) position;

  adjustment = gtk_range_get_adjustment (GTK_RANGE (self->scale));
  gtk_adjustment_set_value (adjustment, position);
}

static void
_update_duration_label (ClapperGtkSeekBar *self, gfloat duration)
{
  GtkAdjustment *adjustment = gtk_range_get_adjustment (GTK_RANGE (self->scale));
  gchar *duration_str = g_strdup_printf ("%" CLAPPER_TIME_FORMAT, CLAPPER_TIME_ARGS (duration));
  gboolean has_hours = (duration >= 3600);

  GST_LOG_OBJECT (self, "Duration changed: %f", duration);

  /* Refresh position label when changing text length */
  if (has_hours != self->has_hours) {
    self->has_hours = has_hours;
    _scale_value_changed_cb (GTK_RANGE (self->scale), self);
  }

  gtk_label_set_label (GTK_LABEL (self->duration_label),
      (self->has_hours) ? duration_str : duration_str + 3);
  g_free (duration_str);

  gtk_adjustment_set_upper (adjustment, duration);
}

static void
_current_item_duration_changed_cb (ClapperMediaItem *current_item,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  _update_duration_label (self, clapper_media_item_get_duration (current_item));
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  /* Disconnect signals from old item */
  if (self->current_item) {
    g_signal_handlers_disconnect_by_func (self->current_item,
        _current_item_duration_changed_cb, self);
  }

  gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item));
  gst_clear_object (&current_item);

  /* Reconnect signals to new item */
  if (self->current_item) {
    g_signal_connect (self->current_item, "notify::duration",
        G_CALLBACK (_current_item_duration_changed_cb), self);

    _current_item_duration_changed_cb (self->current_item, NULL, self);
  } else {
    _update_duration_label (self, 0);
  }
}

static void
_scale_css_classes_changed_cb (GtkWidget *widget,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  const gboolean dragging = gtk_widget_has_css_class (widget, "dragging");
  gdouble value;

  if (self->dragging == dragging)
    return;

  if ((self->dragging = dragging)) {
    GST_DEBUG_OBJECT (self, "Scale drag started");
    return;
  }

  value = gtk_range_get_value (GTK_RANGE (widget));
  GST_DEBUG_OBJECT (self, "Scale dropped at: %lf", value);

  if (G_UNLIKELY (self->player == NULL))
    return;

  clapper_player_seek_custom (self->player, value, self->seek_method);
}

/**
 * clapper_gtk_seek_bar_new:
 *
 * Creates a new #GtkWidget with a seek bar.
 *
 * Returns: (transfer full): a new #GtkWidget instance.
 */
GtkWidget *
clapper_gtk_seek_bar_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_SEEK_BAR, NULL);
}

/**
 * clapper_gtk_seek_bar_set_reveal_labels:
 * @seek_bar: a #ClapperGtkSeekBar
 * @reveal: whether to reveal labels
 *
 * Set whether the position and duration labels should be revealed.
 */
void
clapper_gtk_seek_bar_set_reveal_labels (ClapperGtkSeekBar *self, gboolean reveal)
{
  g_return_if_fail (CLAPPER_GTK_IS_SEEK_BAR (self));

  if (self->reveal_labels != reveal) {
    self->reveal_labels = reveal;
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->position_revealer), reveal);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_REVEAL_LABELS]);
  }
}

/**
 * clapper_gtk_seek_bar_get_reveal_labels:
 * @seek_bar: a #ClapperGtkSeekBar
 *
 * Get whether the position and duration labels are going to be revealed.
 *
 * Returns: TRUE if the labels are going to be revealed, %FALSE otherwise.
 */
gboolean
clapper_gtk_seek_bar_get_reveal_labels (ClapperGtkSeekBar *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_SEEK_BAR (self), FALSE);

  return self->reveal_labels;
}

/**
 * clapper_gtk_seek_bar_set_seek_method:
 * @seek_bar: a #ClapperGtkSeekBar
 * @method: a #ClapperPlayerSeekMethod
 *
 * Set [enum@Clapper.PlayerSeekMethod] to use when seeking with seek bar.
 */
void
clapper_gtk_seek_bar_set_seek_method (ClapperGtkSeekBar *self, ClapperPlayerSeekMethod method)
{
  g_return_if_fail (CLAPPER_GTK_IS_SEEK_BAR (self));

  if (self->seek_method != method) {
    self->seek_method = method;
    GST_DEBUG_OBJECT (self, "Set seek method to: %i", self->seek_method);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_SEEK_METHOD]);
  }
}

/**
 * clapper_gtk_seek_bar_get_seek_method:
 * @seek_bar: a #ClapperGtkSeekBar
 *
 * Get [enum@Clapper.PlayerSeekMethod] used when seeking with seek bar.
 *
 * Returns: #ClapperPlayerSeekMethod used for seeking.
 */
ClapperPlayerSeekMethod
clapper_gtk_seek_bar_get_seek_method (ClapperGtkSeekBar *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_SEEK_BAR (self), DEFAULT_SEEK_METHOD);

  return self->seek_method;
}

static void
clapper_gtk_seek_bar_init (ClapperGtkSeekBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->reveal_labels = DEFAULT_REVEAL_LABELS;
  self->seek_method = DEFAULT_SEEK_METHOD;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->position_revealer), self->reveal_labels);

  g_signal_connect (self->scale, "notify::css-classes",
      G_CALLBACK (_scale_css_classes_changed_cb), self);
  g_signal_connect (self->scale, "value-changed",
      G_CALLBACK (_scale_value_changed_cb), self);
}

static void
clapper_gtk_seek_bar_compute_expand (GtkWidget *widget,
    gboolean *hexpand_p, gboolean *vexpand_p)
{
  GtkWidget *w;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (w = gtk_widget_get_first_child (widget); w != NULL; w = gtk_widget_get_next_sibling (w)) {
    hexpand = (hexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_HORIZONTAL));
    vexpand = (vexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_VERTICAL));
  }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
clapper_gtk_seek_bar_map (GtkWidget *widget)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  if ((self->player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_signal_connect (self->player, "notify::position",
        G_CALLBACK (_player_position_changed_cb), self);
    g_signal_connect (queue, "notify::current-item",
        G_CALLBACK (_queue_current_item_changed_cb), self);

    /* Update duration and then position */
    _queue_current_item_changed_cb (queue, NULL, self);
    _player_position_changed_cb (self->player, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_seek_bar_unmap (GtkWidget *widget)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  if (self->player) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_signal_handlers_disconnect_by_func (self->player, _player_position_changed_cb, self);
    g_signal_handlers_disconnect_by_func (queue, _queue_current_item_changed_cb, self);

    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
clapper_gtk_seek_bar_dispose (GObject *object)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_SEEK_BAR);

  g_clear_pointer (&self->position_revealer, gtk_widget_unparent);
  g_clear_pointer (&self->scale, gtk_widget_unparent);
  g_clear_pointer (&self->duration_revealer, gtk_widget_unparent);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_seek_bar_finalize (GObject *object)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&self->current_item);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_gtk_seek_bar_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (object);

  switch (prop_id) {
    case PROP_REVEAL_LABELS:
      g_value_set_boolean (value, clapper_gtk_seek_bar_get_reveal_labels (self));
      break;
    case PROP_SEEK_METHOD:
      g_value_set_enum (value, clapper_gtk_seek_bar_get_seek_method (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_seek_bar_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (object);

  switch (prop_id) {
    case PROP_REVEAL_LABELS:
      clapper_gtk_seek_bar_set_reveal_labels (self, g_value_get_boolean (value));
      break;
    case PROP_SEEK_METHOD:
      clapper_gtk_seek_bar_set_seek_method (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_seek_bar_class_init (ClapperGtkSeekBarClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkseekbar", 0,
      "Clapper GTK Seek Bar");

  gobject_class->get_property = clapper_gtk_seek_bar_get_property;
  gobject_class->set_property = clapper_gtk_seek_bar_set_property;
  gobject_class->dispose = clapper_gtk_seek_bar_dispose;
  gobject_class->finalize = clapper_gtk_seek_bar_finalize;

  widget_class->compute_expand = clapper_gtk_seek_bar_compute_expand;
  widget_class->map = clapper_gtk_seek_bar_map;
  widget_class->unmap = clapper_gtk_seek_bar_unmap;

  /**
   * ClapperGtkSeekBar:reveal-labels:
   *
   * Reveal state of the position and duration labels.
   */
  param_specs[PROP_REVEAL_LABELS] = g_param_spec_boolean ("reveal-labels",
      NULL, NULL, DEFAULT_REVEAL_LABELS,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkSeekBar:seek-method:
   *
   * Method used for seeking.
   */
  param_specs[PROP_SEEK_METHOD] = g_param_spec_enum ("seek-method",
      NULL, NULL, CLAPPER_TYPE_PLAYER_SEEK_METHOD, DEFAULT_SEEK_METHOD,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      "/com/github/rafostar/Clapper/clapper-gtk/ui/clapper-gtk-seek-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, position_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, position_label);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, scale);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, duration_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, duration_label);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-seek-bar");
}
