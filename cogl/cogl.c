/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <glib/gi18n-lib.h>

#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-winsys-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-matrix-private.h"
#include "cogl-journal-private.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-attribute-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-renderer-private.h"
#include "cogl-config-private.h"
#include "cogl-private.h"
#include "cogl1-context.h"
#include "cogl-offscreen.h"

#ifdef COGL_GL_DEBUG
/* GL error to string conversion */
static const struct {
  GLuint error_code;
  const char *error_string;
} gl_errors[] = {
  { GL_NO_ERROR,          "No error" },
  { GL_INVALID_ENUM,      "Invalid enumeration value" },
  { GL_INVALID_VALUE,     "Invalid value" },
  { GL_INVALID_OPERATION, "Invalid operation" },
#ifdef HAVE_COGL_GL
  { GL_STACK_OVERFLOW,    "Stack overflow" },
  { GL_STACK_UNDERFLOW,   "Stack underflow" },
#endif
  { GL_OUT_OF_MEMORY,     "Out of memory" },

#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
  { GL_INVALID_FRAMEBUFFER_OPERATION_EXT, "Invalid framebuffer operation" }
#endif
};

static const unsigned int n_gl_errors = G_N_ELEMENTS (gl_errors);

const char *
_cogl_gl_error_to_string (GLenum error_code)
{
  int i;

  for (i = 0; i < n_gl_errors; i++)
    {
      if (gl_errors[i].error_code == error_code)
        return gl_errors[i].error_string;
    }

  return "Unknown GL error";
}
#endif /* COGL_GL_DEBUG */

CoglBool
_cogl_check_extension (const char *name, const gchar *ext)
{
  char *end;
  int name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (char*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end)
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

CoglBool
cogl_has_feature (CoglContext *ctx, CoglFeatureID feature)
{
  return COGL_FLAGS_GET (ctx->features, feature);
}

CoglBool
cogl_has_features (CoglContext *ctx, ...)
{
  va_list args;
  CoglFeatureID feature;

  va_start (args, ctx);
  while ((feature = va_arg (args, CoglFeatureID)))
    if (!cogl_has_feature (ctx, feature))
      return FALSE;
  va_end (args);

  return TRUE;
}

void
cogl_foreach_feature (CoglContext *ctx,
                      CoglFeatureCallback callback,
                      void *user_data)
{
  int i;
  for (i = 0; i < _COGL_N_FEATURE_IDS; i++)
    if (COGL_FLAGS_GET (ctx->features, i))
      callback (i, user_data);
}

void
cogl_flush (void)
{
  GList *l;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (l = ctx->framebuffers; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
}

uint32_t
_cogl_driver_error_domain (void)
{
  return g_quark_from_static_string ("cogl-driver-error-quark");
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

/* Transform a homogeneous vertex position from model space to Cogl
 * window coordinates (with 0,0 being top left) */
void
_cogl_transform_point (const CoglMatrix *matrix_mv,
                       const CoglMatrix *matrix_p,
                       const float *viewport,
                       float *x,
                       float *y)
{
  float z = 0;
  float w = 1;

  /* Apply the modelview matrix transform */
  cogl_matrix_transform_point (matrix_mv, x, y, &z, &w);

  /* Apply the projection matrix transform */
  cogl_matrix_transform_point (matrix_p, x, y, &z, &w);

  /* Perform perspective division */
  *x /= w;
  *y /= w;

  /* Apply viewport transform */
  *x = VIEWPORT_TRANSFORM_X (*x, viewport[0], viewport[2]);
  *y = VIEWPORT_TRANSFORM_Y (*y, viewport[1], viewport[3]);
}

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y

uint32_t
_cogl_system_error_domain (void)
{
  return g_quark_from_static_string ("cogl-system-error-quark");
}

void
_cogl_init (void)
{
  static size_t init_status = 0;

  if (g_once_init_enter (&init_status))
    {
      bindtextdomain (GETTEXT_PACKAGE, COGL_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

      g_type_init ();

      _cogl_config_read ();
      _cogl_debug_check_environment ();
      g_once_init_leave (&init_status, 1);
    }
}

/*
 * Returns the number of bytes-per-pixel of a given format. The bpp
 * can be extracted from the least significant nibble of the pixel
 * format (see CoglPixelFormat).
 *
 * The mapping is the following (see discussion on bug #660188):
 *
 * 0     = undefined
 * 1, 8  = 1 bpp (e.g. A_8, G_8)
 * 2     = 3 bpp, aligned (e.g. 888)
 * 3     = 4 bpp, aligned (e.g. 8888)
 * 4-6   = 2 bpp, not aligned (e.g. 565, 4444, 5551)
 * 7     = undefined yuv
 * 9     = 2 bpp, aligned
 * 10     = undefined
 * 11     = undefined
 * 12    = 3 bpp, not aligned
 * 13    = 4 bpp, not aligned (e.g. 2101010)
 * 14-15 = undefined
 */
int
_cogl_pixel_format_get_bytes_per_pixel (CoglPixelFormat format)
{
  int bpp_lut[] = { 0, 1, 3, 4,
                    2, 2, 2, 0,
                    1, 2, 0, 0,
                    3, 4, 0, 0 };

  return bpp_lut [format & 0xf];
}

/* Note: this also refers to the mapping defined above for
 * _cogl_pixel_format_get_bytes_per_pixel() */
CoglBool
_cogl_pixel_format_is_endian_dependant (CoglPixelFormat format)
{
  int aligned_lut[] = { -1, 1,  1,  1,
                         0, 0,  0, -1,
                         1, 1, -1, -1,
                         0, 0, -1, -1};
  int aligned = aligned_lut[format & 0xf];

  _COGL_RETURN_VAL_IF_FAIL (aligned != -1, FALSE);

  /* NB: currently checking whether the format components are aligned
   * or not determines whether the format is endian dependent or not.
   * In the future though we might consider adding formats with
   * aligned components that are also endian independant. */

  return aligned;
}
