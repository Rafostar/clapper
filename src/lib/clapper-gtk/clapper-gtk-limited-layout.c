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

#include <clapper/clapper.h>

#include "clapper-gtk-limited-layout-private.h"
#include "clapper-gtk-container-private.h"

#define GST_CAT_DEFAULT clapper_gtk_limited_layout_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkLimitedLayout
{
  GtkLayoutManager parent;

  gint max_width;
  gint max_height;

  gint adaptive_width;
  gint adaptive_height;

  gboolean adapt;
};

#define parent_class clapper_gtk_limited_layout_parent_class
G_DEFINE_TYPE (ClapperGtkLimitedLayout, clapper_gtk_limited_layout, GTK_TYPE_LAYOUT_MANAGER)

void
clapper_gtk_limited_layout_set_max_width (ClapperGtkLimitedLayout *self, gint max_width)
{
  self->max_width = max_width;
}

gint
clapper_gtk_limited_layout_get_max_width (ClapperGtkLimitedLayout *self)
{
  return self->max_width;
}

void
clapper_gtk_limited_layout_set_max_height (ClapperGtkLimitedLayout *self, gint max_height)
{
  self->max_height = max_height;
}

gint
clapper_gtk_limited_layout_get_max_height (ClapperGtkLimitedLayout *self)
{
  return self->max_height;
}

void
clapper_gtk_limited_layout_set_adaptive_width (ClapperGtkLimitedLayout *self, gint width)
{
  self->adaptive_width = width;
}

gint
clapper_gtk_limited_layout_get_adaptive_width (ClapperGtkLimitedLayout *self)
{
  return self->adaptive_width;
}

void
clapper_gtk_limited_layout_set_adaptive_height (ClapperGtkLimitedLayout *self, gint height)
{
  self->adaptive_height = height;
}

gint
clapper_gtk_limited_layout_get_adaptive_height (ClapperGtkLimitedLayout *self)
{
  return self->adaptive_height;
}

static void
clapper_gtk_limited_layout_measure (GtkLayoutManager *layout_manager,
    GtkWidget *widget, GtkOrientation orientation, gint for_size,
    gint *minimum, gint *natural, gint *minimum_baseline, gint *natural_baseline)
{
  ClapperGtkLimitedLayout *self = CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout_manager);
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child && gtk_widget_should_layout (child)) {
    gint child_min = 0, child_nat = 0;
    gint child_min_baseline = -1, child_nat_baseline = -1;

    gtk_widget_measure (child, orientation, for_size,
        &child_min, &child_nat,
        &child_min_baseline, &child_nat_baseline);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
      *natural = (self->max_width < 0) ? MAX (*natural, child_nat) : self->max_width;
    else if (orientation == GTK_ORIENTATION_VERTICAL)
      *natural = (self->max_height < 0) ? MAX (*natural, child_nat) : self->max_height;

    *minimum = MAX (*minimum, child_min);

    if (child_min_baseline > -1)
      *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
    if (child_nat_baseline > -1)
      *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
  }
}

static void
clapper_gtk_limited_layout_allocate (GtkLayoutManager *layout_manager,
    GtkWidget *widget, gint width, gint height, gint baseline)
{
  ClapperGtkLimitedLayout *self = CLAPPER_GTK_LIMITED_LAYOUT_CAST (layout_manager);
  GtkWidget *child = gtk_widget_get_first_child (widget);
  gboolean adapt = (width <= self->adaptive_width || height <= self->adaptive_height);

  if (child && gtk_widget_should_layout (child))
    gtk_widget_allocate (child, width, height, baseline, NULL);

  if (G_UNLIKELY (self->adapt != adapt)) {
    self->adapt = adapt;
    clapper_gtk_container_emit_adapt (CLAPPER_GTK_CONTAINER_CAST (widget), adapt);
  }
}

static void
clapper_gtk_limited_layout_init (ClapperGtkLimitedLayout *self)
{
  self->max_width = -1;
  self->max_height = -1;

  self->adaptive_width = -1;
  self->adaptive_height = -1;
}

static void
clapper_gtk_limited_layout_class_init (ClapperGtkLimitedLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_manager_class = (GtkLayoutManagerClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtklimitedlayout", 0,
      "Clapper GTK Limited Layout");

  layout_manager_class->measure = clapper_gtk_limited_layout_measure;
  layout_manager_class->allocate = clapper_gtk_limited_layout_allocate;
}
