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

#include <gio/gio.h>

#include "clapper-bus-private.h"
#include "clapper-app-bus-private.h"
#include "clapper-player-private.h"
#include "clapper-media-item-private.h"
#include "clapper-timeline-private.h"

#define GST_CAT_DEFAULT clapper_app_bus_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppBus
{
  GstBus parent;
};

#define parent_class clapper_app_bus_parent_class
G_DEFINE_TYPE (ClapperAppBus, clapper_app_bus, GST_TYPE_BUS);

enum
{
  CLAPPER_APP_BUS_STRUCTURE_UNKNOWN = 0,
  CLAPPER_APP_BUS_STRUCTURE_PROP_NOTIFY,
  CLAPPER_APP_BUS_STRUCTURE_REFRESH_STREAMS,
  CLAPPER_APP_BUS_STRUCTURE_REFRESH_TIMELINE,
  CLAPPER_APP_BUS_STRUCTURE_SIMPLE_SIGNAL,
  CLAPPER_APP_BUS_STRUCTURE_OBJECT_DESC_SIGNAL,
  CLAPPER_APP_BUS_STRUCTURE_DESC_WITH_DETAILS_SIGNAL,
  CLAPPER_APP_BUS_STRUCTURE_ERROR_SIGNAL
};

static ClapperBusQuark _structure_quarks[] = {
  {"unknown", 0},
  {"prop-notify", 0},
  {"refresh-streams", 0},
  {"refresh-timeline", 0},
  {"simple-signal", 0},
  {"object-desc-signal", 0},
  {"desc-with-details-signal", 0},
  {"error-signal", 0},
  {NULL, 0}
};

enum
{
  CLAPPER_APP_BUS_FIELD_UNKNOWN = 0,
  CLAPPER_APP_BUS_FIELD_PSPEC,
  CLAPPER_APP_BUS_FIELD_SIGNAL_ID,
  CLAPPER_APP_BUS_FIELD_OBJECT,
  CLAPPER_APP_BUS_FIELD_DESC,
  CLAPPER_APP_BUS_FIELD_DETAILS,
  CLAPPER_APP_BUS_FIELD_ERROR,
  CLAPPER_APP_BUS_FIELD_DEBUG_INFO
};

static ClapperBusQuark _field_quarks[] = {
  {"unknown", 0},
  {"pspec", 0},
  {"signal-id", 0},
  {"object", 0},
  {"desc", 0},
  {"details", 0},
  {"error", 0},
  {"debug-info", 0},
  {NULL, 0}
};

#define _STRUCTURE_QUARK(q) (_structure_quarks[CLAPPER_APP_BUS_STRUCTURE_##q].quark)
#define _FIELD_QUARK(q) (_field_quarks[CLAPPER_APP_BUS_FIELD_##q].quark)
#define _MESSAGE_SRC_GOBJECT(msg) ((GObject *) GST_MESSAGE_SRC (msg))

void
clapper_app_bus_initialize (void)
{
  gint i;

  for (i = 0; _structure_quarks[i].name; ++i)
    _structure_quarks[i].quark = g_quark_from_static_string (_structure_quarks[i].name);
  for (i = 0; _field_quarks[i].name; ++i)
    _field_quarks[i].quark = g_quark_from_static_string (_field_quarks[i].name);
}

void
clapper_app_bus_forward_message (ClapperAppBus *self, GstMessage *msg)
{
  gst_bus_post (GST_BUS_CAST (self), gst_message_ref (msg));
}

/* FIXME: It should be faster to wait for gst_message_new_property_notify() from
 * playbin bus and forward them to app bus instead of connecting to notify
 * signals of playbin, so change into using gst_message_new_property_notify() here too */
void
clapper_app_bus_post_prop_notify (ClapperAppBus *self,
    GstObject *src, GParamSpec *pspec)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (PROP_NOTIFY),
      _FIELD_QUARK (PSPEC), G_TYPE_PARAM, pspec,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_prop_notify_msg (GstMessage *msg, const GstStructure *structure)
{
  GParamSpec *pspec = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (PSPEC), G_TYPE_PARAM, &pspec,
      NULL);
  g_object_notify_by_pspec (_MESSAGE_SRC_GOBJECT (msg), pspec);

  g_param_spec_unref (pspec);
}

