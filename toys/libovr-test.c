#include <config.h>


#include <stdio.h>

#include <uv.h>

#include <cglib/cglib.h>
#include <clib.h>

#include "OVR_CAPI.h"

#define PIXELS_PER_DISPLAY_PIXEL 1.0

struct eye {
    ovrEyeType type;

    cg_texture_2d_t *tex;
    cg_offscreen_t *fb;
    cg_pipeline_t *pipeline;
    cg_pipeline_t *distort_pipeline;
    int eye_to_source_uv_scale_loc;
    int eye_to_source_uv_offset_loc;
    int eye_rotation_start_loc;
    int eye_rotation_end_loc;

    ovrFovPort fov;
    ovrEyeRenderDesc render_desc;

    ovrPosef head_pose;

    float eye_to_source_uv_scale[2];
    float eye_to_source_uv_offset[2];

    c_matrix_t projection_matrix;

    cg_attribute_buffer_t *attrib_buf;
    cg_attribute_t *attribs[6];
    cg_primitive_t *distortion_prim;

    cg_index_buffer_t *index_buf;
    cg_indices_t *indices;

    float x0, y0, x1, y1;

    float viewport[4];
};

struct data {
    cg_device_t *dev;
    uv_idle_t idle;
    cg_framebuffer_t *fb;
    cg_primitive_t *triangle;
    cg_pipeline_t *pipeline;

    cg_pipeline_t *test_pipeline;

    ovrHmd hmd;

    struct eye eyes[2];

    bool is_dirty;
    bool draw_ready;
};

