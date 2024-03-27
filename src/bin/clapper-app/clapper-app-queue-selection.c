/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <gst/gst.h>
#include <gtk/gtk.h>

#include "clapper-app-queue-selection.h"

#define GST_CAT_DEFAULT clapper_app_queue_selection_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppQueueSelection
{
  GObject parent;

  ClapperQueue *queue;

  ClapperMediaItem *current_item;
  guint current_position;
};

enum
{
  PROP_0,
  PROP_QUEUE,
  PROP_LAST
};

enum
{
  SIGNAL_ITEM_SELECTED,
  SIGNAL_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

static GType
clapper_app_queue_selection_get_item_type (GListModel *model)
{
  return CLAPPER_TYPE_MEDIA_ITEM;
}

static guint
clapper_app_queue_selection_get_n_items (GListModel *model)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (model);

  return (self->queue) ? clapper_queue_get_n_items (self->queue) : 0;
}

static gpointer
clapper_app_queue_selection_get_item (GListModel *model, guint index)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (model);

  return (self->queue) ? clapper_queue_get_item (self->queue, index) : NULL;
}

static void
_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = clapper_app_queue_selection_get_item_type;
  iface->get_n_items = clapper_app_queue_selection_get_n_items;
  iface->get_item = clapper_app_queue_selection_get_item;
}

static inline void
_refresh_current_selection (ClapperAppQueueSelection *self)
{
  guint position, old_position, index, n_changed;

  position = clapper_queue_get_current_index (self->queue);

  /* Clapper -> GTK expected value change.
   * Should be the same, but better be safe. */
  if (position == CLAPPER_QUEUE_INVALID_POSITION)
    position = GTK_INVALID_LIST_POSITION;

  /* No change */
  if (position == self->current_position)
    return;

  old_position = self->current_position;
  self->current_position = position;

  if (old_position == GTK_INVALID_LIST_POSITION) {
    index = position;
    n_changed = 1;
  } else if (position == GTK_INVALID_LIST_POSITION) {
    index = old_position;
    n_changed = 1;
  } else if (position < old_position) {
    index = position;
    n_changed = old_position - position + 1;
  } else {
    index = old_position;
    n_changed = position - old_position + 1;
  }

  GST_DEBUG ("Selection changed, index: %u, n_changed: %u", index, n_changed);
  gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), index, n_changed);
}

static gboolean
clapper_app_queue_selection_is_selected (GtkSelectionModel *model, guint position)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (model);

  return (position == self->current_position);
}

static GtkBitset *
clapper_app_queue_selection_get_selection_in_range (GtkSelectionModel *model, guint position, guint n_items)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (model);
  GtkBitset *bitset = gtk_bitset_new_empty ();

  if (self->current_position != GTK_INVALID_LIST_POSITION
      && position <= self->current_position
      && position + n_items > self->current_position)
    gtk_bitset_add (bitset, self->current_position);

  return bitset;
}

static gboolean
clapper_app_queue_selection_select_item (GtkSelectionModel *model, guint position, gboolean exclusive)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (model);
  gboolean res = TRUE;

  if (G_UNLIKELY (self->queue == NULL))
    return FALSE;

  /* Disallow reselecting of the same item */
  if (self->current_position != position)
    res = clapper_queue_select_index (self->queue, position);

  /* Need to always emit this signal when select item succeeds */
  if (G_LIKELY (res))
    g_signal_emit (self, signals[SIGNAL_ITEM_SELECTED], 0, position);

  return res;
}

static gboolean
clapper_app_queue_selection_unselect_item (GtkSelectionModel *model, guint position)
{
  return FALSE;
}

static void
_selection_model_iface_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = clapper_app_queue_selection_is_selected;
  iface->get_selection_in_range = clapper_app_queue_selection_get_selection_in_range;
  iface->select_item = clapper_app_queue_selection_select_item;
  iface->unselect_item = clapper_app_queue_selection_unselect_item;
}

#define parent_class clapper_app_queue_selection_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperAppQueueSelection, clapper_app_queue_selection, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, _list_model_iface_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL, _selection_model_iface_init))

