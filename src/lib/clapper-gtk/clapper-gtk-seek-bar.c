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
 * ClapperGtkSeekBar:
 *
 * A bar for seeking and displaying playback position.
 */

#include "config.h"

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

  GtkPopover *popover;
  GtkLabel *popover_label;

  GtkWidget *duration_revealer;
  GtkWidget *duration_label;

  gboolean has_hours;
  gboolean has_markers;

  gboolean can_scrub;
  gboolean scrubbing;
  gboolean was_playing;

  gboolean dragging;
  guint position_uint;

  gulong position_signal_id;

  gboolean reveal_labels;
  ClapperPlayerSeekMethod seek_method;

  ClapperPlayer *player;
  ClapperMediaItem *current_item;

  /* Cache */
  gdouble curr_marker_start;
  gdouble next_marker_start;
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

static inline gboolean
_prepare_popover (ClapperGtkSeekBar *self, gdouble x,
    gdouble pointing_val, gdouble upper)
{
  /* Avoid iterating through markers if within last marker range
   * (currently set title label remains the same) */
  gboolean found_title = (pointing_val >= self->curr_marker_start
      && pointing_val < self->next_marker_start);

  if (!found_title) {
    ClapperTimeline *timeline = clapper_media_item_get_timeline (self->current_item);
    guint i = clapper_timeline_get_n_markers (timeline);

    GST_DEBUG ("Searching for marker at: %lf", pointing_val);

    /* We start from the end of scale */
    self->next_marker_start = upper;

    while (i--) {
      ClapperMarker *marker = clapper_timeline_get_marker (timeline, i);
      self->curr_marker_start = clapper_marker_get_start (marker);

      if (self->curr_marker_start <= pointing_val) {
        const gchar *title = clapper_marker_get_title (marker);

        GST_DEBUG ("Found marker, range: (%lf-%lf), title: \"%s\"",
            self->curr_marker_start, self->next_marker_start,
            GST_STR_NULL (title));

        /* XXX: It does string comparison internally, so its more efficient
         * for us and we do not have to compare strings here too */
        gtk_label_set_label (self->popover_label, title);
        found_title = (title != NULL);
      }

      gst_object_unref (marker);

      if (found_title)
        break;

      self->next_marker_start = self->curr_marker_start;
    }
  }

  gtk_popover_set_pointing_to (self->popover,
      &(const GdkRectangle){ x, 0, 1, 1 });

  return found_title;
}

static inline gboolean
_compute_scale_coords (ClapperGtkSeekBar *self,
    gdouble *min_pointing_val, gdouble *max_pointing_val)
{
  graphene_rect_t slider_bounds;

  if (!gtk_widget_compute_bounds (GTK_WIDGET (self), self->scale, &slider_bounds))
    return FALSE;

  /* XXX: Number "2" is the correction for range protruding rounded sides
   * compared to how marks above/below it are positioned */
  *min_pointing_val = -slider_bounds.origin.x + 2;
  *max_pointing_val = slider_bounds.size.width + slider_bounds.origin.x - 2;

  return TRUE;
}

static void
_player_position_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  GtkAdjustment *adjustment;
  gdouble position;

  if (self->dragging)
    return;

  position = clapper_player_get_position (player);

  if (ABS (self->position_uint - position) < 1)
    return;

  GST_LOG_OBJECT (self, "Position changed: %lf", position);

  self->position_uint = (guint) position;

  adjustment = gtk_range_get_adjustment (GTK_RANGE (self->scale));
  gtk_adjustment_set_value (adjustment, position);
}

static void
_player_state_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  switch (clapper_player_get_state (player)) {
    case CLAPPER_PLAYER_STATE_PAUSED:
      /* Force refresh, so scale always reaches end after playback */
      self->position_uint = G_MAXUINT;
      _player_position_changed_cb (player, NULL, self);
      break;
    default:
      break;
  }
}

static void
_player_seek_done_cb (ClapperPlayer *player, ClapperGtkSeekBar *self)
{
  GST_DEBUG ("Seek done");

  if (self->position_signal_id == 0) {
    self->position_signal_id = g_signal_connect (self->player,
        "notify::position", G_CALLBACK (_player_position_changed_cb), self);
  }
  _player_position_changed_cb (player, NULL, self);
}

