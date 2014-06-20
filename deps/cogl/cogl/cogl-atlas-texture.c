/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#include <config.h>

#include "cogl-debug.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-atlas-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-texture-driver.h"
#include "cogl-rectangle-map.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-atlas-set-private.h"
#include "cogl-atlas-private.h"
#include "cogl-sub-texture.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"
#include "cogl-private.h"

#include <stdlib.h>

static void _cogl_atlas_texture_free (CoglAtlasTexture *sub_tex);

COGL_TEXTURE_DEFINE (AtlasTexture, atlas_texture);

static const CoglTextureVtable cogl_atlas_texture_vtable;

static CoglSubTexture *
_cogl_atlas_texture_create_sub_texture (CoglTexture *full_texture,
                                        const CoglAtlasAllocation *allocation)
{
  CoglContext *ctx = full_texture->context;
  /* Create a subtexture for the given rectangle not including the
     1-pixel border */
  return cogl_sub_texture_new (ctx,
                               full_texture,
                               allocation->x + 1,
                               allocation->y + 1,
                               allocation->width - 2,
                               allocation->height - 2);
}

static void
_cogl_atlas_texture_allocate_cb (CoglAtlas *atlas,
                                 CoglTexture *texture,
                                 const CoglAtlasAllocation *allocation,
                                 void *allocation_data,
                                 void *user_data)
{
  CoglAtlasTexture *atlas_tex = allocation_data;

  /* Update the sub texture */
  if (atlas_tex->sub_texture)
    cogl_object_unref (atlas_tex->sub_texture);
  atlas_tex->sub_texture = COGL_TEXTURE (
    _cogl_atlas_texture_create_sub_texture (texture, allocation));

  /* Update the position */
  atlas_tex->allocation = *allocation;
  atlas_tex->atlas = cogl_object_ref (atlas);
}

static void
_cogl_atlas_texture_pre_reorganize_foreach_cb
                                         (CoglAtlas *atlas,
                                          const CoglAtlasAllocation *allocation,
                                          void *allocation_data,
                                          void *user_data)
{
  CoglAtlasTexture *atlas_tex = allocation_data;

  /* Keep a reference to the texture because we don't want it to be
     destroyed during the reorganization */
  cogl_object_ref (atlas_tex);
}

static void
_cogl_atlas_texture_pre_reorganize_cb (CoglAtlas *atlas,
                                       void *user_data)
{
  CoglContext *ctx = user_data;

  /* We don't know if any journal entries currently depend on OpenGL
   * texture coordinates that would be invalidated by reorganizing
   * this atlas so we flush all journals before migrating.
   *
   * We are assuming that texture atlas migration never happens
   * during a flush so we don't have to consider recursion here.
   */
  _cogl_flush (ctx);

  cogl_atlas_foreach (atlas,
                      _cogl_atlas_texture_pre_reorganize_foreach_cb,
                      NULL);
}

typedef struct
{
  CoglAtlasTexture **textures;
  /* Number of textures found so far */
  int n_textures;
} CoglAtlasTextureGetAllocationsData;

static void
_cogl_atlas_texture_get_allocations_cb (CoglAtlas *atlas,
                                        const CoglAtlasAllocation *allocation,
                                        void *allocation_data,
                                        void *user_data)
{
  CoglAtlasTextureGetAllocationsData *data = user_data;

  data->textures[data->n_textures++] = allocation_data;
}

static void
_cogl_atlas_texture_post_reorganize_cb (CoglAtlas *atlas,
                                        void *user_data)
{
  int n_allocations = cogl_atlas_get_n_allocations (atlas);

  if (n_allocations)
    {
      CoglAtlasTextureGetAllocationsData data;
      int i;

      data.textures = c_alloca (sizeof (CoglAtlasTexture *) * n_allocations);
      data.n_textures = 0;

      /* We need to remove all of the references that we took during
       * the preorganize callback. We have to get a separate array of
       * the textures because CoglAtlas doesn't support removing
       * allocations during iteration */
      cogl_atlas_foreach (atlas,
                          _cogl_atlas_texture_get_allocations_cb,
                          &data);

      for (i = 0; i < data.n_textures; i++)
        {
          /* Ignore textures that don't have an atlas yet. This will
             happen when a new texture is added because we allocate
             the structure for the texture so that it can get stored
             in the atlas but it isn't a valid object yet */
          if (data.textures[i]->atlas)
            cogl_object_unref (data.textures[i]);
        }
    }
}

