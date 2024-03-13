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

#include <glib.h>

#include "clapper-app-queue-progression-item.h"

struct _ClapperAppQueueProgressionItem
{
  GObject parent;

  gchar *icon_name;
  gchar *label;
};

enum
{
  PROP_0,
  PROP_ICON_NAME,
  PROP_LABEL,
  PROP_LAST
};

#define parent_class clapper_app_queue_progression_item_parent_class
G_DEFINE_TYPE (ClapperAppQueueProgressionItem, clapper_app_queue_progression_item, G_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

ClapperAppQueueProgressionItem *
clapper_app_queue_progression_item_new (const gchar *icon_name, const gchar *label)
{
  ClapperAppQueueProgressionItem *item;

  item = g_object_new (CLAPPER_APP_TYPE_QUEUE_PROGRESSION_ITEM, NULL);
  item->icon_name = g_strdup (icon_name);
  item->label = g_strdup (label);

  return item;
}

static void
clapper_app_queue_progression_item_init (ClapperAppQueueProgressionItem *self)
{
}

static void
clapper_app_queue_progression_item_finalize (GObject *object)
{
  ClapperAppQueueProgressionItem *self = CLAPPER_APP_QUEUE_PROGRESSION_ITEM_CAST (object);

  g_free (self->icon_name);
  g_free (self->label);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_queue_progression_item_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAppQueueProgressionItem *self = CLAPPER_APP_QUEUE_PROGRESSION_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;
    case PROP_LABEL:
      g_value_set_string (value, self->label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_queue_progression_item_class_init (ClapperAppQueueProgressionItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = clapper_app_queue_progression_item_get_property;
  gobject_class->finalize = clapper_app_queue_progression_item_finalize;

  param_specs[PROP_ICON_NAME] = g_param_spec_string ("icon-name",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_LABEL] = g_param_spec_string ("label",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
