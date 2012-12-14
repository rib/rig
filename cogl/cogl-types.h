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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TYPES_H__
#define __COGL_TYPES_H__

#include <stdint.h>
#include <stddef.h>

#include <cogl/cogl-defines.h>

/* Guard C code in headers, while including them from C++ */
#ifdef  __cplusplus
#define COGL_BEGIN_DECLS  extern "C" {
#define COGL_END_DECLS    }
#else
#define COGL_BEGIN_DECLS
#define COGL_END_DECLS
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-types
 * @short_description: Types used throughout the library
 *
 * General types used by various Cogl functions.
*/

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
 * FALSE:
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

#if __GNUC__ >= 4
#define COGL_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#else
#define COGL_GNUC_NULL_TERMINATED
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define COGL_GNUC_DEPRECATED                       \
  __attribute__((__deprecated__))
#else
#define COGL_GNUC_DEPRECATED
#endif /* __GNUC__ */

/* Some structures are meant to be opaque but they have public
   definitions because we want the size to be public so they can be
   allocated on the stack. This macro is used to ensure that users
   don't accidentally access private members */
#ifdef COGL_COMPILATION
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

typedef struct _CoglColor               CoglColor;

/* Enum declarations */

/**
 * COGL_BITWISE_BIT:
 *
 * A flag that can be masked with a #CoglPixelFormat to determine if
 * the format should be accessed with bitwise operations. If the flag
 * is set then all of the bytes representing a pixel should
 * interpreted as a single integer stored in the native endianness of
 * the CPU. The order of the components in the name of the format
 * represent the bits within that integer in order of decreasing
 * significance. For example, the bytes for a pixel in the format
 * RGB_565 should be interpreted as a 16-bit integer, with the red
 * component in the most significant 5 bits, green in the next 6 bits
 * and so on.
 *
 * If the flag is not set then each component is byte aligned and is
 * stored in order of increasing memory addresses. For example, in
 * RGB_888, there are 24 bits per pixel with the red component stored
 * in the byte with the lowest address and so on.
 */
#define COGL_BITWISE_BIT        (1 << 5)

/**
 * COGL_A_BIT:
 *
 * A flag that can be masked with a #CoglPixelFormat to determine if
 * the format has an alpha component.
 */
#define COGL_A_BIT              (1 << 6)

/**
 * COGL_BGR_BIT:
 *
 * A flag that can be masked with a #CoglPixelFormat to determine if
 * the color components are ordered Blue, followed by Green followed
 * by Red.
 *
 * (Note: it depends on the %COGL_BITWISE_BIT flag whether the order
 * relates to the order of bits or to memory address order)
 */
#define COGL_BGR_BIT            (1 << 7)

/**
 * COGL_AFIRST_BIT:
 *
 * A flag that can be masked with a #CoglPixelFormat to determine if
 * an alpha component is in front of the first color component.
 *
 * (Note: it depends on the %COGL_BITWISE_BIT flag whether "first"
 * means first in terms of bits or first in memory address order)
 */
#define COGL_AFIRST_BIT         (1 << 8)

/**
 * COGL_PREMULT_BIT:
 *
 * A flag that can be masked with a #CoglPixelFormat to determine if
 * it represents a pre-multiplied alpha format.
 */
#define COGL_PREMULT_BIT        (1 << 9)

/**
 * COGL_PIXEL_FORMAT_BPP_MASK:
 *
 * Masks out the bytes per pixel for the given format
 */
#define COGL_PIXEL_FORMAT_BPP_MASK (0x3f)

/* FIXME: Add a separate CoglInternalFormat enum to handle these */
#define COGL_DEPTH_BIT          (1 << 10)
#define COGL_STENCIL_BIT        (1 << 11)

#define COGL_FORMAT_ENUM(X) ((X)<<24)