static void
_cogl_atlas_texture_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglMetaTextureCallback callback,
                                       void *user_data)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);
  CoglMetaTexture *meta_texture = COGL_META_TEXTURE (atlas_tex->sub_texture);

  /* Forward on to the sub texture */
  cogl_meta_texture_foreach_in_region (meta_texture,
                                       virtual_tx_1,
                                       virtual_ty_1,
                                       virtual_tx_2,
                                       virtual_ty_2,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       callback,
                                       user_data);
}

static void
_cogl_atlas_texture_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                       GLenum wrap_mode_s,
                                                       GLenum wrap_mode_t,
                                                       GLenum wrap_mode_p)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_gl_flush_legacy_texobj_wrap_modes (atlas_tex->sub_texture,
                                                   wrap_mode_s,
                                                   wrap_mode_t,
                                                   wrap_mode_p);
}

static void
_cogl_atlas_texture_remove_from_atlas (CoglAtlasTexture *atlas_tex)
{
  if (atlas_tex->atlas)
    {
      _cogl_atlas_remove (atlas_tex->atlas,
                          atlas_tex->allocation.x,
                          atlas_tex->allocation.y,
                          atlas_tex->allocation.width,
                          atlas_tex->allocation.height);

      cogl_object_unref (atlas_tex->atlas);
      atlas_tex->atlas = NULL;
    }
}

static void
_cogl_atlas_texture_free (CoglAtlasTexture *atlas_tex)
{
  _cogl_atlas_texture_remove_from_atlas (atlas_tex);

  if (atlas_tex->sub_texture)
    cogl_object_unref (atlas_tex->sub_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (atlas_tex));
}

static CoglBool
_cogl_atlas_texture_is_sliced (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_is_sliced (atlas_tex->sub_texture);
}

static CoglBool
_cogl_atlas_texture_can_hardware_repeat (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return _cogl_texture_can_hardware_repeat (atlas_tex->sub_texture);
}

static void
_cogl_atlas_texture_transform_coords_to_gl (CoglTexture *tex,
                                            float *s,
                                            float *t)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_transform_coords_to_gl (atlas_tex->sub_texture, s, t);
}

static CoglTransformResult
_cogl_atlas_texture_transform_quad_coords_to_gl (CoglTexture *tex,
                                                 float *coords)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return _cogl_texture_transform_quad_coords_to_gl (atlas_tex->sub_texture,
                                                    coords);
}

static CoglBool
_cogl_atlas_texture_get_gl_texture (CoglTexture *tex,
                                    GLuint *out_gl_handle,
                                    GLenum *out_gl_target)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return cogl_texture_get_gl_texture (atlas_tex->sub_texture,
                                      out_gl_handle,
                                      out_gl_target);
}

static void
_cogl_atlas_texture_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                    GLenum min_filter,
                                                    GLenum mag_filter)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  _cogl_texture_gl_flush_legacy_texobj_filters (atlas_tex->sub_texture,
                                                min_filter, mag_filter);
}

