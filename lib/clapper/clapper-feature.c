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

/**
 * SECTION:clapper-feature
 * @title: ClapperFeature
 * @short_description: base class for creating new features
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
clapper_feature_call_state_changed (ClapperFeature *self, ClapperPlayerState state)
{
  CALL_WITH_ARGS (self, state_changed, state);
}

void
clapper_feature_call_position_changed (ClapperFeature *self, gfloat position)
{
  CALL_WITH_ARGS (self, position_changed, position);
}

void
clapper_feature_call_speed_changed (ClapperFeature *self, gfloat speed)
{
  CALL_WITH_ARGS (self, speed_changed, speed);
}

void
clapper_feature_call_volume_changed (ClapperFeature *self, gfloat volume)
{
  CALL_WITH_ARGS (self, volume_changed, volume);
}

void
clapper_feature_call_mute_changed (ClapperFeature *self, gboolean mute)
{
  CALL_WITH_ARGS (self, mute_changed, mute);
}

void
clapper_feature_call_current_media_item_changed (ClapperFeature *self, ClapperMediaItem *current_item)
{
  CALL_WITH_ARGS (self, current_media_item_changed, current_item);
}

void
clapper_feature_call_media_item_updated (ClapperFeature *self, ClapperMediaItem *item)
{
  CALL_WITH_ARGS (self, media_item_updated, item);
}

void
clapper_feature_call_queue_item_added (ClapperFeature *self, ClapperMediaItem *item, guint index)
{
  CALL_WITH_ARGS (self, queue_item_added, item, index);
}

void
clapper_feature_call_queue_item_removed (ClapperFeature *self, ClapperMediaItem *item)
{
  CALL_WITH_ARGS (self, queue_item_removed, item);
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

  gobject_class->finalize = clapper_feature_finalize;
}
