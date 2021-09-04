/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstclapper-mpris-gdbus.h"
#include "gstclapper-mpris.h"
#include "gstclapper-mpris-private.h"
#include "gstclapper-signal-dispatcher-private.h"

GST_DEBUG_CATEGORY_STATIC (gst_clapper_mpris_debug);
#define GST_CAT_DEFAULT gst_clapper_mpris_debug

#define MPRIS_DEFAULT_VOLUME 1.0

enum
{
  PROP_0,
  PROP_OWN_NAME,
  PROP_ID_PATH,
  PROP_IDENTITY,
  PROP_DESKTOP_ENTRY,
  PROP_DEFAULT_ART_URL,
  PROP_VOLUME,
  PROP_LAST
};

struct _GstClapperMpris
{
  GObject parent;

  GstClapperMprisMediaPlayer2 *base_skeleton;
  GstClapperMprisMediaPlayer2Player *player_skeleton;

  GstClapperSignalDispatcher *signal_dispatcher;
  GstClapperMediaInfo *media_info;

  guint name_id;

  /* Properties */
  gchar *own_name;
  gchar *id_path;
  gchar *identity;
  gchar *desktop_entry;
  gchar *default_art_url;

  gboolean parse_media_info;

  /* Current status */
  gchar *playback_status;
  gboolean can_play;
  guint64 position;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;
};

struct _GstClapperMprisClass
{
  GObjectClass parent_class;
};

#define parent_class gst_clapper_mpris_parent_class
G_DEFINE_TYPE (GstClapperMpris, gst_clapper_mpris, G_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_clapper_mpris_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_clapper_mpris_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_clapper_mpris_dispose (GObject * object);
static void gst_clapper_mpris_finalize (GObject * object);
static void gst_clapper_mpris_constructed (GObject * object);
static gpointer gst_clapper_mpris_main (gpointer data);

static void unregister (GstClapperMpris * self);

static void
gst_clapper_mpris_init (GstClapperMpris * self)
{
  GST_DEBUG_CATEGORY_INIT (gst_clapper_mpris_debug, "ClapperMpris", 0,
      "GstClapperMpris");
  GST_TRACE_OBJECT (self, "Initializing");

  self = gst_clapper_mpris_get_instance_private (self);

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);

  self->base_skeleton = gst_clapper_mpris_media_player2_skeleton_new ();
  self->player_skeleton = gst_clapper_mpris_media_player2_player_skeleton_new ();

  self->name_id = 0;
  self->own_name = NULL;
  self->id_path = NULL;
  self->identity = NULL;
  self->desktop_entry = NULL;
  self->default_art_url = NULL;

  self->signal_dispatcher = NULL;
  self->media_info = NULL;
  self->parse_media_info = FALSE;

  self->playback_status = g_strdup ("Stopped");
  self->can_play = FALSE;
  self->position = 0;

  GST_TRACE_OBJECT (self, "Initialized");
}