static void
_cogl_atlas_texture_migrate_out_of_atlas (CoglAtlasTexture *atlas_tex)
{
  CoglTexture *standalone_tex;
  CoglContext *ctx;

  /* Make sure this texture is not in the atlas */
  if (!atlas_tex->atlas)
    return;

  ctx = COGL_TEXTURE (atlas_tex)->context;

  COGL_NOTE (ATLAS, "Migrating texture out of the atlas");

  /* We don't know if any journal entries currently depend on
   * OpenGL texture coordinates that would be invalidated by
   * migrating textures in this atlas so we flush all journals
   * before migrating.
   *
   * We are assuming that texture atlas migration never happens
   * during a flush so we don't have to consider recursion here.
   */
  _cogl_flush (ctx);

  standalone_tex =
    _cogl_atlas_migrate_allocation (atlas_tex->atlas,
                                    atlas_tex->allocation.x + 1,
                                    atlas_tex->allocation.y + 1,
                                    atlas_tex->allocation.width - 2,
                                    atlas_tex->allocation.height - 2,
                                    atlas_tex->internal_format);
  /* Note: we simply silently ignore failures to migrate a texture
   * out (most likely due to lack of memory) and hope for the
   * best.
   *
   * Maybe we should find a way to report the problem back to the
   * app.
   */
  if (!standalone_tex)
    return;

  /* Notify cogl-pipeline.c that the texture's underlying GL texture
   * storage is changing so it knows it may need to bind a new texture
   * if the CoglTexture is reused with the same texture unit. */
  _cogl_pipeline_texture_storage_change_notify (COGL_TEXTURE (atlas_tex));

  /* We need to unref the sub texture after doing the copy because
     the copy can involve rendering which might cause the texture
     to be used if it is used from a layer that is left in a
     texture unit */
  cogl_object_unref (atlas_tex->sub_texture);
  atlas_tex->sub_texture = standalone_tex;

  _cogl_atlas_texture_remove_from_atlas (atlas_tex);
}

static void
_cogl_atlas_texture_pre_paint (CoglTexture *tex, CoglTexturePrePaintFlags flags)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP))
    /* Mipmaps do not work well with the current atlas so instead
       we'll just migrate the texture out and use a regular texture */
    _cogl_atlas_texture_migrate_out_of_atlas (atlas_tex);

  /* Forward on to the sub texture */
  _cogl_texture_pre_paint (atlas_tex->sub_texture, flags);
}

static void
_cogl_atlas_texture_ensure_non_quad_rendering (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Sub textures can't support non-quad rendering so we'll just
     migrate the texture out */
  _cogl_atlas_texture_migrate_out_of_atlas (atlas_tex);

  /* Forward on to the sub texture */
  _cogl_texture_ensure_non_quad_rendering (atlas_tex->sub_texture);
}

static CoglBool
_cogl_atlas_texture_set_region_with_border (CoglAtlasTexture *atlas_tex,
                                            int src_x,
                                            int src_y,
                                            int dst_x,
                                            int dst_y,
                                            int dst_width,
                                            int dst_height,
                                            CoglBitmap *bmp,
                                            CoglError **error)
{
  CoglAtlas *atlas = atlas_tex->atlas;

  /* Copy the central data */
  if (!cogl_texture_set_region_from_bitmap (atlas->texture,
                                            src_x, src_y,
                                            dst_width,
                                            dst_height,
                                            bmp,
                                            dst_x + atlas_tex->allocation.x + 1,
                                            dst_y + atlas_tex->allocation.y + 1,
                                            0, /* level 0 */
                                            error))
    return FALSE;

  /* Update the left edge pixels */
  if (dst_x == 0 &&
      !cogl_texture_set_region_from_bitmap (atlas->texture,
                                            src_x, src_y,
                                            1, dst_height,
                                            bmp,
                                            atlas_tex->allocation.x,
                                            dst_y + atlas_tex->allocation.y + 1,
                                            0, /* level 0 */
                                            error))
    return FALSE;
  /* Update the right edge pixels */
  if (dst_x + dst_width == atlas_tex->allocation.width - 2 &&
      !cogl_texture_set_region_from_bitmap (atlas->texture,
                                            src_x + dst_width - 1, src_y,
                                            1, dst_height,
                                            bmp,
                                            atlas_tex->allocation.x +
                                            atlas_tex->allocation.width - 1,
                                            dst_y + atlas_tex->allocation.y + 1,
                                            0, /* level 0 */
                                            error))
    return FALSE;
  /* Update the top edge pixels */
  if (dst_y == 0 &&
      !cogl_texture_set_region_from_bitmap (atlas->texture,
                                            src_x, src_y,
                                            dst_width, 1,
                                            bmp,
                                            dst_x + atlas_tex->allocation.x + 1,
                                            atlas_tex->allocation.y,
                                            0, /* level 0 */
                                            error))
    return FALSE;
  /* Update the bottom edge pixels */
  if (dst_y + dst_height == atlas_tex->allocation.height - 2 &&
      !cogl_texture_set_region_from_bitmap (atlas->texture,
                                            src_x, src_y + dst_height - 1,
                                            dst_width, 1,
                                            bmp,
                                            dst_x + atlas_tex->allocation.x + 1,
                                            atlas_tex->allocation.y +
                                            atlas_tex->allocation.height - 1,
                                            0, /* level 0 */
                                            error))
    return FALSE;

  return TRUE;
}

