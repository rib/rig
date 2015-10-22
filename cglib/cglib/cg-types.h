/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_TYPES_H__
#define __CG_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <clib.h>

#include <cglib/cg-defines.h>

/* Guard C code in headers, while including them from C++ */
#ifdef __cplusplus
#define CG_BEGIN_DECLS extern "C" {
#define CG_END_DECLS }
#else
#define CG_BEGIN_DECLS
#define CG_END_DECLS
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cg-types
 * @short_description: Types used throughout the library
 *
 * General types used by various CGlib functions.
 */

#if __GNUC__ >= 4
#define CG_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#else
#define CG_GNUC_NULL_TERMINATED
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define CG_GNUC_DEPRECATED __attribute__((__deprecated__))
#else
#define CG_GNUC_DEPRECATED
#endif /* __GNUC__ */

/* Some structures are meant to be opaque but they have public
   definitions because we want the size to be public so they can be
   allocated on the stack. This macro is used to ensure that users
   don't accidentally access private members */
#ifdef CG_COMPILATION
#define CG_PRIVATE(x) x
#else
#define CG_PRIVATE(x) private_member_##x
#endif

/**
 * cg_func_ptr_t:
 *
 * The type used by cg for function pointers, note that this type
 * is used as a generic catch-all cast for function pointers and the
 * actual arguments and return type may be different.
 */
typedef void (*cg_func_ptr_t)(void);

typedef struct _cg_color_t cg_color_t;

/* Enum declarations */

