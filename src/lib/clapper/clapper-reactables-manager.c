/* Clapper Playback Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>

#include "clapper-reactables-manager-private.h"
#include "clapper-reactable.h"
#include "clapper-bus-private.h"
#include "clapper-player.h"
#include "clapper-enhancer-proxy-list.h"
#include "clapper-enhancer-proxy-private.h"
#include "clapper-utils-private.h"

#include "clapper-functionalities-availability.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include "clapper-enhancers-loader-private.h"
#endif

#define CONFIG_STRUCTURE_NAME "config"

#define GST_CAT_DEFAULT clapper_reactables_manager_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperReactablesManager
{
  ClapperThreadedObject parent;

  GstBus *bus;
  GPtrArray *array;
};

#define parent_class clapper_reactables_manager_parent_class
G_DEFINE_TYPE (ClapperReactablesManager, clapper_reactables_manager, CLAPPER_TYPE_THREADED_OBJECT);

typedef struct
{
  ClapperReactable *reactable;
  ClapperEnhancerProxy *proxy;
  GSettings *settings;
} ClapperReactableManagerData;

enum
{
  CLAPPER_REACTABLES_MANAGER_EVENT_INVALID = 0,
  CLAPPER_REACTABLES_MANAGER_EVENT_STATE_CHANGED,
  CLAPPER_REACTABLES_MANAGER_EVENT_POSITION_CHANGED,
  CLAPPER_REACTABLES_MANAGER_EVENT_SPEED_CHANGED,
  CLAPPER_REACTABLES_MANAGER_EVENT_VOLUME_CHANGED,
  CLAPPER_REACTABLES_MANAGER_EVENT_MUTE_CHANGED,
  CLAPPER_REACTABLES_MANAGER_EVENT_PLAYED_ITEM_CHANGED,
  CLAPPER_REACTABLES_MANAGER_EVENT_ITEM_UPDATED,
  CLAPPER_REACTABLES_MANAGER_EVENT_QUEUE_ITEM_ADDED,
  CLAPPER_REACTABLES_MANAGER_EVENT_QUEUE_ITEM_REMOVED,
  CLAPPER_REACTABLES_MANAGER_EVENT_QUEUE_ITEM_REPOSITIONED,
  CLAPPER_REACTABLES_MANAGER_EVENT_QUEUE_CLEARED,
  CLAPPER_REACTABLES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED
};

enum
{
  CLAPPER_REACTABLES_MANAGER_QUARK_PREPARE = 0,
  CLAPPER_REACTABLES_MANAGER_QUARK_CONFIGURE,
  CLAPPER_REACTABLES_MANAGER_QUARK_EVENT,
  CLAPPER_REACTABLES_MANAGER_QUARK_VALUE,
  CLAPPER_REACTABLES_MANAGER_QUARK_EXTRA_VALUE
};

static ClapperBusQuark _quarks[] = {
  {"prepare", 0},
  {"configure", 0},
  {"event", 0},
  {"value", 0},
  {"extra-value", 0},
  {NULL, 0}
};

#define _EVENT(e) G_PASTE(CLAPPER_REACTABLES_MANAGER_EVENT_, e)
#define _QUARK(q) (_quarks[CLAPPER_REACTABLES_MANAGER_QUARK_##q].quark)

#define _BUS_POST_EVENT_SINGLE(event_id,lower,type,val) { \
  GValue _value = G_VALUE_INIT;                           \
  g_value_init (&_value, type);                           \
  g_value_set_##lower (&_value, val);                     \
  _bus_post_event (self, event_id, &_value, NULL); }

#define _BUS_POST_EVENT_DUAL(event_id,lower1,type1,val1,lower2,type2,val2) { \
  GValue _value1 = G_VALUE_INIT;                                             \
  GValue _value2 = G_VALUE_INIT;                                             \
  g_value_init (&_value1, type1);                                            \
  g_value_init (&_value2, type2);                                            \
  g_value_set_##lower1 (&_value1, val1);                                     \
  g_value_set_##lower2 (&_value2, val2);                                     \
  _bus_post_event (self, event_id, &_value1, &_value2); }

void
clapper_reactables_manager_initialize (void)
{
  gint i;

  for (i = 0; _quarks[i].name; ++i)
    _quarks[i].quark = g_quark_from_static_string (_quarks[i].name);
}

static void
_settings_changed_cb (GSettings *settings, const gchar *key, ClapperReactableManagerData *data)
{
  GST_DEBUG_OBJECT (data->reactable, "Global setting \"%s\" changed", key);

  /* Local settings are applied through bus events, so all that is
   * needed here is a check to not overwrite locally set setting */
  if (!clapper_enhancer_proxy_has_locally_set (data->proxy, key)) {
    GVariant *variant = g_settings_get_value (settings, key);
    GValue value = G_VALUE_INIT;

    if (G_LIKELY (clapper_utils_set_value_from_variant (&value, variant))) {
      g_object_set_property (G_OBJECT (data->reactable), key, &value);
      g_value_unset (&value);
    }

    g_variant_unref (variant);
  }
}

