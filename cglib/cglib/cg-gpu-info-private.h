/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __CG_GPU_INFO_PRIVATE_H
#define __CG_GPU_INFO_PRIVATE_H

#include "cg-device.h"

typedef enum _cg_gpu_info_architecture_flag_t {
    CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE,
    CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED,
    CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE,
    CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
    CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_DEFERRED,
    CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE
} cg_gpu_info_architecture_flag_t;

typedef enum _cg_gpu_info_architecture_t {
    CG_GPU_INFO_ARCHITECTURE_UNKNOWN,
    CG_GPU_INFO_ARCHITECTURE_SANDYBRIDGE,
    CG_GPU_INFO_ARCHITECTURE_SGX,
    CG_GPU_INFO_ARCHITECTURE_MALI,
    CG_GPU_INFO_ARCHITECTURE_LLVMPIPE,
    CG_GPU_INFO_ARCHITECTURE_SOFTPIPE,
    CG_GPU_INFO_ARCHITECTURE_SWRAST
} cg_gpu_info_architecture_t;

typedef enum {
    CG_GPU_INFO_VENDOR_UNKNOWN,
    CG_GPU_INFO_VENDOR_INTEL,
    CG_GPU_INFO_VENDOR_IMAGINATION_TECHNOLOGIES,
    CG_GPU_INFO_VENDOR_ARM,
    CG_GPU_INFO_VENDOR_QUALCOMM,
    CG_GPU_INFO_VENDOR_NVIDIA,
    CG_GPU_INFO_VENDOR_ATI,
    CG_GPU_INFO_VENDOR_MESA
} cg_gpu_info_vendor_t;

typedef enum {
    CG_GPU_INFO_DRIVER_PACKAGE_UNKNOWN,
    CG_GPU_INFO_DRIVER_PACKAGE_MESA
} cg_gpu_info_driver_package_t;

typedef enum {
    /* If this bug is present then it is faster to read pixels into a
     * PBO and then memcpy out of the PBO into system memory rather than
     * directly read into system memory.
     * https://bugs.freedesktop.org/show_bug.cgi?id=46631
     */
    CG_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS = 1 << 0
} cg_gpu_info_driver_bug_t;

typedef struct _cg_gpu_info_version_t cg_gpu_info_version_t;

typedef struct _cg_gpu_info_t cg_gpu_info_t;

struct _cg_gpu_info_t {
    cg_gpu_info_vendor_t vendor;
    const char *vendor_name;

    cg_gpu_info_driver_package_t driver_package;
    const char *driver_package_name;
    int driver_package_version;

    cg_gpu_info_architecture_t architecture;
    const char *architecture_name;
    cg_gpu_info_architecture_flag_t architecture_flags;

    cg_gpu_info_driver_bug_t driver_bugs;
};

/*
 * _cg_gpc_info_init:
 * @dev: A #cg_device_t
 * @gpu: A return location for the GPU information
 *
 * Determines information about the GPU and driver from the given
 * context.
 */
void _cg_gpc_info_init(cg_device_t *dev, cg_gpu_info_t *gpu);

#endif /* __CG_GPU_INFO_PRIVATE_H */
