#ifndef _TEST_UTILS_H_
#define _TEST_UTILS_H_

/* NB: This header is for private and public api testing and so
 * we need consider that if we are testing the public api we should
 * just include <cogl/cogl.h> but since that will only provide
 * opaque typedefs we need to include the specific internal headers
 * for testing private apis...
 */
#ifdef CG_COMPILATION
#include <cogl/cogl-device.h>
#include <cogl/cogl-onscreen.h>
#include <cogl/cogl-offscreen.h>
#include <cogl/cogl-texture-2d.h>
#include <cogl/cogl-primitive-texture.h>
#include <cogl/cogl-texture-2d-sliced.h>
#include <cogl/cogl-meta-texture.h>
#include <cogl/cogl-atlas-texture.h>
#else
#include <cogl/cogl.h>
#endif

#include <clib.h>

/* We don't really care about functions that are defined without a
   header for the unit tests so we can just disable it here */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

typedef enum _TestFlags {
    TEST_KNOWN_FAILURE = 1 << 0,
    TEST_REQUIREMENT_GL = 1 << 1,
    TEST_REQUIREMENT_NPOT = 1 << 2,
    TEST_REQUIREMENT_TEXTURE_3D = 1 << 3,
    TEST_REQUIREMENT_TEXTURE_RG = 1 << 5,
    TEST_REQUIREMENT_POINT_SPRITE = 1 << 6,
    TEST_REQUIREMENT_GLES2_CONTEXT = 1 << 7,
    TEST_REQUIREMENT_MAP_WRITE = 1 << 8,
    TEST_REQUIREMENT_GLSL = 1 << 9,
    TEST_REQUIREMENT_OFFSCREEN = 1 << 10,
    TEST_REQUIREMENT_FENCE = 1 << 11,
    TEST_REQUIREMENT_PER_VERTEX_POINT_SIZE = 1 << 12
} TestFlags;

/**
 * TestUtilsTextureFlags:
 * @TEST_UTILS_TEXTURE_NONE: No flags specified
 * @TEST_UTILS_TEXTURE_NO_AUTO_MIPMAP: Disables the automatic generation of
 *   the mipmap pyramid from the base level image whenever it is
 *   updated. The mipmaps are only generated when the texture is
 *   rendered with a mipmap filter so it should be free to leave out
 *   this flag when using other filtering modes
 * @TEST_UTILS_TEXTURE_NO_SLICING: Disables the slicing of the texture
 * @TEST_UTILS_TEXTURE_NO_ATLAS: Disables the insertion of the texture inside
 *   the texture atlas used by Cogl
 *
 * Flags to pass to the test_utils_texture_new_* family of functions.
 */
typedef enum {
    TEST_UTILS_TEXTURE_NONE = 0,
    TEST_UTILS_TEXTURE_NO_AUTO_MIPMAP = 1 << 0,
    TEST_UTILS_TEXTURE_NO_SLICING = 1 << 1,
    TEST_UTILS_TEXTURE_NO_ATLAS = 1 << 2
} TestUtilsTextureFlags;

extern cg_device_t *test_dev;
extern cg_framebuffer_t *test_fb;

void test_utils_init(TestFlags requirement_flags,
                     TestFlags known_failure_flags);

void test_utils_fini(void);

/*
 * test_utils_texture_new_with_size:
 * @dev: A #cg_device_t
 * @width: width of texture in pixels.
 * @height: height of texture in pixels.
 * @flags: Optional flags for the texture, or %TEST_UTILS_TEXTURE_NONE
 * @components: What texture components are required
 *
 * Creates a new #cg_texture_t with the specified dimensions and pixel format.
 *
 * The storage for the texture is not necesarily created before this
 * function returns. The storage can be explicitly allocated using
 * cg_texture_allocate() or preferably you can let Cogl
 * automatically allocate the storage lazily when uploading data when
 * Cogl may know more about how the texture will be used and can
 * optimize how it is allocated.
 *
 * Return value: A newly created #cg_texture_t
 */
cg_texture_t *
test_utils_texture_new_with_size(cg_device_t *dev,
                                 int width,
                                 int height,
                                 TestUtilsTextureFlags flags,
                                 cg_texture_components_t components);

/*
 * test_utils_texture_new_from_data:
 * @dev: A #cg_device_t
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @flags: Optional flags for the texture, or %TEST_UTILS_TEXTURE_NONE
 * @format: the #cg_pixel_format_t the buffer is stored in in RAM
 * @rowstride: the memory offset in bytes between the starts of
 *    scanlines in @data
 * @data: pointer the memory region where the source buffer resides
 * @error: A #cg_error_t to catch exceptional errors or %NULL
 *
 * Creates a new #cg_texture_t based on data residing in memory.
 *
 * Note: If the given @format has an alpha channel then the data
 * will be loaded into a premultiplied internal format. If you want
 * to avoid having the source data be premultiplied then you can
 * either specify that the data is already premultiplied or use
 * test_utils_texture_new_from_bitmap which lets you explicitly
 * request whether the data should internally be premultipled or not.
 *
 * Return value: A newly created #cg_texture_t or %NULL on failure
 */
