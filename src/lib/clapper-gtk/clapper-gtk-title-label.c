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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * ClapperGtkTitleLabel:
 *
 * A label showing an up to date title of media item.
 *
 * By default #ClapperGtkTitleLabel will automatically show title
 * of [property@Clapper.Queue:current-item] when placed within
 * [class@ClapperGtk.Video] widget hierarchy.
 *
 * Setting [property@ClapperGtk.TitleLabel:media-item] property will
 * make it show title of that particular [class@Clapper.MediaItem]
 * instead. Providing an item to read title from also allows using
 * this [class@Gtk.Widget] outside of [class@ClapperGtk.Video].
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gst/gst.h>

#include "clapper-gtk-title-label.h"
#include "clapper-gtk-utils-private.h"

#define DEFAULT_FALLBACK_TO_URI FALSE

#define GST_CAT_DEFAULT clapper_gtk_title_label_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperGtkTitleLabel
{
  GtkWidget parent;

  GtkLabel *label;

  ClapperMediaItem *current_item;
  ClapperMediaItem *custom_item;
  gboolean fallback_to_uri;

  ClapperPlayer *player;
};

#define parent_class clapper_gtk_title_label_parent_class
G_DEFINE_TYPE (ClapperGtkTitleLabel, clapper_gtk_title_label, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_MEDIA_ITEM,
  PROP_CURRENT_TITLE,
  PROP_FALLBACK_TO_URI,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_label_changed_cb (GtkLabel *label G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkTitleLabel *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_TITLE]);
}

static void
_refresh_title (ClapperGtkTitleLabel *self)
{
  ClapperMediaItem *item;
  gchar *title;

  item = (self->custom_item) ? self->custom_item : self->current_item;

  if (!item) {
    gtk_label_set_label (self->label, _("No media"));
    return;
  }

  title = clapper_media_item_get_title (item);

  if (title) {
    gtk_label_set_label (self->label, title);
    g_free (title);
  } else if (self->fallback_to_uri) {
    gtk_label_set_label (self->label, clapper_media_item_get_uri (item));
  } else {
    gtk_label_set_label (self->label, _("Unknown title"));
  }
}

static void
_media_item_title_changed_cb (ClapperMediaItem *item G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkTitleLabel *self)
{
  _refresh_title (self);
}

static void
_set_current_item (ClapperGtkTitleLabel *self, ClapperMediaItem *current_item)
{
  /* Disconnect signal from old item */
  if (self->current_item) {
    g_signal_handlers_disconnect_by_func (self->current_item,
        _media_item_title_changed_cb, self);
  }

  gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (current_item));
  GST_DEBUG ("Current item changed to: %" GST_PTR_FORMAT, self->current_item);

  /* Reconnect signal to new item */
  if (self->current_item) {
    g_signal_connect (self->current_item, "notify::title",
        G_CALLBACK (_media_item_title_changed_cb), self);
  }
}

static void
_queue_current_item_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperGtkTitleLabel *self)
{
  ClapperMediaItem *current_item = clapper_queue_get_current_item (queue);

  _set_current_item (self, current_item);
  _refresh_title (self);

  gst_clear_object (&current_item);
}

static void
_bind_current_item (ClapperGtkTitleLabel *self)
{
  ClapperQueue *queue = clapper_player_get_queue (self->player);
  ClapperMediaItem *current_item;

  GST_DEBUG ("Binding current item");

  g_signal_connect (queue, "notify::current-item",
      G_CALLBACK (_queue_current_item_changed_cb), self);

  current_item = clapper_queue_get_current_item (queue);
  _set_current_item (self, current_item);
  gst_clear_object (&current_item);
}

static void
_unbind_current_item (ClapperGtkTitleLabel *self)
{
  ClapperQueue *queue = clapper_player_get_queue (self->player);

  GST_DEBUG ("Unbinding current item");

  g_signal_handlers_disconnect_by_func (queue,
      _queue_current_item_changed_cb, self);
  _set_current_item (self, NULL);
}

/**
 * clapper_gtk_title_label_new:
 *
 * Creates a new #ClapperGtkTitleLabel instance.
 *
 * Returns: a new title label #GtkWidget.
 */
GtkWidget *
clapper_gtk_title_label_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_TITLE_LABEL, NULL);
}

