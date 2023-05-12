/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * SECTION:clapper-media-item
 * @title: ClapperMediaItem
 * @short_description: represents a media item
 */

#include "clapper-media-item.h"
#include "clapper-media-item-private.h"
#include "clapper-player-private.h"
#include "clapper-features-manager-private.h"
#include "clapper-utils-private.h"

#define GST_CAT_DEFAULT clapper_media_item_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_URI,
  PROP_SUBURIS,
  PROP_TITLE,
  PROP_CONTAINER_FORMAT,
  PROP_DURATION,
  PROP_LAST
};

#define parent_class clapper_media_item_parent_class
G_DEFINE_TYPE (ClapperMediaItem, clapper_media_item, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static inline void
_announce_updated (ClapperMediaItem *self)
{
  ClapperPlayer *player;

  if ((player = clapper_player_get_from_ancestor (GST_OBJECT_CAST (self)))) {
    if (clapper_player_get_have_features (player))
      clapper_features_manager_trigger_media_item_updated (player->features_manager, self);

    gst_object_unref (player);
  }
}

static gchar *
_get_title_from_uri (const gchar *uri)
{
  gchar *proto = gst_uri_get_protocol (uri);
  gchar *title = NULL;

  if (G_UNLIKELY (proto == NULL))
    return NULL;

  if (strcmp (proto, "file") == 0) {
    const gchar *ext = strrchr (uri, '.');

    if (ext && strlen (ext) < 8) {
      gchar *filename = g_filename_from_uri (uri, NULL, NULL);

      if (filename) {
        gchar *base = g_path_get_basename (filename);

        title = g_strndup (base, strlen (base) - strlen (ext));

        g_free (filename);
        g_free (base);
      }
    }
  } else if (strcmp (proto, "dvb") == 0) {
    const gchar *channel = strrchr (uri, '/') + 1;
    title = g_strdup (channel);
  }

  g_free (proto);

  return (title != NULL) ? title : g_strdup (uri);
}

static gboolean _list_has_tag (const GstTagList *tags, const gchar *tag)
{
  gint i, n_tags = gst_tag_list_n_tags (tags);
  gboolean found = FALSE;

  for (i = 0; i < n_tags; ++i) {
    if ((found = (strcmp (gst_tag_list_nth_tag_name (tags, i), tag) == 0)))
      break;
  }

  return found;
}

void
clapper_media_item_update_from_container_tags (ClapperMediaItem *self, const GstTagList *tags,
    ClapperAppBus *app_bus)
{
  gchar *string = NULL;

  if (gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &string))
    clapper_media_item_take_container_format (self, string, app_bus);
  if (gst_tag_list_get_string (tags, GST_TAG_TITLE, &string))
    clapper_media_item_take_title (self, string, app_bus);

  _announce_updated (self);
}

void
clapper_media_item_update_from_tag_list (ClapperMediaItem *self, const GstTagList *tags,
    ClapperAppBus *app_bus)
{
  GstTagScope scope = gst_tag_list_get_scope (tags);

  switch (scope) {
    case GST_TAG_SCOPE_GLOBAL:
      if (_list_has_tag (tags, GST_TAG_CONTAINER_FORMAT))
        clapper_media_item_update_from_container_tags (self, tags, app_bus);
      break;
    case GST_TAG_SCOPE_STREAM:
      /* TODO */
      GST_FIXME_OBJECT (self, "Handle stream scope tags");
      break;
    default:
      break;
  }
}

/**
 * clapper_media_item_new:
 * @uri: a media URI
 *
 * Creates new #ClapperMediaItem from URI.
 *
 * Use one of the URI protocols supported by plugins in #GStreamer
 * installation. For local files you can use either "file" protocol
 * or clapper_media_item_new_from_file() method.
 *
 * This method can only fail when no URI is provided. It is considered
 * a programmer error trying to create new media item from invalid URI.
 * If URI is valid, but unsupported by user installed plugins, #ClapperPlayer
 * will emit a #ClapperPlayer::missing-plugin signal upon playback.
 *
 * Returns: (transfer full) (nullable): a new #ClapperMediaItem,
 *   %NULL when no URI provided.
 */
