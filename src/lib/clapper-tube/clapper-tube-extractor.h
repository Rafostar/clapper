/* Clapper Tube Library
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

#pragma once

#if !defined(__CLAPPER_TUBE_INSIDE__) && !defined(CLAPPER_TUBE_COMPILATION)
#error "Only <clapper-tube/clapper-tube.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <clapper-tube/clapper-tube-visibility.h>
#include <clapper-tube/clapper-tube-enums.h>
#include <clapper-tube/clapper-tube-harvest.h>
#include <clapper-tube/clapper-tube-cache.h>
#include <clapper-tube/clapper-tube-config.h>

G_BEGIN_DECLS

#define CLAPPER_TUBE_TYPE_EXTRACTOR (clapper_tube_extractor_get_type())
#define CLAPPER_TUBE_EXTRACTOR_CAST(obj) ((ClapperTubeExtractor *)(obj))
#define CLAPPER_TUBE_EXTRACTOR_ERROR (clapper_tube_extractor_error_quark ())

CLAPPER_TUBE_API
G_DECLARE_DERIVABLE_TYPE (ClapperTubeExtractor, clapper_tube_extractor, CLAPPER_TUBE, EXTRACTOR, GstObject)

/**
 * CLAPPER_TUBE_EXTRACTOR_DECLARE:
 * @camel: camel case name of the extractor.
 * @lower: lowercase name of the extractor, with multiple words separated by `_`.
 * @upper: uppercase name of the extractor.
 *
 * Convenient macro to declare a new extractor.
 */
#define CLAPPER_TUBE_EXTRACTOR_DECLARE(camel,lower,upper)                            \
G_DECLARE_FINAL_TYPE (ClapperTube##camel, clapper_tube_##lower,                      \
    CLAPPER_TUBE, upper, ClapperTubeExtractor)                                       \
                                                                                     \
G_MODULE_EXPORT ClapperTubeExtractor *extractor_query (GUri *uri);                   \
                                                                                     \
