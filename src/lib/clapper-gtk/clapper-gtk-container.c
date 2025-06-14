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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * ClapperGtkContainer:
 *
 * A simple container widget that holds just one child.
 *
 * It is designed to work well with OSD overlay, adding some useful functionalities
 * to it, such as width and height that widget should target. This helps with
 * implementing simple adaptive widgets by observing its own width and signalling
 * when adaptive threshold is reached.
 *
 * You can use this when you need to create a widget that is adaptive or should have
 * a limited maximal width/height.
 *
 * If you need to have more then single widget as child, place a widget that
 * can hold multiple children such as [class@Gtk.Box] as a single conatiner child
 * and then your widgets into that child.
 */

#include "clapper-gtk-container.h"
#include "clapper-gtk-container-private.h"
#include "clapper-gtk-limited-layout-private.h"

#define parent_class clapper_gtk_container_parent_class
G_DEFINE_TYPE (ClapperGtkContainer, clapper_gtk_container, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_WIDTH_TARGET,
  PROP_HEIGHT_TARGET,
  PROP_ADAPTIVE_WIDTH,
  PROP_ADAPTIVE_HEIGHT,
  PROP_LAST
};

enum
{
  SIGNAL_ADAPT,
  SIGNAL_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

static inline void
_unparent_child (ClapperGtkContainer *self)
{
  GtkWidget *child;

  if ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);
}

/**
 * clapper_gtk_container_new:
 *
 * Creates a new #ClapperGtkContainer instance.
 *
 * Returns: a new container #GtkWidget.
 */
GtkWidget *
clapper_gtk_container_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_CONTAINER, NULL);
}

/**
 * clapper_gtk_container_set_child:
 * @container: a #ClapperGtkContainer
 * @child: a #GtkWidget
 *
 * Set a child #GtkWidget of @container.
 */
void
clapper_gtk_container_set_child (ClapperGtkContainer *self, GtkWidget *child)
{
  g_return_if_fail (CLAPPER_GTK_IS_CONTAINER (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  _unparent_child (self);
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

/**
 * clapper_gtk_container_get_child:
 * @container: a #ClapperGtkContainer
 *
 * Get a child #GtkWidget of @container.
 *
 * Returns: (transfer none) (nullable): #GtkWidget set as child.
 */
GtkWidget *
clapper_gtk_container_get_child (ClapperGtkContainer *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_CONTAINER (self), NULL);

  return gtk_widget_get_first_child (GTK_WIDGET (self));
}

/**
 * clapper_gtk_container_set_width_target:
 * @container: a #ClapperGtkContainer
 * @width: width to target -1 to restore default behavior
 *
 * Set a width that @container should target. When set container
 * will not stretch beyond set @width while still expanding into
 * possible boundaries trying to reach its target.
 */
void
clapper_gtk_container_set_width_target (ClapperGtkContainer *self, gint width)
{
  GtkLayoutManager *layout;

  g_return_if_fail (CLAPPER_GTK_IS_CONTAINER (self));

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  clapper_gtk_limited_layout_set_max_width (CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout), width);
}

/**
 * clapper_gtk_container_get_width_target:
 * @container: a #ClapperGtkContainer
 *
 * Get a @container width target.
 *
 * Returns: width target set by user or -1 when none.
 */
gint
clapper_gtk_container_get_width_target (ClapperGtkContainer *self)
{
  GtkLayoutManager *layout;

  g_return_val_if_fail (CLAPPER_GTK_IS_CONTAINER (self), -1);

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  return clapper_gtk_limited_layout_get_max_width (CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout));
}

/**
 * clapper_gtk_container_set_height_target:
 * @container: a #ClapperGtkContainer
 * @height: height to target or -1 to restore default behavior
 *
 * Same as clapper_gtk_container_set_width_target() but for widget height.
 */
void
clapper_gtk_container_set_height_target (ClapperGtkContainer *self, gint height)
{
  GtkLayoutManager *layout;

  g_return_if_fail (CLAPPER_GTK_IS_CONTAINER (self));

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  clapper_gtk_limited_layout_set_max_height (CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout), height);
}

/**
 * clapper_gtk_container_get_height_target:
 * @container: a #ClapperGtkContainer
 *
 * Get a @container height target.
 *
 * Returns: height target set by user or -1 when none.
 */
gint
clapper_gtk_container_get_height_target (ClapperGtkContainer *self)
{
  GtkLayoutManager *layout;

  g_return_val_if_fail (CLAPPER_GTK_IS_CONTAINER (self), -1);

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  return clapper_gtk_limited_layout_get_max_height (CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout));
}

/**
 * clapper_gtk_container_set_adaptive_width:
 * @container: a #ClapperGtkContainer
 * @width: a threshold on which adapt signal should be triggered or -1 to disable.
 *
 * Set an adaptive width threshold. When widget is resized to value or lower,
 * an [signal@ClapperGtk.Container::adapt] signal will be emitted with %TRUE to
 * notify implementation about mobile adaptation request, otherwise %FALSE when
 * both threshold values are exceeded.
 */
void
clapper_gtk_container_set_adaptive_width (ClapperGtkContainer *self, gint width)
{
  GtkLayoutManager *layout;

  g_return_if_fail (CLAPPER_GTK_IS_CONTAINER (self));

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  clapper_gtk_limited_layout_set_adaptive_width (
      CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout), width);
}