static void
gst_clapper_mpris_class_init (GstClapperMprisClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_clapper_mpris_set_property;
  gobject_class->get_property = gst_clapper_mpris_get_property;
  gobject_class->dispose = gst_clapper_mpris_dispose;
  gobject_class->finalize = gst_clapper_mpris_finalize;
  gobject_class->constructed = gst_clapper_mpris_constructed;

  param_specs[PROP_OWN_NAME] =
      g_param_spec_string ("own-name", "DBus own name",
      "DBus name to own on connection",
      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_ID_PATH] =
      g_param_spec_string ("id-path", "DBus id path",
      "A valid D-Bus path describing this player",
      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_IDENTITY] =
      g_param_spec_string ("identity", "Player name",
      "A friendly name to identify the media player",
      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DESKTOP_ENTRY] =
      g_param_spec_string ("desktop-entry", "Desktop entry filename",
      "The basename of an installed .desktop file",
      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DEFAULT_ART_URL] =
      g_param_spec_string ("default-art-url", "Default Art URL",
      "Default art to show when media does not provide one",
      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VOLUME] =
      g_param_spec_double ("volume", "Volume", "Volume",
      0, 1.5, MPRIS_DEFAULT_VOLUME, G_PARAM_READWRITE |
      G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gst_clapper_mpris_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (object);

  switch (prop_id) {
    case PROP_OWN_NAME:
      self->own_name = g_value_dup_string (value);
      break;
    case PROP_ID_PATH:
      self->id_path = g_value_dup_string (value);
      break;
    case PROP_IDENTITY:
      self->identity = g_value_dup_string (value);
      break;
    case PROP_DESKTOP_ENTRY:
      self->desktop_entry = g_value_dup_string (value);
      break;
    case PROP_DEFAULT_ART_URL:
      self->default_art_url = g_value_dup_string (value);
      break;
    case PROP_VOLUME:
      g_object_set_property (G_OBJECT (self->player_skeleton), "volume", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_mpris_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (object);

  switch (prop_id) {
    case PROP_OWN_NAME:
      g_value_set_string (value, self->own_name);
      break;
    case PROP_ID_PATH:
      g_value_set_string (value, self->id_path);
      break;
    case PROP_IDENTITY:
      g_value_set_string (value, self->identity);
      break;
    case PROP_DESKTOP_ENTRY:
      g_value_set_string (value, self->desktop_entry);
      break;
    case PROP_DEFAULT_ART_URL:
      g_value_set_string (value, self->default_art_url);
      break;
    case PROP_VOLUME:
      g_object_get_property (G_OBJECT (self->player_skeleton), "volume", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_clapper_mpris_dispose (GObject * object)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (object);

  GST_TRACE_OBJECT (self, "Stopping main thread");

  if (self->loop) {
    g_main_loop_quit (self->loop);

    if (self->thread != g_thread_self ())
      g_thread_join (self->thread);
    else
      g_thread_unref (self->thread);
    self->thread = NULL;

    g_main_loop_unref (self->loop);
    self->loop = NULL;

    g_main_context_unref (self->context);
    self->context = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_clapper_mpris_finalize (GObject * object)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->own_name);
  g_free (self->id_path);
  g_free (self->identity);
  g_free (self->desktop_entry);
  g_free (self->default_art_url);
  g_free (self->playback_status);

  if (self->base_skeleton)
    g_object_unref (self->base_skeleton);
  if (self->player_skeleton)
    g_object_unref (self->player_skeleton);
  if (self->signal_dispatcher)
    g_object_unref (self->signal_dispatcher);
  if (self->media_info)
    g_object_unref (self->media_info);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_clapper_mpris_constructed (GObject * object)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (object);

  GST_TRACE_OBJECT (self, "Constructed");

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstClapperMpris",
      gst_clapper_mpris_main, self);
  while (!self->loop || !g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static gboolean
main_loop_running_cb (gpointer user_data)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (user_data);

  GST_TRACE_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gboolean
handle_play_cb (GstClapperMprisMediaPlayer2Player * player_skeleton,
    GDBusMethodInvocation * invocation, gpointer user_data)
{
  GstClapper *clapper = GST_CLAPPER (user_data);

  GST_DEBUG ("Handle Play");

  gst_clapper_play (clapper);
  gst_clapper_mpris_media_player2_player_complete_play (player_skeleton, invocation);

  return TRUE;
}

static gboolean
handle_pause_cb (GstClapperMprisMediaPlayer2Player * player_skeleton,
    GDBusMethodInvocation * invocation, gpointer user_data)
{
  GstClapper *clapper = GST_CLAPPER (user_data);

  GST_DEBUG ("Handle Pause");

  gst_clapper_pause (clapper);
  gst_clapper_mpris_media_player2_player_complete_pause (player_skeleton, invocation);

  return TRUE;
}

static gboolean
handle_play_pause_cb (GstClapperMprisMediaPlayer2Player * player_skeleton,
    GDBusMethodInvocation * invocation, gpointer user_data)
{
  GstClapper *clapper = GST_CLAPPER (user_data);

  GST_DEBUG ("Handle PlayPause");

  gst_clapper_toggle_play (clapper);
  gst_clapper_mpris_media_player2_player_complete_play_pause (player_skeleton, invocation);

  return TRUE;
}

static gboolean
handle_seek_cb (GstClapperMprisMediaPlayer2Player * player_skeleton,
    GDBusMethodInvocation * invocation, gint64 offset, gpointer user_data)
{
  GstClapper *clapper = GST_CLAPPER (user_data);

  GST_DEBUG ("Handle Seek");

  gst_clapper_seek_offset (clapper, offset * GST_USECOND);
  gst_clapper_mpris_media_player2_player_complete_seek (player_skeleton, invocation);

  return TRUE;
}

static gboolean
handle_set_position_cb (GstClapperMprisMediaPlayer2Player * player_skeleton,
    GDBusMethodInvocation * invocation, const gchar * track_id,
    gint64 position, gpointer user_data)
{
  GstClapper *clapper = GST_CLAPPER (user_data);

  GST_DEBUG ("Handle SetPosition");

  gst_clapper_seek (clapper, position * GST_USECOND);
  gst_clapper_mpris_media_player2_player_complete_set_position (player_skeleton, invocation);

  return TRUE;
}

static gboolean
handle_open_uri_cb (GstClapperMprisMediaPlayer2Player * player_skeleton,
    GDBusMethodInvocation * invocation, const gchar * uri,
    gpointer user_data)
{
  GstClapper *clapper = GST_CLAPPER (user_data);

  GST_DEBUG ("Handle OpenUri");

  /* FIXME: set one item playlist instead */
  gst_clapper_set_uri (clapper, uri);
  gst_clapper_mpris_media_player2_player_complete_open_uri (player_skeleton, invocation);

  return TRUE;
}

static void
volume_notify_dispatch (gpointer user_data)
{
  GstClapperMpris *self = user_data;

  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_VOLUME]);
}

static void
handle_volume_notify_cb (G_GNUC_UNUSED GObject * obj,
    G_GNUC_UNUSED GParamSpec * pspec, GstClapperMpris * self)
{
  gst_clapper_signal_dispatcher_dispatch (self->signal_dispatcher, NULL,
      volume_notify_dispatch, g_object_ref (self),
      (GDestroyNotify) g_object_unref);
}

static void
unregister (GstClapperMpris * self)
{
  if (!self->name_id)
    return;

  GST_DEBUG_OBJECT (self, "Unregister");
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->base_skeleton));
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->player_skeleton));
  g_bus_unown_name (self->name_id);
  self->name_id = 0;
}

