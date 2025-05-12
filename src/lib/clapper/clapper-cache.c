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

#include "clapper-cache-private.h"
#include "clapper-version.h"
#include "clapper-extractable.h"

#define CLAPPER_CACHE_HEADER "CLAPPER"

typedef enum
{
  CLAPPER_CACHE_IFACE_EXTRACTABLE = 1,
} ClapperCacheIfaces;

static GArray *enum_registry = NULL;
static GArray *flags_registry = NULL;
static gboolean cache_disabled = FALSE;

void
clapper_cache_initialize (void)
{
  const gchar *env = g_getenv ("CLAPPER_DISABLE_CACHE");

  if (G_LIKELY (!env || !g_str_has_prefix (env, "1"))) {
    enum_registry = g_array_new (FALSE, TRUE, sizeof (GEnumValue *));
    flags_registry = g_array_new (FALSE, TRUE, sizeof (GFlagsValue *));
  } else {
    cache_disabled = TRUE;
  }
}

GMappedFile *
clapper_cache_open (const gchar *filename, const gchar **data, GError **error)
{
  GMappedFile *file;

  if (G_UNLIKELY (cache_disabled))
    return NULL;

  if (!(file = g_mapped_file_new (filename, FALSE, error)))
    return NULL;

  if (G_UNLIKELY (g_mapped_file_get_length (file) == 0)) {
    g_mapped_file_unref (file);
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "File is empty");
    return NULL;
  }

  *data = g_mapped_file_get_contents (file);

  /* Header name check */
  if (G_UNLIKELY (g_strcmp0 (*data, CLAPPER_CACHE_HEADER) != 0)) {
    g_mapped_file_unref (file);
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "Invalid file header");
    return NULL;
  }
  *data += strlen (*data) + 1;

  /* Header version check */
  if (clapper_cache_read_uint (data) != CLAPPER_VERSION_HEX) {
    g_mapped_file_unref (file);
    /* Just different version, so no error set */
    return NULL;
  }

  return file;
}

inline gboolean
clapper_cache_read_boolean (const gchar **data)
{
  gboolean val = *(const gboolean *) *data;
  *data += sizeof (gboolean);

  return val;
}

inline gint
clapper_cache_read_int (const gchar **data)
{
  gint val = *(const gint *) *data;
  *data += sizeof (gint);

  return val;
}

inline guint
clapper_cache_read_uint (const gchar **data)
{
  guint val = *(const guint *) *data;
  *data += sizeof (guint);

  return val;
}

inline gdouble
clapper_cache_read_double (const gchar **data)
{
  gdouble val = *(const gdouble *) *data;
  *data += sizeof (gdouble);

  return val;
}

inline const gchar *
clapper_cache_read_string (const gchar **data)
{
  const gboolean is_null = clapper_cache_read_boolean (data);
  const gchar *str = NULL;

  if (!is_null) {
    str = *data;
    *data += strlen (str) + 1;
  }

  return str;
}

inline GType
clapper_cache_read_enum (const gchar **data)
{
  GType type;
  const gchar *enum_name;
  guint i, n_values;

  enum_name = clapper_cache_read_string (data);
  n_values = clapper_cache_read_uint (data);

  /* If not registered yet */
  if ((type = g_type_from_name (enum_name)) == 0) {
    GEnumValue *values = g_new0 (GEnumValue, n_values + 1);

    for (i = 0; i < n_values; ++i) {
      values[i].value = clapper_cache_read_int (data);
      values[i].value_name = g_intern_string (clapper_cache_read_string (data));
      values[i].value_nick = g_intern_string (clapper_cache_read_string (data));
    }
    g_array_append_val (enum_registry, values); // store statically

    type = g_enum_register_static (g_intern_string (enum_name),
        g_array_index (enum_registry, GEnumValue *, enum_registry->len - 1));
  } else {
    /* Skip over data */
    for (i = 0; i < n_values; ++i) {
      clapper_cache_read_int (data); // value
      clapper_cache_read_string (data); // value_name
      clapper_cache_read_string (data); // value_nick
    }
  }

  return type;
}

inline GType
clapper_cache_read_flags (const gchar **data)
{
  GType type;
  const gchar *flags_name;
  guint i, n_values;

  flags_name = clapper_cache_read_string (data);
  n_values = clapper_cache_read_uint (data);

  /* If not registered yet */
  if ((type = g_type_from_name (flags_name)) == 0) {
    GFlagsValue *values = g_new0 (GFlagsValue, n_values + 1);

    for (i = 0; i < n_values; ++i) {
      values[i].value = clapper_cache_read_int (data);
      values[i].value_name = g_intern_string (clapper_cache_read_string (data));
      values[i].value_nick = g_intern_string (clapper_cache_read_string (data));
    }
    g_array_append_val (flags_registry, values); // store statically

    type = g_flags_register_static (g_intern_string (flags_name),
        g_array_index (flags_registry, GFlagsValue *, flags_registry->len - 1));
  } else {
    /* Skip over data */
    for (i = 0; i < n_values; ++i) {
      clapper_cache_read_int (data); // value
      clapper_cache_read_string (data); // value_name
      clapper_cache_read_string (data); // value_nick
    }
  }

  return type;
}

