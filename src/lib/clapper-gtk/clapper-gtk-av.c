/* Clapper GTK Integration Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * ClapperGtkAv:
 *
 * A base class for GTK audio and video widgets.
 *
 * See its descendants: [class@ClapperGtk.Audio] and [class@ClapperGtk.Video].
 *
 * # Actions
 *
 * #ClapperGtkAv defines a set of built-in actions:
 *
 * ```yaml
 * - "av.toggle-play": toggle play/pause
 * - "av.play": start/resume playback
 * - "av.pause": pause playback
 * - "av.stop": stop playback
 * - "av.seek": seek to position (variant "d")
 * - "av.seek-custom": seek to position using seek method (variant "(di)")
 * - "av.toggle-mute": toggle mute state
 * - "av.set-mute": set mute state (variant "b")
 * - "av.volume-up": increase volume by 2%
 * - "av.volume-down": decrease volume by 2%
 * - "av.set-volume": set volume to specified value (variant "d")
 * - "av.speed-up": increase speed (from 0.05x - 2x range to nearest quarter)
 * - "av.speed-down": decrease speed (from 0.05x - 2x range to nearest quarter)
 * - "av.set-speed": set speed to specified value (variant "d")
 * - "av.previous-item": select previous item in queue
 * - "av.next-item": select next item in queue
 * - "av.select-item": select item at specified index in queue (variant "u")
 * ```
 *
 * Since: 0.10
 */

#include "config.h"

#include <math.h>

#include "clapper-gtk-av.h"

#define PERCENTAGE_ROUND(a) (round ((gdouble) a / 0.01) * 0.01)

#define DEFAULT_AUTO_INHIBIT FALSE

#define GST_CAT_DEFAULT clapper_gtk_av_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperGtkAvPrivate ClapperGtkAvPrivate;

struct _ClapperGtkAvPrivate
{
  ClapperPlayer *player;
  gboolean auto_inhibit;

  guint inhibit_cookie;
};

#define parent_class clapper_gtk_av_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (ClapperGtkAv, clapper_gtk_av, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_PLAYER,
  PROP_AUTO_INHIBIT,
  PROP_INHIBITED,
  PROP_LAST
};

static gboolean provider_added = FALSE;
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
toggle_play_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  switch (clapper_player_get_state (player)) {
    case CLAPPER_PLAYER_STATE_PLAYING:
      clapper_player_pause (player);
      break;
    case CLAPPER_PLAYER_STATE_STOPPED:
    case CLAPPER_PLAYER_STATE_PAUSED:
      clapper_player_play (player);
      break;
    default:
      break;
  }
}

static void
play_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  clapper_player_play (player);
}

static void
pause_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  clapper_player_pause (player);
}

static void
stop_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  clapper_player_stop (player);
}

static void
seek_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble position = g_variant_get_double (parameter);

  clapper_player_seek (player, position);
}

static void
seek_custom_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  ClapperPlayerSeekMethod method = CLAPPER_PLAYER_SEEK_METHOD_NORMAL;
  gdouble position = 0;

  g_variant_get (parameter, "(di)", &position, &method);
  clapper_player_seek_custom (player, position, method);
}

static void
toggle_mute_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  clapper_player_set_mute (player, !clapper_player_get_mute (player));
}

static void
set_mute_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gboolean mute = g_variant_get_boolean (parameter);

  clapper_player_set_mute (player, mute);
}

static void
volume_up_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble volume = (clapper_player_get_volume (player) + 0.02);

  if (volume > 2.0)
    volume = 2.0;

  clapper_player_set_volume (player, PERCENTAGE_ROUND (volume));
}

static void
volume_down_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble volume = (clapper_player_get_volume (player) - 0.02);

  if (volume < 0)
    volume = 0;

  clapper_player_set_volume (player, PERCENTAGE_ROUND (volume));
}

static void
set_volume_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble volume = g_variant_get_double (parameter);

  clapper_player_set_volume (player, volume);
}

