/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "clapper-features-manager-private.h"
#include "clapper-features-bus-private.h"
#include "clapper-feature-private.h"

#define GST_CAT_DEFAULT clapper_features_manager_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperFeaturesManager
{
  ClapperThreadedObject parent;

  GPtrArray *features;
  ClapperFeaturesBus *bus;
};

#define parent_class clapper_features_manager_parent_class
G_DEFINE_TYPE (ClapperFeaturesManager, clapper_features_manager, CLAPPER_TYPE_THREADED_OBJECT);

static inline void
_post_object (ClapperFeaturesManager *self, ClapperFeaturesManagerEvent event, GObject *data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_OBJECT);
  g_value_set_object (&value, data);

  clapper_features_bus_post_event (self->bus, self, event, &value);
}

static inline void
_post_structure (ClapperFeaturesManager *self, ClapperFeaturesManagerEvent event, GstStructure *data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, GST_TYPE_STRUCTURE);
  g_value_set_object (&value, data);

  clapper_features_bus_post_event (self->bus, self, event, &value);
}

static inline void
_post_int (ClapperFeaturesManager *self, ClapperFeaturesManagerEvent event, gint data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, data);

  clapper_features_bus_post_event (self->bus, self, event, &value);
}

static inline void
_post_float (ClapperFeaturesManager *self, ClapperFeaturesManagerEvent event, gfloat data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_FLOAT);
  g_value_set_float (&value, data);

  clapper_features_bus_post_event (self->bus, self, event, &value);
}

static inline void
_post_boolean (ClapperFeaturesManager *self, ClapperFeaturesManagerEvent event, gboolean data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, data);

  clapper_features_bus_post_event (self->bus, self, event, &value);
}

/*
 * clapper_features_manager_new:
 *
 * Returns: (transfer full): a new #ClapperFeaturesManager instance.
 */
ClapperFeaturesManager *
clapper_features_manager_new (void)
{
  ClapperFeaturesManager *features_manager;

  features_manager = g_object_new (CLAPPER_TYPE_FEATURES_MANAGER, NULL);
  gst_object_ref_sink (features_manager);

  return features_manager;
}

void
clapper_features_manager_add_feature (ClapperFeaturesManager *self, ClapperFeature *feature, GstObject *parent)
{
  guint index = -1;
  gboolean added;

  GST_OBJECT_LOCK (self);

  if ((added = !g_ptr_array_find (self->features, feature, &index)))
    g_ptr_array_add (self->features, gst_object_ref (feature));

  GST_OBJECT_UNLOCK (self);

  if (added) {
    GValue value = G_VALUE_INIT;

    gst_object_set_parent (GST_OBJECT_CAST (feature), parent);

    g_value_init (&value, G_TYPE_OBJECT);
    g_value_set_object (&value, G_OBJECT (feature));

    clapper_features_bus_post_event (self->bus, self,
        CLAPPER_FEATURES_MANAGER_EVENT_FEATURE_ADDED, &value);
  }
}

void
clapper_features_manager_trigger_state_changed (ClapperFeaturesManager *self, ClapperPlayerState state)
{
  _post_int (self, CLAPPER_FEATURES_MANAGER_EVENT_STATE_CHANGED, state);
}

void
clapper_features_manager_trigger_position_changed (ClapperFeaturesManager *self, gfloat position)
{
  _post_float (self, CLAPPER_FEATURES_MANAGER_EVENT_POSITION_CHANGED, position);
}

void
clapper_features_manager_trigger_speed_changed (ClapperFeaturesManager *self, gfloat speed)
{
  _post_float (self, CLAPPER_FEATURES_MANAGER_EVENT_SPEED_CHANGED, speed);
}

void
clapper_features_manager_trigger_volume_changed (ClapperFeaturesManager *self, gfloat volume)
{
  _post_float (self, CLAPPER_FEATURES_MANAGER_EVENT_VOLUME_CHANGED, volume);
}

void
clapper_features_manager_trigger_mute_changed (ClapperFeaturesManager *self, gboolean mute)
{
  _post_boolean (self, CLAPPER_FEATURES_MANAGER_EVENT_MUTE_CHANGED, mute);
}

void
clapper_features_manager_trigger_current_media_item_changed (ClapperFeaturesManager *self, ClapperMediaItem *item)
{
  _post_object (self, CLAPPER_FEATURES_MANAGER_EVENT_CURRENT_MEDIA_ITEM_CHANGED, (GObject *) item);
}

void
clapper_features_manager_trigger_media_item_updated (ClapperFeaturesManager *self, ClapperMediaItem *item)
{
  _post_object (self, CLAPPER_FEATURES_MANAGER_EVENT_MEDIA_ITEM_UPDATED, (GObject *) item);
}

