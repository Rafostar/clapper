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

/**
 * ClapperEnhancerProxy:
 *
 * An intermediary between player and enhancer plugin.
 *
 * Applications can use this to inspect enhancer information,
 * its properties and configure them.
 *
 * Clapper player manages all enhancers internally, including creating when
 * needed and destroying them later. Instead, it provides access to so called
 * enhancer proxy objects which allow to browse available enhancer properties
 * and store their config either globally or locally for each player instance.
 *
 * Use [func@Clapper.get_global_enhancer_proxies] or [property@Clapper.Player:enhancer-proxies]
 * property to access a [class@Clapper.EnhancerProxyList] of available enhancer proxies. While both
 * lists include the same amount of proxies, the difference is which properties can be configured
 * in which list. Only the latter allows tweaking of local (per player instance) properties using
 * [method@Clapper.EnhancerProxy.set_locally] function.
 *
 * Since: 0.10
 */

#include "config.h"

#include <gobject/gvaluecollector.h>

#include "clapper-enhancer-proxy-private.h"
#include "clapper-enhancer-proxy-list.h"
#include "clapper-basic-functions.h"
#include "clapper-cache-private.h"
#include "clapper-extractable.h"
#include "clapper-enums.h"

#include "clapper-functionalities-availability.h"

#if CLAPPER_WITH_ENHANCERS_LOADER
#include <libpeas.h>
#endif

#define CONFIG_STRUCTURE_NAME "config"

#define GST_CAT_DEFAULT clapper_enhancer_proxy_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperEnhancerProxy
{
  GstObject parent;

  /* Hold a ref on info to ensure
   * below props are kept alive */
  GObject *peas_info;

  const gchar *friendly_name;
  const gchar *module_name;
  const gchar *module_dir;
  const gchar *description;
  const gchar *version;

  guint n_ifaces;
  GType *ifaces;

  guint n_pspecs;
  GParamSpec **pspecs;

  ClapperEnhancerParamFlags scope;
  GstStructure *local_config;

  /* GSettings are not thread-safe,
   * so store schema instead */
  GSettingsSchema *schema;
  gboolean schema_init_done;
};

enum
{
  PROP_0,
  PROP_FRIENDLY_NAME,
  PROP_MODULE_NAME,
  PROP_MODULE_DIR,
  PROP_DESCRIPTION,
  PROP_VERSION,
  PROP_LAST
};

