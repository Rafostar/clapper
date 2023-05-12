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
 * ClapperStreamList:
 *
 * A list of media streams.
 */

#include <gio/gio.h>

#include "clapper-stream-list-private.h"
#include "clapper-stream-private.h"
#include "clapper-player-private.h"
#include "clapper-playbin-bus-private.h"

#define GST_CAT_DEFAULT clapper_stream_list_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _ClapperStreamListPrivate ClapperStreamListPrivate;

struct _ClapperStreamListPrivate
{
  GPtrArray *streams;

  ClapperStream *active_stream;

  ClapperStream *current_stream;
  guint current_index;
};

enum
{
  PROP_0,
  PROP_CURRENT_STREAM,
  PROP_CURRENT_INDEX,
  PROP_LAST
};

static void clapper_stream_list_model_iface_init (GListModelInterface *iface);

#define parent_class clapper_stream_list_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperStreamList, clapper_stream_list, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (ClapperStreamList)
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, clapper_stream_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GType
clapper_stream_list_model_get_item_type (GListModel *model)
{
  return CLAPPER_TYPE_STREAM;
}

static guint
clapper_stream_list_model_get_n_items (GListModel *model)
{
  ClapperStreamList *self = CLAPPER_STREAM_LIST_CAST (model);
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  guint n_streams;

  GST_OBJECT_LOCK (self);
  n_streams = priv->streams->len;
  GST_OBJECT_UNLOCK (self);

  return n_streams;
}

static gpointer
clapper_stream_list_model_get_item (GListModel *model, guint index)
{
  ClapperStreamList *self = CLAPPER_STREAM_LIST_CAST (model);
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  ClapperStream *stream = NULL;

  GST_OBJECT_LOCK (self);
  if (G_LIKELY (index < priv->streams->len))
    stream = gst_object_ref (g_ptr_array_index (priv->streams, index));
  GST_OBJECT_UNLOCK (self);

  return stream;
}

static void
clapper_stream_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = clapper_stream_list_model_get_item_type;
  iface->get_n_items = clapper_stream_list_model_get_n_items;
  iface->get_item = clapper_stream_list_model_get_item;
}

static void
_post_stream_change (ClapperStreamList *self)
{
  ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));
  ClapperMediaItem *item = CLAPPER_MEDIA_ITEM_CAST (gst_object_get_parent (GST_OBJECT_CAST (self)));

  if (player && item)
    clapper_playbin_bus_post_stream_change (player->bus, item);

  gst_clear_object (&item);
  gst_clear_object (&player);
}

static void
_announce_current_stream_and_index_change (ClapperStreamList *self)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  ClapperPlayer *player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self));
  gboolean is_main_thread;

  if (G_UNLIKELY (player == NULL))
    return;

  is_main_thread = g_main_context_is_owner (g_main_context_default ());

  GST_DEBUG_OBJECT (self, "Announcing current stream change from %smain thread,"
      " now: %" GST_PTR_FORMAT " (index: %u)",
      (is_main_thread) ? "" : "non-", priv->current_stream, priv->current_index);

  if (is_main_thread) {
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_STREAM]);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_INDEX]);
  } else {
    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_STREAM]);
    clapper_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);
  }

  gst_object_unref (player);
}

static gboolean
clapper_stream_list_select_index_unlocked (ClapperStreamList *self, guint index)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  ClapperStream *stream = NULL;

  if (index != CLAPPER_STREAM_LIST_INVALID_POSITION)
    stream = g_ptr_array_index (priv->streams, index);

  if (gst_object_replace ((GstObject **) &priv->current_stream, GST_OBJECT_CAST (stream))) {
    priv->current_index = index;
    return TRUE;
  }

  return FALSE;
}

/*
 * clapper_stream_list_new:
 *
 * Returns: (transfer full): a new #ClapperStreamList instance
 */
ClapperStreamList *
clapper_stream_list_new (void)
{
  ClapperStreamList *list;

  list = g_object_new (CLAPPER_TYPE_STREAM_LIST, NULL);
  gst_object_ref_sink (list);

  return list;
}