static void
speed_up_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble dest, speed = clapper_player_get_speed (player);

  if (speed >= 2.0)
    return;

  dest = 0.25;
  while (speed >= dest)
    dest += 0.25;

  if (dest > 2.0)
    dest = 2.0;

  clapper_player_set_speed (player, dest);
}

static void
speed_down_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble dest, speed = clapper_player_get_speed (player);

  if (speed <= 0.05)
    return;

  dest = 2.0;
  while (speed <= dest)
    dest -= 0.25;

  if (dest < 0.05)
    dest = 0.05;

  clapper_player_set_speed (player, dest);
}

static void
set_speed_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  gdouble speed = g_variant_get_double (parameter);

  clapper_player_set_speed (player, speed);
}

static void
previous_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  clapper_queue_select_previous_item (clapper_player_get_queue (player));
}

static void
next_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);

  clapper_queue_select_next_item (clapper_player_get_queue (player));
}

static void
select_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperPlayer *player = clapper_gtk_av_get_player (self);
  guint index = g_variant_get_uint32 (parameter);

  clapper_queue_select_index (clapper_player_get_queue (player), index);
}

static void
_ensure_css_provider (void)
{
  GdkDisplay *display;

  if (provider_added)
    return;

  display = gdk_display_get_default ();

  if (G_LIKELY (display != NULL)) {
    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (provider,
        CLAPPER_GTK_RESOURCE_PREFIX "/css/styles.css");

    gtk_style_context_add_provider_for_display (display,
        (GtkStyleProvider *) provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 1);
    g_object_unref (provider);

    provider_added = TRUE;
  }
}

static inline void
_set_inhibit_session (ClapperGtkAv *self, gboolean inhibit)
{
  ClapperGtkAvPrivate *priv = clapper_gtk_av_get_instance_private (self);
  GtkRoot *root;
  GApplication *app;
  gboolean inhibited = (priv->inhibit_cookie != 0);

  if (inhibited == inhibit)
    return;

  GST_DEBUG_OBJECT (self, "Trying to %sinhibit session...", (inhibit) ? "" : "un");

  root = gtk_widget_get_root (GTK_WIDGET (self));

  if (!root || !GTK_IS_WINDOW (root)) {
    GST_WARNING_OBJECT (self, "Cannot %sinhibit session "
        "without root window", (inhibit) ? "" : "un");
    return;
  }

  /* NOTE: Not using application from window prop,
   * as it goes away early when unrooting */
  app = g_application_get_default ();

  if (!app || !GTK_IS_APPLICATION (app)) {
    GST_WARNING_OBJECT (self, "Cannot %sinhibit session "
        "without window application set", (inhibit) ? "" : "un");
    return;
  }

  if (inhibited) {
    gtk_application_uninhibit (GTK_APPLICATION (app), priv->inhibit_cookie);
    priv->inhibit_cookie = 0;
  }
  if (inhibit) {
    priv->inhibit_cookie = gtk_application_inhibit (GTK_APPLICATION (app),
        GTK_WINDOW (root), GTK_APPLICATION_INHIBIT_IDLE,
        "Media is playing");
  }

  GST_DEBUG_OBJECT (self, "Session %sinhibited", (inhibit) ? "" : "un");
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_INHIBITED]);
}

static void
_player_state_changed_cb (ClapperPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkAv *self)
{
  ClapperGtkAvPrivate *priv = clapper_gtk_av_get_instance_private (self);

  if (priv->auto_inhibit) {
    ClapperPlayerState state = clapper_player_get_state (player);
    _set_inhibit_session (self, state == CLAPPER_PLAYER_STATE_PLAYING);
  }
}

/**
 * clapper_gtk_av_get_player:
 * @av: a #ClapperGtkAv
 *
 * Get #ClapperPlayer used by this #ClapperGtkAv instance.
 *
 * Returns: (transfer none): a #ClapperPlayer used by widget.
 *
 * Since: 0.10
 */
