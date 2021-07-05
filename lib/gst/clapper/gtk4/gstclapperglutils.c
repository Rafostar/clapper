/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
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

/* FIXME: remove these once their headers are public in gstreamer:
 * https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/-/merge_requests/804
 */

#include "gstclapperglutils.h"

static const gfloat identity_matrix[] = {
  1.0, 0.0, 0.0, 0.0,
  0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0,
};

static const gfloat from_ndc_matrix[] = {
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 0.5, 0.0,
  0.5, 0.5, 0.5, 1.0,
};

static const gfloat to_ndc_matrix[] = {
  2.0, 0.0, 0.0, 0.0,
  0.0, 2.0, 0.0, 0.0,
  0.0, 0.0, 2.0, 0.0,
  -1.0, -1.0, -1.0, 1.0,
};

/* multiplies two 4x4 matrices, @a X @b, and stores the result in @result
 * https://en.wikipedia.org/wiki/Matrix_multiplication
 */
static void
gst_gl_multiply_matrix4 (const gfloat * a, const gfloat * b, gfloat * result)
{
  int i, j, k;
  gfloat tmp[16] = { 0.0f };

  if (!a || !b || !result)
    return;
  for (i = 0; i < 4; i++) {     /* column */
    for (j = 0; j < 4; j++) {   /* row */
      for (k = 0; k < 4; k++) {
        tmp[j + (i * 4)] += a[k + (i * 4)] * b[j + (k * 4)];
      }
    }
  }

  for (i = 0; i < 16; i++)
    result[i] = tmp[i];
}

/*
 * gst_clapper_gl_get_affine_transformation_meta_as_ndc:
 * @meta: (nullable): a #GstVideoAffineTransformationMeta
 * @matrix: (out): result of the 4x4 matrix
 *
 * Retrieves the stored 4x4 affine transformation matrix stored in @meta in
 * NDC coordinates. if @meta is NULL, an identity matrix is returned.
 *
 * NDC is a left-handed coordinate system
 * - x - [-1, 1] - +ve X moves right
 * - y - [-1, 1] - +ve Y moves up
 * - z - [-1, 1] - +ve Z moves into
 */
void
gst_clapper_gl_get_affine_transformation_meta_as_ndc (GstVideoAffineTransformationMeta *
    meta, gfloat * matrix)
{
  if (!meta) {
    int i;

    for (i = 0; i < 16; i++) {
      matrix[i] = identity_matrix[i];
    }
  } else {
    float tmp[16];

    /* change of basis multiplications */
    gst_gl_multiply_matrix4 (from_ndc_matrix, meta->matrix, tmp);
    gst_gl_multiply_matrix4 (tmp, to_ndc_matrix, matrix);
  }
}