#define parent_class clapper_enhancer_proxy_parent_class
G_DEFINE_TYPE (ClapperEnhancerProxy, clapper_enhancer_proxy, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static gboolean
_update_config_cb (GQuark field_id, const GValue *value, GstStructure *config)
{
  gst_structure_set_value (config, g_quark_to_string (field_id), value);
  return TRUE;
}

static void
_update_local_config_from_structure (ClapperEnhancerProxy *self, const GstStructure *src)
{
  GST_OBJECT_LOCK (self);

  if (!self->local_config)
    self->local_config = gst_structure_copy (src);
  else
    gst_structure_foreach (src, (GstStructureForeachFunc) _update_config_cb, self->local_config);

  GST_OBJECT_UNLOCK (self);
}

/*
 * clapper_enhancer_proxy_new_global_take:
 * @peas_info: (transfer full): a #PeasPluginInfo cast to GObject
 *
 * Create a new proxy. This should be only used for creating
 * global proxies using @peas_info from enhancer loader,
 * while player should use copies of global proxies.
 *
 * Returns: (transfer full): a new #ClapperEnhancerProxy instance.
 */
ClapperEnhancerProxy *
clapper_enhancer_proxy_new_global_take (GObject *peas_info)
{
  ClapperEnhancerProxy *proxy;

#if CLAPPER_WITH_ENHANCERS_LOADER
  const PeasPluginInfo *info = (const PeasPluginInfo *) peas_info;
  gchar obj_name[64];
  const gchar *friendly_name;

  /* Name newly created proxy for easier debugging. Its best
   * to do it with g_object_new(), as this avoid GStreamer
   * naming it first with us renaming it afterwards. */
  friendly_name = peas_plugin_info_get_name (info);
  g_snprintf (obj_name, sizeof (obj_name), "%s-global-proxy", friendly_name);
  proxy = g_object_new (CLAPPER_TYPE_ENHANCER_PROXY, "name", obj_name, NULL);

  proxy->peas_info = peas_info;
  proxy->friendly_name = friendly_name;
  proxy->module_name = peas_plugin_info_get_module_name (info);
  proxy->module_dir = peas_plugin_info_get_module_dir (info);
  proxy->description = peas_plugin_info_get_description (info);
  proxy->version = peas_plugin_info_get_version (info);
#else
  /* XXX: This should never be reached. We do not
   * create proxies if we cannot load enhancers. */
  proxy = g_object_new (CLAPPER_TYPE_ENHANCER_PROXY, NULL);
#endif

  proxy->scope = CLAPPER_ENHANCER_PARAM_GLOBAL;

  gst_object_ref_sink (proxy);

  return proxy;
}

/*
 * clapper_enhancer_proxy_copy:
 * @src_proxy: a #ClapperEnhancerProxy to copy
 * @copy_name: name of the #GstObject copy
 *
 * Create a copy of enhancer proxy.
 *
 * Using another proxy as source to avoids reading cache again.
 * This is mainly for internal usage to create new unconfigured
 * from global proxy list.
 *
 * Returns: (transfer full): a new #ClapperEnhancerProxy instance.
 */
ClapperEnhancerProxy *
clapper_enhancer_proxy_copy (ClapperEnhancerProxy *src_proxy, const gchar *copy_name)
{
  ClapperEnhancerProxy *copy;
  guint i;

  copy = g_object_new (CLAPPER_TYPE_ENHANCER_PROXY,
      "name", copy_name, NULL);

  copy->peas_info = g_object_ref (src_proxy->peas_info);
  copy->friendly_name = src_proxy->friendly_name;
  copy->module_name = src_proxy->module_name;
  copy->module_dir = src_proxy->module_dir;
  copy->description = src_proxy->description;
  copy->version = src_proxy->version;

  /* Copy extra data from source proxy */

  copy->n_ifaces = src_proxy->n_ifaces;
  copy->ifaces = g_new (GType, copy->n_ifaces);

  for (i = 0; i < src_proxy->n_ifaces; ++i)
    copy->ifaces[i] = src_proxy->ifaces[i];

  copy->n_pspecs = src_proxy->n_pspecs;
  copy->pspecs = g_new (GParamSpec *, copy->n_pspecs);

  for (i = 0; i < src_proxy->n_pspecs; ++i)
    copy->pspecs[i] = g_param_spec_ref (src_proxy->pspecs[i]);

  copy->scope = CLAPPER_ENHANCER_PARAM_LOCAL;

  GST_OBJECT_LOCK (src_proxy);

  if (src_proxy->schema)
    copy->schema = g_settings_schema_ref (src_proxy->schema);

  copy->schema_init_done = src_proxy->schema_init_done;

  if (src_proxy->local_config)
    copy->local_config = gst_structure_copy (src_proxy->local_config);

  GST_OBJECT_UNLOCK (src_proxy);

  gst_object_ref_sink (copy);

  return copy;
}

static inline void
_init_schema (ClapperEnhancerProxy *self)
{
  GST_OBJECT_LOCK (self);

  if (self->schema_init_done) {
    GST_OBJECT_UNLOCK (self);
    return;
  }

  if (self->scope == CLAPPER_ENHANCER_PARAM_GLOBAL) {
    guint i;
    gboolean configurable = FALSE;

    GST_TRACE_OBJECT (self, "Initializing settings schema");

    /* Check whether to expect any schema without file query */
    for (i = 0; i < self->n_pspecs; ++i) {
      if ((configurable = (self->pspecs[i]->flags & CLAPPER_ENHANCER_PARAM_GLOBAL)))
        break;
    }

    if (configurable) {
      GSettingsSchemaSource *schema_source;
      GError *error = NULL;

      schema_source = g_settings_schema_source_new_from_directory (
          self->module_dir, g_settings_schema_source_get_default (), TRUE, &error);

      if (!error) {
        gchar **non_reloc = NULL;

        g_settings_schema_source_list_schemas (schema_source,
            FALSE, &non_reloc, NULL);

        if (non_reloc && non_reloc[0]) {
          const gchar *schema_id = non_reloc[0];

          GST_DEBUG_OBJECT (self, "Found settings schema: %s", schema_id);
          self->schema = g_settings_schema_source_lookup (schema_source, schema_id, FALSE);
        }

        g_strfreev (non_reloc);
      } else {
        GST_ERROR_OBJECT (self, "Could not load settings, reason: %s",
            GST_STR_NULL (error->message));
        g_error_free (error);
      }

      /* SAFETY: Not sure if on error invalid source might be returned */
      g_clear_pointer (&schema_source, g_settings_schema_source_unref);
    }
  } else {
    ClapperEnhancerProxyList *proxies;
    ClapperEnhancerProxy *global_proxy;

    proxies = clapper_get_global_enhancer_proxies ();
    global_proxy = clapper_enhancer_proxy_list_get_proxy_by_module (proxies, self->module_name);

    /* Must ensure init was done on global
     * proxy before accessing its schema */
    _init_schema (global_proxy);

    /* Just ref the schema from global one, so we
     * can avoid loading them in local proxies */
    if (global_proxy->schema)
      self->schema = g_settings_schema_ref (global_proxy->schema);

    gst_object_unref (global_proxy);
  }

  self->schema_init_done = TRUE;

  GST_OBJECT_UNLOCK (self);
}

static inline gchar *
_build_cache_filename (ClapperEnhancerProxy *self)
{
  return g_build_filename (g_get_user_cache_dir (), CLAPPER_API_NAME,
      "enhancers", self->module_name, "cache.bin", NULL);
}

gboolean
clapper_enhancer_proxy_fill_from_cache (ClapperEnhancerProxy *self)
{
  GMappedFile *mapped_file;
  GError *error = NULL;
  gchar *filename;
  const gchar *data;
  guint i;

  filename = _build_cache_filename (self);
  mapped_file = clapper_cache_open (filename, &data, &error);
  g_free (filename);

  if (!mapped_file) {
    /* No error if cache disabled or version mismatch */
    if (error) {
      if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT)
        GST_DEBUG_OBJECT (self, "No cache file found");
      else
        GST_ERROR_OBJECT (self, "Could not restore from cache, reason: %s", error->message);

      g_error_free (error);
    }

    return FALSE;
  }

  /* Plugin version check */
  if (g_strcmp0 (clapper_cache_read_string (&data), self->version) != 0) {
    g_mapped_file_unref (mapped_file);
    return FALSE; // not an error
  }

  /* Restore Interfaces */
  if ((self->n_ifaces = clapper_cache_read_uint (&data)) > 0) {
    self->ifaces = g_new (GType, self->n_ifaces);
    for (i = 0; i < self->n_ifaces; ++i) {
      if (G_UNLIKELY ((self->ifaces[i] = clapper_cache_read_iface (&data)) == 0))
        goto abort_reading;
    }
  }

  /* Restore ParamSpecs */
  if ((self->n_pspecs = clapper_cache_read_uint (&data)) > 0) {
    self->pspecs = g_new (GParamSpec *, self->n_pspecs);
    for (i = 0; i < self->n_pspecs; ++i) {
      if (G_UNLIKELY ((self->pspecs[i] = clapper_cache_read_pspec (&data)) == NULL))
        goto abort_reading;
    }
  }

  g_mapped_file_unref (mapped_file);

  GST_DEBUG_OBJECT (self, "Filled proxy \"%s\" from cache, n_ifaces: %u, n_pspecs: %u",
      self->friendly_name, self->n_ifaces, self->n_pspecs);

  return TRUE;

abort_reading:
  GST_ERROR_OBJECT (self, "Cache file is corrupted or invalid");

  g_free (self->ifaces);
  self->n_ifaces = 0;

  for (i = 0; i < self->n_pspecs; ++i) {
    g_clear_pointer (&self->pspecs[i], g_param_spec_unref);
  }
  g_free (self->pspecs);
  self->n_pspecs = 0;

  g_mapped_file_unref (mapped_file);

  return FALSE;
}