ClapperPlayer *
clapper_gtk_av_get_player (ClapperGtkAv *self)
{
  ClapperGtkAvPrivate *priv;

  g_return_val_if_fail (CLAPPER_GTK_IS_AV (self), NULL);

  priv = clapper_gtk_av_get_instance_private (self);

  return priv->player;
}

/**
 * clapper_gtk_av_set_auto_inhibit:
 * @av: a #ClapperGtkAv
 * @inhibit: whether to enable automatic session inhibit
 *
 * Set whether widget should try to automatically inhibit session
 * from idling (and possibly screen going black) when media is playing.
 *
 * Since: 0.10
 */
void
clapper_gtk_av_set_auto_inhibit (ClapperGtkAv *self, gboolean inhibit)
{
  ClapperGtkAvPrivate *priv;

  g_return_if_fail (CLAPPER_GTK_IS_AV (self));

  priv = clapper_gtk_av_get_instance_private (self);

  if (priv->auto_inhibit != inhibit) {
    priv->auto_inhibit = inhibit;

    /* Uninhibit if we were auto inhibited earlier */
    if (!priv->auto_inhibit)
      _set_inhibit_session (self, FALSE);

    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_AUTO_INHIBIT]);
  }
}

/**
 * clapper_gtk_av_get_auto_inhibit:
 * @av: a #ClapperGtkAv
 *
 * Get whether automatic session inhibit is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 *
 * Since: 0.10
 */
gboolean
clapper_gtk_av_get_auto_inhibit (ClapperGtkAv *self)
{
  ClapperGtkAvPrivate *priv;

  g_return_val_if_fail (CLAPPER_GTK_IS_AV (self), FALSE);

  priv = clapper_gtk_av_get_instance_private (self);

  return priv->auto_inhibit;
}

/**
 * clapper_gtk_av_get_inhibited:
 * @av: a #ClapperGtkAv
 *
 * Get whether session is currently inhibited by
 * [property@ClapperGtk.Av:auto-inhibit].
 *
 * Returns: %TRUE if inhibited, %FALSE otherwise.
 *
 * Since: 0.10
 */
gboolean
clapper_gtk_av_get_inhibited (ClapperGtkAv *self)
{
  ClapperGtkAvPrivate *priv;

  g_return_val_if_fail (CLAPPER_GTK_IS_AV (self), FALSE);

  priv = clapper_gtk_av_get_instance_private (self);

  return (priv->inhibit_cookie != 0);
}

static void
clapper_gtk_av_root (GtkWidget *widget)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);
  ClapperGtkAvPrivate *priv = clapper_gtk_av_get_instance_private (self);

  _ensure_css_provider ();

  GTK_WIDGET_CLASS (parent_class)->root (widget);

  if (priv->auto_inhibit) {
    ClapperPlayerState state = clapper_player_get_state (priv->player);
    _set_inhibit_session (self, state == CLAPPER_PLAYER_STATE_PLAYING);
  }
}

static void
clapper_gtk_av_unroot (GtkWidget *widget)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (widget);

  _set_inhibit_session (self, FALSE);

  GTK_WIDGET_CLASS (parent_class)->unroot (widget);
}

static void
clapper_gtk_av_init (ClapperGtkAv *self)
{
  ClapperGtkAvPrivate *priv = clapper_gtk_av_get_instance_private (self);

  priv->auto_inhibit = DEFAULT_AUTO_INHIBIT;
}