static CoglBitmap *
_cogl_atlas_texture_convert_bitmap_for_upload (CoglAtlasTexture *atlas_tex,
                                               CoglBitmap *bmp,
                                               CoglPixelFormat internal_format,
                                               CoglBool can_convert_in_place,
                                               CoglError **error)
{
  CoglBitmap *upload_bmp;
  CoglBitmap *override_bmp;

  /* We'll prepare to upload using the format of the actual texture of
     the atlas texture instead of the format reported by
     _cogl_texture_get_format which would be the original internal
     format specified when the texture was created. However we'll
     preserve the premult status of the internal format because the
     images are all stored in the original premult format of the
     orignal format so we do need to trigger the conversion */

  internal_format = (COGL_PIXEL_FORMAT_RGBA_8888 |
                     (internal_format & COGL_PREMULT_BIT));

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                internal_format,
                                                can_convert_in_place,
                                                error);
  if (upload_bmp == NULL)
    return NULL;

  /* We'll create another bitmap which uses the same data but
     overrides the format to remove the premult flag so that uploads
     to the atlas texture won't trigger the conversion again */

  override_bmp =
    _cogl_bitmap_new_shared (upload_bmp,
                             cogl_bitmap_get_format (upload_bmp) &
                             ~COGL_PREMULT_BIT,
                             cogl_bitmap_get_width (upload_bmp),
                             cogl_bitmap_get_height (upload_bmp),
                             cogl_bitmap_get_rowstride (upload_bmp));

  cogl_object_unref (upload_bmp);

  return override_bmp;
}

static CoglBool
_cogl_atlas_texture_set_region (CoglTexture *tex,
                                int src_x,
                                int src_y,
                                int dst_x,
                                int dst_y,
                                int dst_width,
                                int dst_height,
                                int level,
                                CoglBitmap *bmp,
                                CoglError **error)
{
  CoglAtlasTexture  *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  if (level != 0 && atlas_tex->atlas)
    _cogl_atlas_texture_migrate_out_of_atlas (atlas_tex);

  /* If the texture is in the atlas then we need to copy the edge
     pixels to the border */
  if (atlas_tex->atlas)
    {
      CoglBool ret;
      CoglBitmap *upload_bmp =
        _cogl_atlas_texture_convert_bitmap_for_upload (atlas_tex,
                                                       bmp,
                                                       atlas_tex->internal_format,
                                                       FALSE, /* can't convert
                                                                 in place */
                                                       error);
      if (!upload_bmp)
        return FALSE;

      /* Upload the data ignoring the premult bit */
      ret = _cogl_atlas_texture_set_region_with_border (atlas_tex,
                                                        src_x, src_y,
                                                        dst_x, dst_y,
                                                        dst_width, dst_height,
                                                        upload_bmp,
                                                        error);

      cogl_object_unref (upload_bmp);

      return ret;
    }
  else
    /* Otherwise we can just forward on to the sub texture */
    return cogl_texture_set_region_from_bitmap (atlas_tex->sub_texture,
                                                src_x, src_y,
                                                dst_width, dst_height,
                                                bmp,
                                                dst_x, dst_y,
                                                level,
                                                error);
}