static inline void
clapper_reactables_manager_handle_prepare (ClapperReactablesManager *self)
{
  ClapperPlayer *player;

  GST_INFO_OBJECT (self, "Preparing reactable enhancers");
  player = CLAPPER_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (self)));

  if (G_LIKELY (player != NULL)) {
    ClapperEnhancerProxyList *proxies = clapper_player_get_enhancer_proxies (player);
    guint i, n_proxies = clapper_enhancer_proxy_list_get_n_proxies (proxies);

    for (i = 0; i < n_proxies; ++i) {
      ClapperEnhancerProxy *proxy = clapper_enhancer_proxy_list_peek_proxy (proxies, i);
      ClapperReactable *reactable = NULL;

      if (!clapper_enhancer_proxy_target_has_interface (proxy, CLAPPER_TYPE_REACTABLE))
        continue;

#if CLAPPER_WITH_ENHANCERS_LOADER
      reactable = CLAPPER_REACTABLE_CAST (
          clapper_enhancers_loader_create_enhancer (proxy, CLAPPER_TYPE_REACTABLE));
#endif

      if (G_LIKELY (reactable != NULL)) {
        ClapperReactableManagerData *data;
        GstStructure *config;

        if (g_object_is_floating (reactable))
          gst_object_ref_sink (reactable);

        data = g_new (ClapperReactableManagerData, 1);
        data->reactable = reactable;
        data->proxy = gst_object_ref (proxy);
        data->settings = clapper_enhancer_proxy_get_settings (proxy);

        GST_TRACE_OBJECT (self, "Created data for reactable: %" GST_PTR_FORMAT, data->reactable);

        /* Settings are stored in data in order for this signal to keep working */
        if (data->settings)
          g_signal_connect (data->settings, "changed", G_CALLBACK (_settings_changed_cb), data);

        if ((config = clapper_enhancer_proxy_make_current_config (proxy))) {
          clapper_enhancer_proxy_apply_config_to_enhancer (proxy, config, (GObject *) reactable);
          gst_structure_free (config);
        }

        g_ptr_array_add (self->array, data);
        gst_object_set_parent (GST_OBJECT_CAST (data->reactable), GST_OBJECT_CAST (player));
      }
    }

    GST_INFO_OBJECT (self, "Prepared %i reactable enhancers", self->array->len);
    gst_object_unref (player);
  } else {
    GST_ERROR_OBJECT (self, "Could not prepare reactable enhancers!");
  }
}