/**
 * clapper_stream_list_select_stream:
 * @list: a #ClapperStreamList
 * @stream: a #ClapperStream
 *
 * Selects #ClapperStream from @list to be activated.
 *
 * Returns: %TRUE if stream was in the @list, %FALSE otherwise.
 */
gboolean
clapper_stream_list_select_stream (ClapperStreamList *self, ClapperStream *stream)
{
  ClapperStreamListPrivate *priv;
  gboolean found, changed = FALSE;
  guint index = 0;

  g_return_val_if_fail (CLAPPER_IS_STREAM_LIST (self), FALSE);
  g_return_val_if_fail (CLAPPER_IS_STREAM (stream), FALSE);

  priv = clapper_stream_list_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  if ((found = g_ptr_array_find (priv->streams, stream, &index)))
    changed = clapper_stream_list_select_index_unlocked (self, index);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    _post_stream_change (self);
    _announce_current_stream_and_index_change (self);
  }

  return found;
}

/**
 * clapper_stream_list_select_index:
 * @list: a #ClapperStreamList
 * @index: a stream index
 *
 * Selects #ClapperStream at @index from @list as current one.
 *
 * Returns: %TRUE if stream could be selected, %FALSE otherwise.
 */
gboolean
clapper_stream_list_select_index (ClapperStreamList *self, guint index)
{
  ClapperStreamListPrivate *priv;
  gboolean found, changed = FALSE;

  g_return_val_if_fail (CLAPPER_IS_STREAM_LIST (self), FALSE);
  g_return_val_if_fail (index != CLAPPER_STREAM_LIST_INVALID_POSITION, FALSE);

  priv = clapper_stream_list_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  if ((found = index < priv->streams->len))
    changed = clapper_stream_list_select_index_unlocked (self, index);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    _post_stream_change (self);
    _announce_current_stream_and_index_change (self);
  }

  return found;
}

/**
 * clapper_stream_list_get_stream:
 * @list: a #ClapperStreamList
 * @index: a stream index
 *
 * Get the #ClapperStream at index.
 *
 * This behaves the same as g_list_model_get_item(), and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: (transfer full): The #ClapperStream at @index.
 */
