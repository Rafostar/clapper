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

#include "clapper-app-property-row.h"

struct _ClapperAppPropertyRow
{
  AdwActionRow parent;
};

#define parent_class clapper_app_property_row_parent_class
G_DEFINE_TYPE (ClapperAppPropertyRow, clapper_app_property_row, ADW_TYPE_ACTION_ROW);

static inline void
_ensure_subtitle (AdwActionRow *action_row)
{
  const gchar *subtitle = adw_action_row_get_subtitle (action_row);

  if (!subtitle || strlen (subtitle) == 0)
    adw_action_row_set_subtitle (action_row, "-");
}

static void
_subtitle_changed_cb (AdwActionRow *action_row,
    GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  _ensure_subtitle (action_row);
}

static void
clapper_app_property_row_realize (GtkWidget *widget)
{
  _ensure_subtitle (ADW_ACTION_ROW (widget));

  g_signal_connect (widget, "notify::subtitle",
      G_CALLBACK (_subtitle_changed_cb), NULL);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);
}

static void
clapper_app_property_row_unrealize (GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func (widget,
      _subtitle_changed_cb, NULL);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clapper_app_property_row_init (ClapperAppPropertyRow *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "property");
}

static void
clapper_app_property_row_class_init (ClapperAppPropertyRowClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  widget_class->realize = clapper_app_property_row_realize;
  widget_class->unrealize = clapper_app_property_row_unrealize;
}