/* XXX: Notes to those adding new formats here...
 *
 * First this diagram outlines how we allocate the 32bits of a
 * CoglPixelFormat currently...
 *
 *                            7 bits for flags
 *                       |------|
 *  enum        unused             6 bits for the bytes-per-pixel
 *  |------| |----------|        |----|
 *  00000000 xxxxxxxx xxxSDPFB ABxxxxxx
 *                       ^ stencil
 *                        ^ depth
 *                         ^ premult
 *                          ^ alpha first
 *                           ^ bgr order
 *                             ^ has alpha
 *                              ^ bitwise
 *                               ^^^^^^ bpp
 *
 * Since we don't want to waste bits adding more and more flags, we'd
 * like to see most new pixel formats that can't be represented
 * uniquely with the existing flags in the least significant bits
 * simply be enumerated with sequential values in the most significant
 * enum byte (though still set the flags as appropriate too).
 *
 * Note: Cogl avoids exposing any padded XRGB or RGBX formats and
 * instead we leave it up to applications to decided whether they
 * consider the A component as padding or valid data. We shouldn't
 * change this policy without good reasoning.
 *
 * So to add a new format:
 * 1) OR in the COGL_BITWISE_BIT, COGL_A_BIT, COGL_BGR_BIT,
 *    COGL_AFIRST_BIT and COGL_PREMULT_BIT, flags as appropriate.
 * 2) If the result is not yet unique then also combine with an
 *    increment of the last sequence number in the most significant
 *    byte.
 *
 * The last sequence number used was 2
 *
 * Update this note whenever a new sequence number is used.
 */
/**
 * CoglPixelFormat:
 * @COGL_PIXEL_FORMAT_ANY: Any format
 * @COGL_PIXEL_FORMAT_A_8: 8 bits alpha mask
 * @COGL_PIXEL_FORMAT_RGB_565: RGB, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_4444: RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_5551: RGBA, 16 bits
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
 * @COGL_PIXEL_FORMAT_DEPTH_16: Depth, 16 bits
 * @COGL_PIXEL_FORMAT_DEPTH_32: Depth, 32 bits
 * @COGL_PIXEL_FORMAT_DEPTH_24_STENCIL_8: Depth/Stencil, 24/8 bits
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
 * Since: 0.8
 */