static const gchar *
_get_mpris_trackid (GstClapperMpris * self)
{
  /* TODO: Support more tracks */
  return g_strdup_printf ("%s%s%i", self->id_path, "/Track/", 0);
}

static void
_set_supported_uri_schemes (GstClapperMpris * self)
{
  const gchar *uri_schemes[96] = {};
  GList *elements, *el;
  guint index = 0;

  elements = gst_element_factory_list_get_elements (
      GST_ELEMENT_FACTORY_TYPE_SRC, GST_RANK_NONE);

  for (el = elements; el != NULL; el = el->next) {
    const gchar *const *protocols;
    GstElementFactory *factory = GST_ELEMENT_FACTORY (el->data);

    if (gst_element_factory_get_uri_type (factory) != GST_URI_SRC)
      continue;

    protocols = gst_element_factory_get_uri_protocols (factory);
    if (protocols == NULL || *protocols == NULL)
      continue;

    while (*protocols != NULL) {
      guint j = index;

      while (j--) {
        if (strcmp (uri_schemes[j], *protocols) == 0)
          goto next;
      }
      uri_schemes[index] = *protocols;
      GST_DEBUG_OBJECT (self, "Added supported URI scheme: %s", *protocols);
      ++index;
next:
      ++protocols;
    }
  }
  gst_plugin_feature_list_free (elements);

  gst_clapper_mpris_media_player2_set_supported_uri_schemes (
      self->base_skeleton, uri_schemes);
}

static void
name_acquired_cb (GDBusConnection * connection,
    const gchar *name, gpointer user_data)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (user_data);
  GVariantBuilder builder;

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->base_skeleton),
      connection, "/org/mpris/MediaPlayer2", NULL);
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->player_skeleton),
      connection, "/org/mpris/MediaPlayer2", NULL);

  if (self->identity)
    gst_clapper_mpris_media_player2_set_identity (self->base_skeleton, self->identity);
  if (self->desktop_entry)
    gst_clapper_mpris_media_player2_set_desktop_entry (self->base_skeleton, self->desktop_entry);

  _set_supported_uri_schemes (self);

  gst_clapper_mpris_media_player2_player_set_playback_status (self->player_skeleton, "Stopped");
  gst_clapper_mpris_media_player2_player_set_minimum_rate (self->player_skeleton, 0.01);
  gst_clapper_mpris_media_player2_player_set_maximum_rate (self->player_skeleton, 2.0);
  gst_clapper_mpris_media_player2_player_set_can_seek (self->player_skeleton, TRUE);
  gst_clapper_mpris_media_player2_player_set_can_control (self->player_skeleton, TRUE);

  g_object_bind_property (self->player_skeleton, "can-play",
      self->player_skeleton, "can-pause", G_BINDING_DEFAULT);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (&builder, "{sv}", "mpris:trackid", g_variant_new_string (_get_mpris_trackid (self)));
  g_variant_builder_add (&builder, "{sv}", "mpris:length", g_variant_new_uint64 (0));
  if (self->default_art_url)
    g_variant_builder_add (&builder, "{sv}", "mpris:artUrl", g_variant_new_string (self->default_art_url));
  gst_clapper_mpris_media_player2_player_set_metadata (self->player_skeleton, g_variant_builder_end (&builder));

  GST_DEBUG_OBJECT (self, "Ready");
}