void
clapper_enhancer_proxy_export_to_cache (ClapperEnhancerProxy *self)
{
  GByteArray *bytes;
  GError *error = NULL;
  gchar *filename;
  gboolean data_ok = TRUE;
  guint i;

  bytes = clapper_cache_create ();

  /* If cache disabled */
  if (G_UNLIKELY (bytes == NULL))
    return;

  filename = _build_cache_filename (self);
  GST_TRACE_OBJECT (self, "Exporting data to cache file: \"%s\"", filename);

  /* Store version */
  clapper_cache_store_string (bytes, self->version);

  /* Store Interfaces */
  clapper_cache_store_uint (bytes, self->n_ifaces);
  for (i = 0; i < self->n_ifaces; ++i) {
    /* This should never happen, as we only store Clapper interfaces */
    if (G_UNLIKELY (!(data_ok = clapper_cache_store_iface (bytes, self->ifaces[i])))) {
      g_warning ("Cannot cache enhancer \"%s\" (%s), as it contains"
          " unsupported interface type \"%s\"",
          self->friendly_name, self->module_name, g_type_name (self->ifaces[i]));
      break;
    }
  }

  if (data_ok) {
    /* Store ParamSpecs */
    clapper_cache_store_uint (bytes, self->n_pspecs);
    for (i = 0; i < self->n_pspecs; ++i) {
      /* Can happen if someone writes an enhancer with unsupported
       * param spec type with ClapperEnhancerParamFlags set */
      if (G_UNLIKELY (!(data_ok = clapper_cache_store_pspec (bytes, self->pspecs[i])))) {
        g_warning ("Cannot cache enhancer \"%s\" (%s), as it contains"
            " property \"%s\" of unsupported type",
            self->friendly_name, self->module_name, self->pspecs[i]->name);
        break;
      }
    }
  }

  if (data_ok && clapper_cache_write (filename, bytes, &error)) {
    GST_TRACE_OBJECT (self, "Successfully exported data to cache file");
  } else if (error) {
    GST_ERROR_OBJECT (self, "Could not cache data, reason: %s", error->message);
    g_error_free (error);
  }

  g_free (filename);
  g_byte_array_free (bytes, TRUE);
}

