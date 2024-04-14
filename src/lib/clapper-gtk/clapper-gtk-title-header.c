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
 * ClapperGtkTitleHeader:
 *
 * A header panel widget that displays current media title.
 *
 * #ClapperGtkTitleHeader is a simple, ready to be used header widget that
 * displays current media title. It can be placed as-is as a [class@ClapperGtk.Video]
 * overlay (either fading or not).
 *
 * If you need a further customized header, you can use [class@ClapperGtk.TitleLabel]
 * which is used by this widget to build your own implementation instead.
 */

#include "config.h"

#include <gst/gst.h>

#include "clapper-gtk-title-header.h"
#include "clapper-gtk-title-label.h"

#define DEFAULT_FALLBACK_TO_URI FALSE

#define GST_CAT_DEFAULT clapper_gtk_title_header_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkTitleHeader
{
  ClapperGtkLeadContainer parent;

  ClapperGtkTitleLabel *label;
};

#define parent_class clapper_gtk_title_header_parent_class
G_DEFINE_TYPE (ClapperGtkTitleHeader, clapper_gtk_title_header, CLAPPER_GTK_TYPE_LEAD_CONTAINER)

enum
{
  PROP_0,
  PROP_CURRENT_TITLE,
  PROP_FALLBACK_TO_URI,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_label_current_title_changed_cb (ClapperGtkTitleLabel *label G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkTitleHeader *self)
{
  /* Forward current title changed notify from internal label */
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_TITLE]);
}

/**
 * clapper_gtk_title_header_new:
 *
 * Creates a new #ClapperGtkTitleHeader instance.
 *
 * Returns: a new title header #GtkWidget.
 */
GtkWidget *
clapper_gtk_title_header_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_TITLE_HEADER, NULL);
}

/**
 * clapper_gtk_title_header_get_current_title:
 * @header: a #ClapperGtkTitleHeader
 *
 * Get currently displayed title by @header.
 *
 * Returns: (transfer none): text of title label.
 */
const gchar *
clapper_gtk_title_header_get_current_title (ClapperGtkTitleHeader *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_TITLE_HEADER (self), NULL);

  return clapper_gtk_title_label_get_current_title (self->label);
}

/**
 * clapper_gtk_title_header_set_fallback_to_uri:
 * @header: a #ClapperGtkTitleHeader
 * @enabled: whether enabled
 *
 * Set whether a [property@Clapper.MediaItem:uri] property should
 * be displayed as a header text when no other title could be determined.
 */
void
clapper_gtk_title_header_set_fallback_to_uri (ClapperGtkTitleHeader *self, gboolean enabled)
{
  g_return_if_fail (CLAPPER_GTK_IS_TITLE_HEADER (self));

  clapper_gtk_title_label_set_fallback_to_uri (self->label, enabled);
}

/**
 * clapper_gtk_title_header_get_fallback_to_uri:
 * @header: a #ClapperGtkTitleHeader
 *
 * Get whether a [property@Clapper.MediaItem:uri] property is going
 * be displayed as a header text when no other title could be determined.
 *
 * Returns: %TRUE when item URI will be used as fallback, %FALSE otherwise.
 */
gboolean
clapper_gtk_title_header_get_fallback_to_uri (ClapperGtkTitleHeader *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_TITLE_HEADER (self), FALSE);

  return clapper_gtk_title_label_get_fallback_to_uri (self->label);
}

static void
clapper_gtk_title_header_init (ClapperGtkTitleHeader *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  clapper_gtk_title_label_set_fallback_to_uri (self->label, DEFAULT_FALLBACK_TO_URI);

  g_object_bind_property (self->label, "fallback-to-uri",
      self, "fallback-to-uri", G_BINDING_DEFAULT);
  g_signal_connect (self->label, "notify::current-title",
      G_CALLBACK (_label_current_title_changed_cb), self);
}

static void
clapper_gtk_title_header_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_TITLE_HEADER);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_title_header_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkTitleHeader *self = CLAPPER_GTK_TITLE_HEADER_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_TITLE:
      g_value_set_string (value, clapper_gtk_title_header_get_current_title (self));
      break;
    case PROP_FALLBACK_TO_URI:
      g_value_set_boolean (value, clapper_gtk_title_header_get_fallback_to_uri (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_title_header_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkTitleHeader *self = CLAPPER_GTK_TITLE_HEADER_CAST (object);

  switch (prop_id) {
    case PROP_FALLBACK_TO_URI:
      clapper_gtk_title_header_set_fallback_to_uri (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_title_header_class_init (ClapperGtkTitleHeaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtktitleheader", 0,
      "Clapper GTK Title Header");

  gobject_class->get_property = clapper_gtk_title_header_get_property;
  gobject_class->set_property = clapper_gtk_title_header_set_property;
  gobject_class->dispose = clapper_gtk_title_header_dispose;

  /**
   * ClapperGtkTitleHeader:current-title:
   *
   * Currently displayed title.
   */
  param_specs[PROP_CURRENT_TITLE] = g_param_spec_string ("current-title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkTitleHeader:fallback-to-uri:
   *
   * When title cannot be determined, show URI instead.
   */
  param_specs[PROP_FALLBACK_TO_URI] = g_param_spec_boolean ("fallback-to-uri",
      NULL, NULL, DEFAULT_FALLBACK_TO_URI,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-title-header.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkTitleHeader, label);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-title-header");
}