/**
 * clapper_gtk_title_label_set_media_item:
 * @label: a #ClapperGtkTitleLabel
 * @item: (nullable): a #ClapperMediaItem
 *
 * Set a media item to display title of as label. When set to %NULL,
 * @label will use default behavior (showing title of current queue item).
 */
void
clapper_gtk_title_label_set_media_item (ClapperGtkTitleLabel *self, ClapperMediaItem *item)
{
  g_return_if_fail (CLAPPER_GTK_IS_TITLE_LABEL (self));
  g_return_if_fail (item == NULL || CLAPPER_IS_MEDIA_ITEM (item));

  if (self->custom_item == item)
    return;

  if (self->player) {
    _unbind_current_item (self);
    self->player = NULL;
  }
  if (self->custom_item) {
    g_signal_handlers_disconnect_by_func (self->custom_item,
        _media_item_title_changed_cb, self);
  }

  gst_object_replace ((GstObject **) &self->custom_item, GST_OBJECT_CAST (item));

  GST_DEBUG ("Set media item: %" GST_PTR_FORMAT, self->custom_item);
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_MEDIA_ITEM]);

  if (self->custom_item) {
    g_signal_connect (self->custom_item, "notify::title",
        G_CALLBACK (_media_item_title_changed_cb), self);
  } else if ((self->player = clapper_gtk_get_player_from_ancestor (GTK_WIDGET (self)))) {
    _bind_current_item (self);
  }

  _refresh_title (self);
}

/**
 * clapper_gtk_title_label_get_media_item:
 * @label: a #ClapperGtkTitleLabel
 *
 * Get currently set media item to display title of.
 *
 * Returns: (transfer none) (nullable): currently set media item.
 */
ClapperMediaItem *
clapper_gtk_title_label_get_media_item (ClapperGtkTitleLabel *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_TITLE_LABEL (self), NULL);

  return self->custom_item;
}

/**
 * clapper_gtk_title_label_get_current_title:
 * @label: a #ClapperGtkTitleLabel
 *
 * Get currently displayed title by @label.
 *
 * Returns: (transfer none): text of title label.
 */
const gchar *
clapper_gtk_title_label_get_current_title (ClapperGtkTitleLabel *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_TITLE_LABEL (self), NULL);

  return gtk_label_get_label (self->label);
}

/**
 * clapper_gtk_title_label_set_fallback_to_uri:
 * @label: a #ClapperGtkTitleLabel
 * @enabled: whether enabled
 *
 * Set whether a [property@Clapper.MediaItem:uri] property should
 * be displayed as a label text when no other title could be determined.
 */
void
clapper_gtk_title_label_set_fallback_to_uri (ClapperGtkTitleLabel *self, gboolean enabled)
{
  g_return_if_fail (CLAPPER_GTK_IS_TITLE_LABEL (self));

  if (self->fallback_to_uri != enabled) {
    self->fallback_to_uri = enabled;
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_FALLBACK_TO_URI]);

    _refresh_title (self);
  }
}

/**
 * clapper_gtk_title_label_get_fallback_to_uri:
 * @label: a #ClapperGtkTitleLabel
 *
 * Get whether a [property@Clapper.MediaItem:uri] property is going
 * be displayed as a label text when no other title could be determined.
 *
 * Returns: %TRUE when item URI will be used as fallback, %FALSE otherwise.
 */
gboolean
clapper_gtk_title_label_get_fallback_to_uri (ClapperGtkTitleLabel *self)
{
  g_return_val_if_fail (CLAPPER_GTK_IS_TITLE_LABEL (self), FALSE);

  return self->fallback_to_uri;
}

static void
clapper_gtk_title_label_init (ClapperGtkTitleLabel *self)
{
  self->label = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_single_line_mode (self->label, TRUE);
  gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_END);
  gtk_widget_set_can_target (GTK_WIDGET (self->label), FALSE);
  gtk_widget_set_parent (GTK_WIDGET (self->label), GTK_WIDGET (self));

  self->fallback_to_uri = DEFAULT_FALLBACK_TO_URI;

  /* Apply CSS styles to internal label */
  g_object_bind_property (self, "css-classes",
      self->label, "css-classes", G_BINDING_DEFAULT);
}

