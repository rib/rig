#include "config.h"

#include <stdio.h>
#include <stdint.h>

#include <uv.h>

#include <cogl/cogl.h>
#include <clib.h>
#include <rut.h>

struct particle {
    float x;
    float y;

    bool active;
    rut_list_t active_link;

    float velocity[3];

    struct quad_tree *current_quad;
    rut_list_t quad_link;

    rut_list_t quad_path;
};

/* This simple quad tree is not sparse and has a fixed
 * depth. The quad tree gives us a way to quickly find
 * particles nearby other particles and gives us a
 * way to coarsely track the flow of a particle from
 * the origin so we know which particles to focus on
 * simulating.
 */

enum {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
};

struct quad_tree {
    float x0, y0, x1, y1;

    union {
        struct {
            struct quad_tree *children[4];
        } tree_node;
        struct {
            rut_list_t particles;
        } leaf_node;
    };
};

#define QUAD_TREE_DEPTH 4

struct data {

    c_rand_t *rand;

    cg_device_t *dev;
    uv_idle_t paint_idle;
    cg_framebuffer_t *fb;
    cg_pipeline_t *pipeline;

    cg_pipeline_t *particle_pipeline;

    cg_texture_2d_t *maze_tex;
    int width;
    int height;

    int start_x;
    int start_y;

    uint8_t *maze_data;

    uv_idle_t sim_idle;

    bool is_dirty;
    bool draw_ready;

    struct quad_tree *root;

    rut_list_t active_particles;
};


static bool
point_in_quad(struct quad_tree *quad, float x, float y)
{
    return (x >= quad->x0 && x < quad->x1 &&
            y >= quad->y0 && y < quad->y1);
}

static struct quad_tree *
get_quad_for_point(struct quad_tree *tree,
                   int depth,
                   float x,
                   float y)
{
    struct quad_tree **children = tree->tree_node.children;
    int i;

    for (i = 0; i < 4; i++) {
        struct quad_tree *child = children[i];

        if (point_in_quad(child, x, y)) {
            if (depth < (QUAD_TREE_DEPTH - 2))
                return get_quad_for_point(child, depth + 1, x, y);
            else
                return child;
        }
    }
}

static struct quad_tree *
allocate_quad_tree_of_depth(float x0, float y0, float x1, float y1, int depth)
{
    struct quad_tree *tree = c_malloc(sizeof(struct quad_tree));

    tree->x0 = x0;
    tree->y0 = y0;
    tree->x1 = x1;
    tree->y1 = y1;

    if (depth < (QUAD_TREE_DEPTH - 1)) {
        float half_width = (x1 - x0) / 2.0f;
        float half_height = (y1 - y0) / 2.0f;

        tree->tree_node.children[0] = /* top left */
            allocate_quad_tree_of_depth(x0, y0, x1 + half_width, y1, depth + 1);
        tree->tree_node.children[1] = /* top right */
            allocate_quad_tree_of_depth(x0 + half_width, y0, x1, y1, depth + 1);
        tree->tree_node.children[2] = /* bottom left */
            allocate_quad_tree_of_depth(x0, y0 + half_height / 2, x1, y1, depth + 1);
        tree->tree_node.children[3] = /* bottom right */
            allocate_quad_tree_of_depth(x0 + half_width, y0 + half_height, x1, y1, depth + 1);
    } else
        rut_list_init(&tree->leaf_node.particles);

    return tree;
}

static bool
is_wall(struct data *data, int x, int y)
{
    uint32_t *p = (uint32_t *)(data->maze_data + y * data->width * 4 + x * 4);

    return *p == 0;
}

static float x_grad_filter[] =
    {
        -1, 0, 1,
        -2, 0, 2,
        -1, 0, 1,
    };
static float y_grad_filter[] =
    {
        -1, -1, -1,
         0,  0,  0,
         1,  2,  1,
    };

static float
convolve_filter(struct data *data,
                float *filter3x3,
                int center_x,
                int center_y)
{
    float value = 0;
    int x, y;

    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++) {
            float weight = filter3x3[y * 3 + x];
            value += is_wall(data, center_x + x - 1, center_y + y - 1) * weight;
        }

    return value;
}

static void
get_wall_dir(struct data *data, int center_x, int center_y,
             float *dir)
{
    dir[0] = convolve_filter(data, x_grad_filter, center_x, center_y);
    dir[1] = convolve_filter(data, y_grad_filter, center_x, center_y);
}

static struct particle *
spawn_particle(struct data *data, float x, float y)
{
    struct particle *p = c_malloc(sizeof(struct particle));
    struct quad_tree *quad = get_quad_for_point(data->root, 0, x, y);

    p->x = x;
    p->y = y;

    p->current_quad = quad;
    rut_list_insert(quad->leaf_node.particles.prev, &p->quad_link);
    rut_list_init(&p->quad_path);

    p->velocity[0] = c_rand_float_range(data->rand, -1, 1);
    p->velocity[1] = c_rand_float_range(data->rand, -1, 1);
    p->velocity[2] = 0;
    cg_vector3_normalize(p->velocity);

    p->active = true;
    rut_list_insert(data->active_particles.prev, &p->active_link);
}

static void
paint_cb(uv_idle_t *idle)
{
    struct data *data = idle->data;
    struct particle *p;

    data->is_dirty = false;
    data->draw_ready = false;

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
    cg_framebuffer_draw_textured_rectangle(data->fb,
                                           data->pipeline,
                                           0, 0, 1024, 1024,
                                           0, 0, 1, 1);

    rut_list_for_each(p, &data->active_particles, active_link) {
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->particle_pipeline,
                                      p->x - 0.5f, p->y - 0.5f,
                                      p->x + 0.5f, p->y + 0.5f);
    }

    cg_onscreen_swap_buffers(data->fb);

    uv_idle_stop(&data->paint_idle);
}