void
clapper_features_manager_trigger_queue_item_added (ClapperFeaturesManager *self, ClapperMediaItem *item)
{
  _post_object (self, CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_ADDED, (GObject *) item);
}

void
clapper_features_manager_trigger_queue_item_removed (ClapperFeaturesManager *self, ClapperMediaItem *item)
{
  _post_object (self, CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REMOVED, (GObject *) item);
}

void
clapper_features_manager_trigger_queue_cleared (ClapperFeaturesManager *self)
{
  clapper_features_bus_post_event (self->bus, self, CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_CLEARED, NULL);
}

void
clapper_features_manager_trigger_queue_progression_changed (ClapperFeaturesManager *self, ClapperQueueProgressionMode mode)
{
  _post_int (self, CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED, mode);
}

void
clapper_features_manager_handle_event (ClapperFeaturesManager *self, ClapperFeaturesManagerEvent event,
    const GValue *value)
{
  guint i;

  GST_OBJECT_LOCK (self);

  for (i = 0; i < self->features->len; ++i) {
    ClapperFeature *feature = g_ptr_array_index (self->features, i);

    switch (event) {
      case CLAPPER_FEATURES_MANAGER_EVENT_FEATURE_ADDED:{
        ClapperFeature *added_feature = g_value_get_object (value);

        if (feature == added_feature)
          clapper_feature_call_prepare (feature);
        break;
      }
      case CLAPPER_FEATURES_MANAGER_EVENT_STATE_CHANGED:
        clapper_feature_call_state_changed (feature, g_value_get_int (value));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_POSITION_CHANGED:
        clapper_feature_call_position_changed (feature, g_value_get_float (value));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_SPEED_CHANGED:
        clapper_feature_call_speed_changed (feature, g_value_get_float (value));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_VOLUME_CHANGED:
        clapper_feature_call_volume_changed (feature, g_value_get_float (value));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_MUTE_CHANGED:
        clapper_feature_call_mute_changed (feature, g_value_get_boolean (value));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_CURRENT_MEDIA_ITEM_CHANGED:
        clapper_feature_call_current_media_item_changed (feature,
            CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_MEDIA_ITEM_UPDATED:
        clapper_feature_call_media_item_updated (feature,
            CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_ADDED:
        clapper_feature_call_queue_item_added (feature,
            CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REMOVED:
        clapper_feature_call_queue_item_removed (feature,
            CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)));
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_CLEARED:
        clapper_feature_call_queue_cleared (feature);
        break;
      case CLAPPER_FEATURES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED:
        clapper_feature_call_queue_progression_changed (feature, g_value_get_int (value));
        break;
      default:
        GST_WARNING_OBJECT (self, "Unhandled event ID: %i", event);
        break;
    }
  }

  GST_OBJECT_UNLOCK (self);
}

static void
clapper_features_manager_thread_start (ClapperThreadedObject *threaded_object)
{
  ClapperFeaturesManager *self = CLAPPER_FEATURES_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Features manager thread start");

  self->bus = clapper_features_bus_new ();
}

static void
clapper_features_manager_thread_stop (ClapperThreadedObject *threaded_object)
{
  ClapperFeaturesManager *self = CLAPPER_FEATURES_MANAGER_CAST (threaded_object);
  guint i;

  GST_TRACE_OBJECT (threaded_object, "Features manager thread stop");

  gst_bus_set_flushing (GST_BUS_CAST (self->bus), TRUE);
  gst_bus_remove_watch (GST_BUS_CAST (self->bus));
  gst_clear_object (&self->bus);

  GST_OBJECT_LOCK (self);

  for (i = 0; i < self->features->len; ++i) {
    ClapperFeature *feature = g_ptr_array_index (self->features, i);

    clapper_feature_call_unprepare (feature);
    gst_object_unparent (GST_OBJECT (feature));
  }

  GST_OBJECT_UNLOCK (self);
}

static void
clapper_features_manager_init (ClapperFeaturesManager *self)
{
  self->features = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_object_unref);
}

static void
clapper_features_manager_finalize (GObject *object)
{
  ClapperFeaturesManager *self = CLAPPER_FEATURES_MANAGER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_ptr_array_unref (self->features);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_features_manager_class_init (ClapperFeaturesManagerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperfeaturesmanager", 0,
      "Clapper Features Manager");

  gobject_class->finalize = clapper_features_manager_finalize;

  threaded_object->thread_start = clapper_features_manager_thread_start;
  threaded_object->thread_stop = clapper_features_manager_thread_stop;
}