static inline void
clapper_reactables_manager_handle_configure (ClapperReactablesManager *self, const GstStructure *structure)
{
  const GValue *proxy_val, *config_val;
  ClapperEnhancerProxy *proxy;
  const GstStructure *config;
  guint i;

  proxy_val = gst_structure_id_get_value (structure, _QUARK (VALUE));
  config_val = gst_structure_id_get_value (structure, _QUARK (EXTRA_VALUE));

  proxy = CLAPPER_ENHANCER_PROXY_CAST (g_value_get_object (proxy_val));
  config = gst_value_get_structure (config_val);

  for (i = 0; i < self->array->len; ++i) {
    ClapperReactableManagerData *data = g_ptr_array_index (self->array, i);

    if (data->proxy == proxy) {
      clapper_enhancer_proxy_apply_config_to_enhancer (data->proxy,
          config, (GObject *) data->reactable);
      return;
    }
  }

  GST_ERROR_OBJECT (self, "Triggered configure, but no matching enhancer proxy found");
}

static inline void
clapper_reactables_manager_handle_event (ClapperReactablesManager *self, const GstStructure *structure)
{
  const GValue *value = gst_structure_id_get_value (structure, _QUARK (VALUE));
  const GValue *extra_value = gst_structure_id_get_value (structure, _QUARK (EXTRA_VALUE));
  guint i, event_id;

  if (G_UNLIKELY (!gst_structure_id_get (structure,
      _QUARK (EVENT), G_TYPE_ENUM, &event_id, NULL))) {
    GST_ERROR_OBJECT (self, "Could not read event ID");
    return;
  }

  for (i = 0; i < self->array->len; ++i) {
    ClapperReactableManagerData *data = g_ptr_array_index (self->array, i);
    ClapperReactableInterface *reactable_iface = CLAPPER_REACTABLE_GET_IFACE (data->reactable);

    switch (event_id) {
      case _EVENT (STATE_CHANGED):
        if (reactable_iface->state_changed)
          reactable_iface->state_changed (data->reactable, g_value_get_int (value));
        break;
      case _EVENT (POSITION_CHANGED):
        if (reactable_iface->position_changed)
          reactable_iface->position_changed (data->reactable, g_value_get_double (value));
        break;
      case _EVENT (SPEED_CHANGED):
        if (reactable_iface->speed_changed)
          reactable_iface->speed_changed (data->reactable, g_value_get_double (value));
        break;
      case _EVENT (VOLUME_CHANGED):
        if (reactable_iface->volume_changed)
          reactable_iface->volume_changed (data->reactable, g_value_get_double (value));
        break;
      case _EVENT (MUTE_CHANGED):
        if (reactable_iface->mute_changed)
          reactable_iface->mute_changed (data->reactable, g_value_get_boolean (value));
        break;
      case _EVENT (PLAYED_ITEM_CHANGED):
        if (reactable_iface->played_item_changed) {
          reactable_iface->played_item_changed (data->reactable,
              CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)));
        }
        break;
      case _EVENT (ITEM_UPDATED):
        if (reactable_iface->item_updated) {
          reactable_iface->item_updated (data->reactable,
              CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)));
        }
        break;
      case _EVENT (QUEUE_ITEM_ADDED):
        if (reactable_iface->queue_item_added) {
          reactable_iface->queue_item_added (data->reactable,
              CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)),
              g_value_get_uint (extra_value));
        }
        break;
      case _EVENT (QUEUE_ITEM_REMOVED):
        if (reactable_iface->queue_item_removed) {
          reactable_iface->queue_item_removed (data->reactable,
              CLAPPER_MEDIA_ITEM_CAST (g_value_get_object (value)),
              g_value_get_uint (extra_value));
        }
        break;
      case _EVENT (QUEUE_ITEM_REPOSITIONED):
        if (reactable_iface->queue_item_repositioned) {
          reactable_iface->queue_item_repositioned (data->reactable,
              g_value_get_uint (value),
              g_value_get_uint (extra_value));
        }
        break;
      case _EVENT (QUEUE_CLEARED):
        if (reactable_iface->queue_cleared)
          reactable_iface->queue_cleared (data->reactable);
        break;
      case _EVENT (QUEUE_PROGRESSION_CHANGED):
        if (reactable_iface->queue_progression_changed)
          reactable_iface->queue_progression_changed (data->reactable, g_value_get_int (value));
        break;
      default:
        GST_ERROR_OBJECT (self, "Invalid event ID on reactables bus: %u", event_id);
        break;
    }
  }
}

