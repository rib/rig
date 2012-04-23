/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TYPES_H__
#define __COGL_TYPES_H__

#include <stdint.h>
#include <glib-object.h>

#include <cogl/cogl-defines.h>

G_BEGIN_DECLS

/**
 * CoglBool:
 *
 * A boolean data type used throughout the Cogl C api. This should be
 * used in conjunction with the %TRUE and %FALSE macro defines for
 * setting and testing boolean values.
 *
 * Since: 2.0
 * Stability: stable
 */
typedef int CoglBool;

/**
 * TRUE:
 *
 * A constant to be used with #CoglBool types to indicate a boolean in
 * the "true" state.
 *
 * Since: 2.0
 * Stability: stable
 */
#ifndef TRUE
#define TRUE 1
#endif

/**
 * TRUE:
 *
 * A constant to be used with #CoglBool types to indicate a boolean in
 * the "false" state.
 *
 * Since: 2.0
 * Stability: stable
 */
#ifndef FALSE
#define FALSE 0
#endif

/* Some structures are meant to be opaque but they have public
   definitions because we want the size to be public so they can be
   allocated on the stack. This macro is used to ensure that users
   don't accidentally access private members */
#ifdef CLUTTER_COMPILATION
#define COGL_PRIVATE(x) x
#else
#define COGL_PRIVATE(x) private_member_ ## x
#endif

/* To help catch accidental changes to public structs that should
 * be stack allocated we use this macro to compile time assert that
 * a struct size is as expected.
 */
#define COGL_STRUCT_SIZE_ASSERT(TYPE, SIZE) \
typedef struct { \
          char compile_time_assert_ ## TYPE ## _size[ \
              (sizeof (TYPE) == (SIZE)) ? 1 : -1]; \
        } _ ## TYPE ## SizeCheck

/**
 * cogl_object_ref: (skip)
 * @object: a #CoglObject
 *
 * Increases the reference count of @object by 1
 *
 * Returns: the @object, with its reference count increased
 */
void *
cogl_object_ref (void *object);

/**
 * cogl_object_unref: (skip)
 * @object: a #CoglObject
 *
 * Drecreases the reference count of @object by 1; if the reference
 * count reaches 0, the resources allocated by @object will be freed
 */
void
cogl_object_unref (void *object);

/**
 * CoglFuncPtr:
 *
 * The type used by cogl for function pointers, note that this type
 * is used as a generic catch-all cast for function pointers and the
 * actual arguments and return type may be different.
 */
typedef void (* CoglFuncPtr) (void);

/* We forward declare this in cogl-types to avoid circular dependencies
 * between cogl-matrix.h, cogl-euler.h and cogl-quaterion.h */
typedef struct _CoglMatrix      CoglMatrix;

/* Same as above we forward declared CoglQuaternion to avoid
 * circular dependencies. */
typedef struct _CoglQuaternion CoglQuaternion;

/* Same as above we forward declared CoglEuler to avoid
 * circular dependencies. */
typedef struct _CoglEuler CoglEuler;

/**
 * CoglAngle:
 *
 * Integer representation of an angle such that 1024 corresponds to
 * full circle (i.e., 2 * pi).
 *
 * Since: 1.0
 */
typedef int32_t CoglAngle;

typedef struct _CoglColor               CoglColor;
typedef struct _CoglTextureVertex       CoglTextureVertex;

/* Enum declarations */

#define COGL_A_BIT              (1 << 4)
#define COGL_BGR_BIT            (1 << 5)
#define COGL_AFIRST_BIT         (1 << 6)
#define COGL_PREMULT_BIT        (1 << 7)

