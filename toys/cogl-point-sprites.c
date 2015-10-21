#include <config.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <cglib/cglib.h>
#include <clib.h>

#define N_FIREWORKS 32
/* Units per second per second */
#define GRAVITY -1.5f

#define N_SPARKS (N_FIREWORKS * 32) /* Must be a power of two */
#define TIME_PER_SPARK 0.01f /* in seconds */

#define TEXTURE_SIZE 32

typedef struct {
    uint8_t red, green, blue, alpha;
} Color;

typedef struct {
    float size;
    float x, y;
    float start_x, start_y;
    Color color;

    /* Velocities are in units per second */
    float initial_x_velocity;
    float initial_y_velocity;

    c_timer_t *timer;
} Firework;

typedef struct {
    float x, y;
    Color color;
    Color base_color;
} Spark;

typedef struct {
    Firework fireworks[N_FIREWORKS];

    int next_spark_num;
    Spark sparks[N_SPARKS];
    c_timer_t *last_spark_time;

    cg_device_t *dev;
    cg_framebuffer_t *fb;
    cg_pipeline_t *pipeline;
    cg_primitive_t *primitive;
    cg_attribute_buffer_t *attribute_buffer;
} Data;

static cg_texture_t *
generate_round_texture(cg_device_t *dev)
{
    uint8_t *p, *data;
    int x, y;
    cg_texture_2d_t *tex;

    p = data = c_malloc(TEXTURE_SIZE * TEXTURE_SIZE * 4);

    /* Generate a white circle which gets transparent towards the edges */
    for (y = 0; y < TEXTURE_SIZE; y++)
        for (x = 0; x < TEXTURE_SIZE; x++) {
            int dx = x - TEXTURE_SIZE / 2;
            int dy = y - TEXTURE_SIZE / 2;
            float value = sqrtf(dx * dx + dy * dy) * 255.0 / (TEXTURE_SIZE / 2);
            if (value > 255.0f)
                value = 255.0f;
            value = 255.0f - value;
            *(p++) = value;
            *(p++) = value;
            *(p++) = value;
            *(p++) = value;
        }

    tex = cg_texture_2d_new_from_data(dev,
                                      TEXTURE_SIZE,
                                      TEXTURE_SIZE,
                                      CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                      TEXTURE_SIZE * 4,
                                      data,
                                      NULL /* error */);

    c_free(data);

    return tex;
}

static void
paint(Data *data)
{
    int i;
    float diff_time;

    /* Update all of the firework's positions */
    for (i = 0; i < N_FIREWORKS; i++) {
        Firework *firework = data->fireworks + i;

        if ((fabsf(firework->x - firework->start_x) > 2.0f) ||
            firework->y < -1.0f) {
            firework->size = c_random_double_range(0.001f, 0.1f);
            firework->start_x = 1.0f + firework->size;
            firework->start_y = -1.0f;
            firework->initial_x_velocity = c_random_double_range(-2.0f, -0.1f);
            firework->initial_y_velocity = c_random_double_range(0.1f, 4.0f);
            c_timer_start(firework->timer);

            /* Pick a random color out of six */
            if (c_random_boolean()) {
                memset(&firework->color, 0, sizeof(Color));
                ((uint8_t *)&firework->color)[c_random_int32_range(0, 3)] = 255;
            } else {
                memset(&firework->color, 255, sizeof(Color));
                ((uint8_t *)&firework->color)[c_random_int32_range(0, 3)] = 0;
            }
            firework->color.alpha = 255;

            /* Fire some of the fireworks from the other side */
            if (c_random_boolean()) {
                firework->start_x = -firework->start_x;
                firework->initial_x_velocity = -firework->initial_x_velocity;
            }
        }

        diff_time = c_timer_elapsed(firework->timer, NULL);

        firework->x =
            (firework->start_x + firework->initial_x_velocity * diff_time);

        firework->y = ((firework->initial_y_velocity * diff_time +
                        0.5f * GRAVITY * diff_time * diff_time) +
                       firework->start_y);
    }

    diff_time = c_timer_elapsed(data->last_spark_time, NULL);
    if (diff_time < 0.0f || diff_time >= TIME_PER_SPARK) {
        /* Add a new spark for each firework, overwriting the oldest ones */
        for (i = 0; i < N_FIREWORKS; i++) {
            Spark *spark = data->sparks + data->next_spark_num;
            Firework *firework = data->fireworks + i;

            spark->x =
                (firework->x + c_random_double_range(-firework->size / 2.0f,
                                                     firework->size / 2.0f));
            spark->y =
                (firework->y + c_random_double_range(-firework->size / 2.0f,
                                                     firework->size / 2.0f));
            spark->base_color = firework->color;

            data->next_spark_num = (data->next_spark_num + 1) & (N_SPARKS - 1);
        }

        /* Update the colour of each spark */
        for (i = 0; i < N_SPARKS; i++) {
            float color_value;

            /* First spark is the oldest */
            Spark *spark =
                data->sparks + ((data->next_spark_num + i) & (N_SPARKS - 1));

            color_value = i / (N_SPARKS - 1.0f);
            spark->color.red = spark->base_color.red * color_value;
            spark->color.green = spark->base_color.green * color_value;
            spark->color.blue = spark->base_color.blue * color_value;
            spark->color.alpha = 255.0f * color_value;
        }

        c_timer_start(data->last_spark_time);
    }

    cg_buffer_set_data(data->attribute_buffer,
                       0, /* offset */
                       data->sparks,
                       sizeof(data->sparks),
                       NULL /* error */);

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

    cg_primitive_draw(data->primitive, data->fb, data->pipeline);

    cg_onscreen_swap_buffers(data->fb);
}