static void
paint_eye(struct data *data, struct eye *eye)
{
    c_matrix_t identity;
    c_quaternion_t orientation;
    c_matrix_t orientation_mat;

    cg_framebuffer_clear4f(eye->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

#if 0
    c_matrix_init_identity(&identity);
    cg_framebuffer_set_projection_matrix(eye->fb, &identity);

    cg_pipeline_set_color4f(data->test_pipeline, 1, 1, 1, 1);
    cg_framebuffer_draw_rectangle(eye->fb, data->test_pipeline,
                                  -1, 1, 0, 0);
    cg_pipeline_set_color4f(data->test_pipeline, 1, 0, 1, 1);
    cg_framebuffer_draw_rectangle(eye->fb, data->test_pipeline,
                                  0, 1, 1, 0);
    cg_pipeline_set_color4f(data->test_pipeline, 1, 1, 0, 1);
    cg_framebuffer_draw_rectangle(eye->fb, data->test_pipeline,
                                  0, 0, 1, -1);
    cg_pipeline_set_color4f(data->test_pipeline, 0, 1, 1, 1);
    cg_framebuffer_draw_rectangle(eye->fb, data->test_pipeline,
                                  -1, 0, 0, -1);
#endif
    //printf("paint\n");

    cg_framebuffer_set_projection_matrix(eye->fb, &eye->projection_matrix);

    eye->head_pose = ovrHmd_GetHmdPosePerEye(data->hmd, eye->type);

    /* TODO: double check that OVR quaternions are defined in exactly
     * the same way... */
    orientation.w = eye->head_pose.Orientation.w;
    orientation.x = eye->head_pose.Orientation.x;
    orientation.y = eye->head_pose.Orientation.y;
    orientation.z = eye->head_pose.Orientation.z;

    //c_quaternion_invert(&orientation);
    c_matrix_init_from_quaternion(&orientation_mat, &orientation);

    cg_framebuffer_set_modelview_matrix(eye->fb, &orientation_mat);

    //c_matrix_init_translation(&

    cg_primitive_draw(data->triangle, eye->fb, data->pipeline);
}

static void
composite_eye(struct data *data, struct eye *eye)
{
    ovrMatrix4f timewarp_matrices[2];


#if 0
    cg_framebuffer_set_viewport(data->fb,
                                eye->viewport[0],
                                eye->viewport[1],
                                eye->viewport[2],
                                eye->viewport[3]);
#endif

#if 0

#if 0
    cg_framebuffer_draw_textured_rectangle(data->fb,
                                           eye->pipeline,
                                           -1, 1, 1, -1,
                                           0, 0, 1, 1);
#endif

    cg_framebuffer_draw_textured_rectangle(data->fb,
                                           eye->pipeline,
                                           eye->x0, eye->y0,
                                           eye->x1, eye->y1,
                                           0, 0, 1, 1);

#else

    /* FIXME: finish both eyes before we do the time warp again */

    ovrHmd_GetEyeTimewarpMatrices(data->hmd, eye->type,
                                  eye->head_pose, timewarp_matrices);

    float identity[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
#if 1
    cg_pipeline_set_uniform_matrix(eye->distort_pipeline,
                                   eye->eye_rotation_start_loc,
                                   4, /* dimensions */
                                   1, /* count */
                                   true, /* transpose as ovr matrics
                                            are row major */
                                   //(float *)&timewarp_matrices[0]);
                                   identity);
    cg_pipeline_set_uniform_matrix(eye->distort_pipeline,
                                   eye->eye_rotation_end_loc,
                                   4, /* dimensions */
                                   1, /* count */
                                   true, /* transpose as ovr matrics
                                            are row major */
                                   //(float *)&timewarp_matrices[1]);
                                   identity);
#endif

    cg_primitive_draw(eye->distortion_prim, data->fb, eye->distort_pipeline);
#endif

}


static void
paint_cb(uv_idle_t *idle)
{
    struct data *data = idle->data;
    //ovrTrackingState tracking;
    int i;
    ovrFrameTiming frame_timing = ovrHmd_BeginFrameTiming(data->hmd, 0);
    unsigned char latency_test_color[3];

    //data->is_dirty = false;
    data->draw_ready = false;

#if 0
    tracking = ovrHmd_GetTrackingState(data->hmd, frame_timing.ScanoutMidpointSeconds);
    //tracking = ovrHmd_GetTrackingState(data->hmd, ovr_GetTimeInSeconds());
    if (!(tracking.StatusFlags & ovrStatus_HmdConnected))
        fprintf(stderr, "HMD Not connected!\n");
#endif

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0.0, 0.0, 0.0, 1);

    for (i = 0; i < 2; i++) {
        int eye_idx = data->hmd->EyeRenderOrder[i];
        struct eye *eye = &data->eyes[eye_idx];
        paint_eye(data, eye);
    }

    ovr_WaitTillTime(frame_timing.TimewarpPointSeconds);

    for (i = 0; i < 2; i++) {
        int eye_idx = data->hmd->EyeRenderOrder[i];
        struct eye *eye = &data->eyes[eye_idx];
        composite_eye(data, eye);
    }

    if (ovrHmd_GetLatencyTest2DrawColor(data->hmd, latency_test_color)) {
        cg_pipeline_set_color4ub(data->test_pipeline,
                                 latency_test_color[0],
                                 latency_test_color[1],
                                 latency_test_color[2],
                                 0xff);
#if 0
        cg_framebuffer_set_viewport(data->fb,
                                    0, 0,
                                    cg_framebuffer_get_width(data->fb),
                                    cg_framebuffer_get_height(data->fb));
#endif
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->test_pipeline,
                                      0.95, 1, 1, 0.95);
    }

    cg_onscreen_swap_buffers(data->fb);

    /* XXX: check how this interacts with cogl's frame complete
     * notifications; we shouldn't need to now wait for a swap notify
     * from the X server but it wouldn't be surprising if we do in
     * fact end up delayed waiting for the event from X...
     *
     * XXX: check what Begin/EndTiming is assuming about how the
     * relationship between finishing and the vblank period and
     * see if we can improve the timing apis...
     */
    cg_framebuffer_finish(data->fb);

    ovrHmd_EndFrameTiming(data->hmd);

    /* XXX: ovrHmd_EndFrame() was a useful starting poiny when determining
     * how to use the latency testing apis... */
    ovrHmd_GetMeasuredLatencyTest2(data->hmd);

    uv_idle_stop(&data->idle);
}

