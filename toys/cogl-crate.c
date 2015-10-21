#include <config.h>

#include <clib.h>
#include <cglib/cglib.h>
#include <cogl-pango/cogl-pango.h>

/* The state for this example... */
typedef struct _Data {
    cg_framebuffer_t *fb;
    int framebuffer_width;
    int framebuffer_height;

    c_matrix_t view;

    cg_indices_t *indices;
    cg_primitive_t *prim;
    cg_texture_t *texture;
    cg_pipeline_t *crate_pipeline;

    CgPangoFontMap *pango_font_map;
    PangoContext *pango_context;
    PangoFontDescription *pango_font_desc;

    PangoLayout *hello_label;
    int hello_label_width;
    int hello_label_height;

    c_timer_t *timer;

    bool swap_ready;

} Data;

/* A static identity matrix initialized for convenience. */
static c_matrix_t identity;
/* static colors initialized for convenience. */
static cg_color_t white;

/* A cube modelled using 4 vertices for each face.
 *
 * We use an index buffer when drawing the cube later so the GPU will
 * actually read each face as 2 separate triangles.
 */
static cg_vertex_p3t2_t vertices[] = {
    /* Front face */
    { /* pos = */ -1.0f, -1.0f, 1.0f, /* tex coords = */ 0.0f, 1.0f },
    { /* pos = */ 1.0f, -1.0f, 1.0f, /* tex coords = */ 1.0f, 1.0f },
    { /* pos = */ 1.0f, 1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f },
    { /* pos = */ -1.0f, 1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f },

    /* Back face */
    { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f },
    { /* pos = */ -1.0f, 1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f },
    { /* pos = */ 1.0f, 1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f },
    { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f },

    /* Top face */
    { /* pos = */ -1.0f, 1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f },
    { /* pos = */ -1.0f, 1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f },
    { /* pos = */ 1.0f, 1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f },
    { /* pos = */ 1.0f, 1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f },

    /* Bottom face */
    { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f },
    { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f },
    { /* pos = */ 1.0f, -1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f },
    { /* pos = */ -1.0f, -1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f },

    /* Right face */
    { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f },
    { /* pos = */ 1.0f, 1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f },
    { /* pos = */ 1.0f, 1.0f, 1.0f, /* tex coords = */ 0.0f, 1.0f },
    { /* pos = */ 1.0f, -1.0f, 1.0f, /* tex coords = */ 0.0f, 0.0f },

    /* Left face */
    { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f },
    { /* pos = */ -1.0f, -1.0f, 1.0f, /* tex coords = */ 1.0f, 0.0f },
    { /* pos = */ -1.0f, 1.0f, 1.0f, /* tex coords = */ 1.0f, 1.0f },
    { /* pos = */ -1.0f, 1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f }
};

static void
paint(Data *data)
{
    cg_framebuffer_t *fb = data->fb;
    float rotation;

    cg_framebuffer_clear4f(
        fb, CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH, 0, 0, 0, 1);

    cg_framebuffer_push_matrix(fb);

    cg_framebuffer_translate(
        fb, data->framebuffer_width / 2, data->framebuffer_height / 2, 0);

    cg_framebuffer_scale(fb, 75, 75, 75);

    /* Update the rotation based on the time the application has been
       running so that we get a linear animation regardless of the frame
       rate */
    rotation = c_timer_elapsed(data->timer, NULL) * 60.0f;

    /* Rotate the cube separately around each axis.
     *
     * Note: Cogl matrix manipulation follows the same rules as for
     * OpenGL. We use column-major matrices and - if you consider the
     * transformations happening to the model - then they are combined
     * in reverse order which is why the rotation is done last, since
     * we want it to be a rotation around the origin, before it is
     * scaled and translated.
     */
    cg_framebuffer_rotate(fb, rotation, 0, 0, 1);
    cg_framebuffer_rotate(fb, rotation, 0, 1, 0);
    cg_framebuffer_rotate(fb, rotation, 1, 0, 0);

    cg_primitive_draw(data->prim, fb, data->crate_pipeline);

    cg_framebuffer_pop_matrix(fb);

    /* And finally render our Pango layouts... */

    cg_pango_show_layout(
        fb,
        data->hello_label,
        (data->framebuffer_width / 2) - (data->hello_label_width / 2),
        (data->framebuffer_height / 2) - (data->hello_label_height / 2),
        &white);
}

static void
frame_event_cb(cg_onscreen_t *onscreen,
               cg_frame_event_t event,
               cg_frame_info_t *info,
               void *user_data)
{
    Data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC)
        data->swap_ready = TRUE;
}