static void
create_primitive(Data *data)
{
    cg_attribute_t *attributes[2];
    int i;

    data->attribute_buffer =
        cg_attribute_buffer_new_with_size(data->dev, sizeof(data->sparks));
    cg_buffer_set_update_hint(data->attribute_buffer,
                              CG_BUFFER_UPDATE_HINT_DYNAMIC);

    attributes[0] = cg_attribute_new(data->attribute_buffer,
                                     "cg_position_in",
                                     sizeof(Spark),
                                     C_STRUCT_OFFSET(Spark, x),
                                     2, /* n_components */
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(data->attribute_buffer,
                                     "cg_color_in",
                                     sizeof(Spark),
                                     C_STRUCT_OFFSET(Spark, color),
                                     4, /* n_components */
                                     CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

    data->primitive =
        cg_primitive_new_with_attributes(CG_VERTICES_MODE_POINTS,
                                         N_SPARKS,
                                         attributes,
                                         C_N_ELEMENTS(attributes));

    for (i = 0; i < C_N_ELEMENTS(attributes); i++)
        cg_object_unref(attributes[i]);
}

static void
frame_event_cb(cg_onscreen_t *onscreen,
               cg_frame_event_t event,
               cg_frame_info_t *info,
               void *user_data)
{
    Data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC)
        paint(data);
}

int
main(int argc, char *argv[])
{
    cg_texture_t *tex;
    cg_onscreen_t *onscreen;
    Data data;
    int i;

    data.dev = cg_device_new();

    cg_device_connect(data.dev, NULL);

    create_primitive(&data);

    data.pipeline = cg_pipeline_new(data.dev);
    data.last_spark_time = c_timer_new();
    data.next_spark_num = 0;
    cg_pipeline_set_point_size(data.pipeline, TEXTURE_SIZE);

    tex = generate_round_texture(data.dev);
    cg_pipeline_set_layer_texture(data.pipeline, 0, tex);
    cg_object_unref(tex);

    cg_pipeline_set_layer_point_sprite_coords_enabled(data.pipeline,
                                                      0, /* layer */
                                                      true,
                                                      NULL /* error */);

    for (i = 0; i < N_FIREWORKS; i++) {
        data.fireworks[i].x = -FLT_MAX;
        data.fireworks[i].y = FLT_MAX;
        data.fireworks[i].size = 0.0f;
        data.fireworks[i].timer = c_timer_new();
    }

    for (i = 0; i < N_SPARKS; i++) {
        data.sparks[i].x = 2.0f;
        data.sparks[i].y = 2.0f;
    }

    onscreen = cg_onscreen_new(data.dev, 800, 600);
    cg_onscreen_show(onscreen);
    data.fb = onscreen;

    cg_onscreen_add_frame_callback(
        onscreen, frame_event_cb, &data, NULL /* destroy notify */);

    paint(&data);

    cg_uv_set_mainloop(data.dev, uv_default_loop());
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    cg_object_unref(data.pipeline);
    cg_object_unref(data.attribute_buffer);
    cg_object_unref(data.primitive);
    cg_object_unref(onscreen);
    cg_object_unref(data.dev);

    c_timer_destroy(data.last_spark_time);

    for (i = 0; i < N_FIREWORKS; i++)
        c_timer_destroy(data.fireworks[i].timer);

    return 0;
}