static void
scale_value_changed_cb (GtkRange *range, ClapperGtkSeekBar *self)
{
  gdouble value = gtk_range_get_value (range);
  gchar *position_str = g_strdup_printf ("%" CLAPPER_TIME_FORMAT, CLAPPER_TIME_ARGS (value));

  gtk_label_set_label (GTK_LABEL (self->position_label),
      (self->has_hours) ? position_str : position_str + 3);
  g_free (position_str);

  if (self->dragging && self->has_markers) {
    gdouble min_pointing_val, max_pointing_val;
    gdouble x, upper, scaling;

    if (!_compute_scale_coords (self, &min_pointing_val, &max_pointing_val)) {
      gtk_popover_popdown (self->popover);
      return;
    }

    upper = gtk_adjustment_get_upper (
        gtk_range_get_adjustment (GTK_RANGE (self->scale)));
    scaling = (upper / (max_pointing_val - min_pointing_val));

    x = min_pointing_val + (value / scaling);

    if (_prepare_popover (self, x, value, upper))
      gtk_popover_popup (self->popover);
    else
      gtk_popover_popdown (self->popover);
  }
}

static void
scale_css_classes_changed_cb (GtkWidget *widget,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  const gboolean dragging = gtk_widget_has_css_class (widget, "dragging");
  gdouble value;

  if (self->dragging == dragging)
    return;

  if ((self->dragging = dragging)) {
    GST_DEBUG_OBJECT (self, "Scale drag started");

    if (G_LIKELY (self->player != NULL)) {
      ClapperPlayerState state = clapper_player_get_state (self->player);

      if ((self->was_playing = (state == CLAPPER_PLAYER_STATE_PLAYING)))
        clapper_player_pause (self->player);
    }

    return;
  }

  value = gtk_range_get_value (GTK_RANGE (widget));
  GST_DEBUG_OBJECT (self, "Scale dropped at: %lf", value);

  if (G_UNLIKELY (self->player == NULL))
    return;

  if (self->position_signal_id != 0) {
    g_signal_handler_disconnect (self->player, self->position_signal_id);
    self->position_signal_id = 0;
  }

  /* We should be ALWAYS doing normal seeks if dropped at marker position */
  if (self->has_markers
      && G_APPROX_VALUE (self->curr_marker_start, value, FLT_EPSILON)) {
    GST_DEBUG ("Seeking to marker");
    clapper_player_seek (self->player, value);
  } else {
    clapper_player_seek_custom (self->player, value, self->seek_method);
  }

  if (self->was_playing)
    clapper_player_play (self->player);
}

static void
scale_scroll_begin_cb (GtkEventControllerScroll *scroll, ClapperGtkSeekBar *self)
{
  self->can_scrub = TRUE;
}

static gboolean
scale_scroll_cb (GtkEventControllerScroll *scroll,
    gdouble dx, gdouble dy, ClapperGtkSeekBar *self)
{
  if (self->can_scrub && !self->scrubbing) {
    GST_DEBUG_OBJECT (self, "Scrubbing start");
    self->scrubbing = TRUE;
    gtk_widget_add_css_class (self->scale, "dragging");

    return TRUE;
  }

  return FALSE;
}

static void
scale_scroll_end_cb (GtkEventControllerScroll *scroll, ClapperGtkSeekBar *self)
{
  if (self->scrubbing) {
    GST_DEBUG_OBJECT (self, "Scrubbing end");
    gtk_widget_remove_css_class (self->scale, "dragging");
    self->scrubbing = FALSE;
  }
  self->can_scrub = FALSE;
}

