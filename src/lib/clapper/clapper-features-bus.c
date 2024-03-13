/* Clapper Playback Library
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

#include "clapper-bus-private.h"
#include "clapper-features-manager-private.h"
#include "clapper-features-bus-private.h"

#define GST_CAT_DEFAULT clapper_features_bus_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperFeaturesBus
{
  GstBus parent;
};

#define parent_class clapper_features_bus_parent_class
G_DEFINE_TYPE (ClapperFeaturesBus, clapper_features_bus, GST_TYPE_BUS);

enum
{
  CLAPPER_FEATURES_BUS_STRUCTURE_UNKNOWN = 0,
  CLAPPER_FEATURES_BUS_STRUCTURE_EVENT
};

static ClapperBusQuark _structure_quarks[] = {
  {"unknown", 0},
  {"event", 0},
  {NULL, 0}
};

enum
{
  CLAPPER_FEATURES_BUS_FIELD_UNKNOWN = 0,
  CLAPPER_FEATURES_BUS_FIELD_EVENT,
  CLAPPER_FEATURES_BUS_FIELD_VALUE,
  CLAPPER_FEATURES_BUS_FIELD_EXTRA_VALUE
};

static ClapperBusQuark _field_quarks[] = {
  {"unknown", 0},
  {"event", 0},
  {"value", 0},
  {"extra-value", 0},
  {NULL, 0}
};

#define _STRUCTURE_QUARK(q) (_structure_quarks[CLAPPER_FEATURES_BUS_STRUCTURE_##q].quark)
#define _FIELD_QUARK(q) (_field_quarks[CLAPPER_FEATURES_BUS_FIELD_##q].quark)
#define _MESSAGE_SRC_CLAPPER_FEATURES_MANAGER(msg) ((ClapperFeaturesManager *) GST_MESSAGE_SRC (msg))

void
clapper_features_bus_initialize (void)
{
  gint i;

  for (i = 0; _structure_quarks[i].name; ++i)
    _structure_quarks[i].quark = g_quark_from_static_string (_structure_quarks[i].name);
  for (i = 0; _field_quarks[i].name; ++i)
    _field_quarks[i].quark = g_quark_from_static_string (_field_quarks[i].name);
}

void
clapper_features_bus_post_event (ClapperFeaturesBus *self,
    ClapperFeaturesManager *src, ClapperFeaturesManagerEvent event,
    GValue *value, GValue *extra_value)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (EVENT),
      _FIELD_QUARK (EVENT), G_TYPE_ENUM, event,
      NULL);

  if (value)
    gst_structure_id_take_value (structure, _FIELD_QUARK (VALUE), value);
  if (extra_value)
    gst_structure_id_take_value (structure, _FIELD_QUARK (EXTRA_VALUE), extra_value);

  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (
      GST_OBJECT_CAST (src), structure));
}

static inline void
_handle_event_msg (GstMessage *msg, const GstStructure *structure,
    ClapperFeaturesManager *features_manager)
{
  ClapperFeaturesManagerEvent event = CLAPPER_FEATURES_MANAGER_EVENT_UNKNOWN;
  const GValue *value = gst_structure_id_get_value (structure, _FIELD_QUARK (VALUE));
  const GValue *extra_value = gst_structure_id_get_value (structure, _FIELD_QUARK (EXTRA_VALUE));

  gst_structure_id_get (structure,
      _FIELD_QUARK (EVENT), G_TYPE_ENUM, &event,
      NULL);

  clapper_features_manager_handle_event (features_manager, event, value, extra_value);
}

static gboolean
clapper_features_bus_message_func (GstBus *bus, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
  if (G_LIKELY (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_APPLICATION)) {
    ClapperFeaturesManager *features_manager = _MESSAGE_SRC_CLAPPER_FEATURES_MANAGER (msg);
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark quark = gst_structure_get_name_id (structure);

    if (quark == _STRUCTURE_QUARK (EVENT))
      _handle_event_msg (msg, structure, features_manager);
  }

  return G_SOURCE_CONTINUE;
}

/*
 * clapper_features_bus_new:
 *
 * Returns: (transfer full): a new #ClapperFeaturesBus instance.
 */
ClapperFeaturesBus *
clapper_features_bus_new (void)
{
  GstBus *features_bus;

  features_bus = GST_BUS_CAST (g_object_new (CLAPPER_TYPE_FEATURES_BUS, NULL));
  gst_object_ref_sink (features_bus);

  gst_bus_add_watch (features_bus, (GstBusFunc) clapper_features_bus_message_func, NULL);

  return CLAPPER_FEATURES_BUS_CAST (features_bus);
}

static void
clapper_features_bus_init (ClapperFeaturesBus *self)
{
}

static void
clapper_features_bus_finalize (GObject *object)
{
  ClapperFeaturesBus *self = CLAPPER_FEATURES_BUS_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_features_bus_class_init (ClapperFeaturesBusClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperfeaturesbus", 0,
      "Clapper Features Bus");

  gobject_class->finalize = clapper_features_bus_finalize;
}
