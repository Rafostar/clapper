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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * ClapperFeature:
 *
 * A base class for creating new features for the player.
 *
 * Feature objects are meant for adding additional functionalities that
 * are supposed to either act on playback/properties changes and/or change
 * them themselves due to some external signal/event.
 *
 * For reacting to playback changes subclass should override this class
 * virtual functions logic, while for controlling playback implementation
 * may call [method@Gst.Object.get_parent] to acquire a weak reference on
 * a parent [class@Clapper.Player] object feature was added to.
 *
 * Deprecated: 0.10: Use [iface@Clapper.Reactable] instead.
 */

#include "clapper-feature.h"
#include "clapper-feature-private.h"
#include "clapper-player-private.h"

#define GST_CAT_DEFAULT clapper_feature_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperFeaturePrivate ClapperFeaturePrivate;

struct _ClapperFeaturePrivate
{
  gboolean prepared;
};

#define parent_class clapper_feature_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (ClapperFeature, clapper_feature, GST_TYPE_OBJECT);

#define CALL_WITH_ARGS(_feature,_vfunc,...)                                      \
  ClapperFeaturePrivate *priv = clapper_feature_get_instance_private (_feature); \
  if (priv->prepared) {                                                          \
    ClapperFeatureClass *feature_class = CLAPPER_FEATURE_GET_CLASS (_feature);   \
    if (feature_class->_vfunc)                                                   \
      feature_class->_vfunc (_feature, __VA_ARGS__); }

#define CALL_WITHOUT_ARGS(_feature,_vfunc)                                       \
  ClapperFeaturePrivate *priv = clapper_feature_get_instance_private (_feature); \
  if (priv->prepared) {                                                          \
    ClapperFeatureClass *feature_class = CLAPPER_FEATURE_GET_CLASS (_feature);   \
    if (feature_class->_vfunc)                                                   \
      feature_class->_vfunc (_feature); }

void
clapper_feature_call_prepare (ClapperFeature *self)
{
  ClapperFeaturePrivate *priv = clapper_feature_get_instance_private (self);

  if (!priv->prepared) {
    ClapperFeatureClass *feature_class = CLAPPER_FEATURE_GET_CLASS (self);
    gboolean prepared = TRUE; // mark subclass without prepare method as prepared

    if (feature_class->prepare)
      prepared = feature_class->prepare (self);

    priv->prepared = prepared;
  }
}

void
clapper_feature_call_unprepare (ClapperFeature *self)
{
  ClapperFeaturePrivate *priv = clapper_feature_get_instance_private (self);

  if (priv->prepared) {
    ClapperFeatureClass *feature_class = CLAPPER_FEATURE_GET_CLASS (self);
    gboolean unprepared = TRUE; // mark subclass without unprepare method as unprepared

    if (feature_class->unprepare)
      unprepared = feature_class->unprepare (self);

    priv->prepared = !unprepared;
  }
}

void
clapper_feature_call_property_changed (ClapperFeature *self, GParamSpec *pspec)
{
  CALL_WITH_ARGS (self, property_changed, pspec);
}

void
clapper_feature_call_state_changed (ClapperFeature *self, ClapperPlayerState state)
{
  CALL_WITH_ARGS (self, state_changed, state);
}

void
clapper_feature_call_position_changed (ClapperFeature *self, gdouble position)
{
  CALL_WITH_ARGS (self, position_changed, position);
}

void
clapper_feature_call_speed_changed (ClapperFeature *self, gdouble speed)
{
  CALL_WITH_ARGS (self, speed_changed, speed);
}

void
clapper_feature_call_volume_changed (ClapperFeature *self, gdouble volume)
{
  CALL_WITH_ARGS (self, volume_changed, volume);
}

void
clapper_feature_call_mute_changed (ClapperFeature *self, gboolean mute)
{
  CALL_WITH_ARGS (self, mute_changed, mute);
}

void
clapper_feature_call_played_item_changed (ClapperFeature *self, ClapperMediaItem *item)
{
  CALL_WITH_ARGS (self, played_item_changed, item);
}

void
clapper_feature_call_item_updated (ClapperFeature *self, ClapperMediaItem *item)
{
  CALL_WITH_ARGS (self, item_updated, item);
}

void
clapper_feature_call_queue_item_added (ClapperFeature *self, ClapperMediaItem *item, guint index)
{
  CALL_WITH_ARGS (self, queue_item_added, item, index);
}

void
clapper_feature_call_queue_item_removed (ClapperFeature *self, ClapperMediaItem *item, guint index)
{
  CALL_WITH_ARGS (self, queue_item_removed, item, index);
}

void
clapper_feature_call_queue_item_repositioned (ClapperFeature *self, guint before, guint after)
{
  CALL_WITH_ARGS (self, queue_item_repositioned, before, after);
}

void
clapper_feature_call_queue_cleared (ClapperFeature *self)
{
  CALL_WITHOUT_ARGS (self, queue_cleared);
}

void
clapper_feature_call_queue_progression_changed (ClapperFeature *self, ClapperQueueProgressionMode mode)
{
  CALL_WITH_ARGS (self, queue_progression_changed, mode);
}

static void
clapper_feature_init (ClapperFeature *self)
{
}

static void
clapper_feature_dispatch_properties_changed (GObject *object,
    guint n_pspecs, GParamSpec **pspecs)
{
  ClapperPlayer *player;

  if ((player = CLAPPER_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (object))))) {
    ClapperFeaturesManager *features_manager;

    if ((features_manager = clapper_player_get_features_manager (player))) {
      guint i;

      for (i = 0; i < n_pspecs; ++i) {
        clapper_features_manager_trigger_property_changed (features_manager,
            CLAPPER_FEATURE_CAST (object), pspecs[i]);
      }
    }

    gst_object_unref (player);
  }

  G_OBJECT_CLASS (parent_class)->dispatch_properties_changed (object, n_pspecs, pspecs);
}

static void
clapper_feature_finalize (GObject *object)
{
  ClapperFeature *self = CLAPPER_FEATURE_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_feature_class_init (ClapperFeatureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperfeature", 0,
      "Clapper Feature");

  gobject_class->dispatch_properties_changed = clapper_feature_dispatch_properties_changed;
  gobject_class->finalize = clapper_feature_finalize;
}