cg_texture_t *test_utils_texture_new_from_data(cg_device_t *dev,
                                               int width,
                                               int height,
                                               TestUtilsTextureFlags flags,
                                               cg_pixel_format_t format,
                                               int rowstride,
                                               const uint8_t *data);

/*
 * test_utils_texture_new_from_bitmap:
 * @bitmap: A #cg_bitmap_t pointer
 * @flags: Optional flags for the texture, or %TEST_UTILS_TEXTURE_NONE
 * @premultiplied: Whether the texture should hold premultipled data.
 *                 (if the bitmap already holds premultiplied data
 *                 and %TRUE is given then no premultiplication will
 *                 be done. The data will be premultipled while
 *                 uploading if the bitmap has an alpha channel but
 *                 does not already have a premultiplied format.)
 *
 * Creates a #cg_texture_t from a #cg_bitmap_t.
 *
 * Return value: A newly created #cg_texture_t or %NULL on failure
 */
cg_texture_t *test_utils_texture_new_from_bitmap(cg_bitmap_t *bitmap,
                                                 TestUtilsTextureFlags flags,
                                                 bool premultiplied);

/*
 * test_utils_check_pixel:
 * @framebuffer: The #cg_framebuffer_t to read from
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xRRGGBBAA representing the expected
 *         pixel value
 *
 * This performs reads a pixel on the given cogl @framebuffer and
 * asserts that it matches the given color. The alpha channel of the
 * color is ignored. The pixels are converted to a string and compared
 * with g_assert_cmpstr so that if the comparison fails then the
 * assert will display a meaningful message
 */
void test_utils_check_pixel(cg_framebuffer_t *framebuffer,
                            int x,
                            int y,
                            uint32_t expected_pixel);

/**
 * @framebuffer: The #cg_framebuffer_t to read from
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xRRGGBBAA representing the expected
 *         pixel value
 *
 * This performs reads a pixel on the given cogl @framebuffer and
 * asserts that it matches the given color. The alpha channel is also
 * checked unlike with test_utils_check_pixel(). The pixels are
 * converted to a string and compared with g_assert_cmpstr so that if
 * the comparison fails then the assert will display a meaningful
 * message.
 */
void test_utils_check_pixel_and_alpha(cg_framebuffer_t *fb,
                                      int x,
                                      int y,
                                      uint32_t expected_pixel);

/*
 * test_utils_check_pixel:
 * @framebuffer: The #cg_framebuffer_t to read from
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xrrggbb representing the expected pixel value
 *
 * This performs reads a pixel on the given cogl @framebuffer and
 * asserts that it matches the given color. The alpha channel of the
 * color is ignored. The pixels are converted to a string and compared
 * with g_assert_cmpstr so that if the comparison fails then the
 * assert will display a meaningful message
 */
void test_utils_check_pixel_rgb(
    cg_framebuffer_t *framebuffer, int x, int y, int r, int g, int b);

/*
 * test_utils_check_region:
 * @framebuffer: The #cg_framebuffer_t to read from
 * @x: x co-ordinate of the region to test
 * @y: y co-ordinate of the region to test
 * @width: width of the region to test
 * @height: height of the region to test
 * @pixel: An integer of the form 0xrrggbb representing the expected region
 * color
 *
 * Performs a read pixel on the specified region of the given cogl
 * @framebuffer and asserts that it matches the given color. The alpha
 * channel of the color is ignored. The pixels are converted to a
 * string and compared with g_assert_cmpstr so that if the comparison
 * fails then the assert will display a meaningful message
 */
void test_utils_check_region(cg_framebuffer_t *framebuffer,
                             int x,
                             int y,
                             int width,
                             int height,
                             uint32_t expected_rgba);

/*
 * test_utils_compare_pixel:
 * @screen_pixel: A pixel stored in memory
 * @expected_pixel: The expected RGBA value
 *
 * Compares a pixel from a buffer to an expected value. The pixels are
 * converted to a string and compared with g_assert_cmpstr so that if
 * the comparison fails then the assert will display a meaningful
 * message.
 */
void test_utils_compare_pixel(const uint8_t *screen_pixel,
                              uint32_t expected_pixel);

/*
 * test_utils_compare_pixel_and_alpha:
 * @screen_pixel: A pixel stored in memory
 * @expected_pixel: The expected RGBA value
 *
 * Compares a pixel from a buffer to an expected value. This is
 * similar to test_utils_compare_pixel() except that it doesn't ignore
 * the alpha component.
 */
void test_utils_compare_pixel_and_alpha(const uint8_t *screen_pixel,
                                        uint32_t expected_pixel);

/*
 * test_utils_create_color_texture:
 * @dev: A #cg_device_t
 * @color: A color to put in the texture
 *
 * Creates a 1x1-pixel RGBA texture filled with the given color.
 */
cg_texture_t *test_utils_create_color_texture(cg_device_t *dev,
                                              uint32_t color);

/* cg_test_verbose:
 *
 * Queries if the user asked for verbose output or not.
 */
bool cg_test_verbose(void);

/* test_util_is_pot:
 * @number: A number to test
 *
 * Returns whether the given integer is a power of two
 */
static inline bool
test_utils_is_pot(unsigned int number)
{
    /* Make sure there is only one bit set */
    return (number & (number - 1)) == 0;
}

#endif /* _TEST_UTILS_H_ */