/**
 * cg_pixel_format_t:
 * @CG_PIXEL_FORMAT_ANY: Any format
 * @CG_PIXEL_FORMAT_A_8: 8 bit, unsigned, normalized [0,1] component
 * @CG_PIXEL_FORMAT_A_8SN: 8 bit, signed, normalized [-1,1] component
 * @CG_PIXEL_FORMAT_A_16U: 16 bit unsigned integer component
 * @CG_PIXEL_FORMAT_A_16F: 16 bit half-float component
 * @CG_PIXEL_FORMAT_A_32U: 32 bit unsigned integer component
 * @CG_PIXEL_FORMAT_A_32F: 32 bit float component
 * @CG_PIXEL_FORMAT_RG_88: RG, 8 bit, unsigned, normalized [0,1] value per component
 * @CG_PIXEL_FORMAT_RG_88SN: RG, 8 bit, signed, normalized [-1,1] value per component
 * @CG_PIXEL_FORMAT_RG_1616U: RG, 16 bit unsigned integer per component
 * @CG_PIXEL_FORMAT_RG_1616F: RG, 16 bit half-float per component
 * @CG_PIXEL_FORMAT_RG_3232U: RG, 32 bit unsigned integer per component
 * @CG_PIXEL_FORMAT_RG_3232F: RG, 32 bit float per component
 * @CG_PIXEL_FORMAT_RGB_565: RGB, 16 bits
 * @CG_PIXEL_FORMAT_RGB_888: RGB, 24 bits: 8 bit unsigned normalized [0,1]
 *                           value per component
 * @CG_PIXEL_FORMAT_BGR_888: BGR, 24 bits: 8 bit unsigned normalized [0,1]
 *                           value per component
 * @CG_PIXEL_FORMAT_RGB_161616U: RGB, 16 bit shorts per component
 * @CG_PIXEL_FORMAT_BGR_161616U: BGR, 16 bit shorts per component
 * @CG_PIXEL_FORMAT_RGB_161616F: RGB, 16 bit half-floats per component
 * @CG_PIXEL_FORMAT_BGR_161616F: BGR, 16 bit half-floats per component
 * @CG_PIXEL_FORMAT_RGB_323232U: RGB, 32 bit unsigned ints per component
 * @CG_PIXEL_FORMAT_BGR_323232U: BGR, 32 bit unsigned ints per component
 * @CG_PIXEL_FORMAT_RGB_323232F: RGB, 32 bit floats per component
 * @CG_PIXEL_FORMAT_BGR_323232F: BGR, 32 bit floats per component
 * @CG_PIXEL_FORMAT_RGBA_4444: RGBA, 16 bits
 * @CG_PIXEL_FORMAT_RGBA_5551: RGBA, 16 bits
 * @CG_PIXEL_FORMAT_RGBA_4444_PRE: Premultiplied RGBA, 16 bits
 * @CG_PIXEL_FORMAT_RGBA_5551_PRE: Premultiplied RGBA, 16 bits
 * @CG_PIXEL_FORMAT_RGBA_8888: RGBA, 8 bit, unsigned, normalized [0,1] component
 * @CG_PIXEL_FORMAT_BGRA_8888: BGRA, 8 bit, unsigned, normalized [0,1] component
 * @CG_PIXEL_FORMAT_ARGB_8888: ARGB, 8 bit, unsigned, normalized [0,1] component
 * @CG_PIXEL_FORMAT_ABGR_8888: ABGR, 8 bit, unsigned, normalized [0,1] component
 * @CG_PIXEL_FORMAT_RGBA_8888_PRE: Premultiplied RGBA, 32 bits
 * @CG_PIXEL_FORMAT_BGRA_8888_PRE: Premultiplied BGRA, 32 bits
 * @CG_PIXEL_FORMAT_ARGB_8888_PRE: Premultiplied ARGB, 32 bits
 * @CG_PIXEL_FORMAT_ABGR_8888_PRE: Premultiplied ABGR, 32 bits
 * @CG_PIXEL_FORMAT_RGBA_8888SN: RGBA, 8 bit, signed, normalized [-1,1] value per component
 * @CG_PIXEL_FORMAT_BGRA_8888SN: BGRA, 8 bit, signed, normalized [-1,1] value per component
 * @CG_PIXEL_FORMAT_RGBA_1010102 : RGBA, 10 bit, unsigned, normalized [0,1] RGB components, 2 bit alpha
 * @CG_PIXEL_FORMAT_BGRA_1010102 : BGRA, 10 bit, unsigned, normalized [0,1] RGB components, 2 bit alpha
 * @CG_PIXEL_FORMAT_ARGB_2101010 : ARGB, 10 bit, unsigned, normalized [0,1] RGB components, 2 bit alpha
 * @CG_PIXEL_FORMAT_ABGR_2101010 : ABGR, 10 bit, unsigned, normalized [0,1] RGB components, 2 bit alpha
 * @CG_PIXEL_FORMAT_RGBA_1010102_PRE: Premultiplied RGBA, 32 bits, 10 bpc
 * @CG_PIXEL_FORMAT_BGRA_1010102_PRE: Premultiplied BGRA, 32 bits, 10 bpc
 * @CG_PIXEL_FORMAT_ARGB_2101010_PRE: Premultiplied ARGB, 32 bits, 10 bpc
 * @CG_PIXEL_FORMAT_ABGR_2101010_PRE: Premultiplied ABGR, 32 bits, 10 bpc
 * @CG_PIXEL_FORMAT_RGBA_16161616U: 16 bit unsigned ints per component
 * @CG_PIXEL_FORMAT_BGRA_16161616U: 16 bit unsigned ints per component
 * @CG_PIXEL_FORMAT_RGBA_16161616F: 16 bit half-floats per component
 * @CG_PIXEL_FORMAT_BGRA_16161616F: 16 bit half-floats per component
 * @CG_PIXEL_FORMAT_RGBA_16161616F_PRE: Premultiplied RGBA, 16 bit half-floats per component
 * @CG_PIXEL_FORMAT_BGRA_16161616F_PRE: Premultiplied BGRA, 16 bit half-floats per component
 * @CG_PIXEL_FORMAT_RGBA_32323232U: 32 bit unsigned ints per component
 * @CG_PIXEL_FORMAT_BGRA_32323232U: 32 bit unsigned ints per component
 * @CG_PIXEL_FORMAT_RGBA_32323232F: 32 bit floats per component
 * @CG_PIXEL_FORMAT_BGRA_32323232F: 32 bit floats per component
 * @CG_PIXEL_FORMAT_RGBA_32323232F_PRE: Premultiplied RGBA, 32 bit floats per component
 * @CG_PIXEL_FORMAT_BGRA_32323232F_PRE: Premultiplied BGRA, 32 bit floats per component
 * @CG_PIXEL_FORMAT_DEPTH_16: Depth, 16 bits
 * @CG_PIXEL_FORMAT_DEPTH_32: Depth, 32 bits
 * @CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8: Depth/Stencil, 24/8 bits
 *
 * Pixel formats used by CGlib. For the formats with byte aligned
 * component, the order of the components specify the order in
 * increasing memory addresses. So for example
 * %CG_PIXEL_FORMAT_RGB_888 would have the red component in the lowest
 * address, green in the next address and blue after that regardless
 * of the endianness of the system.
 *
 * For the formats with non byte aligned components the component
 * order specifies the order within a 16-bit or 32-bit number from
 * most significant bit to least significant. So for
 * %CG_PIXEL_FORMAT_RGB_565, the red component would be in bits
 * 11-15, the green component would be in 6-11 and the blue component
 * would be in 1-5. Therefore the order in memory depends on the
 * endianness of the system.
 *
 * Note that red-green textures are only available if
 * %CG_FEATURE_ID_TEXTURE_RG is advertised. See
 * cg_texture_set_components() for details.
 */