static void
name_lost_cb (GDBusConnection * connection,
    const gchar * name, gpointer user_data)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (user_data);

  unregister (self);
}

static gboolean
mpris_update_props_dispatch (gpointer user_data)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (user_data);

  GST_DEBUG_OBJECT (self, "Updating MPRIS props");
  g_mutex_lock (&self->lock);

  if (self->parse_media_info) {
    GVariantBuilder builder;
    guint64 duration;
    const gchar *track_id, *uri, *title;

    GST_DEBUG_OBJECT (self, "Parsing media info");
    g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

    track_id = _get_mpris_trackid (self);
    uri = gst_clapper_media_info_get_uri (self->media_info);
    title = gst_clapper_media_info_get_title (self->media_info);

    if (track_id) {
      g_variant_builder_add (&builder, "{sv}", "mpris:trackid",
          g_variant_new_string (track_id));
      GST_DEBUG_OBJECT (self, "mpris:trackid: %s", track_id);
    }
    if (uri) {
      g_variant_builder_add (&builder, "{sv}", "xesam:url",
          g_variant_new_string (uri));
      GST_DEBUG_OBJECT (self, "xesam:url: %s", uri);
    }
    if (title) {
      g_variant_builder_add (&builder, "{sv}", "xesam:title",
          g_variant_new_string (title));
      GST_DEBUG_OBJECT (self, "xesam:title: %s", title);
    }

    duration = gst_clapper_media_info_get_duration (self->media_info);
    duration = (duration != GST_CLOCK_TIME_NONE) ? duration / GST_USECOND : 0;
    g_variant_builder_add (&builder, "{sv}", "mpris:length", g_variant_new_uint64 (duration));
    GST_DEBUG_OBJECT (self, "mpris:length: %ld", duration);

    /* TODO: Check for image sample */
    if (self->default_art_url) {
      g_variant_builder_add (&builder, "{sv}", "mpris:artUrl", g_variant_new_string (self->default_art_url));
      GST_DEBUG_OBJECT (self, "mpris:artUrl: %s", self->default_art_url);
    }

    GST_DEBUG_OBJECT (self, "Media info parsed");
    self->parse_media_info = FALSE;

    gst_clapper_mpris_media_player2_player_set_metadata (
        self->player_skeleton, g_variant_builder_end (&builder));
  }
  if (gst_clapper_mpris_media_player2_player_get_can_play (
          self->player_skeleton) != self->can_play) {
    /* "can-play" is bound with "can-pause" */
    gst_clapper_mpris_media_player2_player_set_can_play (
        self->player_skeleton, self->can_play);
    GST_DEBUG_OBJECT (self, "CanPlay/CanPause: %s", self->can_play ? "yes" : "no");
  }
  if (strcmp (gst_clapper_mpris_media_player2_player_get_playback_status (
          self->player_skeleton), self->playback_status) != 0) {
    gst_clapper_mpris_media_player2_player_set_playback_status (
        self->player_skeleton, self->playback_status);
    GST_DEBUG_OBJECT (self, "PlaybackStatus: %s", self->playback_status);
  }
  if (gst_clapper_mpris_media_player2_player_get_position (
          self->player_skeleton) != self->position) {
    gst_clapper_mpris_media_player2_player_set_position (
        self->player_skeleton, self->position);
    GST_DEBUG_OBJECT (self, "Position: %ld", self->position);
  }

  g_mutex_unlock (&self->lock);
  GST_DEBUG_OBJECT (self, "MPRIS props updated");

  return G_SOURCE_REMOVE;
}

static void
mpris_dispatcher_update_dispatch (GstClapperMpris * self)
{
  if (!self->name_id)
    return;

  GST_DEBUG_OBJECT (self, "Queued update props dispatch");

  g_main_context_invoke_full (self->context,
      G_PRIORITY_DEFAULT, mpris_update_props_dispatch,
      g_object_ref (self), g_object_unref);
}