gboolean
clapper_enhancer_proxy_fill_from_instance (ClapperEnhancerProxy *self, GObject *enhancer)
{
  GType enhancer_types[1] = { CLAPPER_TYPE_EXTRACTABLE };
  GType *ifaces;
  GParamSpec **pspecs;
  GParamFlags enhancer_flags;
  guint i, j, n, write_index = 0;

  /* Filter to only Clapper interfaces */
  ifaces = g_type_interfaces (G_OBJECT_TYPE (enhancer), &n);
  for (i = 0; i < n; ++i) {
    for (j = 0; j < G_N_ELEMENTS (enhancer_types); ++j) {
      if (ifaces[i] == enhancer_types[j]) {
        ifaces[write_index++] = ifaces[i];
        break; // match found, do next iface
      }
    }
  }

  /* Resize memory */
  self->n_ifaces = write_index;
  self->ifaces = g_realloc (ifaces, self->n_ifaces * sizeof (GType));

  /* Filter to only Clapper param specs */
  write_index = 0;
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (enhancer), &n);
  enhancer_flags = (CLAPPER_ENHANCER_PARAM_GLOBAL | CLAPPER_ENHANCER_PARAM_LOCAL);
  for (i = 0; i < n; ++i) {
    if (pspecs[i]->flags & enhancer_flags)
      pspecs[write_index++] = g_param_spec_ref (pspecs[i]);
  }

  /* Resize memory */
  self->n_pspecs = write_index;
  self->pspecs = g_realloc (pspecs, self->n_pspecs * sizeof (GParamSpec *));

  GST_DEBUG_OBJECT (self, "Filled proxy \"%s\" from instance, n_ifaces: %u, n_pspecs: %u",
      self->friendly_name, self->n_ifaces, self->n_pspecs);

  return TRUE;
}

GObject *
clapper_enhancer_proxy_get_peas_info (ClapperEnhancerProxy *self)
{
  return self->peas_info;
}

static gboolean
_apply_config_cb (GQuark field_id, const GValue *value, GObject *enhancer)
{
  g_object_set_property (enhancer, g_quark_to_string (field_id), value);
  return TRUE;
}

/*
 * clapper_enhancer_proxy_make_current_config:
 *
 * Returns: (transfer full) (nullable): Current merged global and local config as #GstStructure.
 */
GstStructure *
clapper_enhancer_proxy_make_current_config (ClapperEnhancerProxy *self)
{
  GSettings *settings = clapper_enhancer_proxy_get_settings (self);
  GstStructure *merged_config = NULL;
  guint i;

  /* Lock here to ensure consistent local config */
  GST_OBJECT_LOCK (self);

  for (i = 0; i < self->n_pspecs; ++i) {
    GParamSpec *pspec = self->pspecs[i];

    /* Using "has_field", as set value might be %NULL */
    if ((pspec->flags & CLAPPER_ENHANCER_PARAM_LOCAL)
        && gst_structure_has_field (self->local_config, pspec->name)) {
      if (!merged_config)
        merged_config = gst_structure_new_empty (CONFIG_STRUCTURE_NAME);

      gst_structure_set_value (merged_config, pspec->name,
          gst_structure_get_value (self->local_config, pspec->name));
      continue; // local config overshadows global one
    }
    if (settings && (pspec->flags & CLAPPER_ENHANCER_PARAM_GLOBAL)) {
      GVariant *val = g_settings_get_value (settings, pspec->name);
      GVariant *def = g_settings_get_default_value (settings, pspec->name);

      if (!g_variant_equal (val, def)) {
        if (!merged_config)
          merged_config = gst_structure_new_empty (CONFIG_STRUCTURE_NAME);

        switch (pspec->value_type) {
          case G_TYPE_BOOLEAN:
            gst_structure_set (merged_config, pspec->name,
                pspec->value_type, g_variant_get_boolean (val), NULL);
            break;
          case G_TYPE_INT:
            gst_structure_set (merged_config, pspec->name,
                pspec->value_type, g_variant_get_int32 (val), NULL);
            break;
          case G_TYPE_UINT:
            gst_structure_set (merged_config, pspec->name,
                pspec->value_type, g_variant_get_uint32 (val), NULL);
            break;
          case G_TYPE_DOUBLE:
            gst_structure_set (merged_config, pspec->name,
                pspec->value_type, g_variant_get_double (val), NULL);
            break;
          case G_TYPE_STRING:
            gst_structure_set (merged_config, pspec->name,
                pspec->value_type, g_variant_get_string (val, NULL), NULL);
            break;
          default:{
            if (G_IS_PARAM_SPEC_ENUM (pspec)) {
              gst_structure_set (merged_config, pspec->name,
                  G_TYPE_INT, g_variant_get_int32 (val), NULL);
              break;
            } else if (G_IS_PARAM_SPEC_FLAGS (pspec)) {
              gst_structure_set (merged_config, pspec->name,
                  G_TYPE_UINT, g_variant_get_uint32 (val), NULL);
              break;
            }
            GST_ERROR_OBJECT (self, "Unsupported enhancer \"%s\" setting type: %s",
                pspec->name, g_type_name (pspec->value_type));
            break;
          }
        }
      }

      g_variant_unref (val);
      g_variant_unref (def);
    }
  }

  GST_OBJECT_UNLOCK (self);

  g_clear_object (&settings);

  return merged_config;
}