static void
maybe_queue_redraw(struct data *data)
{
    if (data->is_dirty && data->draw_ready) {
        /* We'll draw on idle instead of drawing immediately so that
         * if Cogl reports multiple dirty rectangles we won't
         * redundantly draw multiple frames */
        uv_idle_start(&data->paint_idle, paint_cb);
    }
}

static void
sim_cb(uv_idle_t *idle)
{
    struct data *data = idle->data;
    int width = data->width;
    int height = data->height;
    int x, y;
    float step = 1.0f / 600.0f;
    struct particle *p;

    rut_list_for_each(p, &data->active_particles, active_link) {
        float dx = p->velocity[0] * step;
        float dy = p->velocity[1] * step;
        float new_x = p->x + dx;
        float new_y = p->y + dy;

        float maze_x = c_nearbyint(new_x);
        float maze_y = c_nearbyint(new_y);

        if (is_wall(data, maze_x, maze_y)) {
            float dir[2];

            get_wall_dir(data, maze_x, maze_y, dir);

            printf("Hit wall @ (%f,%f): wall direction = (%f,%f)\n",
                   maze_x, maze_y, dir[0], dir[1]);
            /* calculate intersection point */
            /* calculate reflection vector and new velocity */
        }

        p->x = new_x;
        p->y = new_y;
    }

    maybe_queue_redraw(data);
}

static void
frame_event_cb(cg_onscreen_t *onscreen,
               cg_frame_event_t event,
               cg_frame_info_t *info,
               void *user_data)
{
    struct data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC) {
        data->draw_ready = true;
        maybe_queue_redraw(data);
    }
}

static void
dirty_cb(cg_onscreen_t *onscreen,
         const cg_onscreen_dirty_info_t *info,
         void *user_data)
{
    struct data *data = user_data;

    data->is_dirty = true;
    maybe_queue_redraw(data);
}

int
main(int argc, char **argv)
{
    struct data data;
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    uv_loop_t *loop = uv_default_loop();
    void *pix;
    int x, y;

    data.rand = c_rand_new();

    data.is_dirty = false;
    data.draw_ready = true;

    data.dev = cg_device_new();
    if (!cg_device_connect(data.dev, &error)) {
        cg_object_unref(data.dev);
        fprintf(stderr, "Failed to create device: %s\n", error->message);
        exit(1);
    }

    onscreen = cg_onscreen_new(data.dev, 1024, 1024);
    cg_onscreen_show(onscreen);
    data.fb = onscreen;

    cg_framebuffer_orthographic(data.fb, 0, 0, 1024, 1024,
                                -1, 100);

    //cg_onscreen_set_resizable(onscreen, true);

    data.maze_tex = cg_texture_2d_new_from_file(data.dev, "./maze.png", NULL);
    data.pipeline = cg_pipeline_new(data.dev);
    cg_pipeline_set_layer_texture(data.pipeline, 0, data.maze_tex);
    cg_pipeline_set_blend(data.pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

    data.width = cg_texture_get_width(data.maze_tex);
    data.height = cg_texture_get_height(data.maze_tex);

    data.root = allocate_quad_tree_of_depth(0, 0, data.width, data.height,
                                            0); /* current depth */

    data.maze_data = c_malloc(data.width * data.height * 4);

    cg_texture_get_data(data.maze_tex,
                        CG_PIXEL_FORMAT_RGBA_8888_PRE,
                        0, /* auto rowstride */
                        data.maze_data);

    data.start_x = -1;
    for (y = 0; y < data.height; y++)
        for (x = 0; x < data.width; x++) {
            uint8_t *p = data.maze_data + y * data.width * 4 + x * 4;

#if 0
            if (!((p[0] == 0x0 && p[1] == 0x0 && p[2] == 0x0 && p[3] == 0xff) ||
                 (p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff)))
            {
                printf("non black/white pixel at (%d,%d) %2x,%2x,%2x\n",
                       x, y, p[0], p[1], p[2]);
            }
#endif
            if (p[0] == 0x0 && p[1] == 0xff && p[2] == 0x0) {
                data.start_x = x;
                data.start_y = y;
                goto found_start;
            }
        }
found_start:
    if (data.start_x < 0) {
        fprintf(stderr, "Failed to find start of maze\n");
        exit(1);
    }
    printf("Start of maze found at (%d, %d)\n", data.start_x, data.start_y);

    data.particle_pipeline = cg_pipeline_new(data.dev);
    cg_pipeline_set_color4f(data.particle_pipeline, 1, 0, 0, 1);

    rut_list_init(&data.active_particles);

    spawn_particle(&data, data.start_x, data.start_y);

    cg_onscreen_add_frame_callback(data.fb, frame_event_cb, &data,
                                   NULL); /* destroy notify */
    cg_onscreen_add_dirty_callback(data.fb, dirty_cb, &data,
                                   NULL); /* destroy notify */

    uv_idle_init(loop, &data.paint_idle);
    data.paint_idle.data = &data;
    uv_idle_start(&data.paint_idle, paint_cb);

    uv_idle_init(loop, &data.sim_idle);
    data.sim_idle.data = &data;
    uv_idle_start(&data.sim_idle, sim_cb);

    cg_uv_set_mainloop(data.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