static gboolean
_bus_message_func (GstBus *bus, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
  if (G_LIKELY (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_APPLICATION)) {
    ClapperReactablesManager *self = CLAPPER_REACTABLES_MANAGER_CAST (GST_MESSAGE_SRC (msg));
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark quark = gst_structure_get_name_id (structure);

    if (quark == _QUARK (EVENT)) {
      clapper_reactables_manager_handle_event (self, structure);
    } else if (quark == _QUARK (PREPARE)) {
      clapper_reactables_manager_handle_prepare (self);
    } else if (quark == _QUARK (CONFIGURE)) {
      clapper_reactables_manager_handle_configure (self, structure);
    } else {
      GST_ERROR_OBJECT (self, "Received invalid quark on reactables bus!");
    }
  }

  return G_SOURCE_CONTINUE;
}

static void
_bus_post_event (ClapperReactablesManager *self, guint event_id,
    GValue *value, GValue *extra_value)
{
  GstStructure *structure = gst_structure_new_id (_QUARK (EVENT),
      _QUARK (EVENT), G_TYPE_ENUM, event_id,
      NULL);

  if (value)
    gst_structure_id_take_value (structure, _QUARK (VALUE), value);
  if (extra_value)
    gst_structure_id_take_value (structure, _QUARK (EXTRA_VALUE), extra_value);

  gst_bus_post (self->bus, gst_message_new_application (
      GST_OBJECT_CAST (self), structure));
}

/*
 * clapper_reactables_manager_new:
 *
 * Returns: (transfer full): a new #ClapperReactablesManager instance.
 */
ClapperReactablesManager *
clapper_reactables_manager_new (void)
{
  ClapperReactablesManager *reactables_manager;

  reactables_manager = g_object_new (CLAPPER_TYPE_REACTABLES_MANAGER, NULL);
  gst_object_ref_sink (reactables_manager);

  return reactables_manager;
}

void
clapper_reactables_manager_trigger_prepare (ClapperReactablesManager *self)
{
  GstStructure *structure = gst_structure_new_id_empty (_QUARK (PREPARE));

  gst_bus_post (self->bus, gst_message_new_application (
      GST_OBJECT_CAST (self), structure));
}

void
clapper_reactables_manager_trigger_configure_take_config (ClapperReactablesManager *self,
    ClapperEnhancerProxy *proxy, GstStructure *config)
{
  GstStructure *structure = gst_structure_new_id (_QUARK (CONFIGURE),
      _QUARK (VALUE), G_TYPE_OBJECT, proxy, NULL);
  GValue extra_value = G_VALUE_INIT;

  g_value_init (&extra_value, GST_TYPE_STRUCTURE);
  g_value_take_boxed (&extra_value, config);

  gst_structure_id_take_value (structure, _QUARK (EXTRA_VALUE), &extra_value);

  gst_bus_post (self->bus, gst_message_new_application (
      GST_OBJECT_CAST (self), structure));
}

void
clapper_reactables_manager_trigger_state_changed (ClapperReactablesManager *self, ClapperPlayerState state)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (STATE_CHANGED), int, G_TYPE_INT, state);
}

void
clapper_reactables_manager_trigger_position_changed (ClapperReactablesManager *self, gdouble position)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (POSITION_CHANGED), double, G_TYPE_DOUBLE, position);
}

void
clapper_reactables_manager_trigger_speed_changed (ClapperReactablesManager *self, gdouble speed)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (SPEED_CHANGED), double, G_TYPE_DOUBLE, speed);
}

void
clapper_reactables_manager_trigger_volume_changed (ClapperReactablesManager *self, gdouble volume)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (VOLUME_CHANGED), double, G_TYPE_DOUBLE, volume);
}

