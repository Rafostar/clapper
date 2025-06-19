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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "clapper-utils-private.h"
#include "clapper-timeline-private.h"
#include "../shared/clapper-shared-utils-private.h"

#define GST_CAT_DEFAULT clapper_utils_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef enum
{
  CLAPPER_UTILS_LIST_ALTER_QUEUE_APPEND = 1,
  CLAPPER_UTILS_LIST_ALTER_QUEUE_INSERT,
  CLAPPER_UTILS_LIST_ALTER_QUEUE_REMOVE,
  CLAPPER_UTILS_LIST_ALTER_QUEUE_CLEAR,
  CLAPPER_UTILS_LIST_ALTER_TIMELINE_INSERT,
  CLAPPER_UTILS_LIST_ALTER_TIMELINE_REMOVE
} ClapperUtilsListAlterMethod;

typedef struct
{
  GListModel *list;
  GObject *item;
  GObject *after_item;
  ClapperUtilsListAlterMethod method;
} ClapperUtilsListAlterData;

typedef struct
{
  GObject *object;
  GParamSpec *pspec;
} ClapperUtilsPropNotifyData;

void
clapper_utils_initialize (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperutils", 0,
      "Clapper Utilities");
}

static ClapperUtilsListAlterData *
clapper_utils_list_alter_data_new (GListModel *list, GObject *item,
    GObject *after_item, ClapperUtilsListAlterMethod method)
{
  ClapperUtilsListAlterData *data = g_new (ClapperUtilsListAlterData, 1);

  data->list = list;
  data->item = item;
  data->after_item = after_item;
  data->method = method;

  GST_TRACE ("Created list alter data: %p", data);

  return data;
}

static void
clapper_utils_list_alter_data_free (ClapperUtilsListAlterData *data)
{
  GST_TRACE ("Freeing list alter data: %p", data);

  g_free (data);
}

static ClapperUtilsPropNotifyData *
clapper_utils_prop_notify_data_new (GObject *object, GParamSpec *pspec)
{
  ClapperUtilsPropNotifyData *data = g_new (ClapperUtilsPropNotifyData, 1);

  data->object = object;
  data->pspec = pspec;

  GST_TRACE ("Created prop notify data: %p", data);

  return data;
}

static void
clapper_utils_prop_notify_data_free (ClapperUtilsPropNotifyData *data)
{
  GST_TRACE ("Freeing prop notify data: %p", data);

  g_free (data);
}