typedef enum { /*< prefix=COGL_PIXEL_FORMAT >*/
  COGL_PIXEL_FORMAT_ANY = 0,
  COGL_PIXEL_FORMAT_A_8 = (1 | COGL_A_BIT),

  COGL_PIXEL_FORMAT_RGB_565 = (2 | COGL_BITWISE_BIT),
  COGL_PIXEL_FORMAT_RGBA_4444 = (2 | COGL_BITWISE_BIT | COGL_A_BIT),
  COGL_PIXEL_FORMAT_RGBA_4444_PRE = (2 | COGL_PIXEL_FORMAT_RGBA_4444 | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_RGBA_5551 = (2 | COGL_BITWISE_BIT | COGL_A_BIT | COGL_FORMAT_ENUM(1)),
  COGL_PIXEL_FORMAT_RGBA_5551_PRE = (2 | COGL_PIXEL_FORMAT_RGBA_5551 | COGL_PREMULT_BIT),

  COGL_PIXEL_FORMAT_G_8 = 1,

  COGL_PIXEL_FORMAT_RGB_888 = 3,
  COGL_PIXEL_FORMAT_BGR_888 = (3 | COGL_BGR_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888 = (4 | COGL_A_BIT),
  COGL_PIXEL_FORMAT_BGRA_8888 = (4 | COGL_A_BIT | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_8888 = (4 | COGL_A_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_8888 = (4 | COGL_A_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888_PRE = (4 | COGL_A_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_BGRA_8888_PRE = (4 | COGL_A_BIT | COGL_BGR_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_ARGB_8888_PRE = (4 | COGL_A_BIT | COGL_AFIRST_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_ABGR_8888_PRE = (4 | COGL_A_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT | COGL_PREMULT_BIT),

  COGL_PIXEL_FORMAT_RGBA_1010102 = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2)),
  COGL_PIXEL_FORMAT_BGRA_1010102 = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_2101010 = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_2101010 = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_BGR_BIT | COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_RGBA_1010102_PRE = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_BGRA_1010102_PRE = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_BGR_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_ARGB_2101010_PRE = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_AFIRST_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_ABGR_2101010_PRE = (4 | COGL_A_BIT | COGL_FORMAT_ENUM(2) | COGL_BGR_BIT | COGL_AFIRST_BIT | COGL_PREMULT_BIT),

  COGL_PIXEL_FORMAT_DEPTH_16 = (2 | COGL_DEPTH_BIT),
  COGL_PIXEL_FORMAT_DEPTH_32 = (4 | COGL_DEPTH_BIT),

  COGL_PIXEL_FORMAT_DEPTH_24_STENCIL_8 = (4 | COGL_DEPTH_BIT | COGL_STENCIL_BIT)
} CoglPixelFormat;

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
 * @red: amount of red
 * @green: amount of green
 * @blue: amount of green
 * @alpha: alpha
 *
 * A structure for holding a single color definition.
 *
 * Since: 1.0
 */
struct _CoglColor
{
  float red;
  float green;
  float blue;
  float alpha;
};
COGL_STRUCT_SIZE_ASSERT (CoglColor, 16);

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
 * COGL_BLEND_STRING_ERROR:
 *
 * #CoglError domain for blend string parser errors
 *
 * Since: 1.0
 */
#define COGL_BLEND_STRING_ERROR (cogl_blend_string_error_domain ())

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

uint32_t
cogl_blend_string_error_domain (void);

#define COGL_SYSTEM_ERROR (_cogl_system_error_domain ())

/**
 * CoglSystemError:
 * @COGL_SYSTEM_ERROR_UNSUPPORTED: You tried to use a feature or
 *    configuration not currently available.
 * @COGL_SYSTEM_ERROR_NO_MEMORY: You tried to allocate a resource
 *    such as a texture and there wasn't enough memory.
 *
 * Error enumeration for Cogl
 *
 * The @COGL_SYSTEM_ERROR_UNSUPPORTED error can be thrown for a
 * variety of reasons. For example:
 *
 * <itemizedlist>
 *  <listitem><para>You've tried to use a feature that is not
 *   advertised by cogl_has_feature(). This could happen if you create
 *   a 2d texture with a non-power-of-two size when
 *   %COGL_FEATURE_ID_TEXTURE_NPOT is not advertised.</para></listitem>
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
  COGL_SYSTEM_ERROR_UNSUPPORTED,
  COGL_SYSTEM_ERROR_NO_MEMORY
} CoglSystemError;

uint32_t
_cogl_system_error_domain (void);

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
 * %COGL_FEATURE_ID_UNSIGNED_INT_INDICES feature is available. This
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
 * @COGL_VERTICES_MODE_POINTS: FIXME, equivalent to
 * <constant>GL_POINTS</constant>
 * @COGL_VERTICES_MODE_LINES: FIXME, equivalent to <constant>GL_LINES</constant>
 * @COGL_VERTICES_MODE_LINE_LOOP: FIXME, equivalent to
 * <constant>GL_LINE_LOOP</constant>
 * @COGL_VERTICES_MODE_LINE_STRIP: FIXME, equivalent to
 * <constant>GL_LINE_STRIP</constant>
 * @COGL_VERTICES_MODE_TRIANGLES: FIXME, equivalent to
 * <constant>GL_TRIANGLES</constant>
 * @COGL_VERTICES_MODE_TRIANGLE_STRIP: FIXME, equivalent to
 * <constant>GL_TRIANGLE_STRIP</constant>
 * @COGL_VERTICES_MODE_TRIANGLE_FAN: FIXME, equivalent to <constant>GL_TRIANGLE_FAN</constant>
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
 * cogl_depth_state_set_test_function().
 *
 * The test is only done when depth testing is explicitly enabled. (See
 * cogl_depth_state_set_test_enabled())
 */
typedef enum {
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
  COGL_RENDERER_ERROR_XLIB_DISPLAY_OPEN,
  COGL_RENDERER_ERROR_BAD_CONSTRAINT
} CoglRendererError;

/**
 * CoglFilterReturn:
 * @COGL_FILTER_CONTINUE: The event was not handled, continues the
 *                        processing
 * @COGL_FILTER_REMOVE: Remove the event, stops the processing
 *
 * Return values for the #CoglXlibFilterFunc and #CoglWin32FilterFunc functions.
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

  /* Avaiable if the age of the back buffer can be queried */
  COGL_WINSYS_FEATURE_BUFFER_AGE,

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
  COGL_BUFFER_BIT_COLOR   = 1<<0,
  COGL_BUFFER_BIT_DEPTH   = 1<<1,
  COGL_BUFFER_BIT_STENCIL = 1<<2
} CoglBufferBit;

/**
 * CoglReadPixelsFlags:
 * @COGL_READ_PIXELS_COLOR_BUFFER: Read from the color buffer
 *
 * Flags for cogl_framebuffer_read_pixels_into_bitmap()
 *
 * Since: 1.0
 */
typedef enum { /*< prefix=COGL_READ_PIXELS >*/
  COGL_READ_PIXELS_COLOR_BUFFER = 1L << 0
} CoglReadPixelsFlags;

COGL_END_DECLS

#endif /* __COGL_TYPES_H__ */