static CoglPixelFormat
_cogl_atlas_texture_get_format (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* We don't want to forward this on the sub-texture because it isn't
     the necessarily the same format. This will happen if the texture
     isn't pre-multiplied */
  return atlas_tex->internal_format;
}

static GLenum
_cogl_atlas_texture_get_gl_format (CoglTexture *tex)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);

  /* Forward on to the sub texture */
  return _cogl_texture_gl_get_format (atlas_tex->sub_texture);
}

static CoglBool
_cogl_atlas_texture_can_use_format (CoglPixelFormat format)
{
  /* We don't care about the ordering or the premult status and we can
     accept RGBA or RGB textures. Although we could also accept
     luminance and alpha only textures or 16-bit formats it seems that
     if the application is explicitly using these formats then they've
     got a reason to want the lower memory requirements so putting
     them in the atlas might not be a good idea */
  format &= ~(COGL_PREMULT_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT);
  return (format == COGL_PIXEL_FORMAT_RGB_888 ||
          format == COGL_PIXEL_FORMAT_RGBA_8888);
}

void
_cogl_atlas_texture_atlas_event_handler (CoglAtlasSet *set,
                                         CoglAtlas *atlas,
                                         CoglAtlasSetEvent event,
                                         void *user_data)
{
  switch (event)
    {
    case COGL_ATLAS_SET_EVENT_ADDED:
      {
        CoglAtlasReorganizeCallback pre_callback =
          _cogl_atlas_texture_pre_reorganize_cb;
        CoglAtlasReorganizeCallback post_callback =
          _cogl_atlas_texture_post_reorganize_cb;

        cogl_atlas_add_allocate_callback (atlas,
                                          _cogl_atlas_texture_allocate_cb,
                                          NULL, /* user data */
                                          NULL); /* destroy */
        cogl_atlas_add_pre_reorganize_callback (atlas, pre_callback,
                                                set->context,
                                                NULL); /* destroy */
        cogl_atlas_add_post_reorganize_callback (atlas, post_callback,
                                                 set->context,
                                                 NULL); /* destroy */
        break;
      }
    case COGL_ATLAS_SET_EVENT_REMOVED:
      break;
    }
}

static CoglAtlasTexture *
_cogl_atlas_texture_create_base (CoglContext *ctx,
                                 int width,
                                 int height,
                                 CoglPixelFormat internal_format,
                                 CoglTextureLoader *loader)
{
  CoglAtlasTexture *atlas_tex;

  COGL_NOTE (ATLAS, "Adding texture of size %ix%i", width, height);

  /* We need to allocate the texture now because we need the pointer
     to set as the data for the rectangle in the atlas */
  atlas_tex = c_new0 (CoglAtlasTexture, 1);
  /* Mark it as having no atlas so we don't try to unref it in
     _cogl_atlas_texture_post_reorganize_cb */
  atlas_tex->atlas = NULL;

  _cogl_texture_init (COGL_TEXTURE (atlas_tex),
                      ctx,
                      width, height,
                      internal_format,
                      loader,
                      &cogl_atlas_texture_vtable);

  atlas_tex->sub_texture = NULL;

  atlas_tex->atlas = NULL;

  return _cogl_atlas_texture_object_new (atlas_tex);
}