void
clapper_app_bus_post_refresh_streams (ClapperAppBus *self, GstObject *src)
{
  GstStructure *structure = gst_structure_new_id_empty (_STRUCTURE_QUARK (REFRESH_STREAMS));
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_refresh_streams_msg (GstMessage *msg, const GstStructure *structure)
{
  ClapperPlayer *player = CLAPPER_PLAYER_CAST (GST_MESSAGE_SRC (msg));
  clapper_player_refresh_streams (player);
}

void
clapper_app_bus_post_refresh_timeline (ClapperAppBus *self, GstObject *src)
{
  GstStructure *structure = gst_structure_new_id_empty (_STRUCTURE_QUARK (REFRESH_TIMELINE));
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_refresh_timeline_msg (GstMessage *msg, const GstStructure *structure)
{
  ClapperMediaItem *item = CLAPPER_MEDIA_ITEM_CAST (GST_MESSAGE_SRC (msg));
  ClapperTimeline *timeline = clapper_media_item_get_timeline (item);

  clapper_timeline_refresh (timeline);
}

void
clapper_app_bus_post_simple_signal (ClapperAppBus *self, GstObject *src, guint signal_id)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (SIMPLE_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_simple_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0);
}

void
clapper_app_bus_post_object_desc_signal (ClapperAppBus *self,
    GstObject *src, guint signal_id,
    GstObject *object, const gchar *desc)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (OBJECT_DESC_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      _FIELD_QUARK (OBJECT), GST_TYPE_OBJECT, object,
      _FIELD_QUARK (DESC), G_TYPE_STRING, desc,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_object_desc_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  GstObject *object = NULL;
  gchar *desc = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (OBJECT), GST_TYPE_OBJECT, &object,
      _FIELD_QUARK (DESC), G_TYPE_STRING, &desc,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0, object, desc);

  gst_object_unref (object);
  g_free (desc);
}

void
clapper_app_bus_post_desc_with_details_signal (ClapperAppBus *self,
    GstObject *src, guint signal_id,
    const gchar *desc, const gchar *details)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (DESC_WITH_DETAILS_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      _FIELD_QUARK (DESC), G_TYPE_STRING, desc,
      _FIELD_QUARK (DETAILS), G_TYPE_STRING, details,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_desc_with_details_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  gchar *desc = NULL, *details = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (DESC), G_TYPE_STRING, &desc,
      _FIELD_QUARK (DETAILS), G_TYPE_STRING, &details,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0, desc, details);

  g_free (desc);
  g_free (details);
}

void
clapper_app_bus_post_error_signal (ClapperAppBus *self,
    GstObject *src, guint signal_id,
    GError *error, const gchar *debug_info)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (ERROR_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      _FIELD_QUARK (ERROR), G_TYPE_ERROR, error,
      _FIELD_QUARK (DEBUG_INFO), G_TYPE_STRING, debug_info,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_error_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  GError *error = NULL;
  gchar *debug_info = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (ERROR), G_TYPE_ERROR, &error,
      _FIELD_QUARK (DEBUG_INFO), G_TYPE_STRING, &debug_info,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0, error, debug_info);

  g_clear_error (&error);
  g_free (debug_info);
}

static gboolean
clapper_app_bus_message_func (GstBus *bus, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
  if (G_LIKELY (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_APPLICATION)) {
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark quark = gst_structure_get_name_id (structure);

    if (quark == _STRUCTURE_QUARK (PROP_NOTIFY))
      _handle_prop_notify_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (REFRESH_STREAMS))
      _handle_refresh_streams_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (REFRESH_TIMELINE))
      _handle_refresh_timeline_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (SIMPLE_SIGNAL))
      _handle_simple_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (OBJECT_DESC_SIGNAL))
      _handle_object_desc_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (ERROR_SIGNAL))
      _handle_error_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (DESC_WITH_DETAILS_SIGNAL))
      _handle_desc_with_details_signal_msg (msg, structure);
  }

  return G_SOURCE_CONTINUE;
}

/*
 * clapper_app_bus_new:
 *
 * Returns: (transfer full): a new #ClapperAppBus instance
 */
ClapperAppBus *
clapper_app_bus_new (void)
{
  GstBus *app_bus;

  app_bus = GST_BUS_CAST (g_object_new (CLAPPER_TYPE_APP_BUS, NULL));
  gst_object_ref_sink (app_bus);

  gst_bus_add_watch (app_bus, (GstBusFunc) clapper_app_bus_message_func, NULL);

  return CLAPPER_APP_BUS_CAST (app_bus);
}

static void
clapper_app_bus_init (ClapperAppBus *self)
{
}

static void
clapper_app_bus_finalize (GObject *object)
{
  ClapperAppBus *self = CLAPPER_APP_BUS_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_bus_class_init (ClapperAppBusClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappbus", 0,
      "Clapper App Bus");

  gobject_class->finalize = clapper_app_bus_finalize;
}