ClapperStream *
clapper_stream_list_get_stream (ClapperStreamList *self, guint index)
{
  g_return_val_if_fail (CLAPPER_IS_STREAM_LIST (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * clapper_stream_list_get_current_stream:
 * @list: a #ClapperStreamList
 *
 * Get the currently selected #ClapperStream.
 *
 * Returns: (transfer full) (nullable): The current #ClapperStream.
 */
ClapperStream *
clapper_stream_list_get_current_stream (ClapperStreamList *self)
{
  ClapperStreamListPrivate *priv;
  ClapperStream *stream = NULL;

  g_return_val_if_fail (CLAPPER_IS_STREAM_LIST (self), NULL);

  priv = clapper_stream_list_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  if (priv->current_stream)
    stream = gst_object_ref (priv->current_stream);
  GST_OBJECT_UNLOCK (self);

  return stream;
}

/**
 * clapper_stream_list_get_current_index:
 * @list: a #ClapperStreamList
 *
 * Get index of the currently selected #ClapperStream.
 *
 * Returns: Current stream index or #CLAPPER_STREAM_LIST_INVALID_POSITION
 *   when nothing is selected.
 */
guint
clapper_stream_list_get_current_index (ClapperStreamList *self)
{
  ClapperStreamListPrivate *priv;
  guint index;

  g_return_val_if_fail (CLAPPER_IS_STREAM_LIST (self), CLAPPER_STREAM_LIST_INVALID_POSITION);

  priv = clapper_stream_list_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  index = priv->current_index;
  GST_OBJECT_UNLOCK (self);

  return index;
}

/**
 * clapper_stream_list_get_n_streams:
 * @list: a #ClapperStreamList
 *
 * Get the number of streams in #ClapperStreamList.
 *
 * This behaves the same as g_list_model_get_n_items(), and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: The number of streams in #ClapperStreamList.
 */
guint
clapper_stream_list_get_n_streams (ClapperStreamList *self)
{
  g_return_val_if_fail (CLAPPER_IS_STREAM_LIST (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

void
clapper_stream_list_add_stream (ClapperStreamList *self, ClapperStream *stream)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);

  g_ptr_array_add (priv->streams, stream);
}

gboolean
clapper_stream_list_handle_gst_stream_selected (ClapperStreamList *self, GstStream *gst_stream)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  gboolean changed = FALSE;
  guint i;

  GST_OBJECT_LOCK (self);

  for (i = 0; i < priv->streams->len; ++i) {
    ClapperStream *stream = g_ptr_array_index (priv->streams, i);
    GstStream *list_gst_stream = clapper_stream_get_stream (stream);

    if (gst_stream == list_gst_stream) {
      gst_object_replace ((GstObject **) &priv->active_stream, GST_OBJECT_CAST (stream));
      changed = clapper_stream_list_select_index_unlocked (self, i);
      break;
    }
  }

  GST_OBJECT_UNLOCK (self);

  if (changed)
    _announce_current_stream_and_index_change (self);

  return changed;
}

void
clapper_stream_list_handle_stream_change_failed (ClapperStreamList *self)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  gboolean restored = FALSE;
  guint i;

  GST_OBJECT_LOCK (self);

  if (!priv->active_stream || priv->active_stream == priv->current_stream) {
    GST_OBJECT_UNLOCK (self);
    return;
  }

  for (i = 0; i < priv->streams->len; ++i) {
    ClapperStream *stream = g_ptr_array_index (priv->streams, i);

    if (stream == priv->active_stream) {
      restored = clapper_stream_list_select_index_unlocked (self, i);
      break;
    }
  }

  GST_OBJECT_UNLOCK (self);

  if (restored)
    _announce_current_stream_and_index_change (self);
}

void
clapper_stream_list_clear (ClapperStreamList *self)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);
  gboolean changed = FALSE;

  GST_OBJECT_LOCK (self);
  if (priv->streams->len > 0) {
    changed = clapper_stream_list_select_index_unlocked (self, CLAPPER_STREAM_LIST_INVALID_POSITION);
    gst_clear_object (&priv->active_stream);
    g_ptr_array_remove_range (priv->streams, 0, priv->streams->len);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    _announce_current_stream_and_index_change (self);
}

static void
_stream_remove_func (ClapperStream *stream)
{
  gst_object_unparent (GST_OBJECT (stream));
  gst_object_unref (stream);
}

static void
clapper_stream_list_init (ClapperStreamList *self)
{
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);

  priv->streams = g_ptr_array_new_with_free_func ((GDestroyNotify) _stream_remove_func);
  priv->current_index = CLAPPER_STREAM_LIST_INVALID_POSITION;
}

static void
clapper_stream_list_finalize (GObject *object)
{
  ClapperStreamList *self = CLAPPER_STREAM_LIST_CAST (object);
  ClapperStreamListPrivate *priv = clapper_stream_list_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&priv->active_stream);
  gst_clear_object (&priv->current_stream);
  g_ptr_array_unref (priv->streams);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_stream_list_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperStreamList *self = CLAPPER_STREAM_LIST_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_STREAM:
      g_value_take_object (value, clapper_stream_list_get_current_stream (self));
      break;
    case PROP_CURRENT_INDEX:
      g_value_set_uint (value, clapper_stream_list_get_current_index (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_stream_list_class_init (ClapperStreamListClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperstreamlist", 0,
      "Clapper Stream List");

  gobject_class->get_property = clapper_stream_list_get_property;
  gobject_class->finalize = clapper_stream_list_finalize;

  /**
   * ClapperStreamList:current-stream:
   *
   * Currently selected stream.
   */
  param_specs[PROP_CURRENT_STREAM] = g_param_spec_object ("current-stream",
      NULL, NULL, CLAPPER_TYPE_STREAM,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperStreamList:current-index:
   *
   * Index of currently selected stream.
   */
  param_specs[PROP_CURRENT_INDEX] = g_param_spec_uint ("current-index",
      NULL, NULL, 0, G_MAXUINT, CLAPPER_STREAM_LIST_INVALID_POSITION,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