CoglAtlasTexture *
cogl_atlas_texture_new_with_size (CoglContext *ctx,
                                  int width,
                                  int height)
{
  CoglTextureLoader *loader;

  /* We can't atlas zero-sized textures because it breaks the atlas
   * data structure */
  _COGL_RETURN_VAL_IF_FAIL (width > 0 && height > 0, NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_SIZED;
  loader->src.sized.width = width;
  loader->src.sized.height = height;

  return _cogl_atlas_texture_create_base (ctx, width, height,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          loader);
}

static CoglBool
allocate_space (CoglAtlasTexture *atlas_tex,
                int width,
                int height,
                CoglPixelFormat internal_format,
                CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (atlas_tex);
  CoglContext *ctx = tex->context;
  CoglAtlas *atlas;

  /* If the texture is in a strange format then we won't use it */
  if (!_cogl_atlas_texture_can_use_format (internal_format))
    {
      COGL_NOTE (ATLAS, "Texture can not be added because the "
                 "format is unsupported");
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_FORMAT,
                       "Texture format unsuitable for atlasing");
      return FALSE;
    }

  /* If we can't use FBOs then it will be too slow to migrate textures
     and we shouldn't use the atlas */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Atlasing disabled because migrations "
                       "would be too slow");
      return FALSE;
    }

  /* Add two pixels for the border
   * FIXME: two pixels isn't enough if mipmapping is in use
   */
  atlas = cogl_atlas_set_allocate_space (ctx->atlas_set,
                                         tex->width + 2,
                                         tex->height + 2,
                                         atlas_tex);
  if (!atlas)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_NO_MEMORY,
                       "Not enough memory to atlas texture");
      return FALSE;
    }

  atlas_tex->internal_format = internal_format;

  return TRUE;
}

static CoglBool
allocate_with_size (CoglAtlasTexture *atlas_tex,
                    CoglTextureLoader *loader,
                    CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (atlas_tex);
  CoglPixelFormat internal_format =
    _cogl_texture_determine_internal_format (tex, COGL_PIXEL_FORMAT_ANY);

  if (allocate_space (atlas_tex,
                      loader->src.sized.width,
                      loader->src.sized.height,
                      internal_format,
                      error))
    {
      _cogl_texture_set_allocated (COGL_TEXTURE (atlas_tex),
                                   internal_format,
                                   loader->src.sized.width,
                                   loader->src.sized.height);
      return TRUE;
    }
  else
    return FALSE;
}

static CoglBool
allocate_from_bitmap (CoglAtlasTexture *atlas_tex,
                      CoglTextureLoader *loader,
                      CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (atlas_tex);
  CoglBitmap *bmp = loader->src.bitmap.bitmap;
  CoglPixelFormat bmp_format = cogl_bitmap_get_format (bmp);
  int width = cogl_bitmap_get_width (bmp);
  int height = cogl_bitmap_get_height (bmp);
  CoglBool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
  CoglPixelFormat internal_format;
  CoglBitmap *upload_bmp;

  _COGL_RETURN_VAL_IF_FAIL (atlas_tex->atlas == NULL, FALSE);

  internal_format = _cogl_texture_determine_internal_format (tex, bmp_format);

  upload_bmp =
    _cogl_atlas_texture_convert_bitmap_for_upload (atlas_tex,
                                                   bmp,
                                                   internal_format,
                                                   can_convert_in_place,
                                                   error);
  if (upload_bmp == NULL)
    return FALSE;

  if (!allocate_space (atlas_tex,
                       width,
                       height,
                       internal_format,
                       error))
    {
      cogl_object_unref (upload_bmp);
      return FALSE;
    }

  /* Defer to set_region so that we can share the code for copying the
     edge pixels to the border. */
  if (!_cogl_atlas_texture_set_region_with_border (atlas_tex,
                                                   0, /* src_x */
                                                   0, /* src_y */
                                                   0, /* dst_x */
                                                   0, /* dst_y */
                                                   width, /* dst_width */
                                                   height, /* dst_height */
                                                   upload_bmp,
                                                   error))
    {
      _cogl_atlas_texture_remove_from_atlas (atlas_tex);
      cogl_object_unref (upload_bmp);
      return FALSE;
    }

  cogl_object_unref (upload_bmp);

  _cogl_texture_set_allocated (tex, internal_format, width, height);

  return TRUE;
}

