/*
 * crand: random number generator api
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef WIN32
#define _CRT_RAND_S /* for rand_s api*/
#endif

#include <clib-config.h>

#ifdef C_PLATFORM_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include <stdio.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <math.h>

#include <clib.h>

#include "SFMT.h"

struct _c_rand_t
{
    sfmt_t sfmt;
};

c_rand_t *
c_rand_new(void)
{
#if defined(__linux__) || defined(__ANDROID__) || defined(__APPLE__)
    int fd;
    uint32_t seed[4];
    int len;

    do {
        fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    } while (fd == -1 && errno == EINTR);

    do {
        len = read(fd, seed, sizeof(seed));
    } while (len == -1 && errno == EINTR);

    c_assert(len == sizeof(seed));

    return c_rand_new_with_seed_array(seed, 4);
#elif defined(WIN32)
    uint32_t seed[4];
    for (int i = 0; i < 4; i++)
        rand_s (&seed[i]);
    return c_rand_new_with_seed_array(seed, 4);
#elif defined(__EMSCRIPTEN__)
    uint32_t seed[4];
    for (int i = 0; i < 4; i++)
        seed[i] = emscripten_random() * UINT32_MAX;
    return c_rand_new_with_seed_array(seed, 4);
#else
#error "platform missing implementation for generating random seed"
#endif
}

c_rand_t *
c_rand_new_with_seed_array(uint32_t *array, int len)
{
    c_rand_t *r = c_slice_new(c_rand_t);

    sfmt_init_by_array(&r->sfmt, array, len);

    return r;
}

c_rand_t *
c_rand_new_with_seed(uint32_t seed)
{
    c_rand_t *r = c_slice_new(c_rand_t);

    sfmt_init_gen_rand(&r->sfmt, seed);

    return r;
}

void
c_rand_free(c_rand_t *rand)
{
    c_slice_free(c_rand_t, rand);
}

double
c_rand_double(c_rand_t *rand)
{
    return sfmt_genrand_res53_mix(&rand->sfmt);
}

double
c_rand_double_range(c_rand_t *rand, double begin, double end)
{
    c_return_val_if_fail(!isnan(begin), c_rand_double_range(rand, end, end));
    c_return_val_if_fail(!isnan(end), c_rand_double_range(rand, begin, begin));
    c_return_val_if_fail(begin <= end, c_rand_double_range(rand, end, begin));

    /* XXX: We are careful here to avoid overflow to avoid a bug like this:
     *
     * https://bugzilla.gnome.org/show_bug.cgi?id=502560
     */
    double v = c_rand_double(rand);

    return v * end - (v - 1) * begin;
}

float
c_rand_float(c_rand_t *rand)
{
    uint32_t ui = sfmt_genrand_uint32(&rand->sfmt);
    return ui * (1.0f/4294967296.0f);
}

float
c_rand_float_range(c_rand_t *rand, float begin, float end)
{
    c_return_val_if_fail(!isnan(begin), c_rand_float_range(rand, end, end));
    c_return_val_if_fail(!isnan(end), c_rand_float_range(rand, begin, begin));
    c_return_val_if_fail(begin <= end, c_rand_float_range(rand, end, begin));

    float f = c_rand_float(rand);

    return f * end - (f - 1) * begin;
}

uint32_t
c_rand_uint32(c_rand_t *rand)
{
    return sfmt_genrand_uint32(&rand->sfmt);
}

int32_t
c_rand_int32_range(c_rand_t *rand,
                   int32_t begin,
                   int32_t end)
{
    c_return_val_if_fail(begin <= end, c_rand_int32_range(rand, end, begin));

    /* If this is a performance problem then instead aim to use
     * c_rand_uint32() with a power-of-two range mask. */
    return c_rand_double_range(rand, begin, end);
}

bool
c_rand_boolean(c_rand_t *rand)
{
    return c_rand_uint32(rand) & 0x1;
}

/*
 * XXX: Non thread safe, convenience functions...
 */

static c_rand_t *global_rand = NULL;

float
c_random_float(void)
{
    if (!global_rand)
        global_rand = c_rand_new();
    return c_rand_float(global_rand);
}

float
c_random_float_range(float begin, float end)
{
    if (!global_rand)
        global_rand = c_rand_new();
    return c_rand_float_range(global_rand, begin, end);
}

double
c_random_double(void)
{
    if (!global_rand)
        global_rand = c_rand_new();
    return c_rand_double(global_rand);
}

double
c_random_double_range(double begin, double end)
{
    if (!global_rand)
        global_rand = c_rand_new();
    return c_rand_double_range(global_rand, begin, end);
}

uint32_t
c_random_uint32(void)
{
    if (!global_rand)
        global_rand = c_rand_new();

    return c_rand_uint32(global_rand);
}

int32_t
c_random_int32_range(int32_t begin, int32_t end)
{
    if (!global_rand)
        global_rand = c_rand_new();
    return c_rand_int32_range(global_rand, begin, end);
}

bool
c_random_boolean(void)
{
    if (!global_rand)
        global_rand = c_rand_new();
    return c_rand_boolean(global_rand);
}

