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

#include <cglib-config.h>

#include <string.h>
#include <errno.h>

#include <test-fixtures/test-cg-fixtures.h>

#include "cg-gpu-info-private.h"
#include "cg-device-private.h"
#include "cg-version.h"

typedef struct {
    const char *renderer_string;
    const char *version_string;
    const char *vendor_string;
} cg_gpu_info_strings_t;

typedef struct cg_gpu_info_architecture_description_t {
    cg_gpu_info_architecture_t architecture;
    const char *name;
    cg_gpu_info_architecture_flag_t flags;
    bool (*check_function)(const cg_gpu_info_strings_t *strings);

} cg_gpu_info_architecture_description_t;

typedef struct {
    cg_gpu_info_vendor_t vendor;
    const char *name;
    bool (*check_function)(const cg_gpu_info_strings_t *strings);
    const cg_gpu_info_architecture_description_t *architectures;

} cg_gpu_info_vendor_description_t;

typedef struct {
    cg_gpu_info_driver_package_t driver_package;
    const char *name;
    bool (*check_function)(const cg_gpu_info_strings_t *strings,
                           int *version_out);
} cg_gpu_info_driver_package_description_t;

static bool
_cg_gpc_info_parse_version_string(const char *version_string,
                                  int n_components,
                                  const char **tail,
                                  int *version_ret)
{
    int version = 0;
    uint64_t part;
    int i;

    for (i = 0;; i++) {
        errno = 0;
        part = c_ascii_strtoull(version_string, (char **)&version_string, 10);

        if (errno || part > CG_VERSION_MAX_COMPONENT_VALUE)
            return false;

        version |= part << ((2 - i) * CG_VERSION_COMPONENT_BITS);

        if (i + 1 >= n_components)
            break;

        if (*version_string != '.')
            return false;

        version_string++;
    }

    if (version_ret)
        *version_ret = version;
    if (tail)
        *tail = version_string;

    return true;
}

static bool
match_phrase(const char *string, const char *phrase)
{
    const char *part = strstr(string, phrase);
    int len;

    if (part == NULL)
        return false;

    /* The match must either be at the beginning of the string or
       preceded by a space. */
    if (part > string && part[-1] != ' ')
        return false;

    /* Also match must either be at end of string or followed by a
     * space. */
    len = strlen(phrase);
    if (part[len] != '\0' && part[len] != ' ')
        return false;

    return true;
}

static bool
check_intel_vendor(const cg_gpu_info_strings_t *strings)
{
    return match_phrase(strings->renderer_string, "Intel(R)");
}

static bool
check_imagination_technologies_vendor(const cg_gpu_info_strings_t *strings)
{
    if (strcmp(strings->vendor_string, "Imagination Technologies") != 0)
        return false;
    return true;
}

static bool
check_arm_vendor(const cg_gpu_info_strings_t *strings)
{
    if (strcmp(strings->vendor_string, "ARM") != 0)
        return false;
    return true;
}

static bool
check_qualcomm_vendor(const cg_gpu_info_strings_t *strings)
{
    if (strcmp(strings->vendor_string, "Qualcomm") != 0)
        return false;
    return true;
}

static bool
check_nvidia_vendor(const cg_gpu_info_strings_t *strings)
{
    if (strcmp(strings->vendor_string, "NVIDIA") != 0)
        return false;

    return true;
}

static bool
check_ati_vendor(const cg_gpu_info_strings_t *strings)
{
    if (strcmp(strings->vendor_string, "ATI") != 0)
        return false;

    return true;
}

static bool
check_mesa_vendor(const cg_gpu_info_strings_t *strings)
{
    if (strcmp(strings->vendor_string, "Tungsten Graphics, Inc") == 0)
        return true;
    else if (strcmp(strings->vendor_string, "VMware, Inc.") == 0)
        return true;
    else if (strcmp(strings->vendor_string, "Mesa Project") == 0)
        return true;

    return false;
}

static bool
check_true(const cg_gpu_info_strings_t *strings)
{
    /* This is a last resort so it always matches */
    return true;
}

static bool
check_sandybridge_architecture(const cg_gpu_info_strings_t *strings)
{
    return match_phrase(strings->renderer_string, "Sandybridge");
}

static bool
check_llvmpipe_architecture(const cg_gpu_info_strings_t *strings)
{
    return match_phrase(strings->renderer_string, "llvmpipe");
}

static bool
check_softpipe_architecture(const cg_gpu_info_strings_t *strings)
{
    return match_phrase(strings->renderer_string, "softpipe");
}