static void
maybe_redraw(struct data *data)
{
    if (data->is_dirty && data->draw_ready) {
        /* We'll draw on idle instead of drawing immediately so that
         * if Cogl reports multiple dirty rectangles we won't
         * redundantly draw multiple frames */
        uv_idle_start(&data->idle, paint_cb);
    }
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
        maybe_redraw(data);
    }
}

static void
dirty_cb(cg_onscreen_t *onscreen,
         const cg_onscreen_dirty_info_t *info,
         void *user_data)
{
    struct data *data = user_data;

    data->is_dirty = true;
    maybe_redraw(data);
}

static void
create_eye_distortion_mesh(struct data *data, struct eye *eye)
{
    ovrDistortionMesh mesh_data;

    ovrHmd_CreateDistortionMesh(data->hmd,
                                eye->type,
                                eye->fov,
                                (ovrDistortionCap_Chromatic |
                                 ovrDistortionCap_TimeWarp),
                                &mesh_data);

    eye->attrib_buf = cg_attribute_buffer_new(data->dev,
                                              (sizeof(ovrDistortionVertex) *
                                               mesh_data.VertexCount),
                                              mesh_data.pVertexData);

    eye->attribs[0] = cg_attribute_new(eye->attrib_buf,
                                       "cg_position_in",
                                       sizeof(ovrDistortionVertex),
                                       offsetof(ovrDistortionVertex, ScreenPosNDC),
                                       2, /* n components */
                                       CG_ATTRIBUTE_TYPE_FLOAT);
    eye->attribs[1] = cg_attribute_new(eye->attrib_buf,
                                       "warp_factor_in",
                                       sizeof(ovrDistortionVertex),
                                       offsetof(ovrDistortionVertex, TimeWarpFactor),
                                       1, /* n components */
                                       CG_ATTRIBUTE_TYPE_FLOAT);
    eye->attribs[2] = cg_attribute_new(eye->attrib_buf,
                                       "vignette_factor_in",
                                       sizeof(ovrDistortionVertex),
                                       offsetof(ovrDistortionVertex, VignetteFactor),
                                       1, /* n components */
                                       CG_ATTRIBUTE_TYPE_FLOAT);
    eye->attribs[3] = cg_attribute_new(eye->attrib_buf,
                                       "tan_eye_angles_r_in",
                                       sizeof(ovrDistortionVertex),
                                       offsetof(ovrDistortionVertex, TanEyeAnglesR),
                                       2, /* n components */
                                       CG_ATTRIBUTE_TYPE_FLOAT);
    eye->attribs[4] = cg_attribute_new(eye->attrib_buf,
                                       "tan_eye_angles_g_in",
                                       sizeof(ovrDistortionVertex),
                                       offsetof(ovrDistortionVertex, TanEyeAnglesG),
                                       2, /* n components */
                                       CG_ATTRIBUTE_TYPE_FLOAT);
    eye->attribs[5] = cg_attribute_new(eye->attrib_buf,
                                       "tan_eye_angles_b_in",
                                       sizeof(ovrDistortionVertex),
                                       offsetof(ovrDistortionVertex, TanEyeAnglesB),
                                       2, /* n components */
                                       CG_ATTRIBUTE_TYPE_FLOAT);

    eye->index_buf = cg_index_buffer_new(data->dev, 2 * mesh_data.IndexCount);

    cg_buffer_set_data(eye->index_buf,
                       0, /* offset */
                       mesh_data.pIndexData,
                       2 * mesh_data.IndexCount,
                       NULL); /* exception */

    eye->indices = cg_indices_new_for_buffer(CG_INDICES_TYPE_UNSIGNED_SHORT,
                                             eye->index_buf,
                                             0); /* offset */

    eye->distortion_prim =
        cg_primitive_new_with_attributes(CG_VERTICES_MODE_TRIANGLES,
                                         mesh_data.VertexCount,
                                         eye->attribs,
                                         6); /* n attributes */

    cg_primitive_set_indices(eye->distortion_prim,
                             eye->indices,
                             mesh_data.IndexCount);

    ovrHmd_DestroyDistortionMesh(&mesh_data);
}

