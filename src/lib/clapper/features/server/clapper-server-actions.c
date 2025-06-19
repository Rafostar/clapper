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
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include <gst/gst.h>

#include "clapper-server-actions-private.h"
#include "clapper-server-names-private.h"
#include "clapper-enums.h"

inline ClapperServerAction
clapper_server_actions_get_action (const gchar *text)
{
  /* Actions without arg(s) */
  if (strcmp (text, "toggle_play") == 0)
    return CLAPPER_SERVER_ACTION_TOGGLE_PLAY;
  if (strcmp (text, "play") == 0)
    return CLAPPER_SERVER_ACTION_PLAY;
  if (strcmp (text, "pause") == 0)
    return CLAPPER_SERVER_ACTION_PAUSE;
  if (strcmp (text, "stop") == 0)
    return CLAPPER_SERVER_ACTION_STOP;
  if (strcmp (text, "clear") == 0)
    return CLAPPER_SERVER_ACTION_CLEAR;

  /* Actions followed by space and arg(s) */
  if (g_str_has_prefix (text, "seek "))
    return CLAPPER_SERVER_ACTION_SEEK;
  if (g_str_has_prefix (text, "set_speed "))
    return CLAPPER_SERVER_ACTION_SET_SPEED;
  if (g_str_has_prefix (text, "set_volume "))
    return CLAPPER_SERVER_ACTION_SET_VOLUME;
  if (g_str_has_prefix (text, "set_mute "))
    return CLAPPER_SERVER_ACTION_SET_MUTE;
  if (g_str_has_prefix (text, "set_progression "))
    return CLAPPER_SERVER_ACTION_SET_PROGRESSION;
  if (g_str_has_prefix (text, "add "))
    return CLAPPER_SERVER_ACTION_ADD;
  if (g_str_has_prefix (text, "insert "))
    return CLAPPER_SERVER_ACTION_INSERT;
  if (g_str_has_prefix (text, "select "))
    return CLAPPER_SERVER_ACTION_SELECT;
  if (g_str_has_prefix (text, "remove "))
    return CLAPPER_SERVER_ACTION_REMOVE;

  return CLAPPER_SERVER_ACTION_INVALID;
}

static inline gboolean
_string_is_number (const gchar *string, gboolean decimal)
{
  guint i;

  for (i = 0; string[i] != '\0'; ++i) {
    if (!g_ascii_isdigit (string[i])) {
      if (decimal && string[i] == '.')
        continue;

      return FALSE;
    }
  }

  return (i > 0);
}

static gboolean
_parse_uint (const gchar *text, guint *val)
{
  gint64 tmp_val;

  if (!_string_is_number (text, FALSE))
    return FALSE;

  tmp_val = g_ascii_strtoll (text, NULL, 10);

  /* guint overflow check */
  if (tmp_val < 0 || tmp_val > G_MAXUINT)
    return FALSE;

  *val = (guint) tmp_val;

  return TRUE;
}

static gboolean
_parse_double (const gchar *text, gdouble *val)
{
  if (!_string_is_number (text, TRUE))
    return FALSE;

  *val = g_ascii_strtod (text, NULL);

  return TRUE;
}

static gboolean
_parse_boolean (const gchar *text, gboolean *val)
{
  gboolean res;

  if ((res = (strcmp (text, "true") == 0)))
    *val = TRUE;
  else if ((res = (strcmp (text, "false") == 0)))
    *val = FALSE;

  return res;
}

inline gboolean
clapper_server_actions_parse_seek (const gchar *text, gdouble *position)
{
  /* "seek" + whitespace = 5 */
  if (!_parse_double (text + 5, position))
    return FALSE;

  return (*position >= 0);
}

inline gboolean
clapper_server_actions_parse_set_speed (const gchar *text, gdouble *speed)
{
  /* "set_speed" + whitespace = 10 */
  return _parse_double (text + 10, speed);
}

inline gboolean
clapper_server_actions_parse_set_volume (const gchar *text, gdouble *volume)
{
  /* "set_volume" + whitespace = 11 */
  if (!_parse_double (text + 11, volume))
    return FALSE;

  if (*volume <= 0 || *volume > 2.0)
    return FALSE;

  return TRUE;
}

inline gboolean
clapper_server_actions_parse_set_mute (const gchar *text, gboolean *mute)
{
  /* "set_mute" + whitespace = 9 */
  return _parse_boolean (text + 9, mute);
}

inline gboolean
clapper_server_actions_parse_set_progression (const gchar *text, ClapperQueueProgressionMode *mode)
{
  gboolean res;

  /* "set_progression" + whitespace = 16 */
  text += 16;

  if ((res = (strcmp (text, CLAPPER_SERVER_QUEUE_PROGRESSION_NONE) == 0)))
    *mode = CLAPPER_QUEUE_PROGRESSION_NONE;
  else if ((res = (strcmp (text, CLAPPER_SERVER_QUEUE_PROGRESSION_CONSECUTIVE) == 0)))
    *mode = CLAPPER_QUEUE_PROGRESSION_CONSECUTIVE;
  else if ((res = (strcmp (text, CLAPPER_SERVER_QUEUE_PROGRESSION_REPEAT_ITEM) == 0)))
    *mode = CLAPPER_QUEUE_PROGRESSION_REPEAT_ITEM;
  else if ((res = (strcmp (text, CLAPPER_SERVER_QUEUE_PROGRESSION_CAROUSEL) == 0)))
    *mode = CLAPPER_QUEUE_PROGRESSION_CAROUSEL;
  else if ((res = (strcmp (text, CLAPPER_SERVER_QUEUE_PROGRESSION_SHUFFLE) == 0)))
    *mode = CLAPPER_QUEUE_PROGRESSION_SHUFFLE;

  return res;
}

inline gboolean
clapper_server_actions_parse_add (const gchar *text, const gchar **uri)
{
  /* "add" + whitespace = 4 */
  text += 4;

  /* No more spaces allowed */
  if (strchr (text, ' ') != NULL)
    return FALSE;

  if (!gst_uri_is_valid (text))
    return FALSE;

  *uri = text;

  return TRUE;
}

inline gboolean
clapper_server_actions_parse_insert (const gchar *text, gchar **uri, guint *after_id)
{
  gchar **data;
  gboolean res;

  /* "insert" + whitespace = 7 */
  text += 7;
  data = g_strsplit (text, " ", 2);

  if ((res = (g_strv_length (data) == 2
      && gst_uri_is_valid (data[0])
      && _parse_uint (data[1], after_id)))) {
    *uri = g_strdup (data[0]);
  }

  g_strfreev (data);

  return res;
}

inline gboolean
clapper_server_actions_parse_select (const gchar *text, guint *id)
{
  /* "select" + whitespace = 7 */
  return _parse_uint (text + 7, id);
}

inline gboolean
clapper_server_actions_parse_remove (const gchar *text, guint *id)
{
  /* "remove" + whitespace = 7 */
  return _parse_uint (text + 7, id);
}
