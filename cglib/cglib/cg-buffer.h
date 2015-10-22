/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C)2010 Intel Corporation.
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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_BUFFER_H__
#define __CG_BUFFER_H__

#include <cglib/cg-types.h>
#include <cglib/cg-error.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-buffer
 * @short_description: Common buffer functions, including data upload APIs
 * @stability: unstable
 *
 * The cg_buffer_t API provides a common interface to manipulate
 * buffers that have been allocated either via cg_pixel_buffer_new()
 * or cg_attribute_buffer_new(). The API allows you to upload data
 * to these buffers and define usage hints that help CGlib manage your
 * buffer optimally.
 *
 * Data can either be uploaded by supplying a pointer and size so CGlib
 * can copy your data, or you can mmap() a cg_buffer_t and then you can
 * copy data to the buffer directly.
 *
 * One of the most common uses for cg_buffer_ts is to upload texture
 * data asynchronously since the ability to mmap the buffers into
 * the CPU makes it possible for another thread to handle the IO
 * of loading an image file and unpacking it into the mapped buffer
 * without blocking other CGlib operations.
 */

#ifdef __CG_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void cg_buffer_t;
#else
typedef struct _cg_buffer_t cg_buffer_t;
#define CG_BUFFER(buffer) ((cg_buffer_t *)(buffer))
#endif

#define CG_BUFFER_ERROR (_cg_buffer_error_domain())

/**
 * cg_buffer_error_t:
 * @CG_BUFFER_ERROR_MAP: A buffer could not be mapped either
 *    because the feature isn't supported or because a system
 *    limitation was hit.
 *
 * Error enumeration for #cg_buffer_t
 *
 * Stability: unstable
 */
typedef enum { /*< prefix=CG_BUFFER_ERROR >*/
    CG_BUFFER_ERROR_MAP,
} cg_buffer_error_t;

uint32_t _cg_buffer_error_domain(void);

/**
 * cg_is_buffer:
 * @object: a buffer object
 *
 * Checks whether @buffer is a buffer object.
 *
 * Return value: %true if the handle is a cg_buffer_t, and %false otherwise
 *
 * Stability: unstable
 */
bool cg_is_buffer(void *object);

/**
 * cg_buffer_get_size:
 * @buffer: a buffer object
 *
 * Retrieves the size of buffer
 *
 * Return value: the size of the buffer in bytes
 *
 * Stability: unstable
 */
unsigned int cg_buffer_get_size(cg_buffer_t *buffer);

/**
 * cg_buffer_update_hint_t:
 * @CG_BUFFER_UPDATE_HINT_STATIC: the buffer will not change over time
 * @CG_BUFFER_UPDATE_HINT_DYNAMIC: the buffer will change from time to time
 * @CG_BUFFER_UPDATE_HINT_STREAM: the buffer will be used once or a couple of
 *   times
 *
 * The update hint on a buffer allows the user to give some detail on how often
 * the buffer data is going to be updated.
 *
 * Stability: unstable
 */
typedef enum { /*< prefix=CG_BUFFER_UPDATE_HINT >*/
    CG_BUFFER_UPDATE_HINT_STATIC,
    CG_BUFFER_UPDATE_HINT_DYNAMIC,
    CG_BUFFER_UPDATE_HINT_STREAM
} cg_buffer_update_hint_t;

/**
 * cg_buffer_set_update_hint:
 * @buffer: a buffer object
 * @hint: the new hint
 *
 * Sets the update hint on a buffer. See #cg_buffer_update_hint_t for a
 * description
 * of the available hints.
 *
 * Stability: unstable
 */
void cg_buffer_set_update_hint(cg_buffer_t *buffer,
                               cg_buffer_update_hint_t hint);

/**
 * cg_buffer_get_update_hint:
 * @buffer: a buffer object
 *
 * Retrieves the update hints set using cg_buffer_set_update_hint()
 *
 * Return value: the #cg_buffer_update_hint_t currently used by the buffer
 *
 * Stability: unstable
 */
cg_buffer_update_hint_t cg_buffer_get_update_hint(cg_buffer_t *buffer);

/**
 * cg_buffer_access_t:
 * @CG_BUFFER_ACCESS_READ: the buffer will be read
 * @CG_BUFFER_ACCESS_WRITE: the buffer will written to
 * @CG_BUFFER_ACCESS_READ_WRITE: the buffer will be used for both reading and
 *   writing
 *
 * The access hints for cg_buffer_set_update_hint()
 *
 * Stability: unstable
 */
typedef enum { /*< prefix=CG_BUFFER_ACCESS >*/
    CG_BUFFER_ACCESS_READ = 1 << 0,
    CG_BUFFER_ACCESS_WRITE = 1 << 1,
    CG_BUFFER_ACCESS_READ_WRITE =
        CG_BUFFER_ACCESS_READ | CG_BUFFER_ACCESS_WRITE
} cg_buffer_access_t;

