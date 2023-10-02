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

#include <glib.h>

#include "clapper-app-queue-progression-model.h"
#include "clapper-app-queue-progression-item.h"

struct _ClapperAppQueueProgressionModel
{
  GObject parent;

  GListStore *store;
};

static GType
clapper_app_queue_progression_model_get_item_type (GListModel *model)
{
  return CLAPPER_APP_TYPE_QUEUE_PROGRESSION_ITEM;
}

static guint
clapper_app_queue_progression_model_get_n_items (GListModel *model)
{
  ClapperAppQueueProgressionModel *self = CLAPPER_APP_QUEUE_PROGRESSION_MODEL_CAST (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->store));
}

static gpointer
clapper_app_queue_progression_model_get_item (GListModel *model, guint index)
{
  ClapperAppQueueProgressionModel *self = CLAPPER_APP_QUEUE_PROGRESSION_MODEL_CAST (model);

  return g_list_model_get_item (G_LIST_MODEL (self->store), index);
}

static void
_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = clapper_app_queue_progression_model_get_item_type;
  iface->get_n_items = clapper_app_queue_progression_model_get_n_items;
  iface->get_item = clapper_app_queue_progression_model_get_item;
}

#define parent_class clapper_app_queue_progression_model_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperAppQueueProgressionModel, clapper_app_queue_progression_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, _list_model_iface_init));

static void
clapper_app_queue_progression_model_init (ClapperAppQueueProgressionModel *self)
{
  self->store = g_list_store_new (CLAPPER_APP_TYPE_QUEUE_PROGRESSION_ITEM);
}

static void
clapper_app_queue_progression_model_constructed (GObject *object)
{
  ClapperAppQueueProgressionModel *self = CLAPPER_APP_QUEUE_PROGRESSION_MODEL_CAST (object);
  guint i;

  const char *const icon_names[] = {
    "media-playlist-consecutive-symbolic",
    "media-playlist-repeat-song-symbolic",
    "media-playlist-repeat-symbolic",
    "media-playlist-shuffle-symbolic",
    NULL
  };
  /* FIXME: Translations support */
  const char *const labels[] = {
    "Consecutive",
    "Repeat item",
    "Carousel",
    "Shuffle",
    NULL
  };

  for (i = 0; icon_names[i]; ++i) {
    ClapperAppQueueProgressionItem *item;

    item = clapper_app_queue_progression_item_new (icon_names[i], labels[i]);

    g_list_store_append (self->store, item);
    g_object_unref (item);
  }

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_app_queue_progression_model_finalize (GObject *object)
{
  ClapperAppQueueProgressionModel *self = CLAPPER_APP_QUEUE_PROGRESSION_MODEL_CAST (object);

  g_object_unref (self->store);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_queue_progression_model_class_init (ClapperAppQueueProgressionModelClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = clapper_app_queue_progression_model_constructed;
  gobject_class->finalize = clapper_app_queue_progression_model_finalize;
}
