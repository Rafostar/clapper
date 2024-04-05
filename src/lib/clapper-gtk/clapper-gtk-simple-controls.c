/* Clapper GTK Integration Library
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
 * ClapperGtkSimpleControls:
 *
 * A minimalistic playback controls panel widget.
 *
 * #ClapperGtkSimpleControls is a simple, ready to be used playback controls widget.
 * It is meant to be placed as an overlay (either fading or not) of [class@ClapperGtk.Video]
 * as-is, providing minimal yet universal playback controls for your app.
 *
 * If you need a further customized controls, please use individual widgets this
 * widget consists of to build your own controls implementation instead.
 */

#include "config.h"

#include <clapper/clapper.h>

#include "clapper-gtk-simple-controls.h"
#include "clapper-gtk-seek-bar.h"

#define DEFAULT_FULLSCREENABLE TRUE
#define DEFAULT_SEEK_METHOD CLAPPER_PLAYER_SEEK_METHOD_NORMAL

#define IS_REVEALED(widget) (gtk_revealer_get_child_revealed ((GtkRevealer *) (widget)))
#define IS_REVEAL(widget) (gtk_revealer_get_reveal_child ((GtkRevealer *) (widget)))
#define SET_REVEAL(widget,reveal) (gtk_revealer_set_reveal_child ((GtkRevealer *) (widget), reveal))

#define GST_CAT_DEFAULT clapper_gtk_simple_controls_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkSimpleControls
{
  ClapperGtkContainer parent;

  GtkWidget *seek_bar;
  GtkWidget *extra_menu_button;
  GtkWidget *fullscreen_top_revealer;
  GtkWidget *fullscreen_bottom_revealer;
  GtkWidget *controls_slide_revealer;

  gboolean fullscreenable;
  ClapperPlayerSeekMethod seek_method;

  gboolean adapt;
};

#define parent_class clapper_gtk_simple_controls_parent_class
G_DEFINE_TYPE (ClapperGtkSimpleControls, clapper_gtk_simple_controls, CLAPPER_GTK_TYPE_CONTAINER)

enum
{
  PROP_0,
  PROP_FULLSCREENABLE,
  PROP_SEEK_METHOD,
  PROP_EXTRA_MENU_BUTTON,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
initial_adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperGtkSimpleControls *self)
{
  GST_DEBUG_OBJECT (self, "Initially adapted: %s", (adapt) ? "yes" : "no");

  clapper_gtk_seek_bar_set_reveal_labels (CLAPPER_GTK_SEEK_BAR (self->seek_bar), !adapt);
}

static void
full_adapt_cb (ClapperGtkContainer *container, gboolean adapt,
    ClapperGtkSimpleControls *self)
{
  self->adapt = adapt;

  GST_DEBUG_OBJECT (self, "Width adapted: %s", (self->adapt) ? "yes" : "no");

  /* Take different action, depending on transition step we are currently at */
  if (self->adapt) {
    if (IS_REVEAL (self->fullscreen_bottom_revealer))
      SET_REVEAL (self->fullscreen_bottom_revealer, FALSE);
    else if (IS_REVEAL (self->controls_slide_revealer))
      SET_REVEAL (self->controls_slide_revealer, FALSE);
    else
      SET_REVEAL (self->fullscreen_top_revealer, TRUE);
  } else {
    if (IS_REVEAL (self->fullscreen_top_revealer))
      SET_REVEAL (self->fullscreen_top_revealer, FALSE);
    else if (!IS_REVEAL (self->controls_slide_revealer))
      SET_REVEAL (self->controls_slide_revealer, TRUE);
    else
      SET_REVEAL (self->fullscreen_bottom_revealer, TRUE);
  }
}

static void
controls_revealed_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkSimpleControls *self)
{
  gboolean revealed = IS_REVEALED (revealer);

  GST_DEBUG_OBJECT (self, "Slide revealed: %s", (revealed) ? "yes" : "no");

  /* We should be hidden when adapted, otherwise go back */
  if (G_UNLIKELY (revealed == self->adapt))
    gtk_revealer_set_reveal_child (revealer, !revealed);
}

/**
 * clapper_gtk_simple_controls_new:
 *
 * Creates a new #ClapperGtkSimpleControls instance.
 *
 * Returns: a new simple controls #GtkWidget.
 */
GtkWidget *
clapper_gtk_simple_controls_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_SIMPLE_CONTROLS, NULL);
}

/**
 * clapper_gtk_simple_controls_set_fullscreenable:
 * @controls: a #ClapperGtkSimpleControls
 * @fullscreenable: whether show button for toggling fullscreen state
 *
 * Set whether [class@ClapperGtk.ToggleFullscreenButton] button in the @controls
 * should be visible.
 *
 * You might want to consider setting this to %FALSE, if your application
 * does not implement [signal@ClapperGtk.Video::toggle-fullscreen] signal.
 */
void
clapper_gtk_simple_controls_set_fullscreenable (ClapperGtkSimpleControls *self, gboolean fullscreenable)
{
  g_return_if_fail (CLAPPER_GTK_IS_SIMPLE_CONTROLS (self));

  if (self->fullscreenable != fullscreenable) {
    self->fullscreenable = fullscreenable;
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_FULLSCREENABLE]);
  }
}

/**
 * clapper_gtk_simple_controls_get_fullscreenable:
 * @controls: a #ClapperGtkSimpleControls
 *
 * Get whether [class@ClapperGtk.ToggleFullscreenButton] button in the @controls
 * is set to be visible.
 */
