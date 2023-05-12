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

/**
 * ClapperStream:
 *
 * Represents a stream within media.
 */

#include "clapper-stream-private.h"
#include "clapper-player-private.h"

#define GST_CAT_DEFAULT clapper_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperStreamPrivate ClapperStreamPrivate;

struct _ClapperStreamPrivate
{
  GstStream *stream;
};

enum
{
  PROP_0,
  PROP_CAPS,
  PROP_LAST
};

#define parent_class clapper_stream_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (ClapperStream, clapper_stream, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

ClapperStream *
clapper_stream_new (GstStream *gst_stream)
{
  ClapperStream *stream;
  ClapperStreamPrivate *priv;

  stream = g_object_new (CLAPPER_TYPE_STREAM, NULL);

  priv = clapper_stream_get_instance_private (stream);
  priv->stream = gst_object_ref (gst_stream);

  return stream;
}

/**
 * clapper_stream_get_caps:
 * @stream: a #ClapperStream
 *
 * Get the caps of @stream, if any.
 *
 * Returns: (transfer full) (nullable): #GstCaps of stream.
 */
GstCaps *
clapper_stream_get_caps (ClapperStream *self)
{
  ClapperStreamPrivate *priv;

  g_return_val_if_fail (CLAPPER_IS_STREAM (self), NULL);

  priv = clapper_stream_get_instance_private (self);

  if (!priv->stream)
    return NULL;

  return gst_stream_get_caps (priv->stream);
}

GstStream *
clapper_stream_get_stream (ClapperStream *self)
{
  ClapperStreamPrivate *priv = clapper_stream_get_instance_private (self);

  return priv->stream;
}

static void
clapper_stream_init (ClapperStream *self)
{
}

static void
clapper_stream_finalize (GObject *object)
{
  ClapperStream *self = CLAPPER_STREAM_CAST (object);
  ClapperStreamPrivate *priv = clapper_stream_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&priv->stream);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperStream *self = CLAPPER_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_CAPS:
      g_value_take_boxed (value, clapper_stream_get_caps (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_stream_class_init (ClapperStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperstream", 0,
      "Clapper Stream");

  gobject_class->get_property = clapper_stream_get_property;
  gobject_class->finalize = clapper_stream_finalize;

  /**
   * ClapperStream:caps:
   *
   * The #GstCaps of stream.
   */
  param_specs[PROP_CAPS] = g_param_spec_boxed ("caps",
      NULL, NULL, GST_TYPE_CAPS,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