void
clapper_enhancer_proxy_apply_config_to_enhancer (ClapperEnhancerProxy *self, const GstStructure *config, GObject *enhancer)
{
  GST_DEBUG_OBJECT (self, "Applying config to enhancer");
  gst_structure_foreach (config, (GstStructureForeachFunc) _apply_config_cb, enhancer);
  GST_DEBUG_OBJECT (self, "Enhancer config applied");
}

/**
 * clapper_enhancer_proxy_get_friendly_name:
 * @proxy: a #ClapperEnhancerProxy
 *
 * Get a name from enhancer plugin info file.
 * Can be used for showing in UI and such.
 *
 * Name field in plugin info file is mandatory,
 * so this function never returns %NULL.
 *
 * Returns: (not nullable): name of the proxied enhancer.
 *
 * Since: 0.10
 */
const gchar *
clapper_enhancer_proxy_get_friendly_name (ClapperEnhancerProxy *self)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  return self->friendly_name;
}

/**
 * clapper_enhancer_proxy_get_module_name:
 * @proxy: a #ClapperEnhancerProxy
 *
 * Get name of the module from enhancer plugin info file.
 * This value is used to uniquely identify a particular plugin.
 *
 * Module name in plugin info file is mandatory,
 * so this function never returns %NULL.
 *
 * Returns: (not nullable): name of the proxied enhancer.
 *
 * Since: 0.10
 */
const gchar *
clapper_enhancer_proxy_get_module_name (ClapperEnhancerProxy *self)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  return self->module_name;
}

/**
 * clapper_enhancer_proxy_get_module_dir:
 * @proxy: a #ClapperEnhancerProxy
 *
 * Get a path to the directory from which enhancer is loaded.
 *
 * Returns: (not nullable): installation directory of the proxied enhancer.
 *
 * Since: 0.10
 */
const gchar *
clapper_enhancer_proxy_get_module_dir (ClapperEnhancerProxy *self)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  return self->module_dir;
}

/**
 * clapper_enhancer_proxy_get_description:
 * @proxy: a #ClapperEnhancerProxy
 *
 * Get description from enhancer plugin info file.
 *
 * Returns: (nullable): description of the proxied enhancer.
 *
 * Since: 0.10
 */
const gchar *
clapper_enhancer_proxy_get_description (ClapperEnhancerProxy *self)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  return self->description;
}

/**
 * clapper_enhancer_proxy_get_version:
 * @proxy: a #ClapperEnhancerProxy
 *
 * Get version string from enhancer plugin info file.
 *
 * Returns: (nullable): version string of the proxied enhancer.
 *
 * Since: 0.10
 */
const gchar *
clapper_enhancer_proxy_get_version (ClapperEnhancerProxy *self)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  return self->version;
}

/**
 * clapper_enhancer_proxy_get_extra_data:
 * @proxy: a #ClapperEnhancerProxy
 * @key: name of the data to lookup
 *
 * Get extra data from enhancer plugin info file specified by @key.
 *
 * Extra data in the plugin info file is prefixed with `X-`.
 * For example `X-Schemes=https`.
 *
 * Returns: (nullable): extra data value of the proxied enhancer.
 *
 * Since: 0.10
 */
const gchar *
clapper_enhancer_proxy_get_extra_data (ClapperEnhancerProxy *self, const gchar *key)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

#if CLAPPER_WITH_ENHANCERS_LOADER
  return peas_plugin_info_get_external_data ((const PeasPluginInfo *) self->peas_info, key);
#else
  return NULL;
#endif
}

