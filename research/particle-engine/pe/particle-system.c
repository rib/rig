/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include <clib.h>
#include <cglib/cglib.h>

#include "particle-system.h"
#include "particle-engine.h"

struct particle {
    /* The radius of the orbit */
    float radius;

    /* The angular velocity in the orbital plane (in radians per
     * millisecond). */
    float speed;

    /* The orbital period offset, in seconds. */
    double t_offset;

    /* Longitude of ascending node, in radians. */
    float ascending_node;

    /* Inclination in radians from equatorial plane. If inclination is >
     * pi/2, orbit is retrograde. */
    float inclination;
};

struct particle_system_priv {
    c_timer_t *timer;
    double current_time;
    double last_update_time;

    c_rand_t *rand;

    struct particle *particles;

    cg_device_t *dev;
    cg_framebuffer_t *fb;
    struct particle_engine *engine;
};

struct particle_system* particle_system_new(cg_device_t *dev,
                                            cg_framebuffer_t *fb)
{
    struct particle_system *system = c_slice_new0(struct particle_system);
    struct particle_system_priv *priv = c_slice_new0(struct particle_system_priv);

    priv->dev = cg_object_ref(dev);
    priv->fb = cg_object_ref(fb);

    priv->timer = c_timer_new();
    priv->rand = c_rand_new();

    system->priv = priv;

    return system;
}

void particle_system_free(struct particle_system *system)
{
    struct particle_system_priv *priv = system->priv;

    cg_object_unref(priv->dev);
    cg_object_unref(priv->fb);

    c_rand_free(priv->rand);
    c_timer_destroy(priv->timer);

    particle_engine_free(priv->engine);

    c_slice_free(struct particle_system_priv, priv);
    c_slice_free(struct particle_system, system);
}

static void create_particle(struct particle_system *system,
                            int index)
{
    struct particle_system_priv *priv = system->priv;
    struct particle *particle = &priv->particles[index];
    float period;
    cg_color_t *color;

    color = particle_engine_get_particle_color(priv->engine, index);

    /* Get angle of inclination */
    particle->inclination = fuzzy_float_get_real_value(&system->inclination, priv->rand);

    /* Get the ascending node */
    particle->ascending_node = c_random_double_range(0, M_PI * 2);

    /* Particle color. */
    fuzzy_color_get_cg_color(&system->particle_color, priv->rand, color);

    switch (system->type) {
    case SYSTEM_TYPE_CIRCULAR_ORBIT:
        /* Get orbital radius */
        particle->radius = fuzzy_float_get_real_value(&system->radius,
                                                      priv->rand);

        /* Orbital velocity */
        particle->speed = system->u / particle->radius;

        /* In a circular orbit, the orbital period is:
         *
         *    T = 2pi * sqrt(r^3 / μ)
         *
         * r - radius
         * μ - standard gravitational parameter
         */
        period = 2 * M_PI * sqrt(powf(particle->radius, 3) / system->u);

        /* Start the orbit at a random point around it's circumference. */
        particle->t_offset = c_rand_double_range(priv->rand, 0, period);

        break;
    }
}

static void create_resources(struct particle_system *system)
{
    struct particle_system_priv *priv = system->priv;
    int i;

    priv->engine = particle_engine_new(priv->dev, priv->fb,
                                       system->particle_count,
                                       system->particle_size);

    priv->particles = c_new0(struct particle, system->particle_count);

    particle_engine_push_buffer(priv->engine,
                                CG_BUFFER_ACCESS_READ_WRITE, 0);

    for (i = 0; i < system->particle_count; i++)
        create_particle(system, i);

    particle_engine_pop_buffer(priv->engine);
}

static void update_particle(struct particle_system *system,
                            int index)
{
    struct particle_system_priv *priv = system->priv;
    struct particle *particle = &priv->particles[index];
    float *position, time, x, y, z;

    position = particle_engine_get_particle_position(priv->engine, index);

    /* Get the particle age. */
    time = particle->t_offset + priv->current_time;

    switch (system->type) {
    case SYSTEM_TYPE_CIRCULAR_ORBIT:
        {
            float theta;

            /* Get the angular position. */
            theta = fmod(time * particle->speed, M_PI * 2);

            /* Object space coordinates. */
            x = cosf(theta) * particle->radius;
            y = sinf(theta) * particle->radius;
            z = 0;

            break;
        }
    default:
        c_warning(C_STRLOC "Unrecognised particle system type %d",
                  system->type);
        x = y = z = 0;
        break;
    }

    /* FIXME: Rotate around Z axis to the ascending node */
    /* x = x * cosf(particle->ascending_node) - y * sinf(particle->ascending_node); */
    /* y = x * sinf(particle->ascending_node) + y * cosf(particle->ascending_node); */

    /* FIXME: Rotate about X axis to the inclination */
    /* y = y * cosf(particle->inclination) - z * sinf(particle->inclination); */
    /* z = y * sinf(particle->inclination) + z * cosf(particle->inclination); */

    /* Update the new position. */
    position[0] = system->cog[0] + x;
    position[1] = system->cog[1] + y;
    position[2] = system->cog[2] + z;
}

static void tick(struct particle_system *system)
{
    struct particle_system_priv *priv = system->priv;
    struct particle_engine *engine = priv->engine;
    int i;

    /* Create resources as necessary */
    if (!engine)
        create_resources(system);

    /* Update the clocks */
    priv->last_update_time = priv->current_time;
    priv->current_time = c_timer_elapsed(priv->timer, NULL);

    /* Map the particle engine's buffer before reading or writing particle
     * data.
     */
    particle_engine_push_buffer(priv->engine,
                                CG_BUFFER_ACCESS_READ_WRITE, 0);

    /* Iterate over every particle and update them. */
    for (i = 0; i < system->particle_count; i++)
        update_particle(system, i);

    /* Unmap the modified particle buffer. */
    particle_engine_pop_buffer(priv->engine);
}

void particle_system_paint(struct particle_system *system)
{
    tick(system);
    particle_engine_paint(system->priv->engine);
}
