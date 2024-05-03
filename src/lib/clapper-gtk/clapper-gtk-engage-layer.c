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
 * ClapperGtkEngageLayer:
 *
 * A layer with big `play` button, engaging user to start the playback.
 *
 * #ClapperGtkEngageLayer widget is meant to be overlaid on top of
 * [class@ClapperGtk.Video] as a normal (non-fading) overlay. It takes
 * care of fading itself once clicked and/or when playback is started.
 *
 * Since: 0.8
 */

#include "config.h"

#include <clapper/clapper.h>

#include "clapper-gtk-engage-layer.h"

#define GST_CAT_DEFAULT clapper_gtk_engage_layer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkEngageLayer
{
  ClapperGtkLeadContainer parent;
};

#define parent_class clapper_gtk_engage_layer_parent_class
G_DEFINE_TYPE (ClapperGtkEngageLayer, clapper_gtk_engage_layer, CLAPPER_GTK_TYPE_LEAD_CONTAINER)

static void
adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperGtkEngageLayer *self)
{
  GST_DEBUG_OBJECT (self, "Adapted: %s", (adapt) ? "yes" : "no");
}

/**
 * clapper_gtk_engage_layer_new:
 *
 * Creates a new #ClapperGtkEngageLayer instance.
 *
 * Returns: a new playback engage layer #GtkWidget.
 *
 * Since: 0.8
 */
GtkWidget *
clapper_gtk_engage_layer_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_ENGAGE_LAYER, NULL);
}

static void
clapper_gtk_engage_layer_init (ClapperGtkEngageLayer *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clapper_gtk_engage_layer_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_ENGAGE_LAYER);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_engage_layer_class_init (ClapperGtkEngageLayerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkengagelayer", 0,
      "Clapper GTK Engage Layer");

  gobject_class->dispose = clapper_gtk_engage_layer_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-engage-layer.ui");

  gtk_widget_class_bind_template_callback (widget_class, adapt_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-engage-layer");
}