/**
 * clapper_enhancer_proxy_extra_data_lists_value:
 * @proxy: a #ClapperEnhancerProxy
 * @key: name of the data to lookup
 * @value: string to check for
 *
 * A convenience function to check whether @proxy plugin file
 * has an extra data field with @key that among separated list
 * of values includes @value (works on single value lists too).
 *
 * For example, when extra data in the plugin is `X-Schemes=https;http`,
 * calling this function with "X-Schemes" as key and "http" as value will
 * return %TRUE.
 *
 * It is also safe to call this function when there is no such @key
 * in plugin info file. Use [method@Clapper.EnhancerProxy.get_extra_data]
 * if you need to know whether key exists.
 *
 * Returns: whether list named with @key existed and contained @value.
 *
 * Since: 0.10
 */
gboolean
clapper_enhancer_proxy_extra_data_lists_value (ClapperEnhancerProxy *self,
    const gchar *key, const gchar *value)
{
  const gchar *list_str;

  /* Remaining things are checked in "get_extra_data()" */
  g_return_val_if_fail (value != NULL, FALSE);

  if ((list_str = clapper_enhancer_proxy_get_extra_data (self, key))) {
    gsize value_len = strlen (value);
    guint i = 0;

    while (list_str[i] != '\0') {
      guint end = i;

      while (list_str[end] != ';' && list_str[end] != '\0')
        ++end;

      /* Compare letters count until separator and prefix of whole string */
      if (end - i == value_len && g_str_has_prefix (list_str + i, value))
        return TRUE;

      i = end;

      /* Move to the next letter after ';' */
      if (list_str[i] != '\0')
        ++i;
    }
  }

  return FALSE;
}

/**
 * clapper_enhancer_proxy_get_target_interfaces:
 * @proxy: a #ClapperEnhancerProxy
 * @n_interfaces: (out): return location for the length of the returned array
 *
 * Get an array of interfaces that target enhancer implements.
 *
 * The returned array includes only Clapper specific interfaces
 * for writing enhancers. Applications should not care about any
 * other interface types that given enhancer is using internally.
 *
 * Returns: (transfer none) (nullable) (array length=n_interfaces): an array of #GType interfaces.
 *
 * Since: 0.10
 */
GType *
clapper_enhancer_proxy_get_target_interfaces (ClapperEnhancerProxy *self, guint *n_interfaces)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  if (n_interfaces)
    *n_interfaces = self->n_ifaces;

  return self->ifaces;
}

/**
 * clapper_enhancer_proxy_target_has_interface:
 * @proxy: a #ClapperEnhancerProxy
 * @iface_type: an interface #GType
 *
 * A convenience function to check if target enhancer implements given interface.
 *
 * This works only with Clapper specific interfaces as @iface_type
 * for writing enhancers. Applications should not care about any
 * other interface types that given enhancer is using internally.
 *
 * Returns: whether target implements given interface.
 *
 * Since: 0.10
 */
gboolean
clapper_enhancer_proxy_target_has_interface (ClapperEnhancerProxy *self, GType iface_type)
{
  guint i;

  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), FALSE);

  for (i = 0; i < self->n_ifaces; ++i) {
    if (self->ifaces[i] == iface_type)
      return TRUE;
  }

  return FALSE;
}

/**
 * clapper_enhancer_proxy_get_target_properties:
 * @proxy: a #ClapperEnhancerProxy
 * @n_properties: (out): return location for the length of the returned array
 *
 * Get an array of properties in target enhancer.
 *
 * Implementations can use this in order to find out what properties, type of
 * their values (including valid ranges) are allowed to set for a given enhancer.
 *
 * Use [flags@Clapper.EnhancerParamFlags] against flags of given [class@GObject.ParamSpec]
 * to find out whether they are local, global or neither of them (internal).
 *
 * The returned array includes only Clapper enhancer specific properties (global and local).
 * Applications can not access any other properties that given enhancer is using internally.
 *
 * Returns: (transfer none) (nullable) (array length=n_properties): an array of #GParamSpec objects.
 *
 * Since: 0.10
 */
GParamSpec **
clapper_enhancer_proxy_get_target_properties (ClapperEnhancerProxy *self, guint *n_properties)
{
  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  if (n_properties)
    *n_properties = self->n_pspecs;

  return self->pspecs;
}

/**
 * clapper_enhancer_proxy_get_settings:
 * @proxy: a #ClapperEnhancerProxy
 *
 * Get #GSettings of an enhancer.
 *
 * Implementations can use this together with [method@Clapper.EnhancerProxy.get_target_properties]
 * in order to allow user to configure global enhancer properties.
 *
 * Settings include only keys from properties with [flags@Clapper.EnhancerParamFlags.GLOBAL]
 * flag and are meant ONLY for user to set. To configure application local enhancer properties,
 * use [method@Clapper.EnhancerProxy.set_locally] instead.
 *
 * This function returns a new instance of #GSettings, so settings can be accessed
 * from different threads if needed.
 *
 * Returns: (transfer full) (nullable): A new #GSettings instance for an enhancer.
 *
 * Since: 0.10
 */
