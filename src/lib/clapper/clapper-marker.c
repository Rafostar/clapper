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

/**
 * ClapperMarker:
 *
 * Represents a point in timeline.
 *
 * Markers are a convienient way of marking points of interest within a
 * [class@Clapper.Timeline] of [class@Clapper.MediaItem]. Use them
 * to indicate certain areas on the timeline.
 *
 * Markers are reference counted immutable objects. Once a marker is created
 * it can only be inserted into a single [class@Clapper.Timeline] at a time.
 *
 * Please note that markers are independent of [property@Clapper.MediaItem:duration]
 * and applications should not assume that all markers must have start/end times
 * lower or equal the item duration. This is not the case in e.g. live streams
 * where duration is unknown, but markers are still allowed to mark entries
 * (like EPG titles for example).
 *
 * Remember that [class@Clapper.Player] will also automatically insert certain
 * markers extracted from media such as video chapters. Clapper will never
 * "touch" the ones created by the application. If you want to differentiate
 * your own markers, applications can define and create markers with one of
 * the custom types from [enum@Clapper.MarkerType] enum.
 *
 * Example:
 *
 * ```c
 * #define MY_APP_MARKER (CLAPPER_MARKER_TYPE_CUSTOM_1)
 *
 * ClapperMarker *marker = clapper_marker_new (MY_APP_MARKER, title, start, end);
 * ```
 *
 * ```c
 * ClapperMarkerType marker_type = clapper_marker_get_marker_type (marker);
 *
 * if (marker_type == MY_APP_MARKER) {
 *   // Do something with your custom marker
 * }
 * ```
 */

#include "clapper-marker-private.h"

#define GST_CAT_DEFAULT clapper_marker_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperMarker
{
  GstObject parent;

  ClapperMarkerType marker_type;
  gchar *title;
  gdouble start;
  gdouble end;

  gboolean is_internal;
};

enum
{
  PROP_0,
  PROP_MARKER_TYPE,
  PROP_TITLE,
  PROP_START,
  PROP_END,
  PROP_LAST
};

#define parent_class clapper_marker_parent_class
G_DEFINE_TYPE (ClapperMarker, clapper_marker, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * clapper_marker_new:
 * @marker_type: a #ClapperMarkerType
 * @title: (nullable): title of the marker
 * @start: a start position of the marker
 * @end: an end position of the marker or [const@Clapper.MARKER_NO_END] if none
 *
 * Creates a new #ClapperMarker with given params.
 *
 * It is considered a programmer error trying to set an ending
 * point that is before the starting one. If end is unknown or
 * not defined a special [const@Clapper.MARKER_NO_END] value
 * should be used.
 *
 * Returns: (transfer full): a new #ClapperMarker.
 */
ClapperMarker *
clapper_marker_new (ClapperMarkerType marker_type, const gchar *title,
    gdouble start, gdouble end)
{
  ClapperMarker *marker;

  marker = g_object_new (CLAPPER_TYPE_MARKER,
      "marker-type", marker_type,
      "title", title,
      "start", start,
      "end", end, NULL);
  gst_object_ref_sink (marker);

  return marker;
}

ClapperMarker *
clapper_marker_new_internal (ClapperMarkerType marker_type, const gchar *title,
    gdouble start, gdouble end)
{
  ClapperMarker *marker;

  marker = clapper_marker_new (marker_type, title, start, end);
  marker->is_internal = TRUE;

  return marker;
}

/**
 * clapper_marker_get_marker_type:
 * @marker: a #ClapperMarker
 *
 * Get the #ClapperMarkerType of @marker.
 *
 * Returns: type of marker.
 */
ClapperMarkerType
clapper_marker_get_marker_type (ClapperMarker *self)
{
  g_return_val_if_fail (CLAPPER_IS_MARKER (self), CLAPPER_MARKER_TYPE_UNKNOWN);

  return self->marker_type;
}

/**
 * clapper_marker_get_title:
 * @marker: a #ClapperMarker
 *
 * Get the title of @marker.
 *
 * Returns: (nullable): the marker title.
 */
const gchar *
clapper_marker_get_title (ClapperMarker *self)
{
  g_return_val_if_fail (CLAPPER_IS_MARKER (self), NULL);

  return self->title;
}

/**
 * clapper_marker_get_start:
 * @marker: a #ClapperMarker
 *
 * Get the start position (in seconds) of @marker.
 *
 * Returns: marker start.
 */
gdouble
clapper_marker_get_start (ClapperMarker *self)
{
  g_return_val_if_fail (CLAPPER_IS_MARKER (self), 0);

  return self->start;
}

/**
 * clapper_marker_get_end:
 * @marker: a #ClapperMarker
 *
 * Get the end position (in seconds) of @marker.
 *
 * Returns: marker end.
 */
gdouble
clapper_marker_get_end (ClapperMarker *self)
{
  g_return_val_if_fail (CLAPPER_IS_MARKER (self), CLAPPER_MARKER_NO_END);

  return self->end;
}

gboolean
clapper_marker_is_internal (ClapperMarker *self)
{
  return self->is_internal;
}

static void
clapper_marker_init (ClapperMarker *self)
{
  self->marker_type = CLAPPER_MARKER_TYPE_UNKNOWN;
  self->end = CLAPPER_MARKER_NO_END;
}

static void
clapper_marker_constructed (GObject *object)
{
  ClapperMarker *self = CLAPPER_MARKER_CAST (object);

  G_OBJECT_CLASS (parent_class)->constructed (object);

  GST_TRACE_OBJECT (self, "Created new marker"
      ", type: %i, title: \"%s\", start: %lf, end: %lf",
      self->marker_type, GST_STR_NULL (self->title), self->start, self->end);
}

static void
clapper_marker_finalize (GObject *object)
{
  ClapperMarker *self = CLAPPER_MARKER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_marker_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperMarker *self = CLAPPER_MARKER_CAST (object);

  switch (prop_id) {
    case PROP_MARKER_TYPE:
      g_value_set_enum (value, clapper_marker_get_marker_type (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, clapper_marker_get_title (self));
      break;
    case PROP_START:
      g_value_set_double (value, clapper_marker_get_start (self));
      break;
    case PROP_END:
      g_value_set_double (value, clapper_marker_get_end (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_marker_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperMarker *self = CLAPPER_MARKER_CAST (object);

  switch (prop_id) {
    case PROP_MARKER_TYPE:
      self->marker_type = g_value_get_enum (value);
      break;
    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;
    case PROP_START:
      self->start = g_value_get_double (value);
      break;
    case PROP_END:
      self->end = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_marker_class_init (ClapperMarkerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappermarker", 0,
      "Clapper Marker");

  gobject_class->constructed = clapper_marker_constructed;
  gobject_class->get_property = clapper_marker_get_property;
  gobject_class->set_property = clapper_marker_set_property;
  gobject_class->finalize = clapper_marker_finalize;

  /**
   * ClapperMarker:marker-type:
   *
   * Type of stream.
   */
  param_specs[PROP_MARKER_TYPE] = g_param_spec_enum ("marker-type",
      NULL, NULL, CLAPPER_TYPE_MARKER_TYPE, CLAPPER_MARKER_TYPE_UNKNOWN,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMarker:title:
   *
   * Title of marker.
   */
  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMarker:start:
   *
   * Starting time of marker.
   */
  param_specs[PROP_START] = g_param_spec_double ("start",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMarker:end:
   *
   * Ending time of marker.
   */
  param_specs[PROP_END] = g_param_spec_double ("end",
      NULL, NULL, -1, G_MAXDOUBLE, CLAPPER_MARKER_NO_END,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
