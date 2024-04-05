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

#include "clapper-app-media-item-box.h"

struct _ClapperAppMediaItemBox
{
  GtkBox parent;

  ClapperMediaItem *media_item;
};

enum
{
  PROP_0,
  PROP_MEDIA_ITEM,
  PROP_LAST
};

#define parent_class clapper_app_media_item_box_parent_class
G_DEFINE_TYPE (ClapperAppMediaItemBox, clapper_app_media_item_box, GTK_TYPE_BOX);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

ClapperMediaItem *
clapper_app_media_item_box_get_media_item (ClapperAppMediaItemBox *self)
{
  return self->media_item;
}

static void
clapper_app_media_item_box_init (ClapperAppMediaItemBox *self)
{
}

static void
clapper_app_media_item_box_finalize (GObject *object)
{
  ClapperAppMediaItemBox *self = CLAPPER_APP_MEDIA_ITEM_BOX_CAST (object);

  gst_clear_object (&self->media_item);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_media_item_box_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAppMediaItemBox *self = CLAPPER_APP_MEDIA_ITEM_BOX_CAST (object);

  switch (prop_id) {
    case PROP_MEDIA_ITEM:
      g_value_set_object (value, clapper_app_media_item_box_get_media_item (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_media_item_box_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperAppMediaItemBox *self = CLAPPER_APP_MEDIA_ITEM_BOX_CAST (object);

  switch (prop_id) {
    case PROP_MEDIA_ITEM:
      gst_object_replace ((GstObject **) &self->media_item, GST_OBJECT_CAST (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_media_item_box_class_init (ClapperAppMediaItemBoxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = clapper_app_media_item_box_get_property;
  gobject_class->set_property = clapper_app_media_item_box_set_property;
  gobject_class->finalize = clapper_app_media_item_box_finalize;

  param_specs[PROP_MEDIA_ITEM] = g_param_spec_object ("media-item",
      NULL, NULL, CLAPPER_TYPE_MEDIA_ITEM,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