void
clapper_reactables_manager_trigger_mute_changed (ClapperReactablesManager *self, gboolean mute)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (MUTE_CHANGED), boolean, G_TYPE_BOOLEAN, mute);
}

void
clapper_reactables_manager_trigger_played_item_changed (ClapperReactablesManager *self, ClapperMediaItem *item)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (PLAYED_ITEM_CHANGED), object, CLAPPER_TYPE_MEDIA_ITEM, item);
}

void
clapper_reactables_manager_trigger_item_updated (ClapperReactablesManager *self, ClapperMediaItem *item)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (ITEM_UPDATED), object, CLAPPER_TYPE_MEDIA_ITEM, item);
}

void
clapper_reactables_manager_trigger_queue_item_added (ClapperReactablesManager *self, ClapperMediaItem *item, guint index)
{
  _BUS_POST_EVENT_DUAL (_EVENT (QUEUE_ITEM_ADDED), object, CLAPPER_TYPE_MEDIA_ITEM, item, uint, G_TYPE_UINT, index);
}

void
clapper_reactables_manager_trigger_queue_item_removed (ClapperReactablesManager *self, ClapperMediaItem *item, guint index)
{
  _BUS_POST_EVENT_DUAL (_EVENT (QUEUE_ITEM_REMOVED), object, CLAPPER_TYPE_MEDIA_ITEM, item, uint, G_TYPE_UINT, index);
}

void
clapper_reactables_manager_trigger_queue_item_repositioned (ClapperReactablesManager *self, guint before, guint after)
{
  _BUS_POST_EVENT_DUAL (_EVENT (QUEUE_ITEM_REPOSITIONED), uint, G_TYPE_UINT, before, uint, G_TYPE_UINT, after);
}

void
clapper_reactables_manager_trigger_queue_cleared (ClapperReactablesManager *self)
{
  _bus_post_event (self, _EVENT (QUEUE_CLEARED), NULL, NULL);
}

void
clapper_reactables_manager_trigger_queue_progression_changed (ClapperReactablesManager *self, ClapperQueueProgressionMode mode)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (QUEUE_PROGRESSION_CHANGED), int, G_TYPE_INT, mode);
}

static void
_data_remove_func (ClapperReactableManagerData *data)
{
  GST_TRACE ("Removing data for reactable: %" GST_PTR_FORMAT, data->reactable);

  g_clear_object (&data->settings);

  gst_object_unparent (GST_OBJECT_CAST (data->reactable));
  gst_object_unref (data->reactable);

  gst_object_unref (data->proxy);
  g_free (data);
}

static void
clapper_reactables_manager_thread_start (ClapperThreadedObject *threaded_object)
{
  ClapperReactablesManager *self = CLAPPER_REACTABLES_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Reactables manager thread start");

  self->array = g_ptr_array_new_with_free_func (
      (GDestroyNotify) _data_remove_func);

  self->bus = gst_bus_new ();
  gst_bus_add_watch (self->bus, (GstBusFunc) _bus_message_func, NULL);
}

static void
clapper_reactables_manager_thread_stop (ClapperThreadedObject *threaded_object)
{
  ClapperReactablesManager *self = CLAPPER_REACTABLES_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (self, "Reactables manager thread stop");

  gst_bus_set_flushing (self->bus, TRUE);
  gst_bus_remove_watch (self->bus);
  gst_clear_object (&self->bus);

  g_ptr_array_unref (self->array);
}

static void
clapper_reactables_manager_init (ClapperReactablesManager *self)
{
}

static void
clapper_reactables_manager_finalize (GObject *object)
{
  GST_TRACE_OBJECT (object, "Finalize");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_reactables_manager_class_init (ClapperReactablesManagerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperreactablesmanager", 0,
      "Clapper Reactables Manager");

  gobject_class->finalize = clapper_reactables_manager_finalize;

  threaded_object->thread_start = clapper_reactables_manager_thread_start;
  threaded_object->thread_stop = clapper_reactables_manager_thread_stop;
}