typedef enum { /*< prefix=CG_PIXEL_FORMAT >*/
    CG_PIXEL_FORMAT_ANY,
    CG_PIXEL_FORMAT_A_8,
    CG_PIXEL_FORMAT_A_8SN,
    CG_PIXEL_FORMAT_A_16U,
    CG_PIXEL_FORMAT_A_16F,
    CG_PIXEL_FORMAT_A_32U,
    CG_PIXEL_FORMAT_A_32F,
    CG_PIXEL_FORMAT_RG_88,
    CG_PIXEL_FORMAT_RG_88SN,
    CG_PIXEL_FORMAT_RG_1616U,
    CG_PIXEL_FORMAT_RG_1616F,
    CG_PIXEL_FORMAT_RG_3232U,
    CG_PIXEL_FORMAT_RG_3232F,
    CG_PIXEL_FORMAT_RGB_565,
    CG_PIXEL_FORMAT_RGB_888,
    CG_PIXEL_FORMAT_BGR_888,
    CG_PIXEL_FORMAT_RGB_888SN,
    CG_PIXEL_FORMAT_BGR_888SN,
    CG_PIXEL_FORMAT_RGB_161616U,
    CG_PIXEL_FORMAT_BGR_161616U,
    CG_PIXEL_FORMAT_RGB_161616F,
    CG_PIXEL_FORMAT_BGR_161616F,
    CG_PIXEL_FORMAT_RGB_323232U,
    CG_PIXEL_FORMAT_BGR_323232U,
    CG_PIXEL_FORMAT_RGB_323232F,
    CG_PIXEL_FORMAT_BGR_323232F,
    CG_PIXEL_FORMAT_RGBA_4444,
    CG_PIXEL_FORMAT_RGBA_4444_PRE,
    CG_PIXEL_FORMAT_RGBA_5551,
    CG_PIXEL_FORMAT_RGBA_5551_PRE,
    CG_PIXEL_FORMAT_RGBA_8888,
    CG_PIXEL_FORMAT_BGRA_8888,
    CG_PIXEL_FORMAT_ARGB_8888,
    CG_PIXEL_FORMAT_ABGR_8888,
    CG_PIXEL_FORMAT_RGBA_8888_PRE,
    CG_PIXEL_FORMAT_BGRA_8888_PRE,
    CG_PIXEL_FORMAT_ARGB_8888_PRE,
    CG_PIXEL_FORMAT_ABGR_8888_PRE,
    CG_PIXEL_FORMAT_RGBA_8888SN,
    CG_PIXEL_FORMAT_BGRA_8888SN,
    CG_PIXEL_FORMAT_RGBA_1010102,
    CG_PIXEL_FORMAT_BGRA_1010102,
    CG_PIXEL_FORMAT_ARGB_2101010,
    CG_PIXEL_FORMAT_ABGR_2101010,
    CG_PIXEL_FORMAT_RGBA_1010102_PRE,
    CG_PIXEL_FORMAT_BGRA_1010102_PRE,
    CG_PIXEL_FORMAT_ARGB_2101010_PRE,
    CG_PIXEL_FORMAT_ABGR_2101010_PRE,
    CG_PIXEL_FORMAT_RGBA_16161616U,
    CG_PIXEL_FORMAT_BGRA_16161616U,
    CG_PIXEL_FORMAT_RGBA_16161616F,
    CG_PIXEL_FORMAT_BGRA_16161616F,
    CG_PIXEL_FORMAT_RGBA_16161616F_PRE,
    CG_PIXEL_FORMAT_BGRA_16161616F_PRE,
    CG_PIXEL_FORMAT_RGBA_32323232U,
    CG_PIXEL_FORMAT_BGRA_32323232U,
    CG_PIXEL_FORMAT_RGBA_32323232F,
    CG_PIXEL_FORMAT_BGRA_32323232F,
    CG_PIXEL_FORMAT_RGBA_32323232F_PRE,
    CG_PIXEL_FORMAT_BGRA_32323232F_PRE,
    CG_PIXEL_FORMAT_DEPTH_16,
    CG_PIXEL_FORMAT_DEPTH_32,
    CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8
} cg_pixel_format_t;