gboolean
clapper_gtk_simple_controls_get_fullscreenable (ClapperGtkSimpleControls *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_SIMPLE_CONTROLS (self), FALSE);

  return self->fullscreenable;
}

/**
 * clapper_gtk_simple_controls_set_seek_method:
 * @controls: a #ClapperGtkSimpleControls
 * @method: a #ClapperPlayerSeekMethod
 *
 * Set [enum@Clapper.PlayerSeekMethod] to use when seeking with progress bar.
 */
void
clapper_gtk_simple_controls_set_seek_method (ClapperGtkSimpleControls *self,
    ClapperPlayerSeekMethod method)
{
  g_return_if_fail (CLAPPER_GTK_IS_SIMPLE_CONTROLS (self));

  clapper_gtk_seek_bar_set_seek_method (CLAPPER_GTK_SEEK_BAR (self->seek_bar), method);
}

/**
 * clapper_gtk_simple_controls_get_seek_method:
 * @controls: a #ClapperGtkSimpleControls
 *
 * Get [enum@Clapper.PlayerSeekMethod] used when seeking with progress bar.
 *
 * Returns: #ClapperPlayerSeekMethod used for seeking.
 */
ClapperPlayerSeekMethod
clapper_gtk_simple_controls_get_seek_method (ClapperGtkSimpleControls *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_SIMPLE_CONTROLS (self), DEFAULT_SEEK_METHOD);

  return clapper_gtk_seek_bar_get_seek_method (CLAPPER_GTK_SEEK_BAR (self->seek_bar));
}

/**
 * clapper_gtk_simple_controls_get_extra_menu_button:
 * @controls: a #ClapperGtkSimpleControls
 *
 * Get [class@ClapperGtk.ExtraMenuButton] that resides within @controls.
 *
 * Returns: (transfer none): #ClapperGtkExtraMenuButton within simple controls panel.
 */
ClapperGtkExtraMenuButton *
clapper_gtk_simple_controls_get_extra_menu_button (ClapperGtkSimpleControls *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_SIMPLE_CONTROLS (self), NULL);

  return CLAPPER_GTK_EXTRA_MENU_BUTTON (self->extra_menu_button);
}

static void
clapper_gtk_simple_controls_init (ClapperGtkSimpleControls *self)
{
  self->fullscreenable = DEFAULT_FULLSCREENABLE;
  self->seek_method = DEFAULT_SEEK_METHOD;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Set our defaults to children */
  clapper_gtk_seek_bar_set_seek_method (
      CLAPPER_GTK_SEEK_BAR_CAST (self->seek_bar), self->seek_method);
}

static void
clapper_gtk_simple_controls_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_GTK_TYPE_SIMPLE_CONTROLS);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_simple_controls_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkSimpleControls *self = CLAPPER_GTK_SIMPLE_CONTROLS_CAST (object);

  switch (prop_id) {
    case PROP_FULLSCREENABLE:
      g_value_set_boolean (value, clapper_gtk_simple_controls_get_fullscreenable (self));
      break;
    case PROP_SEEK_METHOD:
      g_value_set_enum (value, clapper_gtk_simple_controls_get_seek_method (self));
      break;
    case PROP_EXTRA_MENU_BUTTON:
      g_value_set_object (value, clapper_gtk_simple_controls_get_extra_menu_button (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_simple_controls_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkSimpleControls *self = CLAPPER_GTK_SIMPLE_CONTROLS_CAST (object);

  switch (prop_id) {
    case PROP_FULLSCREENABLE:
      clapper_gtk_simple_controls_set_fullscreenable (self, g_value_get_boolean (value));
      break;
    case PROP_SEEK_METHOD:
      clapper_gtk_simple_controls_set_seek_method (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_simple_controls_class_init (ClapperGtkSimpleControlsClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtksimplecontrols", 0,
      "Clapper GTK Simple Controls");

  gobject_class->get_property = clapper_gtk_simple_controls_get_property;
  gobject_class->set_property = clapper_gtk_simple_controls_set_property;
  gobject_class->dispose = clapper_gtk_simple_controls_dispose;

  /**
   * ClapperGtkSimpleControls:fullscreenable:
   *
   * Whether toggle fullscreen button should be visible.
   */
  param_specs[PROP_FULLSCREENABLE] = g_param_spec_boolean ("fullscreenable",
      NULL, NULL, DEFAULT_FULLSCREENABLE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkSimpleControls:seek-method:
   *
   * Method used for seeking.
   */
  param_specs[PROP_SEEK_METHOD] = g_param_spec_enum ("seek-method",
      NULL, NULL, CLAPPER_TYPE_PLAYER_SEEK_METHOD, DEFAULT_SEEK_METHOD,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkSimpleControls:extra-menu-button:
   *
   * Access to extra menu button within controls.
   */
  param_specs[PROP_EXTRA_MENU_BUTTON] = g_param_spec_object ("extra-menu-button",
      NULL, NULL, CLAPPER_GTK_TYPE_EXTRA_MENU_BUTTON,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_GTK_RESOURCE_PREFIX "/ui/clapper-gtk-simple-controls.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSimpleControls, seek_bar);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSimpleControls, extra_menu_button);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSimpleControls, fullscreen_top_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSimpleControls, fullscreen_bottom_revealer);
  gtk_widget_class_bind_template_child (widget_class, ClapperGtkSimpleControls, controls_slide_revealer);

  gtk_widget_class_bind_template_callback (widget_class, initial_adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, controls_revealed_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-simple-controls");
}