ClapperMediaItem *
clapper_media_item_new (const gchar *uri)
{
  return clapper_media_item_new_take (g_strdup (uri));
}

/**
 * clapper_media_item_new_take: (skip)
 * @uri: (transfer full): a media URI
 *
 * Creates new #ClapperMediaItem from URI.
 *
 * Same as clapper_media_item_new(), but takes ownership of passed URI.
 *
 * Returns: (transfer full) (nullable): a new #ClapperMediaItem,
 *   %NULL when no URI provided.
 */
ClapperMediaItem *
clapper_media_item_new_take (gchar *uri)
{
  ClapperMediaItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  if (G_UNLIKELY (uri == NULL))
    return NULL;

  item = g_object_new (CLAPPER_TYPE_MEDIA_ITEM, NULL);
  item->uri = uri;
  item->title = _get_title_from_uri (uri);

  gst_object_ref_sink (item);

  /* FIXME: Set initial container format from file extension parsing */

  GST_TRACE_OBJECT (item, "New media item, URI: %s, title: %s",
      item->uri, item->title);

  return item;
}

/**
 * clapper_media_item_new_from_file:
 * @file: a #GFile
 *
 * Creates new #ClapperMediaItem from #GFile.
 *
 * Same as clapper_media_item_new(), but uses a #GFile for convenience
 * in some situations instead of an URI.
 *
 * Returns: (transfer full) (nullable): a new #ClapperMediaItem,
 *   %NULL when no #GFile provided.
 */
ClapperMediaItem *
clapper_media_item_new_from_file (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return clapper_media_item_new_take (g_file_get_uri (file));
}

/**
 * clapper_media_item_get_uri:
 * @item: a #ClapperMediaItem
 *
 * Get the URI of #ClapperMediaItem.
 *
 * Returns: an URI of #ClapperMediaItem.
 */
const gchar *
clapper_media_item_get_uri (ClapperMediaItem *self)
{
  g_return_val_if_fail (CLAPPER_IS_MEDIA_ITEM (self), NULL);

  return self->uri;
}

void
clapper_media_item_append_suburi (ClapperMediaItem *self, const gchar *suburi)
{
  gchar **tmp_suburis;
  guint i = 0, count = 0;

  if (!suburi)
    return;

  GST_OBJECT_LOCK (self);

  if (self->suburis)
    count = g_strv_length (self->suburis);

  /* Reserve space for current suburis copy,
   * one addition and NULL termination */
  tmp_suburis = g_new (gchar *, count + 2);

  if (self->suburis) {
    for (i = 0; self->suburis[i]; i++)
      tmp_suburis[i] = g_strdup (self->suburis[i]);
  }
  self->suburis[i] = g_strdup (suburi);
  self->suburis[i + 1] = NULL;

  self->suburis = tmp_suburis;

  GST_OBJECT_UNLOCK (self);
}

void
clapper_media_item_set_suburis (ClapperMediaItem *self, const gchar **suburis)
{
  GST_OBJECT_LOCK (self);
  g_strfreev (self->suburis);
  self->suburis = g_strdupv ((gchar **) suburis);
  GST_OBJECT_UNLOCK (self);
}

gchar **
clapper_media_item_get_suburis (ClapperMediaItem *self)
{
  gchar **suburis;

  GST_OBJECT_LOCK (self);
  suburis = g_strdupv (self->suburis);
  GST_OBJECT_UNLOCK (self);

  return suburis;
}

void
clapper_media_item_take_title (ClapperMediaItem *self, gchar *title,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = clapper_utils_replace_string (&self->title, title);
  GST_OBJECT_UNLOCK (self);

  if (changed)
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_TITLE]);
}

/**
 * clapper_media_item_get_title:
 * @item: a #ClapperMediaItem
 *
 * Get media item title for displaying in app UI. This function
 * always returns a media title of some sort for convenience
 * of displaying it in the application UI.
 *
 * The title can be either text detected by media discovery once it
 * completes, file basename for local files or eventually a media URI.
 *
 * Returns: (transfer full): media title.
 */