/**
 * cg_color_t:
 * @red: amount of red
 * @green: amount of green
 * @blue: amount of green
 * @alpha: alpha
 *
 * A structure for holding a single color definition.
 *
 */
struct _cg_color_t {
    float red;
    float green;
    float blue;
    float alpha;
};
_C_STRUCT_SIZE_ASSERT(cg_color_t, 16);

/**
 * CG_BLEND_STRING_ERROR:
 *
 * #cg_error_t domain for blend string parser errors
 *
 */
#define CG_BLEND_STRING_ERROR (cg_blend_string_error_domain())

/**
 * cg_blend_string_error_t:
 * @CG_BLEND_STRING_ERROR_PARSE_ERROR: Generic parse error
 * @CG_BLEND_STRING_ERROR_ARGUMENT_PARSE_ERROR: Argument parse error
 * @CG_BLEND_STRING_ERROR_INVALID_ERROR: Internal parser error
 * @CG_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR: Blend string not
 *   supported by the GPU
 *
 * Error enumeration for the blend strings parser
 *
 */
typedef enum { /*< prefix=CG_BLEND_STRING_ERROR >*/
    CG_BLEND_STRING_ERROR_PARSE_ERROR,
    CG_BLEND_STRING_ERROR_ARGUMENT_PARSE_ERROR,
    CG_BLEND_STRING_ERROR_INVALID_ERROR,
    CG_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR
} cg_blend_string_error_t;

uint32_t cg_blend_string_error_domain(void);

#define CG_SYSTEM_ERROR (_cg_system_error_domain())

/**
 * cg_system_error_t:
 * @CG_SYSTEM_ERROR_UNSUPPORTED: You tried to use a feature or
 *    configuration not currently available.
 * @CG_SYSTEM_ERROR_NO_MEMORY: You tried to allocate a resource
 *    such as a texture and there wasn't enough memory.
 *
 * Error enumeration for CGlib
 *
 * The @CG_SYSTEM_ERROR_UNSUPPORTED error can be thrown for a
 * variety of reasons. For example:
 *
 * <itemizedlist>
 *  <listitem><para>You've tried to use a feature that is not
 *   advertised by cg_has_feature(). This could happen if you create
 *   a 2d texture with a non-power-of-two size when
 *   %CG_FEATURE_ID_TEXTURE_NPOT is not advertised.</para></listitem>
 *  <listitem><para>The GPU can not handle the configuration you have
 *   requested. An example might be if you try to use too many texture
 *   layers in a single #cg_pipeline_t</para></listitem>
 *  <listitem><para>The driver does not support some
 *   configuration.</para></listiem>
 * </itemizedlist>
 *
 * Currently this is only used by CGlib API marked as experimental so
 * this enum should also be considered experimental.
 *
 * Stability: unstable
 */