static void
clapper_gtk_title_label_constructed (GObject *object)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (object);

  /* Ensure label if no custom item set yet */
  if (!self->custom_item)
    _refresh_title (self);

  /* This avoids us from comparing label changes as GTK will do this
   * for us and emit this signal only when label text actually changes. */
  g_signal_connect (self->label, "notify::label",
      G_CALLBACK (_label_changed_cb), self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_gtk_title_label_compute_expand (GtkWidget *widget,
    gboolean *hexpand_p, gboolean *vexpand_p)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (widget);

  *hexpand_p = gtk_widget_compute_expand ((GtkWidget *) self->label, GTK_ORIENTATION_HORIZONTAL);
  *vexpand_p = gtk_widget_compute_expand ((GtkWidget *) self->label, GTK_ORIENTATION_VERTICAL);
}

static void
clapper_gtk_title_label_root (GtkWidget *widget)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (widget);

  GTK_WIDGET_CLASS (parent_class)->root (widget);

  if (!self->custom_item
      && (self->player = clapper_gtk_get_player_from_ancestor (widget))) {
    GST_LOG ("Label placed without media item set");
    _bind_current_item (self);
    _refresh_title (self);
  }
}

static void
clapper_gtk_title_label_unroot (GtkWidget *widget)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (widget);

  if (self->player) {
    _unbind_current_item (self);
    self->player = NULL;
  }

  GTK_WIDGET_CLASS (parent_class)->unroot (widget);
}

static void
_label_unparent (GtkLabel *label)
{
  gtk_widget_unparent (GTK_WIDGET (label));
}

static void
clapper_gtk_title_label_dispose (GObject *object)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (object);

  if (self->custom_item) {
    g_signal_handlers_disconnect_by_func (self->custom_item,
        _media_item_title_changed_cb, self);
  }
  if (self->label) {
    g_signal_handlers_disconnect_by_func (self->label,
        _label_changed_cb, self);
  }

  gst_clear_object (&self->current_item);
  gst_clear_object (&self->custom_item);
  g_clear_pointer (&self->label, _label_unparent);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_gtk_title_label_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (object);

  switch (prop_id) {
    case PROP_MEDIA_ITEM:
      g_value_set_object (value, clapper_gtk_title_label_get_media_item (self));
      break;
    case PROP_CURRENT_TITLE:
      g_value_set_string (value, clapper_gtk_title_label_get_current_title (self));
      break;
    case PROP_FALLBACK_TO_URI:
      g_value_set_boolean (value, clapper_gtk_title_label_get_fallback_to_uri (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_title_label_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperGtkTitleLabel *self = CLAPPER_GTK_TITLE_LABEL_CAST (object);

  switch (prop_id) {
    case PROP_MEDIA_ITEM:
      clapper_gtk_title_label_set_media_item (self, g_value_get_object (value));
      break;
    case PROP_FALLBACK_TO_URI:
      clapper_gtk_title_label_set_fallback_to_uri (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_gtk_title_label_class_init (ClapperGtkTitleLabelClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappergtktitlelabel", 0,
      "Clapper GTK Title Label");
  clapper_gtk_init_translations ();

  gobject_class->constructed = clapper_gtk_title_label_constructed;
  gobject_class->get_property = clapper_gtk_title_label_get_property;
  gobject_class->set_property = clapper_gtk_title_label_set_property;
  gobject_class->dispose = clapper_gtk_title_label_dispose;

  widget_class->compute_expand = clapper_gtk_title_label_compute_expand;

  /* Using root/unroot so label "current-title" is immediately
   * updated and can be accessed before label was made visible */
  widget_class->root = clapper_gtk_title_label_root;
  widget_class->unroot = clapper_gtk_title_label_unroot;

  /**
   * ClapperGtkTitleLabel:media-item:
   *
   * Currently set media item to display title of.
   */
  param_specs[PROP_MEDIA_ITEM] = g_param_spec_object ("media-item",
      NULL, NULL, CLAPPER_TYPE_MEDIA_ITEM,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkTitleLabel:current-title:
   *
   * Currently displayed title.
   */
  param_specs[PROP_CURRENT_TITLE] = g_param_spec_string ("current-title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperGtkTitleLabel:fallback-to-uri:
   *
   * When title cannot be determined, show URI instead.
   */
  param_specs[PROP_FALLBACK_TO_URI] = g_param_spec_boolean ("fallback-to-uri",
      NULL, NULL, DEFAULT_FALLBACK_TO_URI,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "clapper-gtk-title-label");
}
