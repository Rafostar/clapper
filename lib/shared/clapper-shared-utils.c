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

#include "clapper-shared-utils-private.h"

struct ClapperSharedUtilsInvoke
{
  GMutex lock;
  GCond cond;
  gboolean fired;
  GThreadFunc func;
  gpointer data;

  gpointer res;
};

static gboolean
_invoke_func (struct ClapperSharedUtilsInvoke *invoke)
{
  g_mutex_lock (&invoke->lock);
  invoke->res = invoke->func (invoke->data);
  invoke->fired = TRUE;
  g_cond_signal (&invoke->cond);
  g_mutex_unlock (&invoke->lock);

  return G_SOURCE_REMOVE;
}

inline gpointer
clapper_shared_utils_context_invoke_sync (GMainContext *context, GThreadFunc func, gpointer data)
{
  struct ClapperSharedUtilsInvoke invoke;

  g_mutex_init (&invoke.lock);
  g_cond_init (&invoke.cond);
  invoke.fired = FALSE;
  invoke.func = func;
  invoke.data = data;

  g_main_context_invoke (context,
      (GSourceFunc) _invoke_func, &invoke);

  g_mutex_lock (&invoke.lock);
  while (!invoke.fired)
    g_cond_wait (&invoke.cond, &invoke.lock);
  g_mutex_unlock (&invoke.lock);

  g_mutex_clear (&invoke.lock);
  g_cond_clear (&invoke.cond);

  return invoke.res;
}

inline gpointer
clapper_shared_utils_context_invoke_sync_full (GMainContext *context, GThreadFunc func, gpointer data, GDestroyNotify destroy_func)
{
  gpointer res = clapper_shared_utils_context_invoke_sync (context, func, data);

  if (destroy_func)
    destroy_func (data);

  return res;
}

inline GSource *
clapper_shared_utils_context_timeout_add_full (GMainContext *context, gint priority, guint interval,
    GSourceFunc func, gpointer data, GDestroyNotify destroy_func)
{
  GSource *source = g_timeout_source_new (interval);

  g_source_set_priority (source, priority);
  g_source_set_callback (source, func, data, destroy_func);
  g_source_attach (source, context);

  return source;
}