GSettings *
clapper_enhancer_proxy_get_settings (ClapperEnhancerProxy *self)
{
  GSettings *settings = NULL;

  g_return_val_if_fail (CLAPPER_IS_ENHANCER_PROXY (self), NULL);

  /* Try to lazy load schemas */
  _init_schema (self);

  if (self->schema)
    settings = g_settings_new_full (self->schema, NULL, NULL);

  return settings;
}

static GParamSpec *
_find_target_pspec_by_name (ClapperEnhancerProxy *self, const gchar *name)
{
  guint i;

  name = g_intern_string (name);
  for (i = 0; i < self->n_pspecs; ++i) {
    GParamSpec *pspec = self->pspecs[i];

    /* GParamSpec names are always interned */
    if (pspec->name == name)
      return pspec;
  }

  g_warning ("No property \"%s\" in target of \"%s\" (%s)",
      name, self->friendly_name, self->module_name);

  return NULL;
}

static gboolean
_structure_take_value_by_pspec (ClapperEnhancerProxy *self,
    GstStructure *structure, GParamSpec *pspec, GValue *value)
{
  if (G_LIKELY (G_VALUE_TYPE (value) == pspec->value_type)
      && !g_param_value_validate (pspec, value)) {
    if (G_LIKELY (pspec->flags & self->scope)) {
      gst_structure_take_value (structure, pspec->name, value);
      return TRUE;
    } else {
      g_warning ("Trying to set \"%s\" (%s) target property \"%s\" that is outside of proxy %s scope",
          self->friendly_name, self->module_name, pspec->name,
          (self->scope == CLAPPER_ENHANCER_PARAM_GLOBAL) ? "GLOBAL" : "LOCAL");
    }
  } else {
    g_warning ("Wrong value type or out of range for \"%s\" (%s) target property \"%s\"",
        self->friendly_name, self->module_name, pspec->name);
  }

  return FALSE;
}

/**
 * clapper_enhancer_proxy_set_locally:
 * @proxy: a #ClapperEnhancerProxy
 * @first_property_name: name of the first property to configure
 * @...: %NULL-terminated list of arguments
 *
 * Configure one or more properties which have [flags@Clapper.EnhancerParamFlags.LOCAL]
 * flag set on the target enhancer instance.
 *
 * Implementations can use this together with [method@Clapper.EnhancerProxy.get_target_properties]
 * in order to configure local enhancer properties.
 *
 * Arguments should be %NULL terminated list of property name and value to set
 * (like [method@GObject.Object.set] arguments).
 *
 * Since: 0.10
 */
void
clapper_enhancer_proxy_set_locally (ClapperEnhancerProxy *self, const gchar *first_property_name, ...)
{
  GstStructure *structure;
  const gchar *name;
  va_list args;

  g_return_if_fail (CLAPPER_IS_ENHANCER_PROXY (self));
  g_return_if_fail (first_property_name != NULL);

  if (G_UNLIKELY (self->scope != CLAPPER_ENHANCER_PARAM_LOCAL)) {
    g_warning ("Trying to apply local config to a non-local enhancer proxy!");
    return;
  }

  structure = gst_structure_new_empty (CONFIG_STRUCTURE_NAME);

  va_start (args, first_property_name);
  name = first_property_name;

  while (name) {
    GParamSpec *pspec = _find_target_pspec_by_name (self, name);

    if (G_LIKELY (pspec != NULL)) {
      GValue value = G_VALUE_INIT;
      gchar *err = NULL;

      G_VALUE_COLLECT_INIT (&value, pspec->value_type, args, 0, &err);
      if (G_UNLIKELY (err != NULL)) {
        g_critical ("%s", err);
        g_free (err);
        g_value_unset (&value);
        break;
      }

      if (!_structure_take_value_by_pspec (self, structure, pspec, &value))
        g_value_unset (&value);
    } else {
      break;
    }

    name = va_arg (args, const gchar *);
  }

  va_end (args);

  if (G_UNLIKELY (gst_structure_n_fields (structure) == 0)) {
    gst_structure_free (structure);
    return;
  }

  _update_local_config_from_structure (self, structure);

  /* TODO: _post_local_config instead of free if managed
   * (for when managed interfaces are implemented) */

  gst_structure_free (structure);
}

/**
 * clapper_enhancer_proxy_set_locally_with_table: (rename-to clapper_enhancer_proxy_set_locally)
 * @proxy: a #ClapperEnhancerProxy
 * @table: (transfer none) (element-type utf8 GObject.Value): a #GHashTable with property names and values
 *
 * Same as [method@Clapper.EnhancerProxy.set_locally], but to configure uses
 * [struct@GLib.HashTable] with string keys and [struct@GObject.Value] as their values.
 *
 * Since: 0.10
 */