static bool
check_swrast_architecture(const cg_gpu_info_strings_t *strings)
{
    return match_phrase(strings->renderer_string, "software rasterizer") ||
           match_phrase(strings->renderer_string, "Software Rasterizer");
}

static bool
check_sgx_architecture(const cg_gpu_info_strings_t *strings)
{
    if (strncmp(strings->renderer_string, "PowerVR SGX", 12) != 0)
        return false;

    return true;
}

static bool
check_mali_architecture(const cg_gpu_info_strings_t *strings)
{
    if (strncmp(strings->renderer_string, "Mali-", 5) != 0)
        return false;

    return true;
}

static const cg_gpu_info_architecture_description_t intel_architectures[] = {
    { CG_GPU_INFO_ARCHITECTURE_SANDYBRIDGE, "Sandybridge",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_sandybridge_architecture },
    { CG_GPU_INFO_ARCHITECTURE_UNKNOWN, "Unknown",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_true }
};

static const cg_gpu_info_architecture_description_t powervr_architectures[] = {
    { CG_GPU_INFO_ARCHITECTURE_SGX, "SGX",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_DEFERRED,
      check_sgx_architecture },
    { CG_GPU_INFO_ARCHITECTURE_UNKNOWN, "Unknown",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED,
      check_true }
};

static const cg_gpu_info_architecture_description_t arm_architectures[] = {
    { CG_GPU_INFO_ARCHITECTURE_MALI, "Mali",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_mali_architecture },
    { CG_GPU_INFO_ARCHITECTURE_UNKNOWN, "Unknown",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_TILED |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_true }
};

static const cg_gpu_info_architecture_description_t mesa_architectures[] = {
    { CG_GPU_INFO_ARCHITECTURE_LLVMPIPE, "LLVM Pipe",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE,
      check_llvmpipe_architecture },
    { CG_GPU_INFO_ARCHITECTURE_SOFTPIPE, "Softpipe",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE,
      check_softpipe_architecture },
    { CG_GPU_INFO_ARCHITECTURE_SWRAST, "SWRast",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_SOFTWARE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_SOFTWARE,
      check_swrast_architecture },
    { CG_GPU_INFO_ARCHITECTURE_UNKNOWN, "Unknown",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_FRAGMENT_IMMEDIATE_MODE,
      check_true }
};

static const cg_gpu_info_architecture_description_t unknown_architectures[] = {
    { CG_GPU_INFO_ARCHITECTURE_UNKNOWN, "Unknown",
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE |
      CG_GPU_INFO_ARCHITECTURE_FLAG_VERTEX_IMMEDIATE_MODE,
      check_true }
};

static const cg_gpu_info_vendor_description_t _cg_gpc_info_vendors
[] =
{ { CG_GPU_INFO_VENDOR_INTEL, "Intel",
    check_intel_vendor,       intel_architectures },
  { CG_GPU_INFO_VENDOR_IMAGINATION_TECHNOLOGIES,
            "Imagination Technologies",
            check_imagination_technologies_vendor,
            powervr_architectures },
  { CG_GPU_INFO_VENDOR_ARM, "ARM", check_arm_vendor, arm_architectures },
  { CG_GPU_INFO_VENDOR_QUALCOMM, "Qualcomm",
            check_qualcomm_vendor,       unknown_architectures },
  { CG_GPU_INFO_VENDOR_NVIDIA, "Nvidia",
            check_nvidia_vendor,       unknown_architectures },
  { CG_GPU_INFO_VENDOR_ATI, "ATI",
            check_ati_vendor,       unknown_architectures },
  /* Must be last */
  { CG_GPU_INFO_VENDOR_MESA, "Mesa",
            check_mesa_vendor,       mesa_architectures },
  { CG_GPU_INFO_VENDOR_UNKNOWN, "Unknown",
            check_true,                 unknown_architectures } };