static gpointer
clapper_utils_list_alter_on_main (ClapperUtilsListAlterData *data)
{
  GST_DEBUG ("Queue alter invoked");

  switch (data->method) {
    case CLAPPER_UTILS_LIST_ALTER_QUEUE_APPEND:
      clapper_queue_add_item (CLAPPER_QUEUE_CAST (data->list),
          CLAPPER_MEDIA_ITEM_CAST (data->item));
      break;
    case CLAPPER_UTILS_LIST_ALTER_QUEUE_INSERT:{
      guint index;

      /* If we have "after_item" then we need to insert after it, otherwise prepend */
      if (data->after_item) {
        if (clapper_queue_find_item (CLAPPER_QUEUE_CAST (data->list),
              CLAPPER_MEDIA_ITEM_CAST (data->after_item), &index)) {
          index++;
        } else {
          index = -1; // if not found, just append at the end
        }
      } else {
        index = 0;
      }

      clapper_queue_insert_item (CLAPPER_QUEUE_CAST (data->list),
          CLAPPER_MEDIA_ITEM_CAST (data->item), index);
      break;
    }
    case CLAPPER_UTILS_LIST_ALTER_QUEUE_REMOVE:
      clapper_queue_remove_item (CLAPPER_QUEUE_CAST (data->list),
          CLAPPER_MEDIA_ITEM_CAST (data->item));
      break;
    case CLAPPER_UTILS_LIST_ALTER_QUEUE_CLEAR:
      clapper_queue_clear (CLAPPER_QUEUE_CAST (data->list));
      break;
    case CLAPPER_UTILS_LIST_ALTER_TIMELINE_INSERT:
      clapper_timeline_insert_marker_internal (CLAPPER_TIMELINE_CAST (data->list),
          CLAPPER_MARKER_CAST (data->item));
      break;
    case CLAPPER_UTILS_LIST_ALTER_TIMELINE_REMOVE:
      clapper_timeline_remove_marker_internal (CLAPPER_TIMELINE_CAST (data->list),
          CLAPPER_MARKER_CAST (data->item));
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return NULL;
}

static gpointer
clapper_utils_prop_notify_on_main (ClapperUtilsPropNotifyData *data)
{
  GST_DEBUG ("Prop notify invoked");
  g_object_notify_by_pspec (data->object, data->pspec);

  return NULL;
}

static inline void
clapper_utils_list_alter_invoke_on_main_sync_take (ClapperUtilsListAlterData *data)
{
  GST_DEBUG ("Invoking queue alter on main...");

  clapper_shared_utils_context_invoke_sync_full (g_main_context_default (),
      (GThreadFunc) clapper_utils_list_alter_on_main, data,
      (GDestroyNotify) clapper_utils_list_alter_data_free);

  GST_DEBUG ("Queue alter invoke finished");
}

void
clapper_utils_queue_append_on_main_sync (ClapperQueue *queue, ClapperMediaItem *item)
{
  ClapperUtilsListAlterData *data = clapper_utils_list_alter_data_new (
      (GListModel *) queue, (GObject *) item, NULL,
      CLAPPER_UTILS_LIST_ALTER_QUEUE_APPEND);
  clapper_utils_list_alter_invoke_on_main_sync_take (data);
}

void
clapper_utils_queue_insert_on_main_sync (ClapperQueue *queue,
    ClapperMediaItem *item, ClapperMediaItem *after_item)
{
  ClapperUtilsListAlterData *data = clapper_utils_list_alter_data_new (
      (GListModel *) queue, (GObject *) item, (GObject *) after_item,
      CLAPPER_UTILS_LIST_ALTER_QUEUE_INSERT);
  clapper_utils_list_alter_invoke_on_main_sync_take (data);
}

void
clapper_utils_queue_remove_on_main_sync (ClapperQueue *queue, ClapperMediaItem *item)
{
  ClapperUtilsListAlterData *data = clapper_utils_list_alter_data_new (
      (GListModel *) queue, (GObject *) item, NULL,
      CLAPPER_UTILS_LIST_ALTER_QUEUE_REMOVE);
  clapper_utils_list_alter_invoke_on_main_sync_take (data);
}

void
clapper_utils_queue_clear_on_main_sync (ClapperQueue *queue)
{
  ClapperUtilsListAlterData *data = clapper_utils_list_alter_data_new (
      (GListModel *) queue, NULL, NULL,
      CLAPPER_UTILS_LIST_ALTER_QUEUE_CLEAR);
  clapper_utils_list_alter_invoke_on_main_sync_take (data);
}

void
clapper_utils_timeline_insert_on_main_sync (ClapperTimeline *timeline, ClapperMarker *marker)
{
  ClapperUtilsListAlterData *data = clapper_utils_list_alter_data_new (
      (GListModel *) timeline, (GObject *) marker, NULL,
      CLAPPER_UTILS_LIST_ALTER_TIMELINE_INSERT);
  clapper_utils_list_alter_invoke_on_main_sync_take (data);
}

void
clapper_utils_timeline_remove_on_main_sync (ClapperTimeline *timeline, ClapperMarker *marker)
{
  ClapperUtilsListAlterData *data = clapper_utils_list_alter_data_new (
      (GListModel *) timeline, (GObject *) marker, NULL,
      CLAPPER_UTILS_LIST_ALTER_TIMELINE_REMOVE);
  clapper_utils_list_alter_invoke_on_main_sync_take (data);
}

void
clapper_utils_prop_notify_on_main_sync (GObject *object, GParamSpec *pspec)
{
  ClapperUtilsPropNotifyData *data;

  if (g_main_context_is_owner (g_main_context_default ())) { // already in main thread
    g_object_notify_by_pspec (object, pspec);
    return;
  }

  data = clapper_utils_prop_notify_data_new (object, pspec);

  GST_DEBUG ("Invoking prop notify on main...");

  clapper_shared_utils_context_invoke_sync_full (g_main_context_default (),
      (GThreadFunc) clapper_utils_prop_notify_on_main, data,
      (GDestroyNotify) clapper_utils_prop_notify_data_free);

  GST_DEBUG ("Prop notify invoke finished");
}

gchar *
clapper_utils_uri_from_file (GFile *file)
{
  gchar *uri = g_file_get_uri (file);
  gsize length = strlen (uri);

  /* GFile might incorrectly append "/" at the end of an URI,
   * remove it to make it work with GStreamer URI handling */
  if (uri[length - 1] == '/') {
    gchar *fixed_uri;

    /* NULL terminated copy without last character */
    fixed_uri = g_new0 (gchar, length);
    memcpy (fixed_uri, uri, length - 1);

    g_free (uri);
    uri = fixed_uri;
  }

  return uri;
}

gchar *
clapper_utils_title_from_uri (const gchar *uri)
{
  gchar *proto, *title = NULL;

  proto = gst_uri_get_protocol (uri);

  if (G_UNLIKELY (proto == NULL))
    return NULL;

  if (strcmp (proto, "file") == 0) {
    gchar *filename = g_filename_from_uri (uri, NULL, NULL);

    if (filename) {
      const gchar *ext;

      title = g_path_get_basename (filename);
      ext = strrchr (title, '.');

      g_free (filename);

      if (ext && strlen (ext) <= 4) {
        gchar *tmp = g_strndup (title, strlen (title) - strlen (ext));

        g_free (title);
        title = tmp;
      }
    }
  } else if (strcmp (proto, "dvb") == 0) {
    const gchar *channel = strrchr (uri, '/') + 1;
    title = g_strdup (channel);
  }

  g_free (proto);

  return title;
}

gboolean
clapper_utils_set_value_from_variant (GValue *value, GVariant *variant)
{
  const gchar *var_type = g_variant_get_type_string (variant);
  GType val_type;

  switch (var_type[0]) {
    case 'b':
      val_type = G_TYPE_BOOLEAN;
      break;
    case 'i':
      val_type = G_TYPE_INT;
      break;
    case 'u':
      val_type = G_TYPE_UINT;
      break;
    case 'd':
      val_type = G_TYPE_DOUBLE;
      break;
    case 's':
      val_type = G_TYPE_STRING;
      break;
    default:
      goto error;
  }

  g_value_init (value, val_type);

  switch (val_type) {
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, g_variant_get_boolean (variant));
      break;
    case G_TYPE_INT:
      g_value_set_int (value, g_variant_get_int32 (variant));
      break;
    case G_TYPE_UINT:
      g_value_set_uint (value, g_variant_get_uint32 (variant));
      break;
    case G_TYPE_DOUBLE:
      g_value_set_double (value, g_variant_get_double (variant));
      break;
    case G_TYPE_STRING:
      g_value_set_string (value, g_variant_get_string (variant, NULL));
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;

error:
  GST_ERROR ("Unsupported conversion for variant type: %s", var_type);
  return FALSE;
}