static void
clapper_gtk_av_constructed (GObject *object)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (object);
  ClapperGtkAvPrivate *priv = clapper_gtk_av_get_instance_private (self);
  GstElement *afilter;

  priv->player = clapper_player_new ();

  g_signal_connect (priv->player, "notify::state",
      G_CALLBACK (_player_state_changed_cb), self);

  afilter = gst_element_factory_make ("scaletempo", NULL);
  if (G_LIKELY (afilter != NULL))
    clapper_player_set_audio_filter (priv->player, afilter);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_gtk_av_dispose (GObject *object)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (object);
  ClapperGtkAvPrivate *priv = clapper_gtk_av_get_instance_private (self);

  /* Something else might still be holding a reference on the player,
   * thus we should disconnect everything before disposing template */
  if (priv->player) {
    g_signal_handlers_disconnect_by_func (priv->player,
        _player_state_changed_cb, self);
  }

  gst_clear_object (&priv->player);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_av_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (object);

  switch (prop_id) {
    case PROP_PLAYER:
      g_value_set_object (value, clapper_gtk_av_get_player (self));
      break;
    case PROP_AUTO_INHIBIT:
      g_value_set_boolean (value, clapper_gtk_av_get_auto_inhibit (self));
      break;
    case PROP_INHIBITED:
      g_value_set_boolean (value, clapper_gtk_av_get_inhibited (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_av_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkAv *self = CLAPPER_GTK_AV_CAST (object);

  switch (prop_id) {
    case PROP_AUTO_INHIBIT:
      clapper_gtk_av_set_auto_inhibit (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_av_class_init (ClapperGtkAvClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkav", GST_DEBUG_FG_MAGENTA,
      "Clapper GTK AV");

  widget_class->root = clapper_gtk_av_root;
  widget_class->unroot = clapper_gtk_av_unroot;

  gobject_class->constructed = clapper_gtk_av_constructed;
  gobject_class->get_property = clapper_gtk_av_get_property;
  gobject_class->set_property = clapper_gtk_av_set_property;
  gobject_class->dispose = clapper_gtk_av_dispose;

  /**
   * ClapperGtkAv:player:
   *
   * A #ClapperPlayer used by widget.
   */
  param_specs[PROP_PLAYER] = g_param_spec_object ("player",
      NULL, NULL, CLAPPER_TYPE_PLAYER,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkAv:auto-inhibit:
   *
   * Try to automatically inhibit session when media is playing.
   */
  param_specs[PROP_AUTO_INHIBIT] = g_param_spec_boolean ("auto-inhibit",
      NULL, NULL, DEFAULT_AUTO_INHIBIT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkAv:inhibited:
   *
   * Get whether session is currently inhibited by playback.
   */
  param_specs[PROP_INHIBITED] = g_param_spec_boolean ("inhibited",
      NULL, NULL, FALSE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_install_action (widget_class, "av.toggle-play", NULL, toggle_play_action_cb);
  gtk_widget_class_install_action (widget_class, "av.play", NULL, play_action_cb);
  gtk_widget_class_install_action (widget_class, "av.pause", NULL, pause_action_cb);
  gtk_widget_class_install_action (widget_class, "av.stop", NULL, stop_action_cb);
  gtk_widget_class_install_action (widget_class, "av.seek", "d", seek_action_cb);
  gtk_widget_class_install_action (widget_class, "av.seek-custom", "(di)", seek_custom_action_cb);
  gtk_widget_class_install_action (widget_class, "av.toggle-mute", NULL, toggle_mute_action_cb);
  gtk_widget_class_install_action (widget_class, "av.set-mute", "b", set_mute_action_cb);
  gtk_widget_class_install_action (widget_class, "av.volume-up", NULL, volume_up_action_cb);
  gtk_widget_class_install_action (widget_class, "av.volume-down", NULL, volume_down_action_cb);
  gtk_widget_class_install_action (widget_class, "av.set-volume", "d", set_volume_action_cb);
  gtk_widget_class_install_action (widget_class, "av.speed-up", NULL, speed_up_action_cb);
  gtk_widget_class_install_action (widget_class, "av.speed-down", NULL, speed_down_action_cb);
  gtk_widget_class_install_action (widget_class, "av.set-speed", "d", set_speed_action_cb);
  gtk_widget_class_install_action (widget_class, "av.previous-item", NULL, previous_item_action_cb);
  gtk_widget_class_install_action (widget_class, "av.next-item", NULL, next_item_action_cb);
  gtk_widget_class_install_action (widget_class, "av.select-item", "u", select_item_action_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-av");
}