G_GNUC_UNUSED static inline ClapperTube##camel *                                     \
G_PASTE (clapper_tube_##lower, _new) (void) {                                        \
  return g_object_new (G_PASTE (clapper_tube_##lower, _get_type) (), NULL); }        \
G_GNUC_UNUSED static inline gchar *                                                  \
G_PASTE (clapper_tube_##lower, _cache_read) (const gchar *key) {                     \
  return clapper_tube_cache_extractor_read (G_STRINGIFY (lower), key); }             \
G_GNUC_UNUSED static inline void                                                     \
G_PASTE (clapper_tube_##lower, _cache_write) (const gchar *key,                      \
    const gchar *val, gint64 exp) {                                                  \
  clapper_tube_cache_extractor_write (G_STRINGIFY (lower), key, val, exp); }         \
G_GNUC_UNUSED static inline void                                                     \
G_PASTE (clapper_tube_##lower, _cache_write_epoch) (const gchar *key,                \
    const gchar *val, gint64 epoch) {                                                \
  clapper_tube_cache_extractor_write_epoch (G_STRINGIFY (lower), key, val, epoch); }

/**
 * CLAPPER_TUBE_EXTRACTOR_DEFINE:
 * @camel: camel case name of the extractor.
 * @lower: lowercase name of the extractor, with multiple words separated by `_`.
 *
 * Convenient macro that wraps around #G_DEFINE_TYPE with #CLAPPER_TUBE_TYPE_EXTRACTOR.
 */
#define CLAPPER_TUBE_EXTRACTOR_DEFINE(camel,lower)                                    \
G_DEFINE_TYPE (ClapperTube##camel, clapper_tube_##lower, CLAPPER_TUBE_TYPE_EXTRACTOR) \

/**
 * CLAPPER_TUBE_EXTRACTOR_EXPORT_SCHEMES:
 * @...: %NULL terminated list of supported schemes.
 *
 * Convenient macro that exports extractor supported schemes.
 */
#define CLAPPER_TUBE_EXTRACTOR_EXPORT_SCHEMES(...)                \
static const gchar *_schemes_compat[] = { __VA_ARGS__ };          \
G_MODULE_EXPORT const gchar *const *extractor_get_schemes (void); \
const gchar *const *extractor_get_schemes (void) {                \
  return _schemes_compat; }

/**
 * CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS:
 * @...: %NULL terminated list of supported hosts.
 *
 * Convenient macro that exports extractor supported hosts.
 */
#define CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS(...)                \
static const gchar *_hosts_compat[] = { __VA_ARGS__ };          \
G_MODULE_EXPORT const gchar *const *extractor_get_hosts (void); \
const gchar *const *extractor_get_hosts (void) {                \
  return _hosts_compat; }

/**
 * CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS_FROM_FILE:
 * @lower: lowercase name of the extractor, with multiple words separated by `_`.
 *
 * Convenient macro that exports extractor supported hosts from
 * user provided config file.
 */
#define CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS_FROM_FILE(lower)                         \
G_MODULE_EXPORT const gchar *const *extractor_get_hosts (void);                      \
const gchar *const *extractor_get_hosts (void) {                                     \
  static GOnce _hosts_once = G_ONCE_INIT;                                            \
  g_once (&_hosts_once, (GThreadFunc) clapper_tube_config_read_extractor_hosts_file, \
      (const gchar *) G_STRINGIFY (G_PASTE (lower, _hosts)));                        \
  return (const gchar *const *) _hosts_once.retval; }

/**
 * CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS_FROM_FILE_WITH_FALLBACK:
 * @lower: lowercase name of the extractor, with multiple words separated by `_`.
 * @...: %NULL terminated list of supported hosts.
 *
 * Convenient macro that exports extractor supported hosts from
 * user provided config file and if it does not exist, uses
 * hardcoded list of hosts as fallback.
 */
#define CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS_FROM_FILE_WITH_FALLBACK(lower, ...)      \
static const gchar *_hosts_compat[] = { __VA_ARGS__ };                               \
G_MODULE_EXPORT const gchar *const *extractor_get_hosts (void);                      \
const gchar *const *extractor_get_hosts (void) {                                     \
  static GOnce _hosts_once = G_ONCE_INIT;                                            \
  g_once (&_hosts_once, (GThreadFunc) clapper_tube_config_read_extractor_hosts_file, \
      (gchar *) G_STRINGIFY (G_PASTE (lower, _hosts)));                              \
  return (_hosts_once.retval) ? (const gchar *const *) _hosts_once.retval            \
      : _hosts_compat; }

/**
 * CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS_FROM_FILE_WITH_PREPEND:
 * @lower: lowercase name of the extractor, with multiple words separated by `_`.
 * @...: %NULL terminated list of supported hosts.
 *
 * Convenient macro that exports extractor supported hosts from
 * user provided config file with prepended additional hosts
 * without touching said file.
 */
#define CLAPPER_TUBE_EXTRACTOR_EXPORT_HOSTS_FROM_FILE_WITH_PREPEND(lower, ...) \
G_MODULE_EXPORT const gchar *const *extractor_get_hosts (void);                \
static gpointer _prepend_hosts_once_cb (gpointer *data) {                      \
  return clapper_tube_config_read_extractor_hosts_file_with_prepend (          \
      (const gchar *) data, __VA_ARGS__); }                                    \
const gchar *const *extractor_get_hosts (void) {                               \
  static GOnce _hosts_once = G_ONCE_INIT;                                      \
  g_once (&_hosts_once, (GThreadFunc) _prepend_hosts_once_cb,                  \
      (gchar *) G_STRINGIFY (G_PASTE (lower, _hosts)));                        \
  return (const gchar *const *) _hosts_once.retval; }

/**
 * ClapperTubeExtractorClass:
 * @parent_class: The object class structure.
 * @extract: Extract data and fill harvest.
 */
struct _ClapperTubeExtractorClass
{
  GstObjectClass parent_class;

  /**
   * ClapperTubeExtractorClass::extract:
   * @extractor: a #ClapperTubeExtractor
   * @cancellable: a #GCancellable object
   * @error: a #GError
   *
   * Extract data and fill harvest.
   *
   * Returns: a #ClapperTubeFlow of extraction.
   */
  ClapperTubeFlow (* extract) (ClapperTubeExtractor *extractor, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[4];
};

CLAPPER_TUBE_API
GQuark clapper_tube_extractor_error_quark (void);

CLAPPER_TUBE_API
GUri * clapper_tube_extractor_get_uri (ClapperTubeExtractor *extractor);

CLAPPER_TUBE_API
void clapper_tube_extractor_set_uri (ClapperTubeExtractor *extractor, GUri *uri);

CLAPPER_TUBE_API
ClapperTubeHarvest * clapper_tube_extractor_get_harvest (ClapperTubeExtractor *extractor);

G_END_DECLS