GType
clapper_cache_read_iface (const gchar **data)
{
  gint iface_id = clapper_cache_read_int (data);

  switch (iface_id) {
    case CLAPPER_CACHE_IFACE_EXTRACTABLE:
      return CLAPPER_TYPE_EXTRACTABLE;
    default:
      return 0;
  }
}

GParamSpec *
clapper_cache_read_pspec (const gchar **data)
{
  GParamSpec *pspec;
  GType value_type;
  const gchar *name, *nick, *blurb;
  GParamFlags flags;

  value_type = *(const GType *) *data;
  *data += sizeof (GType);

  name = clapper_cache_read_string (data);
  nick = clapper_cache_read_string (data);
  blurb = clapper_cache_read_string (data);

  flags = *(const GParamFlags *) *data;
  *data += sizeof (GParamFlags);

  /* NOTE: C does not guarantee order in which function arguments
   * are evaluated, so read into variables and then create pspec  */

  switch (value_type) {
    case G_TYPE_BOOLEAN:
      pspec = g_param_spec_boolean (name, nick, blurb,
          clapper_cache_read_boolean (data), flags);
      break;
    case G_TYPE_INT:{
      gint minimum = clapper_cache_read_int (data);
      gint maximum = clapper_cache_read_int (data);
      gint default_value = clapper_cache_read_int (data);

      pspec = g_param_spec_int (name, nick, blurb,
          minimum, maximum, default_value, flags);
      break;
    }
    case G_TYPE_UINT:{
      guint minimum = clapper_cache_read_uint (data);
      guint maximum = clapper_cache_read_uint (data);
      guint default_value = clapper_cache_read_uint (data);

      pspec = g_param_spec_uint (name, nick, blurb,
          minimum, maximum, default_value, flags);
      break;
    }
    case G_TYPE_DOUBLE:{
      gdouble minimum = clapper_cache_read_double (data);
      gdouble maximum = clapper_cache_read_double (data);
      gdouble default_value = clapper_cache_read_double (data);

      pspec = g_param_spec_double (name, nick, blurb,
          minimum, maximum, default_value, flags);
      break;
    }
    case G_TYPE_STRING:
      pspec = g_param_spec_string (name, nick, blurb,
          clapper_cache_read_string (data), flags);
      break;
    case G_TYPE_ENUM:{
      GType enum_type = clapper_cache_read_enum (data);
      gint default_value = clapper_cache_read_int (data);

      pspec = g_param_spec_enum (name, nick, blurb,
          enum_type, default_value, flags);
      break;
    }
    case G_TYPE_FLAGS:{
      GType flags_type = clapper_cache_read_flags (data);
      guint default_value = clapper_cache_read_uint (data);

      pspec = g_param_spec_flags (name, nick, blurb,
          flags_type, default_value, flags);
      break;
    }
    default:
      return NULL;
  }

  return g_param_spec_ref_sink (pspec);
}

GByteArray *
clapper_cache_create (void)
{
  GByteArray *bytes;

  if (G_UNLIKELY (cache_disabled))
    return NULL;

  bytes = g_byte_array_new ();

  /* NOTE: We do not store whether string is NULL here, since it never is */
  g_byte_array_append (bytes, (const guint8 *) CLAPPER_CACHE_HEADER, 8); // 7 + 1
  clapper_cache_store_uint (bytes, CLAPPER_VERSION_HEX);

  return bytes;
}

inline void
clapper_cache_store_boolean (GByteArray *bytes, gboolean val)
{
  g_byte_array_append (bytes, (const guint8 *) &val, sizeof (gboolean));
}

inline void
clapper_cache_store_int (GByteArray *bytes, gint val)
{
  g_byte_array_append (bytes, (const guint8 *) &val, sizeof (gint));
}

inline void
clapper_cache_store_uint (GByteArray *bytes, guint val)
{
  g_byte_array_append (bytes, (const guint8 *) &val, sizeof (guint));
}

inline void
clapper_cache_store_double (GByteArray *bytes, gdouble val)
{
  g_byte_array_append (bytes, (const guint8 *) &val, sizeof (gdouble));
}

inline void
clapper_cache_store_string (GByteArray *bytes, const gchar *val)
{
  /* Distinguish empty string from NULL */
  const gboolean is_null = (val == NULL);

  clapper_cache_store_boolean (bytes, is_null);
  if (!is_null)
    g_byte_array_append (bytes, (const guint8 *) val, strlen (val) + 1);
}

inline void
clapper_cache_store_enum (GByteArray *bytes, GType enum_type)
{
  GEnumClass *enum_class = G_ENUM_CLASS (g_type_class_peek (enum_type));
  guint i;

  clapper_cache_store_string (bytes, g_type_name (enum_type));
  clapper_cache_store_uint (bytes, enum_class->n_values);

  for (i = 0; i < enum_class->n_values; ++i) {
    clapper_cache_store_int (bytes, enum_class->values[i].value);
    clapper_cache_store_string (bytes, enum_class->values[i].value_name);
    clapper_cache_store_string (bytes, enum_class->values[i].value_nick);
  }
}

