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

#include <glib/gi18n.h>
#include <gst/gst.h>
#include <clapper-gtk/clapper-gtk.h>

#include "clapper-app-info-window.h"
#include "clapper-app-property-row.h"
#include "clapper-app-list-item-utils.h"

#define GST_CAT_DEFAULT clapper_app_info_window_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppInfoWindow
{
  AdwWindow parent;

  GtkWidget *vstreams_list;
  GtkWidget *astreams_list;
  GtkWidget *sstreams_list;

  ClapperPlayer *player;
};

#define parent_class clapper_app_info_window_parent_class
G_DEFINE_TYPE (ClapperAppInfoWindow, clapper_app_info_window, ADW_TYPE_WINDOW);

enum
{
  PROP_0,
  PROP_PLAYER,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static gchar *
media_duration_closure (ClapperAppInfoWindow *self, gdouble duration)
{
  return g_strdup_printf ("%" CLAPPER_TIME_MS_FORMAT, CLAPPER_TIME_MS_ARGS (duration));
}

static gchar *
playback_element_name_closure (ClapperAppInfoWindow *self, GstElement *element)
{
  GstElementFactory *factory;

  if (!element || !(factory = gst_element_get_factory (element)))
    return NULL;

  return gst_object_get_name (GST_OBJECT_CAST (factory));
}

static gchar *
playback_decoder_closure (ClapperAppInfoWindow *self, GstElement *decoder)
{
  GstElementFactory *factory;
  gchar *el_name, *text;
  gboolean is_hardware;

  if (!decoder || !(factory = gst_element_get_factory (decoder)))
    return NULL;

  el_name = gst_object_get_name (GST_OBJECT_CAST (factory));
  is_hardware = gst_element_factory_list_is_type (factory,
      GST_ELEMENT_FACTORY_TYPE_HARDWARE);

  text = g_strdup_printf ("%s [%s]", el_name,
      (is_hardware) ? _("Hardware") : _("Software"));
  g_free (el_name);

  return text;
}

static GtkSelectionModel *
create_no_selection_closure (ClapperAppInfoWindow *self, ClapperStreamList *stream_list)
{
  return GTK_SELECTION_MODEL (gtk_no_selection_new (gst_object_ref (stream_list)));
}

static gboolean
has_streams_closure (ClapperAppInfoWindow *self, guint n_streams)
{
  return (n_streams > 0);
}

GtkWidget *
clapper_app_info_window_new (GtkApplication *gtk_app, ClapperPlayer *player)
{
  ClapperAppInfoWindow *window;

  window = g_object_new (CLAPPER_APP_TYPE_INFO_WINDOW,
      "application", gtk_app,
      "transient-for", gtk_application_get_active_window (gtk_app),
      "player", player,
      NULL);

  return GTK_WIDGET (window);
}

static void
clapper_app_info_window_init (ClapperAppInfoWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_remove_css_class (self->vstreams_list, "view");
  gtk_widget_remove_css_class (self->astreams_list, "view");
  gtk_widget_remove_css_class (self->sstreams_list, "view");
}

static void
clapper_app_info_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_INFO_WINDOW);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_info_window_finalize (GObject *object)
{
  ClapperAppInfoWindow *self = CLAPPER_APP_INFO_WINDOW_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&self->player);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_info_window_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAppInfoWindow *self = CLAPPER_APP_INFO_WINDOW_CAST (object);

  switch (prop_id) {
    case PROP_PLAYER:
      g_value_set_object (value, self->player);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_info_window_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperAppInfoWindow *self = CLAPPER_APP_INFO_WINDOW_CAST (object);

  switch (prop_id) {
    case PROP_PLAYER:
      self->player = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_info_window_class_init (ClapperAppInfoWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappinfowindow", 0,
      "Clapper App Info Window");

  gobject_class->get_property = clapper_app_info_window_get_property;
  gobject_class->set_property = clapper_app_info_window_set_property;
  gobject_class->dispose = clapper_app_info_window_dispose;
  gobject_class->finalize = clapper_app_info_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-info-window.ui");

  param_specs[PROP_PLAYER] = g_param_spec_object ("player",
      NULL, NULL, CLAPPER_TYPE_PLAYER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppInfoWindow, vstreams_list);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppInfoWindow, astreams_list);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppInfoWindow, sstreams_list);

  gtk_widget_class_bind_template_callback (widget_class, media_duration_closure);
  gtk_widget_class_bind_template_callback (widget_class, playback_element_name_closure);
  gtk_widget_class_bind_template_callback (widget_class, playback_decoder_closure);
  gtk_widget_class_bind_template_callback (widget_class, create_no_selection_closure);
  gtk_widget_class_bind_template_callback (widget_class, has_streams_closure);
}