int
main(int argc, char **argv)
{
    cg_device_t *dev;
    cg_onscreen_t *onscreen;
    cg_framebuffer_t *fb;
    cg_error_t *error = NULL;
    Data data;
    PangoRectangle hello_label_size;
    float fovy, aspect, z_near, z_2d, z_far;
    cg_depth_state_t depth_state;
    cg_bitmap_t *bitmap;

    dev = cg_device_new();
    if (!cg_device_connect(dev, &error)) {
        fprintf(stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    onscreen = cg_onscreen_new(dev, 640, 480);
    fb = onscreen;
    data.fb = fb;
    data.framebuffer_width = cg_framebuffer_get_width(fb);
    data.framebuffer_height = cg_framebuffer_get_height(fb);

    data.timer = c_timer_new();

    cg_onscreen_show(onscreen);

    cg_framebuffer_set_viewport(
        fb, 0, 0, data.framebuffer_width, data.framebuffer_height);

    fovy = 60; /* y-axis field of view */
    aspect = (float)data.framebuffer_width / (float)data.framebuffer_height;
    z_near = 0.1; /* distance to near clipping plane */
    z_2d = 1000; /* position to 2d plane */
    z_far = 2000; /* distance to far clipping plane */

    cg_framebuffer_perspective(fb, fovy, aspect, z_near, z_far);

    /* Since the pango renderer emits geometry in pixel/device coordinates
     * and the anti aliasing is implemented with the assumption that the
     * geometry *really* does end up pixel aligned, we setup a modelview
     * matrix so that for geometry in the plane z = 0 we exactly map x
     * coordinates in the range [0,stage_width] and y coordinates in the
     * range [0,stage_height] to the framebuffer extents with (0,0) being
     * the top left.
     *
     * This is roughly what Clutter does for a ClutterStage, but this
     * demonstrates how it is done manually using Cogl.
     */
    c_matrix_init_identity(&data.view);
    c_matrix_view_2d_in_perspective(&data.view,
                                     fovy,
                                     aspect,
                                     z_near,
                                     z_2d,
                                     data.framebuffer_width,
                                     data.framebuffer_height);
    cg_framebuffer_set_modelview_matrix(fb, &data.view);

    /* Initialize some convenient constants */
    c_matrix_init_identity(&identity);
    cg_color_init_from_4ub(&white, 0xff, 0xff, 0xff, 0xff);

    /* rectangle indices allow the GPU to interpret a list of quads (the
     * faces of our cube) as a list of triangles.
     *
     * Since this is a very common thing to do
     * cg_get_rectangle_indices() is a convenience function for
     * accessing internal index buffers that can be shared.
     */
    data.indices = cg_get_rectangle_indices(dev, 6 /* n_rectangles */);
    data.prim = cg_primitive_new_p3t2(dev,
                                      CG_VERTICES_MODE_TRIANGLES,
                                      sizeof(vertices) / sizeof(vertices[0]),
                                      vertices);
    /* Each face will have 6 indices so we have 6 * 6 indices in total... */
    cg_primitive_set_indices(data.prim, data.indices, 6 * 6);

    /* Load a jpeg crate texture from a file */
    printf("crate.jpg (CC by-nc-nd http://bit.ly/9kP45T) ShadowRunner27 "
           "http://bit.ly/m1YXLh\n");
    bitmap = cg_bitmap_new_from_file(dev, CG_EXAMPLES_DATA "crate.jpg", &error);
    if (!bitmap)
        g_error("Failed to load texture: %s", error->message);

    data.texture = cg_texture_2d_new_from_bitmap(bitmap);
    cg_texture_set_components(data.texture, CG_TEXTURE_COMPONENTS_RGBA32F);
    cg_texture_allocate(data.texture, &error);

    if (!data.texture)
        g_error("Failed to load texture: %s", error->message);

    /* a cg_pipeline_t conceptually describes all the state for vertex
     * processing, fragment processing and blending geometry. When
     * drawing the geometry for the crate this pipeline says to sample a
     * single texture during fragment processing... */
    data.crate_pipeline = cg_pipeline_new(dev);
    cg_pipeline_set_layer_texture(data.crate_pipeline, 0, data.texture);

    /* Since the box is made of multiple triangles that will overlap
     * when drawn and we don't control the order they are drawn in, we
     * enable depth testing to make sure that triangles that shouldn't
     * be visible get culled by the GPU. */
    cg_depth_state_init(&depth_state);
    cg_depth_state_set_test_enabled(&depth_state, TRUE);

    cg_pipeline_set_depth_state(data.crate_pipeline, &depth_state, NULL);

    /* Setup a Pango font map and context */

    data.pango_font_map = CG_PANGO_FONT_MAP(cg_pango_font_map_new(dev));

    cg_pango_font_map_set_use_mipmapping(data.pango_font_map, TRUE);

    data.pango_context =
        pango_font_map_create_context(PANGO_FONT_MAP(data.pango_font_map));

    data.pango_font_desc = pango_font_description_new();
    pango_font_description_set_family(data.pango_font_desc, "Sans");
    pango_font_description_set_size(data.pango_font_desc, 30 * PANGO_SCALE);

    /* Setup the "Hello Cogl" text */

    data.hello_label = pango_layout_new(data.pango_context);
    pango_layout_set_font_description(data.hello_label, data.pango_font_desc);
    pango_layout_set_text(data.hello_label, "Hello Cogl", -1);

    pango_layout_get_extents(data.hello_label, NULL, &hello_label_size);
    data.hello_label_width = PANGO_PIXELS(hello_label_size.width);
    data.hello_label_height = PANGO_PIXELS(hello_label_size.height);

    data.swap_ready = TRUE;

    cg_onscreen_add_frame_callback(fb, frame_event_cb, &data,
                                   NULL); /* destroy notify */

    while (1) {
        cg_poll_fd_t *poll_fds;
        int n_poll_fds;
        int64_t timeout;

        if (data.swap_ready) {
            static gboolean swapped = FALSE;
            int rect[4] = { 0, 0, 320, 240 };

            paint(&data);
            if (!swapped) {
                cg_onscreen_swap_buffers(fb);
                swapped = TRUE;
            } else
                cg_onscreen_swap_buffers_with_damage(fb, rect, 1);
        }

        cg_loop_get_info(
            cg_device_get_renderer(dev), &poll_fds, &n_poll_fds, &timeout);

        g_poll((GPollFD *)poll_fds,
               n_poll_fds,
               timeout == -1 ? -1 : timeout / 1000);

        cg_loop_dispatch(cg_device_get_renderer(dev),
                                  poll_fds, n_poll_fds);
    }

    return 0;
}