/* XXX: Notes to those adding new formats here...
 *
 * First this diagram outlines how we allocate the 32bits of a
 * CoglPixelFormat currently...
 *
 *                             4 bits for flags
 *                             |--|
 *  enum        unused             4 bits for the bytes-per-pixel
 *                                 and component alignment info
 *  |------| |---------------|     |--|
 *  00000000 xxxxxxxx xxxxxxxx PFBA0000
 *                             ^ premult
 *                              ^ alpha first
 *                               ^ bgr order
 *                                ^ has alpha
 *
 * The most awkward part about the formats is how we use the last 4
 * bits to encode the bytes per pixel and component alignment
 * information. Ideally we should have had 3 bits for the bpp and a
 * flag for alignment but we didn't plan for that in advance so we
 * instead use a small lookup table to query the bpp and whether the
 * components are byte aligned or not.
 *
 * The mapping is the following (see discussion on bug #660188):
 *
 * 0     = undefined
 * 1, 8  = 1 bpp (e.g. A_8, G_8)
 * 2     = 3 bpp, aligned (e.g. 888)
 * 3     = 4 bpp, aligned (e.g. 8888)
 * 4-6   = 2 bpp, not aligned (e.g. 565, 4444, 5551)
 * 7     = YUV: undefined bpp, undefined alignment
 * 9     = 2 bpp, aligned
 * 10    = undefined
 * 11    = undefined
 * 12    = 3 bpp, not aligned
 * 13    = 4 bpp, not aligned (e.g. 2101010)
 * 14-15 = undefined
 *
 * Note: the gap at 10-11 is just because we wanted to maintain that
 * all non-aligned formats have the third bit set in case that's
 * useful later.
 *
 * Since we don't want to waste bits adding more and more flags, we'd
 * like to see most new pixel formats that can't be represented
 * uniquely with the existing flags in the least significant byte
 * simply be enumerated with sequential values in the most significant
 * enum byte.
 *
 * Note: Cogl avoids exposing any padded XRGB or RGBX formats and
 * instead we leave it up to applications to decided whether they
 * consider the A component as padding or valid data. We shouldn't
 * change this policy without good reasoning.
 *
 * So to add a new format:
 * 1) Use the mapping table above to figure out what to but in
 *    the lowest nibble.
 * 2) OR in the COGL_PREMULT_BIT, COGL_AFIRST_BIT, COGL_A_BIT and
 *    COGL_BGR_BIT flags as appropriate.
 * 3) If the result is not yet unique then also combine with an
 *    increment of the last sequence number in the most significant
 *    byte.
 *
 * The last sequence number used was 0 (i.e. no formats currently need
 *                                      a sequence number)
 * Update this note whenever a new sequence number is used.
 */
/**
 * CoglPixelFormat:
 * @COGL_PIXEL_FORMAT_ANY: Any format
 * @COGL_PIXEL_FORMAT_A_8: 8 bits alpha mask
 * @COGL_PIXEL_FORMAT_RGB_565: RGB, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_4444: RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_5551: RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_YUV: Not currently supported
 * @COGL_PIXEL_FORMAT_G_8: Single luminance component
 * @COGL_PIXEL_FORMAT_RGB_888: RGB, 24 bits
 * @COGL_PIXEL_FORMAT_BGR_888: BGR, 24 bits
 * @COGL_PIXEL_FORMAT_RGBA_8888: RGBA, 32 bits
 * @COGL_PIXEL_FORMAT_BGRA_8888: BGRA, 32 bits
 * @COGL_PIXEL_FORMAT_ARGB_8888: ARGB, 32 bits
 * @COGL_PIXEL_FORMAT_ABGR_8888: ABGR, 32 bits
 * @COGL_PIXEL_FORMAT_RGBA_1010102 : RGBA, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_BGRA_1010102 : BGRA, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_ARGB_2101010 : ARGB, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_ABGR_2101010 : ABGR, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_RGBA_8888_PRE: Premultiplied RGBA, 32 bits
 * @COGL_PIXEL_FORMAT_BGRA_8888_PRE: Premultiplied BGRA, 32 bits
 * @COGL_PIXEL_FORMAT_ARGB_8888_PRE: Premultiplied ARGB, 32 bits
 * @COGL_PIXEL_FORMAT_ABGR_8888_PRE: Premultiplied ABGR, 32 bits
 * @COGL_PIXEL_FORMAT_RGBA_4444_PRE: Premultiplied RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_5551_PRE: Premultiplied RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_1010102_PRE: Premultiplied RGBA, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_BGRA_1010102_PRE: Premultiplied BGRA, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_ARGB_2101010_PRE: Premultiplied ARGB, 32 bits, 10 bpc
 * @COGL_PIXEL_FORMAT_ABGR_2101010_PRE: Premultiplied ABGR, 32 bits, 10 bpc
 *
 * Pixel formats used by Cogl. For the formats with a byte per
 * component, the order of the components specify the order in
 * increasing memory addresses. So for example
 * %COGL_PIXEL_FORMAT_RGB_888 would have the red component in the
 * lowest address, green in the next address and blue after that
 * regardless of the endianness of the system.
 *
 * For the formats with non byte aligned components the component
 * order specifies the order within a 16-bit or 32-bit number from
 * most significant bit to least significant. So for
 * %COGL_PIXEL_FORMAT_RGB_565, the red component would be in bits
 * 11-15, the green component would be in 6-11 and the blue component
 * would be in 1-5. Therefore the order in memory depends on the
 * endianness of the system.
 *
 * When uploading a texture %COGL_PIXEL_FORMAT_ANY can be used as the
 * internal format. Cogl will try to pick the best format to use
 * internally and convert the texture data if necessary.
 *
 * Since: 0.8
 */
