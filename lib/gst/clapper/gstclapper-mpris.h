/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_CLAPPER_MPRIS_H__
#define __GST_CLAPPER_MPRIS_H__

#include <glib.h>
#include <gio/gio.h>

#include <gst/clapper/clapper-prelude.h>

G_BEGIN_DECLS

typedef struct _GstClapperMpris GstClapperMpris;
typedef struct _GstClapperMprisClass GstClapperMprisClass;

#define GST_TYPE_CLAPPER_MPRIS             (gst_clapper_mpris_get_type ())
#define GST_IS_CLAPPER_MPRIS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CLAPPER_MPRIS))
#define GST_IS_CLAPPER_MPRIS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CLAPPER_MPRIS))
#define GST_CLAPPER_MPRIS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CLAPPER_MPRIS, GstClapperMprisClass))
#define GST_CLAPPER_MPRIS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CLAPPER_MPRIS, GstClapperMpris))
#define GST_CLAPPER_MPRIS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CLAPPER_MPRIS, GstClapperMprisClass))
#define GST_CLAPPER_MPRIS_CAST(obj)        ((GstClapperMpris*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstClapperMpris, g_object_unref)
#endif

GST_CLAPPER_API
GType gst_clapper_mpris_get_type                      (void);

GST_CLAPPER_API
GstClapperMpris * gst_clapper_mpris_new               (const gchar *own_name, const gchar *id_path, const gchar *identity,
                                                          const gchar *desktop_entry, const gchar *default_art_url);

G_END_DECLS

#endif /* __GST_CLAPPER_MPRIS_H__ */