static CoglBool
_cogl_atlas_texture_allocate (CoglTexture *tex,
                              CoglError **error)
{
  CoglAtlasTexture *atlas_tex = COGL_ATLAS_TEXTURE (tex);
  CoglTextureLoader *loader = tex->loader;

  _COGL_RETURN_VAL_IF_FAIL (loader, FALSE);

  switch (loader->src_type)
    {
    case COGL_TEXTURE_SOURCE_TYPE_SIZED:
      return allocate_with_size (atlas_tex, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_BITMAP:
      return allocate_from_bitmap (atlas_tex, loader, error);
    default:
      break;
    }

  c_return_val_if_reached (FALSE);
}

static CoglAtlasTexture *
_cogl_atlas_texture_new_from_bitmap (CoglBitmap *bmp,
                                     CoglBool can_convert_in_place)
{
  CoglTextureLoader *loader;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_bitmap (bmp), NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_BITMAP;
  loader->src.bitmap.bitmap = cogl_object_ref (bmp);
  loader->src.bitmap.can_convert_in_place = can_convert_in_place;

  return _cogl_atlas_texture_create_base (_cogl_bitmap_get_context (bmp),
                                          cogl_bitmap_get_width (bmp),
                                          cogl_bitmap_get_height (bmp),
                                          cogl_bitmap_get_format (bmp),
                                          loader);
}

CoglAtlasTexture *
cogl_atlas_texture_new_from_bitmap (CoglBitmap *bmp)
{
  return _cogl_atlas_texture_new_from_bitmap (bmp, FALSE);
}

CoglAtlasTexture *
cogl_atlas_texture_new_from_data (CoglContext *ctx,
                                  int width,
                                  int height,
                                  CoglPixelFormat format,
                                  int rowstride,
                                  const uint8_t *data,
                                  CoglError **error)
{
  CoglBitmap *bmp;
  CoglAtlasTexture *atlas_tex;

  _COGL_RETURN_VAL_IF_FAIL (format != COGL_PIXEL_FORMAT_ANY, NULL);
  _COGL_RETURN_VAL_IF_FAIL (data != NULL, NULL);

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);

  /* Wrap the data into a bitmap */
  bmp = cogl_bitmap_new_for_data (ctx,
                                  width, height,
                                  format,
                                  rowstride,
                                  (uint8_t *) data);

  atlas_tex = cogl_atlas_texture_new_from_bitmap (bmp);

  cogl_object_unref (bmp);

  if (atlas_tex &&
      !cogl_texture_allocate (COGL_TEXTURE (atlas_tex), error))
    {
      cogl_object_unref (atlas_tex);
      return NULL;
    }

  return atlas_tex;
}

CoglAtlasTexture *
cogl_atlas_texture_new_from_file (CoglContext *ctx,
                                  const char *filename,
                                  CoglError **error)
{
  CoglBitmap *bmp;
  CoglAtlasTexture *atlas_tex = NULL;

  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, NULL);

  bmp = cogl_bitmap_new_from_file (ctx, filename, error);
  if (bmp == NULL)
    return NULL;

  atlas_tex = _cogl_atlas_texture_new_from_bitmap (bmp,
                                                   TRUE); /* convert in-place */

  cogl_object_unref (bmp);

  return atlas_tex;
}

static CoglTextureType
_cogl_atlas_texture_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_2D;
}

static const CoglTextureVtable
cogl_atlas_texture_vtable =
  {
    FALSE, /* not primitive */
    _cogl_atlas_texture_allocate,
    _cogl_atlas_texture_set_region,
    NULL, /* get_data */
    _cogl_atlas_texture_foreach_sub_texture_in_region,
    _cogl_atlas_texture_is_sliced,
    _cogl_atlas_texture_can_hardware_repeat,
    _cogl_atlas_texture_transform_coords_to_gl,
    _cogl_atlas_texture_transform_quad_coords_to_gl,
    _cogl_atlas_texture_get_gl_texture,
    _cogl_atlas_texture_gl_flush_legacy_texobj_filters,
    _cogl_atlas_texture_pre_paint,
    _cogl_atlas_texture_ensure_non_quad_rendering,
    _cogl_atlas_texture_gl_flush_legacy_texobj_wrap_modes,
    _cogl_atlas_texture_get_format,
    _cogl_atlas_texture_get_gl_format,
    _cogl_atlas_texture_get_type,
    NULL, /* is_foreign */
    NULL /* set_auto_mipmap */
  };