inline void
clapper_cache_store_flags (GByteArray *bytes, GType flags_type)
{
  GFlagsClass *flags_class = G_FLAGS_CLASS (g_type_class_peek (flags_type));
  guint i;

  clapper_cache_store_string (bytes, g_type_name (flags_type));
  clapper_cache_store_uint (bytes, flags_class->n_values);

  for (i = 0; i < flags_class->n_values; ++i) {
    clapper_cache_store_int (bytes, flags_class->values[i].value);
    clapper_cache_store_string (bytes, flags_class->values[i].value_name);
    clapper_cache_store_string (bytes, flags_class->values[i].value_nick);
  }
}

gboolean
clapper_cache_store_iface (GByteArray *bytes, GType iface)
{
  gint iface_id = 0;

  if (iface == CLAPPER_TYPE_EXTRACTABLE)
    iface_id = CLAPPER_CACHE_IFACE_EXTRACTABLE;
  else
    return FALSE;

  clapper_cache_store_int (bytes, iface_id);
  return TRUE;
}

gboolean
clapper_cache_store_pspec (GByteArray *bytes, GParamSpec *pspec)
{
  GParamFlags flags;
  const gboolean is_enum = G_IS_PARAM_SPEC_ENUM (pspec);
  const gboolean is_flags = (!is_enum && G_IS_PARAM_SPEC_FLAGS (pspec));

  if (is_enum) {
    GType enum_type = G_TYPE_ENUM;
    g_byte_array_append (bytes, (const guint8 *) &enum_type, sizeof (GType));
  } else if (is_flags) {
    GType flags_type = G_TYPE_FLAGS;
    g_byte_array_append (bytes, (const guint8 *) &flags_type, sizeof (GType));
  } else {
    g_byte_array_append (bytes, (const guint8 *) &pspec->value_type, sizeof (GType));
  }

  clapper_cache_store_string (bytes, g_param_spec_get_name (pspec));
  clapper_cache_store_string (bytes, g_param_spec_get_nick (pspec));
  clapper_cache_store_string (bytes, g_param_spec_get_blurb (pspec));

  flags = pspec->flags;
  flags &= ~G_PARAM_STATIC_STRINGS; // Data read from cache is never static
  g_byte_array_append (bytes, (const guint8 *) &flags, sizeof (GParamFlags));

  switch (pspec->value_type) {
    case G_TYPE_BOOLEAN:{
      GParamSpecBoolean *p = (GParamSpecBoolean *) pspec;
      clapper_cache_store_boolean (bytes, p->default_value);
      break;
    }
    case G_TYPE_INT:{
      GParamSpecInt *p = (GParamSpecInt *) pspec;
      clapper_cache_store_int (bytes, p->minimum);
      clapper_cache_store_int (bytes, p->maximum);
      clapper_cache_store_int (bytes, p->default_value);
      break;
    }
    case G_TYPE_UINT:{
      GParamSpecUInt *p = (GParamSpecUInt *) pspec;
      clapper_cache_store_uint (bytes, p->minimum);
      clapper_cache_store_uint (bytes, p->maximum);
      clapper_cache_store_uint (bytes, p->default_value);
      break;
    }
    case G_TYPE_DOUBLE:{
      GParamSpecDouble *p = (GParamSpecDouble *) pspec;
      clapper_cache_store_double (bytes, p->minimum);
      clapper_cache_store_double (bytes, p->maximum);
      clapper_cache_store_double (bytes, p->default_value);
      break;
    }
    case G_TYPE_STRING:{
      GParamSpecString *p = (GParamSpecString *) pspec;
      clapper_cache_store_string (bytes, p->default_value);
      break;
    }
    default:{
      if (is_enum) {
        GParamSpecEnum *p = (GParamSpecEnum *) pspec;
        clapper_cache_store_enum (bytes, pspec->value_type);
        clapper_cache_store_int (bytes, p->default_value);
        break;
      } else if (is_flags) {
        GParamSpecFlags *p = (GParamSpecFlags *) pspec;
        clapper_cache_store_flags (bytes, pspec->value_type);
        clapper_cache_store_uint (bytes, p->default_value);
        break;
      }
      return FALSE;
    }
  }

  return TRUE;
}

gboolean
clapper_cache_write (const gchar *filename, GByteArray *bytes, GError **error)
{
  gchar *dirname = g_path_get_dirname (filename);
  gboolean has_dir;

  has_dir = (g_mkdir_with_parents (dirname, 0755) == 0);
  g_free (dirname);

  if (!has_dir) {
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "Could not create directory to store cache content");
    return FALSE;
  }

  /* Using "g_file_set_contents" to replace file atomically */
  return g_file_set_contents (filename, (const gchar *) bytes->data, bytes->len, error);
}