static bool
check_mesa_driver_package(const cg_gpu_info_strings_t *strings,
                          int *version_ret)
{
    uint64_t micro_part;
    const char *v;

    /* The version string should always begin a two-part GL version
       number */
    if (!_cg_gpc_info_parse_version_string(strings->version_string,
                                           2, /* n_components */
                                           &v, /* tail */
                                           NULL /* version_ret */))
        return false;

    /* In mesa this will be followed optionally by "(Core Profile)" and
     * then "Mesa" */
    v = strstr(v, " Mesa ");
    if (!v)
        return false;

    v += 6;

    /* Next there will be a version string that is at least two
       components. On a git devel build the version will be something
       like "-devel<git hash>" instead */
    if (!_cg_gpc_info_parse_version_string(v,
                                           2, /* n_components */
                                           &v, /* tail */
                                           version_ret))
        return false;

    /* If it is a development build then we'll just leave the micro
       number as 0 */
    if (c_str_has_prefix(v, "-devel"))
        return true;

    /* Otherwise there should be a micro version number */
    if (*v != '.')
        return false;

    errno = 0;
    micro_part = c_ascii_strtoull(v + 1, NULL /* endptr */, 10 /* base */);
    if (errno || micro_part > CG_VERSION_MAX_COMPONENT_VALUE)
        return false;

    *version_ret = CG_VERSION_ENCODE(CG_VERSION_GET_MAJOR(*version_ret),
                                     CG_VERSION_GET_MINOR(*version_ret),
                                     micro_part);

    return true;
}

TEST(check_mesa_driver_package_parser)
{
    /* renderer_string, version_string, vendor_string;*/
    const cg_gpu_info_strings_t test_strings[2] = {
        { NULL, "3.1 Mesa 9.2-devel15436ad", NULL },
        { NULL, "3.1 (Core Profile) Mesa 9.2.0-devel (git-15436ad)", NULL }
    };
    int i;
    int version;

    test_cg_init();

    for (i = 0; i < C_N_ELEMENTS(test_strings); i++) {
        c_assert(check_mesa_driver_package(&test_strings[i], &version));
        c_assert_cmpint(version, ==, CG_VERSION_ENCODE(9, 2, 0));
    }

    test_cg_fini();
}

static bool
check_unknown_driver_package(const cg_gpu_info_strings_t *strings,
                             int *version_out)
{
    *version_out = 0;

    /* This is a last resort so it always matches */
    return true;
}

static const cg_gpu_info_driver_package_description_t
    _cg_gpc_info_driver_packages[] = {
    { CG_GPU_INFO_DRIVER_PACKAGE_MESA, "Mesa", check_mesa_driver_package },
    /* Must be last */
    { CG_GPU_INFO_DRIVER_PACKAGE_UNKNOWN, "Unknown",
      check_unknown_driver_package }
};

void
_cg_gpc_info_init(cg_device_t *dev, cg_gpu_info_t *gpu)
{
    cg_gpu_info_strings_t strings;
    int i;

    strings.renderer_string = (const char *)dev->glGetString(GL_RENDERER);
    strings.version_string = _cg_device_get_gl_version(dev);
    strings.vendor_string = (const char *)dev->glGetString(GL_VENDOR);

    /* Determine the driver package */
    for (i = 0;; i++) {
        const cg_gpu_info_driver_package_description_t *description =
            _cg_gpc_info_driver_packages + i;

        if (description->check_function(&strings,
                                        &gpu->driver_package_version)) {
            gpu->driver_package = description->driver_package;
            gpu->driver_package_name = description->name;
            break;
        }
    }

    /* Determine the GPU vendor */
    for (i = 0;; i++) {
        const cg_gpu_info_vendor_description_t *description =
            _cg_gpc_info_vendors + i;

        if (description->check_function(&strings)) {
            int j;

            gpu->vendor = description->vendor;
            gpu->vendor_name = description->name;

            for (j = 0;; j++) {
                const cg_gpu_info_architecture_description_t *architecture =
                    description->architectures + j;

                if (architecture->check_function(&strings)) {
                    gpu->architecture = architecture->architecture;
                    gpu->architecture_name = architecture->name;
                    gpu->architecture_flags = architecture->flags;
                    goto probed;
                }
            }
        }
    }

probed:

    CG_NOTE(WINSYS,
            "Driver package = %s, vendor = %s, architecture = %s\n",
            gpu->driver_package_name,
            gpu->vendor_name,
            gpu->architecture_name);

    /* Determine the driver bugs */

    /* In Mesa the glReadPixels implementation is really slow
       when using the Intel driver. The Intel
       driver has a fast blit path when reading into a PBO. Reading into
       a temporary PBO and then memcpying back out to the application's
       memory is faster than a regular glReadPixels in this case */
    if (gpu->vendor == CG_GPU_INFO_VENDOR_INTEL &&
        gpu->driver_package == CG_GPU_INFO_DRIVER_PACKAGE_MESA)
        gpu->driver_bugs |= CG_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS;
}