static void
_queue_model_items_changed_cb (GListModel *model, guint position, guint removed, guint added,
    ClapperAppQueueSelection *self)
{
  /* Forward event from internal model */
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
_queue_current_index_changed_cb (ClapperQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppQueueSelection *self)
{
  _refresh_current_selection (self);
}

/*
 * clapper_app_queue_selection_new:
 * @queue: (nullable): a #ClapperQueue
 *
 * Creates a new #ClapperAppQueueSelection instance.
 *
 * Returns: (transfer full): a new #ClapperAppQueueSelection.
 */
ClapperAppQueueSelection *
clapper_app_queue_selection_new (ClapperQueue *queue)
{
  return g_object_new (CLAPPER_APP_TYPE_QUEUE_SELECTION, "queue", queue, NULL);
}

/*
 * clapper_app_queue_selection_set_queue:
 * @selection: a #ClapperAppQueueSelection
 * @queue: a #ClapperQueue
 *
 * Set #ClapperQueue to be managed by this selection model.
 */
void
clapper_app_queue_selection_set_queue (ClapperAppQueueSelection *self, ClapperQueue *queue)
{
  guint n_before = 0, n_after = 0;

  g_return_if_fail (CLAPPER_APP_IS_QUEUE_SELECTION (self));
  g_return_if_fail (CLAPPER_IS_QUEUE (queue));

  if (self->queue) {
    g_signal_handlers_disconnect_by_func (G_LIST_MODEL (self->queue), _queue_model_items_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->queue, _queue_current_index_changed_cb, self);

    n_before = clapper_queue_get_n_items (self->queue);
  }

  gst_object_replace ((GstObject **) &self->queue, GST_OBJECT_CAST (queue));

  g_signal_connect (G_LIST_MODEL (self->queue), "items-changed",
      G_CALLBACK (_queue_model_items_changed_cb), self);
  g_signal_connect (self->queue, "notify::current-index",
      G_CALLBACK (_queue_current_index_changed_cb), self);

  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_QUEUE]);

  n_after = clapper_queue_get_n_items (self->queue);

  /* Refresh selected item after queue change */
  self->current_position = GTK_INVALID_LIST_POSITION;
  _queue_model_items_changed_cb (G_LIST_MODEL (self->queue), 0, n_before, n_after, self);
  _refresh_current_selection (self);
}

/*
 * clapper_app_queue_selection_get_queue:
 * @selection: a #ClapperAppQueueSelection
 *
 * Get #ClapperQueue managed by this selection model.
 *
 * Returns: (transfer none): #ClapperQueue being managed.
 */
ClapperQueue *
clapper_app_queue_selection_get_queue (ClapperAppQueueSelection *self)
{
  g_return_val_if_fail (CLAPPER_APP_IS_QUEUE_SELECTION (self), NULL);

  return self->queue;
}

static void
clapper_app_queue_selection_init (ClapperAppQueueSelection *self)
{
  self->current_position = GTK_INVALID_LIST_POSITION;
}

static void
clapper_app_queue_selection_finalize (GObject *object)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (object);

  if (self->queue) {
    g_signal_handlers_disconnect_by_func (G_LIST_MODEL (self->queue), _queue_model_items_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->queue, _queue_current_index_changed_cb, self);

    g_object_unref (self->queue);
  }
  g_clear_object (&self->current_item);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_queue_selection_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (object);

  switch (prop_id) {
    case PROP_QUEUE:
      g_value_set_object (value, clapper_app_queue_selection_get_queue (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_queue_selection_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperAppQueueSelection *self = CLAPPER_APP_QUEUE_SELECTION_CAST (object);

  switch (prop_id) {
    case PROP_QUEUE:
      clapper_app_queue_selection_set_queue (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_queue_selection_class_init (ClapperAppQueueSelectionClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappqueueselection", 0,
      "Clapper App Queue Selection");

  gobject_class->get_property = clapper_app_queue_selection_get_property;
  gobject_class->set_property = clapper_app_queue_selection_set_property;
  gobject_class->finalize = clapper_app_queue_selection_finalize;

  /*
   * ClapperAppQueueSelection:queue:
   *
   * The queue being managed.
   */
  param_specs[PROP_QUEUE] = g_param_spec_object ("queue",
      NULL, NULL, CLAPPER_TYPE_QUEUE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /*
   * ClapperAppQueueSelection::item-selected:
   * @selection: a #ClapperAppQueueSelection
   * @index: an index of selected item
   *
   * Signals when user selected item within the [iface@Gtk.SelectionModel].
   *
   * Note that this signal is emitted only when item gets selected from
   * the GTK side (also when the same item is reselected). If item was
   * changed internally by e.g. progression of [class@Clapper.Queue],
   * this signal will not be emitted.
   *
   * #ClapperAppQueueSelection automatically takes care of having its
   * selection in sync with passed [class@Clapper.Queue], so you do not
   * have to listen for the changes. This signal is useful if you need
   * to differentiate what caused item selection, otherwise use either
   * [signal@Gtk.SelectionModel::selection-changed] signal or listen for
   * changes of [property@Clapper.Queue:current-item] property.
   */
  signals[SIGNAL_ITEM_SELECTED] = g_signal_new ("item-selected",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