typedef enum { /*< prefix=CG_ERROR >*/
    CG_SYSTEM_ERROR_UNSUPPORTED,
    CG_SYSTEM_ERROR_NO_MEMORY
} cg_system_error_t;

uint32_t _cg_system_error_domain(void);

/**
 * cg_attribute_type_t:
 * @CG_ATTRIBUTE_TYPE_BYTE: Data is the same size of a byte
 * @CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE: Data is the same size of an
 *   unsigned byte
 * @CG_ATTRIBUTE_TYPE_SHORT: Data is the same size of a short integer
 * @CG_ATTRIBUTE_TYPE_UNSIGNED_SHORT: Data is the same size of
 *   an unsigned short integer
 * @CG_ATTRIBUTE_TYPE_FLOAT: Data is the same size of a float
 *
 * Data types for the components of a vertex attribute.
 *
 */
typedef enum {
    CG_ATTRIBUTE_TYPE_BYTE = 0x1400,
    CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE = 0x1401,
    CG_ATTRIBUTE_TYPE_SHORT = 0x1402,
    CG_ATTRIBUTE_TYPE_UNSIGNED_SHORT = 0x1403,
    CG_ATTRIBUTE_TYPE_FLOAT = 0x1406
} cg_attribute_type_t;

/**
 * cg_indices_type_t:
 * @CG_INDICES_TYPE_UNSIGNED_BYTE: Your indices are unsigned bytes
 * @CG_INDICES_TYPE_UNSIGNED_SHORT: Your indices are unsigned shorts
 * @CG_INDICES_TYPE_UNSIGNED_INT: Your indices are unsigned ints
 *
 * You should aim to use the smallest data type that gives you enough
 * range, since it reduces the size of your index array and can help
 * reduce the demand on memory bandwidth.
 *
 * Note that %CG_INDICES_TYPE_UNSIGNED_INT is only supported if the
 * %CG_FEATURE_ID_UNSIGNED_INT_INDICES feature is available. This
 * should always be available on OpenGL but on OpenGL ES it will only
 * be available if the GL_OES_element_index_uint extension is
 * advertized.
 */
typedef enum {
    CG_INDICES_TYPE_UNSIGNED_BYTE,
    CG_INDICES_TYPE_UNSIGNED_SHORT,
    CG_INDICES_TYPE_UNSIGNED_INT
} cg_indices_type_t;

/**
 * cg_vertices_mode_t:
 * @CG_VERTICES_MODE_POINTS: FIXME, equivalent to
 * <constant>GL_POINTS</constant>
 * @CG_VERTICES_MODE_LINES: FIXME, equivalent to <constant>GL_LINES</constant>
 * @CG_VERTICES_MODE_LINE_LOOP: FIXME, equivalent to
 * <constant>GL_LINE_LOOP</constant>
 * @CG_VERTICES_MODE_LINE_STRIP: FIXME, equivalent to
 * <constant>GL_LINE_STRIP</constant>
 * @CG_VERTICES_MODE_TRIANGLES: FIXME, equivalent to
 * <constant>GL_TRIANGLES</constant>
 * @CG_VERTICES_MODE_TRIANGLE_STRIP: FIXME, equivalent to
 * <constant>GL_TRIANGLE_STRIP</constant>
 * @CG_VERTICES_MODE_TRIANGLE_FAN: FIXME, equivalent to
 *<constant>GL_TRIANGLE_FAN</constant>
 *
 * Different ways of interpreting vertices when drawing.
 *
 */
typedef enum {
    CG_VERTICES_MODE_POINTS = 0x0000,
    CG_VERTICES_MODE_LINES = 0x0001,
    CG_VERTICES_MODE_LINE_LOOP = 0x0002,
    CG_VERTICES_MODE_LINE_STRIP = 0x0003,
    CG_VERTICES_MODE_TRIANGLES = 0x0004,
    CG_VERTICES_MODE_TRIANGLE_STRIP = 0x0005,
    CG_VERTICES_MODE_TRIANGLE_FAN = 0x0006
} cg_vertices_mode_t;