static void
motion_cb (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, ClapperGtkSeekBar *self)
{
  gdouble min_pointing_val, max_pointing_val, pointing_val;
  gdouble upper, scaling;

  /* If no markers, popover should never popup,
   * so we do not try to pop it down here */
  if (!self->has_markers)
    return;

  if (!_compute_scale_coords (self, &min_pointing_val, &max_pointing_val)
      || (x < min_pointing_val || x > max_pointing_val)) {
    gtk_popover_popdown (self->popover);
    return;
  }

  upper = gtk_adjustment_get_upper (
      gtk_range_get_adjustment (GTK_RANGE (self->scale)));
  scaling = (upper / (max_pointing_val - min_pointing_val));

  pointing_val = (x - min_pointing_val) * scaling;
  GST_LOG ("Cursor pointing to: %lf", pointing_val);

  if (_prepare_popover (self, x, pointing_val, upper))
    gtk_popover_popup (self->popover);
  else
    gtk_popover_popdown (self->popover);
}

static void
motion_leave_cb (GtkEventControllerMotion *motion, ClapperGtkSeekBar *self)
{
  gtk_popover_popdown (self->popover);
}

static void
touch_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, ClapperGtkSeekBar *self)
{
  gtk_popover_popdown (self->popover);
}

static void
_update_duration_label (ClapperGtkSeekBar *self, gdouble duration)
{
  GtkAdjustment *adjustment = gtk_range_get_adjustment (GTK_RANGE (self->scale));
  gchar *duration_str = g_strdup_printf ("%" CLAPPER_TIME_FORMAT, CLAPPER_TIME_ARGS (duration));
  gboolean has_hours = (duration >= 3600);

  GST_LOG_OBJECT (self, "Duration changed: %lf", duration);

  /* Refresh position label when changing text length */
  if (has_hours != self->has_hours) {
    self->has_hours = has_hours;
    scale_value_changed_cb (GTK_RANGE (self->scale), self);
  }

  gtk_label_set_label (GTK_LABEL (self->duration_label),
      (self->has_hours) ? duration_str : duration_str + 3);
  g_free (duration_str);

  gtk_adjustment_set_upper (adjustment, duration);
}

static void
_update_scale_marks (ClapperGtkSeekBar *self, ClapperTimeline *timeline)
{
  GtkAdjustment *adjustment;
  guint i, n_markers = clapper_timeline_get_n_markers (timeline);

  GST_DEBUG_OBJECT (self, "Placing %u markers on scale", n_markers);

  gtk_scale_clear_marks (GTK_SCALE (self->scale));

  self->curr_marker_start = -1;
  self->next_marker_start = -1;
  self->has_markers = FALSE;

  if (n_markers == 0) {
    gtk_popover_popdown (self->popover);
    return;
  }

  adjustment = gtk_range_get_adjustment (GTK_RANGE (self->scale));

  /* Avoid placing marks when duration is zero. Otherwise we may
   * end up with a single mark at zero until another refresh. */
  if (gtk_adjustment_get_upper (adjustment) <= 0)
    return;

  for (i = 0; i < n_markers; ++i) {
    ClapperMarker *marker = clapper_timeline_get_marker (timeline, i);
    gdouble start = clapper_marker_get_start (marker);

    gtk_scale_add_mark (GTK_SCALE (self->scale), start, GTK_POS_TOP, NULL);
    gtk_scale_add_mark (GTK_SCALE (self->scale), start, GTK_POS_BOTTOM, NULL);

    gst_object_unref (marker);
  }

  self->has_markers = TRUE;
}

static void
_current_item_duration_changed_cb (ClapperMediaItem *current_item,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  /* GtkScale ignores markers placed post its adjustment upper range.
   * We need to place them again on scale AFTER duration changes. */
  _update_duration_label (self, clapper_media_item_get_duration (current_item));
  _update_scale_marks (self, clapper_media_item_get_timeline (current_item));
}