/**
 * clapper_gtk_container_get_adaptive_width:
 * @container: a #ClapperGtkContainer
 *
 * Get a @container adaptive width threshold.
 *
 * Returns: adaptive width set by user or -1 when none.
 */
gint
clapper_gtk_container_get_adaptive_width (ClapperGtkContainer *self)
{
  GtkLayoutManager *layout;

  g_return_val_if_fail (CLAPPER_GTK_IS_CONTAINER (self), -1);

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  return clapper_gtk_limited_layout_get_adaptive_width (CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout));
}

/**
 * clapper_gtk_container_set_adaptive_height:
 * @container: a #ClapperGtkContainer
 * @height: a threshold on which adapt signal should be triggered or -1 to disable.
 *
 * Set an adaptive height threshold. When widget is resized to value or lower,
 * an [signal@ClapperGtk.Container::adapt] signal will be emitted with %TRUE to
 * notify implementation about mobile adaptation request, otherwise %FALSE when
 * both threshold values are exceeded.
 */
void
clapper_gtk_container_set_adaptive_height (ClapperGtkContainer *self, gint height)
{
  GtkLayoutManager *layout;

  g_return_if_fail (CLAPPER_GTK_IS_CONTAINER (self));

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  clapper_gtk_limited_layout_set_adaptive_height (
      CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout), height);
}

/**
 * clapper_gtk_container_get_adaptive_height:
 * @container: a #ClapperGtkContainer
 *
 * Get a @container adaptive height threshold.
 *
 * Returns: adaptive height set by user or -1 when none.
 */
gint
clapper_gtk_container_get_adaptive_height (ClapperGtkContainer *self)
{
  GtkLayoutManager *layout;

  g_return_val_if_fail (CLAPPER_GTK_IS_CONTAINER (self), -1);

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  return clapper_gtk_limited_layout_get_adaptive_height (CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout));
}

void
clapper_gtk_container_emit_adapt (ClapperGtkContainer *self, gboolean adapt)
{
  if (g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
      signals[SIGNAL_ADAPT], 0, NULL, NULL, NULL) != 0) {
    g_signal_emit (self, signals[SIGNAL_ADAPT], 0, adapt);
  }
}

static void
clapper_gtk_container_init (ClapperGtkContainer *self)
{
}

static void
clapper_gtk_container_dispose (GObject *object)
{
  ClapperGtkContainer *self = CLAPPER_GTK_CONTAINER_CAST (object);

  _unparent_child (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_container_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkContainer *self = CLAPPER_GTK_CONTAINER_CAST (object);

  switch (prop_id) {
    case PROP_WIDTH_TARGET:
      g_value_set_int (value, clapper_gtk_container_get_width_target (self));
      break;
    case PROP_HEIGHT_TARGET:
      g_value_set_int (value, clapper_gtk_container_get_height_target (self));
      break;
    case PROP_ADAPTIVE_WIDTH:
      g_value_set_int (value, clapper_gtk_container_get_adaptive_width (self));
      break;
    case PROP_ADAPTIVE_HEIGHT:
      g_value_set_int (value, clapper_gtk_container_get_adaptive_height (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_container_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkContainer *self = CLAPPER_GTK_CONTAINER_CAST (object);

  switch (prop_id) {
    case PROP_WIDTH_TARGET:
      clapper_gtk_container_set_width_target (self, g_value_get_int (value));
      break;
    case PROP_HEIGHT_TARGET:
      clapper_gtk_container_set_height_target (self, g_value_get_int (value));
      break;
    case PROP_ADAPTIVE_WIDTH:
      clapper_gtk_container_set_adaptive_width (self, g_value_get_int (value));
      break;
    case PROP_ADAPTIVE_HEIGHT:
      clapper_gtk_container_set_adaptive_height (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_container_class_init (ClapperGtkContainerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  gobject_class->get_property = clapper_gtk_container_get_property;
  gobject_class->set_property = clapper_gtk_container_set_property;
  gobject_class->dispose = clapper_gtk_container_dispose;

  /**
   * ClapperGtkContainer:width-target:
   *
   * Width that container should target.
   */
  param_specs[PROP_WIDTH_TARGET] = g_param_spec_int ("width-target",
      NULL, NULL, -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkContainer:height-target:
   *
   * Height that container should target.
   */
  param_specs[PROP_HEIGHT_TARGET] = g_param_spec_int ("height-target",
      NULL, NULL, -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkContainer:adaptive-width:
   *
   * Adaptive width threshold that triggers [signal@ClapperGtk.Container::adapt] signal.
   */
  param_specs[PROP_ADAPTIVE_WIDTH] = g_param_spec_int ("adaptive-width",
      NULL, NULL, -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkContainer:adaptive-height:
   *
   * Adaptive height threshold that triggers [signal@ClapperGtk.Container::adapt] signal.
   */
  param_specs[PROP_ADAPTIVE_HEIGHT] = g_param_spec_int ("adaptive-height",
      NULL, NULL, -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkContainer::adapt:
   * @container: a #ClapperGtkContainer
   * @adapt: %TRUE if narrowness reached adaptive threshold, %FALSE otherwise
   *
   * A helper signal for implementing mobile/narrow adaptive
   * behavior on descendants.
   */
  signals[SIGNAL_ADAPT] = g_signal_new ("adapt",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_layout_manager_type (widget_class, CLAPPER_GTK_TYPE_LIMITED_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-container");
}