/**
 * cg_buffer_map_hint_t:
 * @CG_BUFFER_MAP_HINT_DISCARD: Tells CGlib that you plan to replace
 *    all the buffer's contents. When this flag is used to map a
 *    buffer, the entire contents of the buffer become undefined, even
 *    if only a subregion of the buffer is mapped.
 * @CG_BUFFER_MAP_HINT_DISCARD_RANGE: Tells CGlib that you plan to
 *    replace all the contents of the mapped region. The contents of
 *    the region specified are undefined after this flag is used to
 *    map a buffer.
 *
 * Hints to CGlib about how you are planning to modify the data once it
 * is mapped.
 *
 * Stability: unstable
 */
typedef enum { /*< prefix=CG_BUFFER_MAP_HINT >*/
    CG_BUFFER_MAP_HINT_DISCARD = 1 << 0,
    CG_BUFFER_MAP_HINT_DISCARD_RANGE = 1 << 1
} cg_buffer_map_hint_t;

/**
 * cg_buffer_map:
 * @buffer: a buffer object
 * @access: how the mapped buffer will be used by the application
 * @hints: A mask of #cg_buffer_map_hint_t<!-- -->s that tell CGlib how
 *   the data will be modified once mapped.
 * @error: A #cg_error_t for catching exceptional errors
 *
 * Maps the buffer into the application address space for direct
 * access. This is equivalent to calling cg_buffer_map_range() with
 * zero as the offset and the size of the entire buffer as the size.
 *
 * It is strongly recommended that you pass
 * %CG_BUFFER_MAP_HINT_DISCARD as a hint if you are going to replace
 * all the buffer's data. This way if the buffer is currently being
 * used by the GPU then the driver won't have to stall the CPU and
 * wait for the hardware to finish because it can instead allocate a
 * new buffer to map.
 *
 * The behaviour is undefined if you access the buffer in a way
 * conflicting with the @access mask you pass. It is also an error to
 * release your last reference while the buffer is mapped.
 *
 * Return value: (transfer none): A pointer to the mapped memory or
 *        %NULL is the call fails
 *
 * Stability: unstable
 */
void *cg_buffer_map(cg_buffer_t *buffer,
                    cg_buffer_access_t access,
                    cg_buffer_map_hint_t hints,
                    cg_error_t **error);

/**
 * cg_buffer_map_range:
 * @buffer: a buffer object
 * @offset: Offset within the buffer to start the mapping
 * @size: The size of data to map
 * @access: how the mapped buffer will be used by the application
 * @hints: A mask of #cg_buffer_map_hint_t<!-- -->s that tell CGlib how
 *   the data will be modified once mapped.
 * @error: A #cg_error_t for catching exceptional errors
 *
 * Maps a sub-region of the buffer into the application's address space
 * for direct access.
 *
 * It is strongly recommended that you pass
 * %CG_BUFFER_MAP_HINT_DISCARD as a hint if you are going to replace
 * all the buffer's data. This way if the buffer is currently being
 * used by the GPU then the driver won't have to stall the CPU and
 * wait for the hardware to finish because it can instead allocate a
 * new buffer to map. You can pass
 * %CG_BUFFER_MAP_HINT_DISCARD_RANGE instead if you want the
 * regions outside of the mapping to be retained.
 *
 * The behaviour is undefined if you access the buffer in a way
 * conflicting with the @access mask you pass. It is also an error to
 * release your last reference while the buffer is mapped.
 *
 * Return value: (transfer none): A pointer to the mapped memory or
 *        %NULL is the call fails
 *
 * Stability: unstable
 */
void *cg_buffer_map_range(cg_buffer_t *buffer,
                          size_t offset,
                          size_t size,
                          cg_buffer_access_t access,
                          cg_buffer_map_hint_t hints,
                          cg_error_t **error);

/**
 * cg_buffer_unmap:
 * @buffer: a buffer object
 *
 * Unmaps a buffer previously mapped by cg_buffer_map().
 *
 * Stability: unstable
 */
void cg_buffer_unmap(cg_buffer_t *buffer);

/**
 * cg_buffer_set_data:
 * @buffer: a buffer object
 * @offset: destination offset (in bytes) in the buffer
 * @data: a pointer to the data to be copied into the buffer
 * @size: number of bytes to copy
 * @error: A #cg_error_t for catching exceptional errors
 *
 * Updates part of the buffer with new data from @data. Where to put this new
 * data is controlled by @offset and @offset + @data should be less than the
 * buffer size.
 *
 * Return value: %true is the operation succeeded, %false otherwise
 *
 * Stability: unstable
 */
bool cg_buffer_set_data(cg_buffer_t *buffer,
                        size_t offset,
                        const void *data,
                        size_t size,
                        cg_error_t **error);

CG_END_DECLS

#endif /* __CG_BUFFER_H__ */
