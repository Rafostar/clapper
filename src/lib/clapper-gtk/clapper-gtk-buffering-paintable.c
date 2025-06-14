/* Clapper GTK Integration Library
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

#include "clapper-gtk-buffering-paintable-private.h"

#define CIRCLE_MAX_SIZE 48
#define CIRCLE_SPACING 10
#define CIRCLE_OUTLINE 2
#define INTRINSIC_SIZE 184 // 3 * CIRCLE_MAX_SIZE + 4 * CIRCLE_SPACING

#define BLACK ((GdkRGBA) { 0, 0, 0, 1 })
#define WHITE ((GdkRGBA) { 1, 1, 1, 1 })

struct _ClapperGtkBufferingPaintable
{
  GObject parent;

  gfloat sizes[3]; // current size of each circle
  gboolean reverses[3]; // grow/shrink direction
  gboolean initialized[3]; // big enough to start growing the next one
};

static GdkPaintableFlags
clapper_gtk_buffering_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE;
}

static gint
clapper_gtk_buffering_paintable_get_intrinsic_size (GdkPaintable *paintable)
{
  return INTRINSIC_SIZE;
}

static void
_draw_scaled_circle (GdkSnapshot *snapshot, gfloat scale)
{
  GskRoundedRect outline;
  gfloat half_size = ((gfloat) CIRCLE_MAX_SIZE / 2) * scale;
  gfloat inside_size = ((gfloat) (CIRCLE_MAX_SIZE - 2 * CIRCLE_OUTLINE) / 2) * scale;

  /* Draw white inner circle */
  gsk_rounded_rect_init_from_rect (&outline,
      &GRAPHENE_RECT_INIT (-inside_size, -inside_size, 2 * inside_size, 2 * inside_size), inside_size);
  gtk_snapshot_append_border (snapshot, &outline,
      (gfloat[4]) { inside_size, inside_size, inside_size, inside_size },
      (GdkRGBA[4]) { WHITE, WHITE, WHITE, WHITE });

  /* Draw black circle border */
  gsk_rounded_rect_init_from_rect (&outline,
      &GRAPHENE_RECT_INIT (-half_size, -half_size, 2 * half_size, 2 * half_size), half_size);
  gtk_snapshot_append_border (snapshot, &outline,
      (gfloat[4]) { CIRCLE_OUTLINE, CIRCLE_OUTLINE, CIRCLE_OUTLINE, CIRCLE_OUTLINE },
      (GdkRGBA[4]) { BLACK, BLACK, BLACK, BLACK });
}

static void
clapper_gtk_buffering_paintable_snapshot (GdkPaintable *paintable,
    GdkSnapshot *snapshot, gdouble width, gdouble height)
{
  ClapperGtkBufferingPaintable *self = CLAPPER_GTK_BUFFERING_PAINTABLE_CAST (paintable);
  guint i;

  gtk_snapshot_save (snapshot);

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, height / 2));
  gtk_snapshot_scale (snapshot, MIN (width, height) / INTRINSIC_SIZE, MIN (width, height) / INTRINSIC_SIZE);

  for (i = 0; i < 3; ++i) {
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (CIRCLE_SPACING + CIRCLE_MAX_SIZE / 2, 0));
    _draw_scaled_circle (snapshot, self->sizes[i]);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (CIRCLE_MAX_SIZE / 2, 0));
  }

  gtk_snapshot_restore (snapshot);
}

static GdkPaintable *
clapper_gtk_buffering_paintable_get_current_image (GdkPaintable *paintable)
{
  ClapperGtkBufferingPaintable *self = CLAPPER_GTK_BUFFERING_PAINTABLE_CAST (paintable);
  ClapperGtkBufferingPaintable *copy = clapper_gtk_buffering_paintable_new ();
  guint i;

  /* Only current sizes are needed for static image */
  for (i = 0; i < 3; ++i)
    copy->sizes[i] = self->sizes[i];

  return GDK_PAINTABLE (copy);
}

static void
_paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->get_flags = clapper_gtk_buffering_paintable_get_flags;
  iface->get_intrinsic_width = clapper_gtk_buffering_paintable_get_intrinsic_size;
  iface->get_intrinsic_height = clapper_gtk_buffering_paintable_get_intrinsic_size;
  iface->snapshot = clapper_gtk_buffering_paintable_snapshot;
  iface->get_current_image = clapper_gtk_buffering_paintable_get_current_image;
}

#define parent_class clapper_gtk_buffering_paintable_parent_class
G_DEFINE_TYPE_WITH_CODE (ClapperGtkBufferingPaintable, clapper_gtk_buffering_paintable, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, _paintable_iface_init))

ClapperGtkBufferingPaintable *
clapper_gtk_buffering_paintable_new (void)
{
  return g_object_new (CLAPPER_GTK_TYPE_BUFFERING_PAINTABLE, NULL);
}

void
clapper_gtk_buffering_paintable_step (ClapperGtkBufferingPaintable *self)
{
  guint i;

  for (i = 0; i < 3; ++i) {
    /* If previous circle is not big enough
     * do not start animating the next one */
    if (i > 0 && !self->initialized[i - 1])
      break;

    if (!self->initialized[i] && self->sizes[i] >= 0.3)
      self->initialized[i] = TRUE;

    self->sizes[i] += (self->reverses[i]) ? -0.04 : 0.04;
    if (self->sizes[i] > 1.0) {
      self->sizes[i] = 1.0;
      self->reverses[i] = TRUE;
    } else if (self->sizes[i] < 0.0) {
      self->sizes[i] = 0.0;
      self->reverses[i] = FALSE;
    }
  }

  gdk_paintable_invalidate_contents ((GdkPaintable *) self);
}

void
clapper_gtk_buffering_paintable_reset (ClapperGtkBufferingPaintable *self)
{
  guint i;

  for (i = 0; i < 3; ++i) {
    self->sizes[i] = 0;
    self->reverses[i] = FALSE;
    self->initialized[i] = FALSE;
  }

  gdk_paintable_invalidate_contents ((GdkPaintable *) self);
}

static void
clapper_gtk_buffering_paintable_init (ClapperGtkBufferingPaintable *self)
{
}

static void
clapper_gtk_buffering_paintable_class_init (ClapperGtkBufferingPaintableClass *klass)
{
}
