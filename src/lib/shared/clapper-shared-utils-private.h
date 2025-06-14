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

G_BEGIN_DECLS

gpointer clapper_shared_utils_context_invoke_sync (GMainContext *context, GThreadFunc func, gpointer data);

gpointer clapper_shared_utils_context_invoke_sync_full (GMainContext *context, GThreadFunc func, gpointer data, GDestroyNotify destroy_func);

GSource * clapper_shared_utils_context_timeout_add_full (GMainContext *context, gint priority, guint interval, GSourceFunc func, gpointer data, GDestroyNotify destroy_func);

G_END_DECLS