int
main(int argc, char **argv)
{
    struct data data;
    cg_onscreen_t *onscreen;
    cg_error_t *error = NULL;
    cg_vertex_p3c4_t triangle_vertices[] = {
        {  0,    500, 500, 0xff, 0x00, 0x00, 0xff },
        { -500, -500, 500, 0x00, 0xff, 0x00, 0xff },
        {  500, -500, 500, 0x00, 0x00, 0xff, 0xff }
    };
    uv_loop_t *loop = uv_default_loop();
    struct eye *left_eye = &data.eyes[0];
    struct eye *right_eye = &data.eyes[1];
    int i;

    data.is_dirty = false;
    data.draw_ready = true;

    ovr_Initialize();

    data.hmd = ovrHmd_Create(0);

    if (!data.hmd) {
        fprintf(stderr, "Failed to initialize a head mounted display\n"
                        "Creating dummy DK2 device...\n");

        data.hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
        if (!data.hmd) {
            fprintf(stderr, "Failed to create dummy DK2 device\n");
            goto cleanup;
        }
    }

    printf("Headset type = %s\n", data.hmd->ProductName);


    ovrHmd_SetEnabledCaps(data.hmd,
                          (ovrHmdCap_DynamicPrediction
                           //| ovrHmdCap_LowPersistence
                           ));

    ovrHmd_ConfigureTracking(data.hmd,
                             ovrTrackingCap_Orientation |
                             ovrTrackingCap_MagYawCorrection |
                             ovrTrackingCap_Position, /* supported */
                             0); /* required */

    data.dev = cg_device_new();
    if (!cg_device_connect(data.dev, &error)) {
        cg_object_unref(data.dev);
        fprintf(stderr, "Failed to create device: %s\n", error->message);
        goto cleanup;
    }

    onscreen = cg_onscreen_new(data.dev,
                               data.hmd->Resolution.w,
                               data.hmd->Resolution.h);
    cg_onscreen_show(onscreen);
    data.fb = onscreen;

#if 0
    cg_framebuffer_orthographic(data.fb, 0, 0,
                                data.hmd->Resolution.w,
                                data.hmd->Resolution.h,
                                -1, 100);
#endif

    //cg_onscreen_set_resizable(onscreen, true);

    memset(data.eyes, 0, sizeof(data.eyes));

    left_eye->type = ovrEye_Left;
    left_eye->x0 = -1;
    left_eye->y0 = 1;
    left_eye->x1 = 0;
    left_eye->y1 = -1;
    left_eye->viewport[0] = 0;
    left_eye->viewport[1] = 0;
    left_eye->viewport[2] = data.hmd->Resolution.w / 2;
    left_eye->viewport[3] = data.hmd->Resolution.h;

    right_eye->type = ovrEye_Right;
    right_eye->x0 = 0;
    right_eye->y0 = 1;
    right_eye->x1 = 1;
    right_eye->y1 = -1;
    right_eye->viewport[0] = (data.hmd->Resolution.w + 1) / 2;
    right_eye->viewport[1] = 0;
    right_eye->viewport[2] = data.hmd->Resolution.w / 2;
    right_eye->viewport[3] = data.hmd->Resolution.h;

    for (i = 0; i < 2; i++) {
        struct eye *eye = &data.eyes[i];
        ovrSizei recommended_size;
        ovrMatrix4f projection_matrix;
        ovrRecti tex_viewport;
        ovrVector2f uv_scale_offset[2];
        cg_snippet_t *snippet;

        eye->fov = data.hmd->DefaultEyeFov[i];

        recommended_size =
            ovrHmd_GetFovTextureSize(data.hmd, eye->type, eye->fov,
                                     PIXELS_PER_DISPLAY_PIXEL);

        eye->tex = cg_texture_2d_new_with_size(data.dev,
                                               recommended_size.w,
                                               recommended_size.h);
        eye->fb = cg_offscreen_new_with_texture(eye->tex);
        cg_framebuffer_allocate(eye->fb, NULL);

        eye->render_desc = ovrHmd_GetRenderDesc(data.hmd, eye->type, eye->fov);

        tex_viewport.Size = recommended_size;
        tex_viewport.Pos.x = 0;
        tex_viewport.Pos.y = 0;

        /* XXX: The size and viewport this api expects are the size of
         * the eye render target and the viewport used when rendering
         * the eye. I.e. not the size of the final destination
         * framebuffer or viewport used when finally compositing the
         * eyes with mesh distortion. */
        ovrHmd_GetRenderScaleAndOffset(eye->fov, recommended_size, tex_viewport,
                                       uv_scale_offset);

        eye->eye_to_source_uv_scale[0] = uv_scale_offset[0].x;
        eye->eye_to_source_uv_scale[1] = uv_scale_offset[0].y;

        eye->eye_to_source_uv_offset[0] = uv_scale_offset[1].x;
        eye->eye_to_source_uv_offset[1] = uv_scale_offset[1].y;

        projection_matrix = ovrMatrix4f_Projection(eye->fov, 0.01, 10000,
                                                   false); /* left handed */

        c_matrix_init_from_array(&eye->projection_matrix,
                                  (float *)&projection_matrix);
        /* XXX: A ovrMatrix4f is stored in row-major order but
         * init_from_array will have assumed a column-major order */
        c_matrix_transpose(&eye->projection_matrix);

        eye->pipeline = cg_pipeline_new(data.dev);
        cg_pipeline_set_layer_texture(eye->pipeline, 0, eye->tex);
        cg_pipeline_set_blend(eye->pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

        eye->distort_pipeline = cg_pipeline_new(data.dev);
        cg_pipeline_set_layer_texture(eye->distort_pipeline, 0, eye->tex);
        cg_pipeline_set_blend(eye->distort_pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

        snippet = cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                                 "uniform vec2 eye_to_source_uv_scale;\n"
                                 "uniform vec2 eye_to_source_uv_offset;\n"
                                 "uniform mat4 eye_rotation_start;\n"
                                 "uniform mat4 eye_rotation_end;\n"
                                 "\n"
                                 "in vec2 tan_eye_angles_r_in;\n"
                                 "in vec2 tan_eye_angles_g_in;\n"
                                 "in vec2 tan_eye_angles_b_in;\n"
                                 "out vec2 tex_coord_r;\n"
                                 "out vec2 tex_coord_g;\n"
                                 "out vec2 tex_coord_b;\n"
                                 "in float warp_factor_in;\n"
                                 "in float vignette_factor_in;\n"
                                 "out float vignette_factor;\n"
                                 "\n"
                                 "vec2 timewarp(vec2 coord, mat4 rot)\n"
                                 "{\n"
                                 //"  vec3 transformed = (rot * vec4(coord.xy, 1.0, 1.0)).xyz;\n"
                                 "  vec3 transformed = vec3(coord.xy, 1.0);\n"
                                 "  vec2 flattened = transformed.xy / transformed.z;\n"
                                 "\n"
                                 "  return eye_to_source_uv_scale * flattened + eye_to_source_uv_offset;\n"
                                 "}\n"
                                 ,
                                 NULL); /* post */
        cg_snippet_set_replace(snippet,
                               "  mat4 lerped_eye_rot = (eye_rotation_start * (1.0 - warp_factor_in)) + \n"
                               "                        (eye_rotation_end * warp_factor_in);\n"
                               "  tex_coord_r = timewarp(tan_eye_angles_r_in, lerped_eye_rot);\n"
                               "  tex_coord_g = timewarp(tan_eye_angles_g_in, lerped_eye_rot);\n"
                               "  tex_coord_b = timewarp(tan_eye_angles_b_in, lerped_eye_rot);\n"
                               //"  tex_coord_r = (cg_position_in.xy * 0.5) + 0.5;\n"
                               //"  tex_coord_g = (cg_position_in.xy * 0.5) + 1.0;\n"
                               //"  tex_coord_b = (cg_position_in.xy * 0.5) + 1.0;\n"
                               //"  tex_coord_g = tan_eye_angles_g_in;\n"
                               //"  tex_coord_b = tan_eye_angles_b_in;\n"
                               "  vignette_factor = vignette_factor_in;\n"
                               "  cg_position_out = vec4(cg_position_in.xy, 0.5, 1.0);\n"
                              );
        cg_pipeline_add_snippet(eye->distort_pipeline, snippet);
        cg_object_unref(snippet);

        snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                                 "in vec2 tex_coord_r;\n"
                                 "in vec2 tex_coord_g;\n"
                                 "in vec2 tex_coord_b;\n"
                                 "in float vignette_factor;\n"
                                 ,
                                 NULL); /* post */
        cg_snippet_set_replace(snippet,
                               //"  cg_color_out = vec4(texture(cg_sampler0, tex_coord_r).rgb, 1.0);\n"
                               "  float R = cg_texture_lookup0(cg_sampler0, vec4(tex_coord_r, 0.0, 0.0)).r;\n"
                               "  float G = cg_texture_lookup0(cg_sampler0, vec4(tex_coord_g, 0.0, 0.0)).g;\n"
                               "  float B = cg_texture_lookup0(cg_sampler0, vec4(tex_coord_b, 0.0, 0.0)).b;\n"
                               "  cg_color_out = vignette_factor * vec4(R, G, B, 1.0);\n"
                               //"  cg_color_out = vec4(R, G, B, 1.0);\n"
                               //"  cg_color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
                               );
        cg_pipeline_add_snippet(eye->distort_pipeline, snippet);
        cg_object_unref(snippet);

        eye->eye_to_source_uv_scale_loc =
            cg_pipeline_get_uniform_location(eye->distort_pipeline, "eye_to_source_uv_scale");
        eye->eye_to_source_uv_offset_loc =
            cg_pipeline_get_uniform_location(eye->distort_pipeline, "eye_to_source_uv_offset");
        eye->eye_rotation_start_loc =
            cg_pipeline_get_uniform_location(eye->distort_pipeline, "eye_rotation_start");
        eye->eye_rotation_end_loc =
            cg_pipeline_get_uniform_location(eye->distort_pipeline, "eye_rotation_end");

        cg_pipeline_set_uniform_float(eye->distort_pipeline,
                                      eye->eye_to_source_uv_scale_loc,
                                      2, /* n components */
                                      1, /* count */
                                      eye->eye_to_source_uv_scale);

        cg_pipeline_set_uniform_float(eye->distort_pipeline,
                                      eye->eye_to_source_uv_offset_loc,
                                      2, /* n components */
                                      1, /* count */
                                      eye->eye_to_source_uv_offset);

        create_eye_distortion_mesh(&data, eye);
    }

    data.triangle = cg_primitive_new_p3c4(
        data.dev, CG_VERTICES_MODE_TRIANGLES, 3, triangle_vertices);
    data.pipeline = cg_pipeline_new(data.dev);
    cg_pipeline_set_blend(data.pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

    data.test_pipeline = cg_pipeline_new(data.dev);
    cg_pipeline_set_blend(data.test_pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

    cg_onscreen_add_frame_callback(
        data.fb, frame_event_cb, &data, NULL); /* destroy notify */
    cg_onscreen_add_dirty_callback(
        data.fb, dirty_cb, &data, NULL); /* destroy notify */

    uv_idle_init(loop, &data.idle);
    data.idle.data = &data;
    uv_idle_start(&data.idle, paint_cb);

    cg_uv_set_mainloop(data.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

cleanup:
    if (data.hmd)
        ovrHmd_Destroy(data.hmd);
    ovr_Shutdown();

    return 0;
}
