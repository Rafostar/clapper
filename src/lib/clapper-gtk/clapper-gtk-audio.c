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
 * ClapperGtkAudio:
 *
 * A GTK widget for audio playback with Clapper API.
 *
 * #ClapperGtkAudio is a widget meant for integrating audio playback
 * within GTK application. It exposes [class@Clapper.Player] through its
 * base class [property@ClapperGtk.Av:player] property.
 *
 * Other widgets (buttons, seek bar, etc.) provided by `ClapperGtk` library, once placed
 * anywhere inside audio container (including nesting within another widget like [class@Gtk.Box])
 * will automatically control #ClapperGtkAudio they are within. This allows to freely create
 * custom UI best suited for specific application.
 *
 * # Basic usage
 *
 * A typical use case is to embed audio widget as part of your app where audio playback
 * is needed (can be even the very first child of the window). Get the [class@Clapper.Player]
 * belonging to the AV widget and start adding new [class@Clapper.MediaItem] items to the
 * [class@Clapper.Queue] for playback. For more information please refer to the Clapper
 * playback library documentation.
 *
 * # Actions
 *
 * You can use built-in actions of parent [class@ClapperGtk.Av].
 * See its documentation for the list of available ones.
 *
 * # ClapperGtkAudio as GtkBuildable
 *
 * #ClapperGtkAudio implementation of the [iface@Gtk.Buildable] interface supports
 * placing a single widget (which might then hold multiple widgets) as `<child>` element.
 *
 * ```xml
 * <object class="ClapperGtkAudio" id="audio">
 *   <child>
 *     <object class="GtkBox">
 *       <property name="orientation">horizontal</property>
 *       <child>
 *         <object class="ClapperGtkPreviousItemButton">
 *       </child>
 *       <child>
 *         <object class="ClapperGtkTogglePlayButton">
 *       </child>
 *       <child>
 *         <object class="ClapperGtkNextItemButton">
 *       </child>
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * Since: 0.10
 */

#include "config.h"

#include "clapper-gtk-audio.h"

#define GST_CAT_DEFAULT clapper_gtk_audio_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkAudio
{
  ClapperGtkAv parent;

  GtkWidget *child;
};

static void
clapper_gtk_audio_add_child (GtkBuildable *buildable,
    GtkBuilder *builder, GObject *child, const char *type)
{
  if (GTK_IS_WIDGET (child)) {
    clapper_gtk_audio_set_child (CLAPPER_GTK_AUDIO (buildable), GTK_WIDGET (child));
  } else {
    GtkBuildableIface *parent_iface = g_type_interface_peek_parent (GTK_BUILDABLE_GET_IFACE (buildable));
    parent_iface->add_child (buildable, builder, child, type);
  }
}

static void
_buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = clapper_gtk_audio_add_child;
}

#define parent_class clapper_gtk_audio_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperGtkAudio, clapper_gtk_audio, CLAPPER_GTK_TYPE_AV,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, _buildable_iface_init))

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static inline void
_unparent_child (ClapperGtkAudio *self)
{
  GtkWidget *child;

  if ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);
}

/**
 * clapper_gtk_audio_new:
 *
 * Creates a new #ClapperGtkAudio instance.
 *
 * Newly created audio widget will also have set "scaletempo" GStreamer element
 * as default audio filter on its [class@Clapper.Player] and disable video and
 * subtitle streams. This can be changed after construction by setting
 * corresponding player properties.
 *
 * Returns: a new audio #GtkWidget.
 */
GtkWidget *
clapper_gtk_audio_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_AUDIO, NULL);
}

/**
 * clapper_gtk_audio_set_child:
 * @audio: a #ClapperGtkAudio
 * @child: (nullable): a #GtkWidget
 *
 * Set a child #GtkWidget of @audio.
 */
void
clapper_gtk_audio_set_child (ClapperGtkAudio *self, GtkWidget *child)
{
  g_return_if_fail (CLAPPER_GTK_IS_AUDIO (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  _unparent_child (self);
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}

/**
 * clapper_gtk_audio_get_child:
 * @audio: a #ClapperGtkAudio
 *
 * Get a child #GtkWidget of @audio.
 *
 * Returns: (transfer none) (nullable): #GtkWidget set as child.
 */
GtkWidget *
clapper_gtk_audio_get_child (ClapperGtkAudio *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_AUDIO (self), NULL);

  return gtk_widget_get_first_child (GTK_WIDGET (self));
}

static void
clapper_gtk_audio_init (ClapperGtkAudio *self)
{
}

static void
clapper_gtk_audio_constructed (GObject *object)
{
  ClapperGtkAudio *self = CLAPPER_GTK_AUDIO_CAST (object);
  ClapperPlayer *player;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  player = clapper_gtk_av_get_player (CLAPPER_GTK_AV_CAST (self));

  clapper_player_set_video_enabled (player, FALSE);
  clapper_player_set_subtitles_enabled (player, FALSE);
}

static void
clapper_gtk_audio_dispose (GObject *object)
{
  ClapperGtkAudio *self = CLAPPER_GTK_AUDIO_CAST (object);

  _unparent_child (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_audio_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkAudio *self = CLAPPER_GTK_AUDIO_CAST (object);

  switch (prop_id) {
    case PROP_CHILD:
      g_value_set_object (value, clapper_gtk_audio_get_child (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_audio_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkAudio *self = CLAPPER_GTK_AUDIO_CAST (object);

  switch (prop_id) {
    case PROP_CHILD:
      clapper_gtk_audio_set_child (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_audio_class_init (ClapperGtkAudioClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtkaudio", GST_DEBUG_FG_MAGENTA,
      "Clapper GTK Audio");

  gobject_class->constructed = clapper_gtk_audio_constructed;
  gobject_class->get_property = clapper_gtk_audio_get_property;
  gobject_class->set_property = clapper_gtk_audio_set_property;
  gobject_class->dispose = clapper_gtk_audio_dispose;

  /**
   * ClapperGtkAudio:child:
   *
   * The child widget of `ClapperGtkAudio`.
   */
  param_specs[PROP_CHILD] = g_param_spec_object ("child",
      NULL, NULL, GTK_TYPE_WIDGET,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-audio");
}