typedef enum { /*< prefix=COGL_PIXEL_FORMAT >*/
  COGL_PIXEL_FORMAT_ANY           = 0,
  COGL_PIXEL_FORMAT_A_8           = 1 | COGL_A_BIT,

  COGL_PIXEL_FORMAT_RGB_565       = 4,
  COGL_PIXEL_FORMAT_RGBA_4444     = 5 | COGL_A_BIT,
  COGL_PIXEL_FORMAT_RGBA_5551     = 6 | COGL_A_BIT,
  COGL_PIXEL_FORMAT_YUV           = 7,
  COGL_PIXEL_FORMAT_G_8           = 8,

  COGL_PIXEL_FORMAT_RGB_888       = 2,
  COGL_PIXEL_FORMAT_BGR_888       = (2 | COGL_BGR_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888     = (3 | COGL_A_BIT),
  COGL_PIXEL_FORMAT_BGRA_8888     = (3 | COGL_A_BIT | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_8888     = (3 | COGL_A_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_8888     = (3 | COGL_A_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_RGBA_1010102  = (13 | COGL_A_BIT),
  COGL_PIXEL_FORMAT_BGRA_1010102  = (13 | COGL_A_BIT | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_2101010  = (13 | COGL_A_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_2101010  = (13 | COGL_A_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888_PRE = (3 | COGL_A_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_BGRA_8888_PRE = (3 | COGL_A_BIT | COGL_PREMULT_BIT | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_8888_PRE = (3 | COGL_A_BIT | COGL_PREMULT_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_8888_PRE = (3 | COGL_A_BIT | COGL_PREMULT_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_RGBA_4444_PRE = (COGL_PIXEL_FORMAT_RGBA_4444 | COGL_A_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_RGBA_5551_PRE = (COGL_PIXEL_FORMAT_RGBA_5551 | COGL_A_BIT | COGL_PREMULT_BIT),

  COGL_PIXEL_FORMAT_RGBA_1010102_PRE = (COGL_PIXEL_FORMAT_RGBA_1010102 | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_BGRA_1010102_PRE = (COGL_PIXEL_FORMAT_BGRA_1010102 | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_ARGB_2101010_PRE = (COGL_PIXEL_FORMAT_ARGB_2101010 | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_ABGR_2101010_PRE = (COGL_PIXEL_FORMAT_ABGR_2101010 | COGL_PREMULT_BIT)
} CoglPixelFormat;

/**
 * CoglFeatureFlags:
 * @COGL_FEATURE_TEXTURE_RECTANGLE: ARB_texture_rectangle support
 * @COGL_FEATURE_TEXTURE_NPOT: Non power of two textures are supported
 *    by the hardware. This is a equivalent to the
 *    %COGL_FEATURE_TEXTURE_NPOT_BASIC, %COGL_FEATURE_TEXTURE_NPOT_MIPMAP
 *    and %COGL_FEATURE_TEXTURE_NPOT_REPEAT features combined.
 * @COGL_FEATURE_TEXTURE_YUV: ycbcr conversion support
 * @COGL_FEATURE_TEXTURE_READ_PIXELS: glReadPixels() support
 * @COGL_FEATURE_SHADERS_GLSL: GLSL support
 * @COGL_FEATURE_SHADERS_ARBFP: ARBFP support
 * @COGL_FEATURE_OFFSCREEN: FBO support
 * @COGL_FEATURE_OFFSCREEN_MULTISAMPLE: Multisample support on FBOs
 * @COGL_FEATURE_OFFSCREEN_BLIT: Blit support on FBOs
 * @COGL_FEATURE_FOUR_CLIP_PLANES: At least 4 clip planes available
 * @COGL_FEATURE_STENCIL_BUFFER: Stencil buffer support
 * @COGL_FEATURE_VBOS: VBO support
 * @COGL_FEATURE_PBOS: PBO support
 * @COGL_FEATURE_UNSIGNED_INT_INDICES: Set if
 *     %COGL_INDICES_TYPE_UNSIGNED_INT is supported in
 *     cogl_indices_new().
 * @COGL_FEATURE_DEPTH_RANGE: cogl_material_set_depth_range() support
 * @COGL_FEATURE_TEXTURE_NPOT_BASIC: The hardware supports non power
 *     of two textures, but you also need to check the
 *     %COGL_FEATURE_TEXTURE_NPOT_MIPMAP and %COGL_FEATURE_TEXTURE_NPOT_REPEAT
 *     features to know if the hardware supports npot texture mipmaps
 *     or repeat modes other than
 *     %COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE respectively.
 * @COGL_FEATURE_TEXTURE_NPOT_MIPMAP: Mipmapping is supported in
 *     conjuntion with non power of two textures.
 * @COGL_FEATURE_TEXTURE_NPOT_REPEAT: Repeat modes other than
 *     %COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE are supported by the
 *     hardware.
 * @COGL_FEATURE_POINT_SPRITE: Whether
 *     cogl_material_set_layer_point_sprite_coords_enabled() is supported.
 * @COGL_FEATURE_TEXTURE_3D: 3D texture support
 * @COGL_FEATURE_MAP_BUFFER_FOR_READ: Whether cogl_buffer_map() is
 *     supported with CoglBufferAccess including read support.
 * @COGL_FEATURE_MAP_BUFFER_FOR_WRITE: Whether cogl_buffer_map() is
 *     supported with CoglBufferAccess including write support.
 *
 * Flags for the supported features.
 *
 * Since: 0.8
 */
typedef enum
{
  COGL_FEATURE_TEXTURE_RECTANGLE      = (1 << 1),
  COGL_FEATURE_TEXTURE_NPOT           = (1 << 2),
  COGL_FEATURE_TEXTURE_YUV            = (1 << 3),
  COGL_FEATURE_TEXTURE_READ_PIXELS    = (1 << 4),
  COGL_FEATURE_SHADERS_GLSL           = (1 << 5),
  COGL_FEATURE_OFFSCREEN              = (1 << 6),
  COGL_FEATURE_OFFSCREEN_MULTISAMPLE  = (1 << 7),
  COGL_FEATURE_OFFSCREEN_BLIT         = (1 << 8),
  COGL_FEATURE_FOUR_CLIP_PLANES       = (1 << 9),
  COGL_FEATURE_STENCIL_BUFFER         = (1 << 10),
  COGL_FEATURE_VBOS		      = (1 << 11),
  COGL_FEATURE_PBOS		      = (1 << 12),
  COGL_FEATURE_UNSIGNED_INT_INDICES   = (1 << 13),
  COGL_FEATURE_DEPTH_RANGE            = (1 << 14),
  COGL_FEATURE_TEXTURE_NPOT_BASIC     = (1 << 15),
  COGL_FEATURE_TEXTURE_NPOT_MIPMAP    = (1 << 16),
  COGL_FEATURE_TEXTURE_NPOT_REPEAT    = (1 << 17),
  COGL_FEATURE_POINT_SPRITE           = (1 << 18),
  COGL_FEATURE_TEXTURE_3D             = (1 << 19),
  COGL_FEATURE_SHADERS_ARBFP          = (1 << 20),
  COGL_FEATURE_MAP_BUFFER_FOR_READ    = (1 << 21),
  COGL_FEATURE_MAP_BUFFER_FOR_WRITE   = (1 << 22),
  COGL_FEATURE_ONSCREEN_MULTIPLE      = (1 << 23)
} CoglFeatureFlags;

/**
 * CoglBufferTarget:
 * @COGL_WINDOW_BUFFER: FIXME
 * @COGL_OFFSCREEN_BUFFER: FIXME
 *
 * Target flags for FBOs.
 *
 * Since: 0.8
 */
typedef enum
{
  COGL_WINDOW_BUFFER      = (1 << 1),
  COGL_OFFSCREEN_BUFFER   = (1 << 2)
} CoglBufferTarget;

/**
 * CoglColor:
 *
 * A structure for holding a color definition. The contents of
 * the CoglColor structure are private and should never by accessed
 * directly.
 *
 * Since: 1.0
 */
struct _CoglColor
{
  /*< private >*/
  uint8_t COGL_PRIVATE (red);
  uint8_t COGL_PRIVATE (green);
  uint8_t COGL_PRIVATE (blue);

  uint8_t COGL_PRIVATE (alpha);

  /* padding in case we want to change to floats at
   * some point */
  uint32_t COGL_PRIVATE (padding0);
  uint32_t COGL_PRIVATE (padding1);
  uint32_t COGL_PRIVATE (padding2);
};
COGL_STRUCT_SIZE_ASSERT (CoglColor, 16);

/**
 * CoglTextureVertex:
 * @x: Model x-coordinate
 * @y: Model y-coordinate
 * @z: Model z-coordinate
 * @tx: Texture x-coordinate
 * @ty: Texture y-coordinate
 * @color: The color to use at this vertex. This is ignored if
 *   use_color is %FALSE when calling cogl_polygon()
 *
 * Used to specify vertex information when calling cogl_polygon()
 */
struct _CoglTextureVertex
{
  float x, y, z;
  float tx, ty;

  CoglColor color;
};
COGL_STRUCT_SIZE_ASSERT (CoglTextureVertex, 36);

/**
 * CoglTextureFlags:
 * @COGL_TEXTURE_NONE: No flags specified
 * @COGL_TEXTURE_NO_AUTO_MIPMAP: Disables the automatic generation of
 *   the mipmap pyramid from the base level image whenever it is
 *   updated. The mipmaps are only generated when the texture is
 *   rendered with a mipmap filter so it should be free to leave out
 *   this flag when using other filtering modes
 * @COGL_TEXTURE_NO_SLICING: Disables the slicing of the texture
 * @COGL_TEXTURE_NO_ATLAS: Disables the insertion of the texture inside
 *   the texture atlas used by Cogl
 *
 * Flags to pass to the cogl_texture_new_* family of functions.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_TEXTURE_NONE           = 0,
  COGL_TEXTURE_NO_AUTO_MIPMAP = 1 << 0,
  COGL_TEXTURE_NO_SLICING     = 1 << 1,
  COGL_TEXTURE_NO_ATLAS       = 1 << 2
} CoglTextureFlags;

/**
 * CoglFogMode:
 * @COGL_FOG_MODE_LINEAR: Calculates the fog blend factor as:
 * |[
 *   f = end - eye_distance / end - start
 * ]|
 * @COGL_FOG_MODE_EXPONENTIAL: Calculates the fog blend factor as:
 * |[
 *   f = e ^ -(density * eye_distance)
 * ]|
 * @COGL_FOG_MODE_EXPONENTIAL_SQUARED: Calculates the fog blend factor as:
 * |[
 *   f = e ^ -(density * eye_distance)^2
 * ]|
 *
 * The fog mode determines the equation used to calculate the fogging blend
 * factor while fogging is enabled. The simplest %COGL_FOG_MODE_LINEAR mode
 * determines f as:
 *
 * |[
 *   f = end - eye_distance / end - start
 * ]|
 *
 * Where eye_distance is the distance of the current fragment in eye
 * coordinates from the origin.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_FOG_MODE_LINEAR,
  COGL_FOG_MODE_EXPONENTIAL,
  COGL_FOG_MODE_EXPONENTIAL_SQUARED
} CoglFogMode;

/**
 * COGL_BLEND_STRING_ERROR:
 *
 * #GError domain for blend string parser errors
 *
 * Since: 1.0
 */
#define COGL_BLEND_STRING_ERROR (cogl_blend_string_error_quark ())

/**
 * CoglBlendStringError:
 * @COGL_BLEND_STRING_ERROR_PARSE_ERROR: Generic parse error
 * @COGL_BLEND_STRING_ERROR_ARGUMENT_PARSE_ERROR: Argument parse error
 * @COGL_BLEND_STRING_ERROR_INVALID_ERROR: Internal parser error
 * @COGL_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR: Blend string not
 *   supported by the GPU
 *
 * Error enumeration for the blend strings parser
 *
 * Since: 1.0
 */
typedef enum { /*< prefix=COGL_BLEND_STRING_ERROR >*/
  COGL_BLEND_STRING_ERROR_PARSE_ERROR,
  COGL_BLEND_STRING_ERROR_ARGUMENT_PARSE_ERROR,
  COGL_BLEND_STRING_ERROR_INVALID_ERROR,
  COGL_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR
} CoglBlendStringError;

GQuark
cogl_blend_string_error_quark (void);

#define COGL_ERROR (_cogl_error_quark ())

/**
 * CoglError:
 * @COGL_ERROR_UNSUPPORTED: You tried to use a feature or
 *    configuration not currently available.
 * @COGL_ERROR_NO_MEMORY: You tried to allocate a resource
 *    such as a texture and there wasn't enough memory.
 *
 * Error enumeration for Cogl
 *
 * The @COGL_ERROR_UNSUPPORTED error can be thrown for a variety of
 * reasons. For example:
 *
 * <itemizedlist>
 *  <listitem><para>You've tried to use a feature that is not
 *   advertised by cogl_get_features(). This could happen if you create
 *   a 2d texture with a non-power-of-two size when
 *   %COGL_FEATURE_TEXTURE_NPOT is not advertised.</para></listitem>
 *  <listitem><para>The GPU can not handle the configuration you have
 *   requested. An example might be if you try to use too many texture
 *   layers in a single #CoglPipeline</para></listitem>
 *  <listitem><para>The driver does not support some
 *   configuration.</para></listiem>
 * </itemizedlist>
 *
 * Currently this is only used by Cogl API marked as experimental so
 * this enum should also be considered experimental.
 *
 * Since: 1.4
 * Stability: unstable
 */
typedef enum { /*< prefix=COGL_ERROR >*/
  COGL_ERROR_UNSUPPORTED,
  COGL_ERROR_NO_MEMORY
} CoglError;

GQuark
_cogl_error_quark (void);

/**
 * CoglAttributeType:
 * @COGL_ATTRIBUTE_TYPE_BYTE: Data is the same size of a byte
 * @COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE: Data is the same size of an
 *   unsigned byte
 * @COGL_ATTRIBUTE_TYPE_SHORT: Data is the same size of a short integer
 * @COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT: Data is the same size of
 *   an unsigned short integer
 * @COGL_ATTRIBUTE_TYPE_FLOAT: Data is the same size of a float
 *
 * Data types for the components of a vertex attribute.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_ATTRIBUTE_TYPE_BYTE           = 0x1400,
  COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE  = 0x1401,
  COGL_ATTRIBUTE_TYPE_SHORT          = 0x1402,
  COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT = 0x1403,
  COGL_ATTRIBUTE_TYPE_FLOAT          = 0x1406
} CoglAttributeType;

/**
 * CoglIndicesType:
 * @COGL_INDICES_TYPE_UNSIGNED_BYTE: Your indices are unsigned bytes
 * @COGL_INDICES_TYPE_UNSIGNED_SHORT: Your indices are unsigned shorts
 * @COGL_INDICES_TYPE_UNSIGNED_INT: Your indices are unsigned ints
 *
 * You should aim to use the smallest data type that gives you enough
 * range, since it reduces the size of your index array and can help
 * reduce the demand on memory bandwidth.
 *
 * Note that %COGL_INDICES_TYPE_UNSIGNED_INT is only supported if the
 * %COGL_FEATURE_UNSIGNED_INT_INDICES feature is available. This
 * should always be available on OpenGL but on OpenGL ES it will only
 * be available if the GL_OES_element_index_uint extension is
 * advertized.
 */
typedef enum {
  COGL_INDICES_TYPE_UNSIGNED_BYTE,
  COGL_INDICES_TYPE_UNSIGNED_SHORT,
  COGL_INDICES_TYPE_UNSIGNED_INT
} CoglIndicesType;

/**
 * CoglVerticesMode:
 * @COGL_VERTICES_MODE_POINTS: FIXME, equivalent to %GL_POINTS
 * @COGL_VERTICES_MODE_LINES: FIXME, equivalent to %GL_LINES
 * @COGL_VERTICES_MODE_LINE_LOOP: FIXME, equivalent to %GL_LINE_LOOP
 * @COGL_VERTICES_MODE_LINE_STRIP: FIXME, equivalent to %GL_LINE_STRIP
 * @COGL_VERTICES_MODE_TRIANGLES: FIXME, equivalent to %GL_TRIANGLES
 * @COGL_VERTICES_MODE_TRIANGLE_STRIP: FIXME, equivalent to %GL_TRIANGLE_STRIP
 * @COGL_VERTICES_MODE_TRIANGLE_FAN: FIXME, equivalent to %GL_TRIANGLE_FAN
 *
 * Different ways of interpreting vertices when drawing.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_VERTICES_MODE_POINTS = 0x0000,
  COGL_VERTICES_MODE_LINES = 0x0001,
  COGL_VERTICES_MODE_LINE_LOOP = 0x0002,
  COGL_VERTICES_MODE_LINE_STRIP = 0x0003,
  COGL_VERTICES_MODE_TRIANGLES = 0x0004,
  COGL_VERTICES_MODE_TRIANGLE_STRIP = 0x0005,
  COGL_VERTICES_MODE_TRIANGLE_FAN = 0x0006
} CoglVerticesMode;

/* NB: The above definitions are taken from gl.h equivalents */


/* XXX: should this be CoglMaterialDepthTestFunction?
 * It makes it very verbose but would be consistent with
 * CoglMaterialWrapMode */

/**
 * CoglDepthTestFunction:
 * @COGL_DEPTH_TEST_FUNCTION_NEVER: Never passes.
 * @COGL_DEPTH_TEST_FUNCTION_LESS: Passes if the fragment's depth
 * value is less than the value currently in the depth buffer.
 * @COGL_DEPTH_TEST_FUNCTION_EQUAL: Passes if the fragment's depth
 * value is equal to the value currently in the depth buffer.
 * @COGL_DEPTH_TEST_FUNCTION_LEQUAL: Passes if the fragment's depth
 * value is less or equal to the value currently in the depth buffer.
 * @COGL_DEPTH_TEST_FUNCTION_GREATER: Passes if the fragment's depth
 * value is greater than the value currently in the depth buffer.
 * @COGL_DEPTH_TEST_FUNCTION_NOTEQUAL: Passes if the fragment's depth
 * value is not equal to the value currently in the depth buffer.
 * @COGL_DEPTH_TEST_FUNCTION_GEQUAL: Passes if the fragment's depth
 * value greater than or equal to the value currently in the depth buffer.
 * @COGL_DEPTH_TEST_FUNCTION_ALWAYS: Always passes.
 *
 * When using depth testing one of these functions is used to compare
 * the depth of an incoming fragment against the depth value currently
 * stored in the depth buffer. The function is changed using
 * cogl_material_set_depth_test_function().
 *
 * The test is only done when depth testing is explicitly enabled. (See
 * cogl_material_set_depth_test_enabled())
 */
typedef enum
{
  COGL_DEPTH_TEST_FUNCTION_NEVER    = 0x0200,
  COGL_DEPTH_TEST_FUNCTION_LESS     = 0x0201,
  COGL_DEPTH_TEST_FUNCTION_EQUAL    = 0x0202,
  COGL_DEPTH_TEST_FUNCTION_LEQUAL   = 0x0203,
  COGL_DEPTH_TEST_FUNCTION_GREATER  = 0x0204,
  COGL_DEPTH_TEST_FUNCTION_NOTEQUAL = 0x0205,
  COGL_DEPTH_TEST_FUNCTION_GEQUAL   = 0x0206,
  COGL_DEPTH_TEST_FUNCTION_ALWAYS   = 0x0207
} CoglDepthTestFunction;
/* NB: The above definitions are taken from gl.h equivalents */

typedef enum { /*< prefix=COGL_RENDERER_ERROR >*/
  COGL_RENDERER_ERROR_NOT_FOUND,
  COGL_RENDERER_ERROR_XLIB_DISPLAY_OPEN
} CoglRendererError;

/*
 * CoglFilterReturn:
 * @COGL_FILTER_CONTINUE: The event was not handled, continues the
 *                        processing
 * @COGL_FILTER_REMOVE: Remove the event, stops the processing
 *
 * Return values for the #CoglFilterFunc function.
 *
 * Stability: Unstable
 */
typedef enum _CoglFilterReturn { /*< prefix=COGL_FILTER >*/
  COGL_FILTER_CONTINUE,
  COGL_FILTER_REMOVE
} CoglFilterReturn;

typedef enum _CoglWinsysFeature
{
  /* Available if the window system can support multiple onscreen
   * framebuffers at the same time. */
  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,

  /* Available if onscreen framebuffer swaps can be automatically
   * throttled to the vblank frequency. */
  COGL_WINSYS_FEATURE_SWAP_THROTTLE,

  /* Available if its possible to query a counter that
   * increments at each vblank. */
  COGL_WINSYS_FEATURE_VBLANK_COUNTER,

  /* Available if its possible to wait until the next vertical
   * blank period */
  COGL_WINSYS_FEATURE_VBLANK_WAIT,

  /* Available if the window system supports mapping native
   * pixmaps to textures. */
  COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP,

  /* Available if the window system supports reporting an event
   * for swap buffer completions. */
  COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT,

  /* Available if it's possible to swap a list of sub rectangles
   * from the back buffer to the front buffer */
  COGL_WINSYS_FEATURE_SWAP_REGION,

  /* Available if swap_region requests can be automatically throttled
   * to the vblank frequency. */
  COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE,

  /* Available if the swap region implementation won't tear and thus
   * only needs to be throttled to the framerate */
  COGL_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED,

  COGL_WINSYS_FEATURE_N_FEATURES
} CoglWinsysFeature;

/**
 * CoglColorMask:
 * @COGL_COLOR_MASK_NONE: None of the color channels are masked
 * @COGL_COLOR_MASK_RED: Masks the red color channel
 * @COGL_COLOR_MASK_GREEN: Masks the green color channel
 * @COGL_COLOR_MASK_BLUE: Masks the blue color channel
 * @COGL_COLOR_MASK_ALPHA: Masks the alpha color channel
 * @COGL_COLOR_MASK_ALL: All of the color channels are masked
 *
 * Defines a bit mask of color channels. This can be used with
 * cogl_pipeline_set_color_mask() for example to define which color
 * channels should be written to the current framebuffer when
 * drawing something.
 */
typedef enum
{
  COGL_COLOR_MASK_NONE = 0,
  COGL_COLOR_MASK_RED = 1L<<0,
  COGL_COLOR_MASK_GREEN = 1L<<1,
  COGL_COLOR_MASK_BLUE = 1L<<2,
  COGL_COLOR_MASK_ALPHA = 1L<<3,
  /* XXX: glib-mkenums is a perl script that can't cope if we split
   * this onto multiple lines! *sigh* */
  COGL_COLOR_MASK_ALL = (COGL_COLOR_MASK_RED | COGL_COLOR_MASK_GREEN | COGL_COLOR_MASK_BLUE | COGL_COLOR_MASK_ALPHA)
} CoglColorMask;

/**
 * CoglWinding:
 * @COGL_WINDING_CLOCKWISE: Vertices are in a clockwise order
 * @COGL_WINDING_COUNTER_CLOCKWISE: Vertices are in a counter-clockwise order
 *
 * Enum used to represent the two directions of rotation. This can be
 * used to set the front face for culling by calling
 * cogl_pipeline_set_front_face_winding().
 */
typedef enum
{
  COGL_WINDING_CLOCKWISE,
  COGL_WINDING_COUNTER_CLOCKWISE
} CoglWinding;

/**
 * CoglBufferBit:
 * @COGL_BUFFER_BIT_COLOR: Selects the primary color buffer
 * @COGL_BUFFER_BIT_DEPTH: Selects the depth buffer
 * @COGL_BUFFER_BIT_STENCIL: Selects the stencil buffer
 *
 * Types of auxiliary buffers
 *
 * Since: 1.0
 */
typedef enum {
  COGL_BUFFER_BIT_COLOR   = 1L<<0,
  COGL_BUFFER_BIT_DEPTH   = 1L<<1,
  COGL_BUFFER_BIT_STENCIL = 1L<<2
} CoglBufferBit;

/**
 * CoglReadPixelsFlags:
 * @COGL_READ_PIXELS_COLOR_BUFFER: Read from the color buffer
 *
 * Flags for cogl_read_pixels()
 *
 * Since: 1.0
 */
typedef enum { /*< prefix=COGL_READ_PIXELS >*/
  COGL_READ_PIXELS_COLOR_BUFFER = 1L << 0
} CoglReadPixelsFlags;

G_END_DECLS

#endif /* __COGL_TYPES_H__ */