static gpointer
gst_clapper_mpris_main (gpointer data)
{
  GstClapperMpris *self = GST_CLAPPER_MPRIS (data);

  GDBusConnectionFlags flags;
  GDBusConnection *connection;
  GSource *source;
  gchar *address;

  GST_TRACE_OBJECT (self, "Starting main thread");

  g_main_context_push_thread_default (self->context);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->context);
  g_source_unref (source);

  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (!address) {
    GST_WARNING_OBJECT (self, "No MPRIS bus address");
    goto no_mpris;
  }

  GST_DEBUG_OBJECT (self, "Obtained MPRIS DBus address");

  flags = G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
      G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION;
  connection = g_dbus_connection_new_for_address_sync (address,
      flags, NULL, NULL, NULL);
  g_free (address);

  if (!connection) {
    GST_WARNING_OBJECT (self, "No MPRIS bus connection");
    goto no_mpris;
  }

  GST_DEBUG_OBJECT (self, "Obtained MPRIS DBus connection");

  self->name_id = g_bus_own_name_on_connection (connection, self->own_name,
      G_BUS_NAME_OWNER_FLAGS_NONE,
      (GBusNameAcquiredCallback) name_acquired_cb,
      (GBusNameLostCallback) name_lost_cb,
      self, NULL);
  g_object_unref (connection);
  goto done;

no_mpris:
  g_warning ("GstClapperMpris: failed to create DBus connection");

done:
  GST_TRACE_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_TRACE_OBJECT (self, "Stopped main loop");

  unregister (self);
  g_main_context_pop_thread_default (self->context);

  GST_TRACE_OBJECT (self, "Stopped main thread");

  return NULL;
}

void
gst_clapper_mpris_set_clapper (GstClapperMpris * self, GstClapper * clapper,
    GstClapperSignalDispatcher * signal_dispatcher)
{
  if (signal_dispatcher)
    self->signal_dispatcher = g_object_ref (signal_dispatcher);

  g_signal_connect (self->player_skeleton, "handle-play",
      G_CALLBACK (handle_play_cb), clapper);
  g_signal_connect (self->player_skeleton, "handle-pause",
      G_CALLBACK (handle_pause_cb), clapper);
  g_signal_connect (self->player_skeleton, "handle-play-pause",
      G_CALLBACK (handle_play_pause_cb), clapper);
  g_signal_connect (self->player_skeleton, "handle-seek",
      G_CALLBACK (handle_seek_cb), clapper);
  g_signal_connect (self->player_skeleton, "handle-set-position",
      G_CALLBACK (handle_set_position_cb), clapper);
  g_signal_connect (self->player_skeleton, "handle-open-uri",
      G_CALLBACK (handle_open_uri_cb), clapper);

  g_object_bind_property (clapper, "volume", self, "volume", G_BINDING_BIDIRECTIONAL);
  g_signal_connect (self->player_skeleton, "notify::volume",
      G_CALLBACK (handle_volume_notify_cb), self);
}

void
gst_clapper_mpris_set_playback_status (GstClapperMpris * self, const gchar * status)
{
  g_mutex_lock (&self->lock);
  if (strcmp (self->playback_status, status) == 0) {
    g_mutex_unlock (&self->lock);
    return;
  }
  g_free (self->playback_status);
  self->playback_status = g_strdup (status);
  self->can_play = strcmp (status, "Stopped") != 0;
  g_mutex_unlock (&self->lock);

  mpris_dispatcher_update_dispatch (self);
}

void
gst_clapper_mpris_set_position (GstClapperMpris * self, gint64 position)
{
  position /= GST_USECOND;

  g_mutex_lock (&self->lock);
  if (self->position == position) {
    g_mutex_unlock (&self->lock);
    return;
  }
  self->position = position;
  g_mutex_unlock (&self->lock);

  mpris_dispatcher_update_dispatch (self);
}

void
gst_clapper_mpris_set_media_info (GstClapperMpris *self, GstClapperMediaInfo *info)
{
  g_mutex_lock (&self->lock);
  if (self->media_info)
    g_object_unref (self->media_info);
  self->media_info = info;
  self->parse_media_info = TRUE;
  g_mutex_unlock (&self->lock);

  mpris_dispatcher_update_dispatch (self);
}

/**
 * gst_clapper_mpris_new:
 * @own_name: DBus own name
 * @id_path: DBus id path used for prefix
 * @identity: (allow-none): friendly name
 * @desktop_entry: (allow-none): Desktop entry filename
 * @default_art_url: (allow-none): filepath to default art
 *
 * Creates a new #GstClapperMpris instance.
 *
 * Returns: (transfer full): a new #GstClapperMpris instance
 */
GstClapperMpris *
gst_clapper_mpris_new (const gchar * own_name, const gchar * id_path,
    const gchar * identity, const gchar * desktop_entry,
    const gchar * default_art_url)
{
  return g_object_new (GST_TYPE_CLAPPER_MPRIS,
      "own-name", own_name, "id_path", id_path,
      "identity", identity, "desktop-entry", desktop_entry,
      "default-art-url", default_art_url, NULL);
}
