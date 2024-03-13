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
 * ClapperGtkLeadContainer:
 *
 * A #ClapperGtkContainer that can take priority in user interactions with the #ClapperGtkVideo.
 *
 * #ClapperGtkLeadContainer is a special type of [class@ClapperGtk.Container] that can
 * lead in interaction events. When "leading", it is assumed that user interactions
 * over it which would normally trigger actions can be blocked/ignored when set in mask
 * of actions that this widget should block.
 *
 * This kind of container is useful when creating some statically visible overlays
 * covering top of [class@ClapperGtk.Video] that you want to take priority instead of
 * triggering default actions such as toggle play on click or revealing fading overlays.
 *
 * For more info how container widget works see [class@ClapperGtk.Container] documentation.
 */

#include "clapper-gtk-lead-container.h"

#define DEFAULT_LEADING TRUE
#define DEFAULT_BLOCKED_ACTIONS CLAPPER_GTK_VIDEO_ACTION_NONE

typedef struct _ClapperGtkLeadContainerPrivate ClapperGtkLeadContainerPrivate;

struct _ClapperGtkLeadContainerPrivate
{
  gboolean leading;
  ClapperGtkVideoActionMask blocked_actions;
};

#define parent_class clapper_gtk_lead_container_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (ClapperGtkLeadContainer, clapper_gtk_lead_container, CLAPPER_GTK_TYPE_CONTAINER)

enum
{
  PROP_0,
  PROP_LEADING,
  PROP_BLOCKED_ACTIONS,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * clapper_gtk_lead_container_new:
 *
 * Creates a new #ClapperGtkLeadContainer instance.
 *
 * Returns: a new lead container #GtkWidget.
 */
GtkWidget *
clapper_gtk_lead_container_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_LEAD_CONTAINER, NULL);
}

/**
 * clapper_gtk_lead_container_set_leading:
 * @lead_container: a #ClapperGtkLeadContainer
 * @leading: enable leadership
 *
 * Set if @lead_container leadership should be enabled.
 *
 * When enabled, interactions with @lead_container will not trigger
 * their default behavior, instead container and its contents will take priority.
 */
void
clapper_gtk_lead_container_set_leading (ClapperGtkLeadContainer *self, gboolean leading)
{
  ClapperGtkLeadContainerPrivate *priv;

  g_return_if_fail (CLAPPER_GTK_IS_LEAD_CONTAINER (self));

  priv = clapper_gtk_lead_container_get_instance_private (self);

  priv->leading = leading;
}

/**
 * clapper_gtk_lead_container_get_leading:
 * @lead_container: a #ClapperGtkLeadContainer
 *
 * Get a whenever @lead_container has leadership set.
 *
 * Returns: %TRUE if container is leading, %FALSE otherwise.
 */
gboolean
clapper_gtk_lead_container_get_leading (ClapperGtkLeadContainer *self)
{
  ClapperGtkLeadContainerPrivate *priv;

  g_return_val_if_fail (CLAPPER_GTK_IS_LEAD_CONTAINER (self), FALSE);

  priv = clapper_gtk_lead_container_get_instance_private (self);

  return priv->leading;
}

/**
 * clapper_gtk_lead_container_set_blocked_actions:
 * @lead_container: a #ClapperGtkLeadContainer
 * @actions: a #ClapperGtkVideoActionMask of actions to block
 *
 * Set @actions that #ClapperGtkVideo should skip when #GdkEvent which
 * would normally trigger them happens inside @lead_container.
 */
void
clapper_gtk_lead_container_set_blocked_actions (ClapperGtkLeadContainer *self, ClapperGtkVideoActionMask actions)
{
  ClapperGtkLeadContainerPrivate *priv;

  g_return_if_fail (CLAPPER_GTK_IS_LEAD_CONTAINER (self));

  priv = clapper_gtk_lead_container_get_instance_private (self);

  priv->blocked_actions = actions;
}

/**
 * clapper_gtk_lead_container_get_blocked_actions:
 * @lead_container: a #ClapperGtkLeadContainer
 *
 * Get @actions that were set for this @lead_container to block.
 *
 * Returns: a mask of actions that container blocks from being triggered on video.
 */
ClapperGtkVideoActionMask
clapper_gtk_lead_container_get_blocked_actions (ClapperGtkLeadContainer *self)
{
  ClapperGtkLeadContainerPrivate *priv;

  g_return_val_if_fail (CLAPPER_GTK_IS_LEAD_CONTAINER (self), 0);

  priv = clapper_gtk_lead_container_get_instance_private (self);

  return priv->blocked_actions;
}

static void
clapper_gtk_lead_container_init (ClapperGtkLeadContainer *self)
{
  ClapperGtkLeadContainerPrivate *priv = clapper_gtk_lead_container_get_instance_private (self);

  priv->leading = DEFAULT_LEADING;
  priv->blocked_actions = DEFAULT_BLOCKED_ACTIONS;
}

static void
clapper_gtk_lead_container_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkLeadContainer *self = CLAPPER_GTK_LEAD_CONTAINER_CAST (object);

  switch (prop_id) {
    case PROP_LEADING:
      g_value_set_boolean (value, clapper_gtk_lead_container_get_leading (self));
      break;
    case PROP_BLOCKED_ACTIONS:
      g_value_set_flags (value, clapper_gtk_lead_container_get_blocked_actions (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_lead_container_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkLeadContainer *self = CLAPPER_GTK_LEAD_CONTAINER_CAST (object);

  switch (prop_id) {
    case PROP_LEADING:
      clapper_gtk_lead_container_set_leading (self, g_value_get_boolean (value));
      break;
    case PROP_BLOCKED_ACTIONS:
      clapper_gtk_lead_container_set_blocked_actions (self, g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_lead_container_class_init (ClapperGtkLeadContainerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  gobject_class->get_property = clapper_gtk_lead_container_get_property;
  gobject_class->set_property = clapper_gtk_lead_container_set_property;

  /**
   * ClapperGtkLeadContainer:leading:
   *
   * Width that container should target.
   */
  param_specs[PROP_LEADING] = g_param_spec_boolean ("leading",
      NULL, NULL, DEFAULT_LEADING,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkLeadContainer:blocked-actions:
   *
   * Mask of actions that container blocks from being triggered on video.
   */
  param_specs[PROP_BLOCKED_ACTIONS] = g_param_spec_flags ("blocked-actions",
      NULL, NULL, CLAPPER_GTK_TYPE_VIDEO_ACTION_MASK, DEFAULT_BLOCKED_ACTIONS,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-lead-container");
}
