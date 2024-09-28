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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "clapper-extractors-manager-private.h"
#include "clapper-extractor-private.h"
#include "clapper-plugins-loader-private.h"

#define GST_CAT_DEFAULT clapper_extractors_manager_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperExtractorsManager
{
  ClapperThreadedObject parent;
};

#define parent_class clapper_extractors_manager_parent_class
G_DEFINE_TYPE (ClapperExtractorsManager, clapper_extractors_manager, CLAPPER_TYPE_THREADED_OBJECT);

static ClapperExtractorsManager *_manager = NULL;
static GMutex manager_lock;

/*
 * clapper_extractors_manager_get_default:
 *
 * Returns: (transfer none): a #ClapperExtractorsManager instance.
 */
ClapperExtractorsManager *
clapper_extractors_manager_get_default (void)
{
  g_mutex_lock (&manager_lock);
  if (!_manager)
    _manager = g_object_new (CLAPPER_TYPE_EXTRACTORS_MANAGER, NULL);
  g_mutex_unlock (&manager_lock);

  return _manager;
}

static void
clapper_extractors_manager_thread_start (ClapperThreadedObject *threaded_object)
{
  ClapperExtractorsManager *self = CLAPPER_EXTRACTORS_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Extractors manager thread start");
}

static void
clapper_extractors_manager_thread_stop (ClapperThreadedObject *threaded_object)
{
  ClapperExtractorsManager *self = CLAPPER_EXTRACTORS_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Extractors manager thread stop");
}

static void
clapper_extractors_manager_init (ClapperExtractorsManager *self)
{
}

static void
clapper_extractors_manager_class_init (ClapperExtractorsManagerClass *klass)
{
  ClapperThreadedObjectClass *threaded_object = (ClapperThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperextractorsmanager", 0,
      "Clapper Extractors Manager");

  threaded_object->thread_start = clapper_extractors_manager_thread_start;
  threaded_object->thread_stop = clapper_extractors_manager_thread_stop;
}