/* NB: The above definitions are taken from gl.h equivalents */

/* XXX: should this be CGlibMaterialDepthTestFunction?
 * It makes it very verbose but would be consistent with
 * CGlibMaterialWrapMode */

/**
 * cg_depth_test_function_t:
 * @CG_DEPTH_TEST_FUNCTION_NEVER: Never passes.
 * @CG_DEPTH_TEST_FUNCTION_LESS: Passes if the fragment's depth
 * value is less than the value currently in the depth buffer.
 * @CG_DEPTH_TEST_FUNCTION_EQUAL: Passes if the fragment's depth
 * value is equal to the value currently in the depth buffer.
 * @CG_DEPTH_TEST_FUNCTION_LEQUAL: Passes if the fragment's depth
 * value is less or equal to the value currently in the depth buffer.
 * @CG_DEPTH_TEST_FUNCTION_GREATER: Passes if the fragment's depth
 * value is greater than the value currently in the depth buffer.
 * @CG_DEPTH_TEST_FUNCTION_NOTEQUAL: Passes if the fragment's depth
 * value is not equal to the value currently in the depth buffer.
 * @CG_DEPTH_TEST_FUNCTION_GEQUAL: Passes if the fragment's depth
 * value greater than or equal to the value currently in the depth buffer.
 * @CG_DEPTH_TEST_FUNCTION_ALWAYS: Always passes.
 *
 * When using depth testing one of these functions is used to compare
 * the depth of an incoming fragment against the depth value currently
 * stored in the depth buffer. The function is changed using
 * cg_depth_state_set_test_function().
 *
 * The test is only done when depth testing is explicitly enabled. (See
 * cg_depth_state_set_test_enabled())
 */
typedef enum {
    CG_DEPTH_TEST_FUNCTION_NEVER = 0x0200,
    CG_DEPTH_TEST_FUNCTION_LESS = 0x0201,
    CG_DEPTH_TEST_FUNCTION_EQUAL = 0x0202,
    CG_DEPTH_TEST_FUNCTION_LEQUAL = 0x0203,
    CG_DEPTH_TEST_FUNCTION_GREATER = 0x0204,
    CG_DEPTH_TEST_FUNCTION_NOTEQUAL = 0x0205,
    CG_DEPTH_TEST_FUNCTION_GEQUAL = 0x0206,
    CG_DEPTH_TEST_FUNCTION_ALWAYS = 0x0207
} cg_depth_test_function_t;
/* NB: The above definitions are taken from gl.h equivalents */

typedef enum { /*< prefix=CG_RENDERER_ERROR >*/
    CG_RENDERER_ERROR_XLIB_DISPLAY_OPEN,
    CG_RENDERER_ERROR_BAD_CONSTRAINT
} cg_renderer_error_t;

/**
 * cg_filter_return_t:
 * @CG_FILTER_CONTINUE: The event was not handled, continues the
 *                        processing
 * @CG_FILTER_REMOVE: Remove the event, stops the processing
 *
 * Return values for the #cg_xlib_filter_func_t and #cg_win32_filter_func_t
 * functions.
 *
 * Stability: Unstable
 */
typedef enum _cg_filter_return_t { /*< prefix=CG_FILTER >*/
    CG_FILTER_CONTINUE,
    CG_FILTER_REMOVE
} cg_filter_return_t;