void
clapper_enhancer_proxy_set_locally_with_table (ClapperEnhancerProxy *self, GHashTable *table)
{
  GstStructure *structure;
  GHashTableIter iter;
  gpointer key_ptr, val_ptr;

  g_return_if_fail (CLAPPER_IS_ENHANCER_PROXY (self));
  g_return_if_fail (table != NULL);

  if (G_UNLIKELY (self->scope != CLAPPER_ENHANCER_PARAM_LOCAL)) {
    g_warning ("Trying to apply local config to a non-local enhancer proxy!");
    return;
  }

  structure = gst_structure_new_empty (CONFIG_STRUCTURE_NAME);

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &key_ptr, &val_ptr)) {
    const gchar *name = (const gchar *) key_ptr;
    GParamSpec *pspec = _find_target_pspec_by_name (self, name);

    if (G_LIKELY (pspec != NULL)) {
      GValue value_copy = G_VALUE_INIT;

      if (val_ptr) {
        const GValue *value = (const GValue *) val_ptr;
        g_value_init (&value_copy, G_VALUE_TYPE (value));
        g_value_copy (value, &value_copy);
      } else { // when setting property to NULL
        if (pspec->value_type == G_TYPE_STRING) {
          g_value_init (&value_copy, G_TYPE_STRING);
          g_value_set_string (&value_copy, NULL);
        } else {
          g_value_init (&value_copy, G_TYPE_POINTER);
          g_value_set_pointer (&value_copy, NULL);
        }
      }

      if (!_structure_take_value_by_pspec (self, structure, pspec, &value_copy))
        g_value_unset (&value_copy);
    }
  }

  if (G_UNLIKELY (gst_structure_n_fields (structure) == 0)) {
    gst_structure_free (structure);
    return;
  }

  _update_local_config_from_structure (self, structure);

  /* TODO: _post_local_config instead of free if managed
   * (for when managed interfaces are implemented) */

  gst_structure_free (structure);
}

static void
clapper_enhancer_proxy_init (ClapperEnhancerProxy *self)
{
}

static void
clapper_enhancer_proxy_finalize (GObject *object)
{
  ClapperEnhancerProxy *self = CLAPPER_ENHANCER_PROXY_CAST (object);
  guint i;

  GST_TRACE_OBJECT (self, "Finalize");

  g_object_unref (self->peas_info);
  g_free (self->ifaces);

  for (i = 0; i < self->n_pspecs; ++i) {
    g_param_spec_unref (self->pspecs[i]);
  }
  g_free (self->pspecs);

  gst_clear_structure (&self->local_config);
  g_clear_pointer (&self->schema, g_settings_schema_unref);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_enhancer_proxy_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperEnhancerProxy *self = CLAPPER_ENHANCER_PROXY_CAST (object);

  switch (prop_id) {
    case PROP_FRIENDLY_NAME:
      g_value_set_string (value, clapper_enhancer_proxy_get_friendly_name (self));
      break;
    case PROP_MODULE_NAME:
      g_value_set_string (value, clapper_enhancer_proxy_get_module_name (self));
      break;
    case PROP_MODULE_DIR:
      g_value_set_string (value, clapper_enhancer_proxy_get_module_dir (self));
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, clapper_enhancer_proxy_get_description (self));
      break;
    case PROP_VERSION:
      g_value_set_string (value, clapper_enhancer_proxy_get_version (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_enhancer_proxy_class_init (ClapperEnhancerProxyClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperenhancerproxy", 0,
      "Clapper Enhancer Proxy");

  gobject_class->get_property = clapper_enhancer_proxy_get_property;
  gobject_class->finalize = clapper_enhancer_proxy_finalize;

  /**
   * ClapperEnhancerProxy:friendly-name:
   *
   * Name from enhancer plugin info file.
   *
   * Since: 0.10
   */
  param_specs[PROP_FRIENDLY_NAME] = g_param_spec_string ("friendly-name",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperEnhancerProxy:module-name:
   *
   * Module name from enhancer plugin info file.
   *
   * Since: 0.10
   */
  param_specs[PROP_MODULE_NAME] = g_param_spec_string ("module-name",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperEnhancerProxy:module-dir:
   *
   * Module directory.
   *
   * Since: 0.10
   */
  param_specs[PROP_MODULE_DIR] = g_param_spec_string ("module-dir",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperEnhancerProxy:description:
   *
   * Description from enhancer plugin info file.
   *
   * Since: 0.10
   */
  param_specs[PROP_DESCRIPTION] = g_param_spec_string ("description",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * ClapperEnhancerProxy:version:
   *
   * Version from enhancer plugin info file.
   *
   * Since: 0.10
   */
  param_specs[PROP_VERSION] = g_param_spec_string ("version",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