gchar *
clapper_media_item_get_title (ClapperMediaItem *self)
{
  gchar *title;

  GST_OBJECT_LOCK (self);
  title = g_strdup (self->title);
  GST_OBJECT_UNLOCK (self);

  return title;
}

void
clapper_media_item_take_container_format (ClapperMediaItem *self, gchar *container_format,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = clapper_utils_replace_string (&self->container_format, container_format);
  GST_OBJECT_UNLOCK (self);

  if (changed)
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_CONTAINER_FORMAT]);
}

/**
 * clapper_media_item_get_container_format:
 * @item: a #ClapperMediaItem
 *
 * Get media item container format.
 *
 * Returns: (transfer full) (nullable): media container format.
 */
gchar *
clapper_media_item_get_container_format (ClapperMediaItem *self)
{
  gchar *container_format;

  GST_OBJECT_LOCK (self);
  container_format = g_strdup (self->container_format);
  GST_OBJECT_UNLOCK (self);

  return container_format;
}

void
clapper_media_item_set_duration (ClapperMediaItem *self, gfloat duration,
    ClapperAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = FLT_IS_DIFFERENT (self->duration, duration)))
    self->duration = duration;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_DEBUG_OBJECT (self, "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (duration * GST_SECOND));
    _announce_updated (self);
    clapper_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_DURATION]);
  }
}

/**
 * clapper_media_item_get_duration:
 * @item: a #ClapperMediaItem
 *
 * Get media item duration as decimal number in seconds.
 *
 * Returns: media duration.
 */
gfloat
clapper_media_item_get_duration (ClapperMediaItem *self)
{
  gfloat duration;

  GST_OBJECT_LOCK (self);
  duration = self->duration;
  GST_OBJECT_UNLOCK (self);

  return duration;
}

static void
clapper_media_item_init (ClapperMediaItem *self)
{
}

static void
clapper_media_item_constructed (GObject *object)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  if (self->uri)
    self->title = _get_title_from_uri (self->uri);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
clapper_media_item_finalize (GObject *object)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->uri);
  g_strfreev (self->suburis);
  g_free (self->title);
  g_free (self->container_format);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_media_item_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;
    case PROP_SUBURIS:
      clapper_media_item_set_suburis (self, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_media_item_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperMediaItem *self = CLAPPER_MEDIA_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, clapper_media_item_get_uri (self));
      break;
    case PROP_SUBURIS:
      g_value_take_boxed (value, clapper_media_item_get_suburis (self));
      break;
    case PROP_TITLE:
      g_value_take_string (value, clapper_media_item_get_title (self));
      break;
    case PROP_CONTAINER_FORMAT:
      g_value_take_string (value, clapper_media_item_get_container_format (self));
      break;
    case PROP_DURATION:
      g_value_set_float (value, clapper_media_item_get_duration (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_media_item_class_init (ClapperMediaItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappermediaitem", 0,
      "Clapper Media Item");

  gobject_class->constructed = clapper_media_item_constructed;
  gobject_class->set_property = clapper_media_item_set_property;
  gobject_class->get_property = clapper_media_item_get_property;
  gobject_class->finalize = clapper_media_item_finalize;

  /**
   * ClapperMediaItem:uri:
   *
   * Media URI.
   */
  param_specs[PROP_URI] = g_param_spec_string ("uri",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:suburis:
   *
   * External Subtitle URIs.
   */
  param_specs[PROP_SUBURIS] = g_param_spec_boxed ("suburis",
      NULL, NULL, G_TYPE_STRV,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:title:
   *
   * Media title.
   */
  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:container-format:
   *
   * Media container format.
   */
  param_specs[PROP_CONTAINER_FORMAT] = g_param_spec_string ("container-format",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperMediaItem:duration:
   *
   * Media duration as a decimal number in seconds.
   */
  param_specs[PROP_DURATION] = g_param_spec_float ("duration",
      NULL, NULL, 0, G_MAXFLOAT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