typedef enum _cg_winsys_feature_t {
    /* Available if the window system can support multiple onscreen
     * framebuffers at the same time. */
    CG_WINSYS_FEATURE_MULTIPLE_ONSCREEN,

    /* Available if onscreen framebuffer swaps can be automatically
     * throttled to the vblank frequency. */
    CG_WINSYS_FEATURE_SWAP_THROTTLE,

    /* Available if its possible to query a counter that
     * increments at each vblank. */
    CG_WINSYS_FEATURE_VBLANK_COUNTER,

    /* Available if its possible to wait until the next vertical
     * blank period */
    CG_WINSYS_FEATURE_VBLANK_WAIT,

    /* Available if the window system supports mapping native
     * pixmaps to textures. */
    CG_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP,

    /* Available if it's possible to swap a list of sub rectangles
     * from the back buffer to the front buffer */
    CG_WINSYS_FEATURE_SWAP_REGION,

    /* Available if swap_region requests can be automatically throttled
     * to the vblank frequency. */
    CG_WINSYS_FEATURE_SWAP_REGION_THROTTLE,

    /* Available if the swap region implementation won't tear and thus
     * only needs to be throttled to the framerate */
    CG_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED,

    /* Avaiable if the age of the back buffer can be queried */
    CG_WINSYS_FEATURE_BUFFER_AGE,

    /* Avaiable if the winsys directly handles _SYNC and _COMPLETE events */
    CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
    CG_WINSYS_FEATURE_N_FEATURES
} cg_winsys_feature_t;

/**
 * cg_color_mask_t:
 * @CG_COLOR_MASK_NONE: None of the color channels are masked
 * @CG_COLOR_MASK_RED: Masks the red color channel
 * @CG_COLOR_MASK_GREEN: Masks the green color channel
 * @CG_COLOR_MASK_BLUE: Masks the blue color channel
 * @CG_COLOR_MASK_ALPHA: Masks the alpha color channel
 * @CG_COLOR_MASK_ALL: All of the color channels are masked
 *
 * Defines a bit mask of color channels. This can be used with
 * cg_pipeline_set_color_mask() for example to define which color
 * channels should be written to the current framebuffer when
 * drawing something.
 */
typedef enum {
    CG_COLOR_MASK_NONE = 0,
    CG_COLOR_MASK_RED = 1L << 0,
    CG_COLOR_MASK_GREEN = 1L << 1,
    CG_COLOR_MASK_BLUE = 1L << 2,
    CG_COLOR_MASK_ALPHA = 1L << 3,
    /* XXX: glib-mkenums is a perl script that can't cope if we split
     * this onto multiple lines! *sigh* */
    CG_COLOR_MASK_ALL = (CG_COLOR_MASK_RED | CG_COLOR_MASK_GREEN |
                         CG_COLOR_MASK_BLUE | CG_COLOR_MASK_ALPHA)
} cg_color_mask_t;

/**
 * cg_winding_t:
 * @CG_WINDING_CLOCKWISE: Vertices are in a clockwise order
 * @CG_WINDING_COUNTER_CLOCKWISE: Vertices are in a counter-clockwise order
 *
 * Enum used to represent the two directions of rotation. This can be
 * used to set the front face for culling by calling
 * cg_pipeline_set_front_face_winding().
 */
typedef enum {
    CG_WINDING_CLOCKWISE,
    CG_WINDING_COUNTER_CLOCKWISE
} cg_winding_t;

/**
 * cg_buffer_bit_t:
 * @CG_BUFFER_BIT_COLOR: Selects the primary color buffer
 * @CG_BUFFER_BIT_DEPTH: Selects the depth buffer
 * @CG_BUFFER_BIT_STENCIL: Selects the stencil buffer
 *
 * Types of auxiliary buffers
 *
 */
typedef enum {
    CG_BUFFER_BIT_COLOR = 1 << 0,
    CG_BUFFER_BIT_DEPTH = 1 << 1,
    CG_BUFFER_BIT_STENCIL = 1 << 2
} cg_buffer_bit_t;

/**
 * cg_read_pixels_flags_t:
 * @CG_READ_PIXELS_COLOR_BUFFER: Read from the color buffer
 *
 * Flags for cg_framebuffer_read_pixels_into_bitmap()
 *
 */
typedef enum { /*< prefix=CG_READ_PIXELS >*/
    CG_READ_PIXELS_COLOR_BUFFER = 1L << 0
} cg_read_pixels_flags_t;

CG_END_DECLS

#endif /* __CG_TYPES_H__ */
