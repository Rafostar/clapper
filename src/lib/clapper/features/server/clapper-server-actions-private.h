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

#pragma once

#include <glib.h>

#include "clapper-enums.h"

G_BEGIN_DECLS

typedef enum
{
  CLAPPER_SERVER_ACTION_INVALID = 0,
  CLAPPER_SERVER_ACTION_TOGGLE_PLAY,
  CLAPPER_SERVER_ACTION_PLAY,
  CLAPPER_SERVER_ACTION_PAUSE,
  CLAPPER_SERVER_ACTION_STOP,
  CLAPPER_SERVER_ACTION_SEEK,
  CLAPPER_SERVER_ACTION_SET_SPEED,
  CLAPPER_SERVER_ACTION_SET_VOLUME,
  CLAPPER_SERVER_ACTION_SET_MUTE,
  CLAPPER_SERVER_ACTION_SET_PROGRESSION,
  CLAPPER_SERVER_ACTION_ADD,
  CLAPPER_SERVER_ACTION_INSERT,
  CLAPPER_SERVER_ACTION_SELECT,
  CLAPPER_SERVER_ACTION_REMOVE,
  CLAPPER_SERVER_ACTION_CLEAR
} ClapperServerAction;

G_GNUC_INTERNAL
ClapperServerAction clapper_server_actions_get_action (const gchar *text);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_seek (const gchar *text, gdouble *position);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_set_speed (const gchar *text, gdouble *speed);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_set_volume (const gchar *text, gdouble *volume);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_set_mute (const gchar *text, gboolean *mute);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_set_progression (const gchar *text, ClapperQueueProgressionMode *mode);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_add (const gchar *text, const gchar **uri);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_insert (const gchar *text, gchar **uri, guint *after_id);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_select (const gchar *text, guint *id);

G_GNUC_INTERNAL
gboolean clapper_server_actions_parse_remove (const gchar *text, guint *id);

G_END_DECLS