static void
_timeline_markers_changed_cb (GListModel *list_model, guint position,
    guint removed, guint added, ClapperGtkSeekBar *self)
{
  _update_scale_marks (self, CLAPPER_TIMELINE (list_model));
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSeekBar *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  /* Disconnect signals from old item */
  if (self->current_item) {
    ClapperTimeline *timeline = clapper_media_item_get_timeline (self->current_item);

    g_signal_handlers_disconnect_by_func (self->current_item,
        _current_item_duration_changed_cb, self);
    g_signal_handlers_disconnect_by_func (timeline,
        _timeline_markers_changed_cb, self);
  }

  gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item));
  gst_clear_object (&current_item);

  /* Reconnect signals to new item */
  if (self->current_item) {
    ClapperTimeline *timeline = clapper_media_item_get_timeline (self->current_item);

    g_signal_connect (self->current_item, "notify::duration",
        G_CALLBACK (_current_item_duration_changed_cb), self);
    g_signal_connect (timeline, "items-changed",
        G_CALLBACK (_timeline_markers_changed_cb), self);

    _update_duration_label (self, clapper_media_item_get_duration (self->current_item));
    _update_scale_marks (self, timeline);
  } else {
    gtk_scale_clear_marks (GTK_SCALE (self->scale));
    _update_duration_label (self, 0);
  }
}

/**
 * clapper_gtk_seek_bar_new:
 *
 * Creates a new #ClapperGtkSeekBar instance.
 *
 * Returns: a new seek bar #GtkWidget.
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

  self->curr_marker_start = -1;
  self->next_marker_start = -1;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->position_revealer), self->reveal_labels);

  /* Correction for calculated popover position when marks are drawn */
  gtk_popover_set_offset (self->popover, 0, -2);
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
clapper_gtk_seek_bar_size_allocate (GtkWidget *widget,
    gint width, gint height, gint baseline)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  gtk_popover_present (self->popover);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, width, height, baseline);
}

static void
clapper_gtk_seek_bar_realize (GtkWidget *widget)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  if ((self->player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    g_signal_connect (queue, "notify::current-item",
        G_CALLBACK (_queue_current_item_changed_cb), self);
    _queue_current_item_changed_cb (queue, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->realize (widget);
}

static void
clapper_gtk_seek_bar_unrealize (GtkWidget *widget)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  if (self->player) {
    ClapperQueue *queue = clapper_player_get_queue (self->player);

    if (self->position_signal_id != 0) {
      g_signal_handler_disconnect (self->player, self->position_signal_id);
      self->position_signal_id = 0;
    }
    g_signal_handlers_disconnect_by_func (queue, _queue_current_item_changed_cb, self);

    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clapper_gtk_seek_bar_map (GtkWidget *widget)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  if (self->player) {
    if (self->position_signal_id == 0) {
      self->position_signal_id = g_signal_connect (self->player,
          "notify::position", G_CALLBACK (_player_position_changed_cb), self);
    }
    g_signal_connect (self->player, "notify::state",
        G_CALLBACK (_player_state_changed_cb), self);
    g_signal_connect (self->player, "seek-done",
        G_CALLBACK (_player_seek_done_cb), self);

    _player_position_changed_cb (self->player, NULL, self);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
clapper_gtk_seek_bar_unmap (GtkWidget *widget)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (widget);

  if (self->player) {
    if (self->position_signal_id != 0) {
      g_signal_handler_disconnect (self->player, self->position_signal_id);
      self->position_signal_id = 0;
    }
    g_signal_handlers_disconnect_by_func (self->player, _player_state_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->player, _player_seek_done_cb, self);
  }

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
_popover_unparent (GtkPopover *popover)
{
  gtk_widget_unparent (GTK_WIDGET (popover));
}

static void
clapper_gtk_seek_bar_dispose (GObject *object)
{
  ClapperGtkSeekBar *self = CLAPPER_GTK_SEEK_BAR_CAST (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_SEEK_BAR);

  g_clear_pointer (&self->position_revealer, gtk_widget_unparent);
  g_clear_pointer (&self->scale, gtk_widget_unparent);
  g_clear_pointer (&self->popover, _popover_unparent);
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
  widget_class->size_allocate = clapper_gtk_seek_bar_size_allocate;
  widget_class->realize = clapper_gtk_seek_bar_realize;
  widget_class->unrealize = clapper_gtk_seek_bar_unrealize;
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
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-seek-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, position_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, position_label);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, scale);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, popover);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, popover_label);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, duration_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSeekBar, duration_label);

  gtk_widget_class_bind_template_callback (widget_class, scale_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, scale_css_classes_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, scale_scroll_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, scale_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, scale_scroll_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, touch_released_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-seek-bar");
}
