#include <config.h>

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <clib.h>
#include <rut.h>
#include <cogl/cogl.h>

#include <uv.h>

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif
//#define USE_FAKE_DATA 1

#define CLEAR(X) memset(&X, 0, sizeof(X))

struct buffer {
    uint8_t *start;
    size_t length;
};

struct data {
    rut_shell_t *shell;
    cg_device_t *dev;

    rut_shell_onscreen_t *shell_onscreen;
    cg_framebuffer_t *fb;

    int depth_width;
    int depth_height;
    cg_texture_2d_t *depth_tex0;
    cg_texture_2d_t *depth_tex1;
    uint8_t *depth_buf0;
    uint8_t *depth_buf1;
    cg_pipeline_t *depth_pipeline0;
    cg_pipeline_t *depth_pipeline1;

    cg_pipeline_t *depth_final;

    int v4l_fd;
    struct buffer *v4l_buffers;
    int n_v4l_buffers;
    uv_poll_t v4l_poll;
    uv_timer_t v4l_timer0;
    uv_timer_t v4l_timer1;
    uv_timer_t v4l_timer2;

    rut_linear_gradient_t *ir_gradient;
    rut_linear_gradient_t *depth_gradient;
};

static char *dev_name = "/dev/video2";

//static void paint_cb(uv_idle_t *idle);

static void
delay(void)
{
    struct timespec delay;

    delay.tv_sec = 0;
    //delay.tv_nsec = 50000000;
    //delay.tv_nsec = 20000000;
    delay.tv_nsec = 20000000;

    nanosleep(&delay, NULL);
}

static void
errno_exit(const char *s)
{
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int
xioctl(int fd, int request, void *data)
{
    int r;

    do {
        r = ioctl(fd, request, data);
    } while (r == -1 && errno == EINTR);

    if (r < 0)
        errno_exit("ioctl error:");

    delay();
#if 0
    {
        struct timespec delay;

        delay.tv_sec = 0;
        delay.tv_nsec = 500000000;
        //delay.tv_nsec = 20000000;

        nanosleep(&delay, NULL);
    }
#endif

    return r;
}

#if 0
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
#endif

static void
read_depth_buffer(struct data *data)
{
    struct v4l2_buffer buf;
    struct uvc_xu_control_query query;
    int x, y;
    int out_bpp = 2;
    int out_stride = data->depth_width * out_bpp;
    uint8_t *start;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(data->v4l_fd, VIDIOC_DQBUF, &buf) == -1) {
        switch (errno) {
        case EAGAIN:
            return;

        case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

        default:
            errno_exit("VIDIOC_DQBUF");
        }
    }
    start = data->v4l_buffers[buf.index].start;

    c_warn_if_fail(buf.index < data->n_v4l_buffers);

    //printf("New frame\n");

    for (y = 0; y < data->depth_height; y++) {
        int x0 = 0;
        int x1 = 0;
        for (x = 0; x < data->depth_width; x++) {
            int in_stride = data->depth_width * 2;
            uint8_t *in = start + y * in_stride + x * 2;
            uint16_t short_val;
            float fval;
            uint8_t *out;

            uint16_t val = in[1] << 8 | (in[0] );
            fval = ((float)val)/65535.0;


            out = data->depth_buf0 + y * out_stride + x0 * out_bpp;
            x0++;

            short_val = fval * 65535.0f;
            out[0] = short_val & 0xff;
            out[1] = (short_val & 0xff00) >> 8;

        }
    }

    cg_texture_set_region(data->depth_tex0,
                          data->depth_width,
                          data->depth_height,
                          CG_PIXEL_FORMAT_RG_88,
                          0, /* auto row stride */
                          (uint8_t *)data->depth_buf0,
                          0, 0,
                          0, /* level */
                          NULL); /* error */

    rut_shell_queue_redraw(data->shell);

    if (xioctl(data->v4l_fd, VIDIOC_QBUF, &buf) == -1)
        errno_exit("VIDIOC_QBUF");
}

static void
shell_redraw_cb(rut_shell_t *shell, void *user_data)
{
    //struct data *data = idle->data;
    struct data *data = user_data;
    cg_matrix_t identity;
    cg_matrix_init_identity(&identity);

    rut_shell_start_redraw(shell);

    rut_shell_update_timelines(shell);

    rut_shell_run_pre_paint_callbacks(shell);

    rut_shell_run_start_paint_callbacks(shell);

    cg_framebuffer_identity_matrix(data->fb);
    cg_framebuffer_set_projection_matrix(data->fb, &identity);

    cg_framebuffer_clear4f(data->fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);


    cg_framebuffer_draw_textured_rectangle(data->fb,
                                           data->depth_pipeline0,
                                           -1, 0, 0, -1,
                                           1, 0, 0, 1);

#if 0
    cg_framebuffer_draw_textured_rectangle(data->fb,
                                           data->depth_pipeline1,
                                           0, 0, 1, -1,
                                           1, 0, 0, 1);

    cg_framebuffer_draw_textured_rectangle(data->fb,
                                           data->depth_final,
                                           -1, 1, 0, 0,
                                           1, 0, 0, 1);
#endif

    cg_onscreen_swap_buffers(data->fb);

    rut_shell_run_post_paint_callbacks(shell);

    rut_shell_end_redraw(shell);

    /* FIXME: we should hook into an asynchronous notification of
     * when rendering has finished for determining when a frame is
     * finished. */
    rut_shell_finish_frame(shell);

    if (rut_shell_check_timelines(shell))
        rut_shell_queue_redraw(shell);
}

static void
init_mmap(struct data *data)
{
    struct v4l2_requestbuffers req;
    int n_buffers;

    CLEAR(req);

    req.count = 2;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(data->v4l_fd, VIDIOC_REQBUFS, &req) == -1) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support "
                    "memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n",
                dev_name);
        exit(EXIT_FAILURE);
    }

    data->v4l_buffers = calloc(req.count, sizeof(struct buffer));

    if (!data->v4l_buffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == xioctl(data->v4l_fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        data->v4l_buffers[n_buffers].length = buf.length;
        data->v4l_buffers[n_buffers].start =
            mmap(NULL /* start anywhere */,
                 buf.length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 data->v4l_fd, buf.m.offset);

        if (MAP_FAILED == data->v4l_buffers[n_buffers].start)
            errno_exit("mmap");
    }

    data->n_v4l_buffers = n_buffers;
}

static void
start_capture(struct data *data)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int i;

    for (i = 0; i < data->n_v4l_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(data->v4l_fd, VIDIOC_QBUF, &buf) == -1)
            errno_exit("VIDIOC_QBUF");
    }

    if (xioctl(data->v4l_fd, VIDIOC_STREAMON, &type) == -1)
        errno_exit("VIDIOC_STREAMON");
}


static void
trace_replay(struct data *d)
{
    struct uvc_xu_control_query query;

    {
        struct v4l2_format fmt;

        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(d->v4l_fd, VIDIOC_G_FMT, &fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width=640;
        fmt.fmt.pix.height=240;
        //fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.pixelformat=0x56595559; //YUYV
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
#if 1
        fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 2;
        fmt.fmt.pix.sizeimage = fmt.fmt.pix.width * fmt.fmt.pix.height * 2;
        fmt.fmt.pix.colorspace = 8;
        //fmt.fmt.pix.flags = 0x6c0008c8;//???
#endif
        xioctl(d->v4l_fd, VIDIOC_S_FMT, &fmt);

        d->depth_width = fmt.fmt.pix.width;
        d->depth_height = fmt.fmt.pix.height;
    }

    {
        struct v4l2_streamparm param;

        CLEAR(param);
        param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        param.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
        param.parm.capture.timeperframe.numerator = 1;
        param.parm.capture.timeperframe.denominator = 30;
        xioctl(d->v4l_fd, VIDIOC_G_PARM, &param);

#if 0
        param.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
        param.parm.capture.capturemode = 0;
        param.parm.capture.timeperframe.numerator = 1;
        param.parm.capture.timeperframe.denominator = 30;
        param.parm.capture.extendedmode = 0;
        param.parm.capture.readbuffers = 0;
#endif
        xioctl(d->v4l_fd, VIDIOC_S_PARM, &param);
    }

    init_mmap(d);
    start_capture(d);
}

static void
probe_device(struct data *data)
{
    int u, s;

    for (u = 0; u < 256; u++) {
        for (s = 0; s < 256; s++) {
            struct uvc_xu_control_query query;
            uint16_t len = 0;
            int r;

            CLEAR(query);
            query.unit = u;
            query.selector = s;
            query.query = UVC_GET_LEN;
            query.size = 2;
            query.data = (void *)&len;

            do {
                r = ioctl(data->v4l_fd, UVCIOC_CTRL_QUERY, &query);
            } while (r == -1 && errno == EINTR);

            if (r != -1) {
                printf("%d,%d: len = %d\n", u, s, (int)len);
            }
        }
    }

    {
        uint8_t query_data[7];
        struct uvc_xu_control_query query;
        uint16_t len = 0;
        int r;

        CLEAR(query);
        query.unit = 6;
        query.selector = 2;
        query.query = UVC_GET_MIN;
        query.size = 7;
        query.data = query_data;

        do {
            r = ioctl(data->v4l_fd, UVCIOC_CTRL_QUERY, &query);
        } while (r == -1 && errno == EINTR);

        if (r != -1) {
            uint8_t *buf = query_data;
            printf("min value = %x,%x,%x,%x,%x,%x,%x\n",
                   buf[0],
                   buf[1],
                   buf[2],
                   buf[3],
                   buf[4],
                   buf[5],
                   buf[6]);

        }
    }

    {
        uint8_t query_data[7];
        struct uvc_xu_control_query query;
        uint16_t len = 0;
        int r;

        CLEAR(query);
        query.unit = 6;
        query.selector = 2;
        query.query = UVC_GET_MIN;
        query.size = 7;
        query.data = query_data;

        do {
            r = ioctl(data->v4l_fd, UVCIOC_CTRL_QUERY, &query);
        } while (r == -1 && errno == EINTR);

        if (r != -1) {
            uint8_t *buf = query_data;
            printf("max value = %x,%x,%x,%x,%x,%x,%x\n",
                   buf[0],
                   buf[1],
                   buf[2],
                   buf[3],
                   buf[4],
                   buf[5],
                   buf[6]);
        }
    }

}

static void
init_v4l2(struct data *data)
{
    struct v4l2_format fmt;
    struct v4l2_streamparm param;
    struct uvc_xu_control_query query;
    uint8_t ctrl_data[8];

    data->v4l_fd = open(dev_name, O_RDWR|O_CLOEXEC);
    if (data->v4l_fd < 0) {
        fprintf(stderr, "failed to open %s: %m\n", dev_name);
        exit(1);
    }

    //probe_device(data);

#if 0
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(data->v4l_fd, VIDIOC_G_FMT, &fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width=640;
    fmt.fmt.pix.height=240;
    fmt.fmt.pix.pixelformat=0x56595559; //YUYV
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = 1280;
    fmt.fmt.pix.sizeimage = 307200;
    fmt.fmt.pix.colorspace = 8;
    fmt.fmt.pix.flags = 0x6c0008c8;//???
    xioctl(data->v4l_fd, VIDIOC_S_FMT, &fmt);

    CLEAR(param);
    param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    param.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
    param.parm.capture.timeperframe.numerator = 1;
    param.parm.capture.timeperframe.denominator = 60;
    xioctl(data->v4l_fd, VIDIOC_G_PARM, &param);

    param.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
    param.parm.capture.capturemode = 0;
    param.parm.capture.timeperframe.numerator = 1;
    param.parm.capture.timeperframe.denominator = 30;
    param.parm.capture.extendedmode = 0;
    param.parm.capture.readbuffers = 0;
    xioctl(data->v4l_fd, VIDIOC_S_PARM, &param);
#endif
    trace_replay(data);

    //init_mmap(data);
    #if 1
#if 0
    CLEAR(query);
    CLEAR(ctrl_data);
    ctrl_data[0] = 80;
    query.unit = 3;
    query.selector = 3;
    query.query = UVC_SET_CUR;
    query.size = 1;
    query.data = ctrl_data;
    xioctl(data->v4l_fd, UVCIOC_CTRL_QUERY, &query);
#endif

#if 0
    CLEAR(query);
    CLEAR(ctrl_data);
    ctrl_data[0] = 134;
    query.unit = 3;
    query.selector = 3;
    query.query = UVC_GET_CUR;
    query.size = 1;
    query.data = ctrl_data;
    xioctl(data->v4l_fd, UVCIOC_CTRL_QUERY, &query);
    printf("3,2 = %d\n", ctrl_data[0]);
#endif
#endif

}


static void
v4l_ready_cb(uv_poll_t *poll, int status, int events)
{
    struct data *data = poll->data;

    read_depth_buffer(data);
}

static void
on_run_cb(rut_shell_t *shell, void *user_data)
{
    struct data *data = user_data;
    uv_loop_t *loop = shell->uv_loop;
    rut_gradient_stop_t ir_grad_stops[] = {
        {
            .color = { .red = 0, .green = 0, .blue = 0, .alpha = 1 }, //black
            .offset = 0
        },
        {
            .color = { .red = 1, .green = 0, .blue = 0, .alpha = 1 }, //red
            .offset = 0.0025
        },
        {
            .color = { .red = 1, .green = 1, .blue = 0, .alpha = 1 }, //yellow
            .offset = 0.025
        },
        {
            .color = { .red = 0, .green = 1, .blue = 0, .alpha = 1 }, //green
            .offset = 0.05
        },
        {
            .color = { .red = 0, .green = 1, .blue = 1, .alpha = 1 }, //cyan
            .offset = 0.1
        },
        {
            .color = { .red = 0, .green = 0, .blue = 1, .alpha = 1 }, //blue
            .offset = 1
        },
    };
#if 0
    rut_gradient_stop_t ir_grad_stops[] = {
        {
            .color = { .red = 0, .green = 0, .blue = 0, .alpha = 1 }, //black
            .offset = 0
        },
        {
            .color = { .red = 1, .green = 0, .blue = 0, .alpha = 1 }, //red
            .offset = 0.2
        },
        {
            .color = { .red = 1, .green = 1, .blue = 0, .alpha = 1 }, //yellow
            .offset = 0.4
        },
        {
            .color = { .red = 0, .green = 1, .blue = 0, .alpha = 1 }, //green
            .offset = 0.6
        },
        {
            .color = { .red = 0, .green = 1, .blue = 1, .alpha = 1 }, //cyan
            .offset = 0.8
        },
        {
            .color = { .red = 0, .green = 0, .blue = 1, .alpha = 1 }, //blue
            .offset = 1
        },
    };
#endif
    rut_gradient_stop_t depth_grad_stops[] = {
        {
            .color = { .red = 0, .green = 0, .blue = 1, .alpha = 1 },
            .offset = 0
        },
        {
            .color = { .red = 0, .green = 1, .blue = 1, .alpha = 1 },
            .offset = 0.2
        },
        {
            .color = { .red = 0, .green = 1, .blue = 0, .alpha = 1 },
            .offset = 0.4
        },
        {
            .color = { .red = 1, .green = 1, .blue = 0, .alpha = 1 },
            .offset = 0.6
        },
        {
            .color = { .red = 1, .green = 0, .blue = 0, .alpha = 1 },
            .offset = 0.8
        },
        {
            .color = { .red = 1, .green = 0, .blue = 1, .alpha = 1 },
            .offset = 1
        },
    };

    cg_snippet_t *snippet;


    data->dev = data->shell->cg_device;

    data->shell_onscreen = rut_shell_onscreen_new(shell, 640, 480);
    rut_shell_onscreen_allocate(data->shell_onscreen);
    rut_shell_onscreen_set_resizable(data->shell_onscreen, true);
    rut_shell_onscreen_show(data->shell_onscreen);
    data->fb = data->shell_onscreen->cg_onscreen;

    data->ir_gradient = rut_linear_gradient_new(data->shell,
                                                RUT_EXTEND_PAD,
                                                (sizeof(ir_grad_stops) /
                                                 sizeof(rut_gradient_stop_t)),
                                                ir_grad_stops);

    data->depth_gradient = rut_linear_gradient_new(data->shell,
                                                   RUT_EXTEND_PAD,
                                                   (sizeof(depth_grad_stops) /
                                                    sizeof(rut_gradient_stop_t)),
                                                   depth_grad_stops);


    init_v4l2(data);

    data->depth_buf0 = malloc(data->depth_width * data->depth_height * 3);
    data->depth_tex0 = cg_texture_2d_new_with_size(data->dev,
                                                   data->depth_width,
                                                   data->depth_height);
    cg_texture_set_components(data->depth_tex0,
                              CG_TEXTURE_COMPONENTS_RGB);
    data->depth_pipeline0 = cg_pipeline_new(data->dev);
    cg_pipeline_set_layer_texture(data->depth_pipeline0, 0, data->depth_tex0);
    cg_pipeline_set_layer_wrap_mode(data->depth_pipeline0, 0, CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    cg_pipeline_set_layer_filters(data->depth_pipeline0, 0,
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);
    cg_pipeline_set_layer_texture(data->depth_pipeline0, 1, data->ir_gradient->texture);
    cg_pipeline_set_layer_wrap_mode(data->depth_pipeline0, 1, CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             "", /* declarations */
                             ""); /* pre */
    cg_snippet_set_replace(snippet,
                           "vec4 tex0 = texture2D(cg_sampler0, cg_tex_coord0_in.st);\n" //IR image
                           "float v0 = (tex0.g * 0.996108949 + tex0.r * 0.003891051);\n" //Green = most significant
                           //"float v0 = tex0.r;\n"
                           //"if (v0 > 1.0)\n"
                           //"  cg_color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
                           //"if (v0 == 0.0)\n"
                           //"  cg_color_out = vec4(0.2, 0.2, 0.2, 1.0);\n"
                           //"else\n"
                           "  cg_color_out = texture2D(cg_sampler1, vec2(v0, 0.5));\n");
    cg_pipeline_add_snippet(data->depth_pipeline0, snippet);
    cg_object_unref(snippet);

    data->depth_buf1 = malloc(data->depth_width * data->depth_height * 3);
    data->depth_tex1 = cg_texture_2d_new_with_size(data->dev,
                                                   data->depth_width,
                                                   data->depth_height);
    cg_texture_set_components(data->depth_tex1,
                              CG_TEXTURE_COMPONENTS_RGB);
    data->depth_pipeline1 = cg_pipeline_copy(data->depth_pipeline0);
    cg_pipeline_set_layer_texture(data->depth_pipeline1, 0, data->depth_tex1);
    cg_pipeline_set_layer_texture(data->depth_pipeline1, 1, data->depth_gradient->texture);

    data->depth_final = cg_pipeline_new(data->dev);
    cg_pipeline_set_layer_texture(data->depth_final, 0, data->depth_tex0);
    cg_pipeline_set_layer_wrap_mode(data->depth_final, 0, CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    cg_pipeline_set_layer_filters(data->depth_final, 0,
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);
    cg_pipeline_set_layer_texture(data->depth_final, 1, data->depth_tex1);
    cg_pipeline_set_layer_wrap_mode(data->depth_final, 1, CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    cg_pipeline_set_layer_filters(data->depth_final, 1,
                                  CG_PIPELINE_FILTER_NEAREST,
                                  CG_PIPELINE_FILTER_NEAREST);
    cg_pipeline_set_layer_texture(data->depth_final, 2, data->depth_gradient->texture);
    cg_pipeline_set_layer_wrap_mode(data->depth_final, 2, CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             "", /* declarations */
                             ""); /* pre */

#if 0
    //Just the IR image data:
    cg_snippet_set_replace(snippet,
                           "vec4 tex0 = texture2D(cg_sampler0, cg_tex_coord0_in.st);\n" //IR image
                           "float v0 = tex0.g * 0.996108949 + tex0.r * 0.003891051;\n" //Green = most significant
                           //"vec4 tex1 = texture2D(cg_sampler1, vec2(1.0 - cg_tex_coord0_in.s, cg_tex_coord0_in.t));\n"
                           //XXX: magic * 100 factor to see a nice range of values...
                           "cg_color_out = vec4(v0 * 100, 0.0, 0.0, 1.0);\n");
#endif

    //Depth combined with IR data
#if 1
    cg_snippet_set_replace(snippet,
                           "vec4 tex0 = texture2D(cg_sampler0, cg_tex_coord0_in.st);\n" //IR image
                           //XXX: magic * 100 factor to have a nice range of IR values
                           "float v0 = (tex0.g * 0.996108949 + tex0.r * 0.003891051);\n" //Green = most significant
                           //"vec4 tex1 = texture2D(cg_sampler1, vec2(1.0 - cg_tex_coord0_in.s, cg_tex_coord0_in.t));\n"
                           "vec4 tex1 = texture2D(cg_sampler1, vec2(cg_tex_coord0_in.st));\n" //Depth image
                           "float v1 = tex1.g * 0.996108949 + tex1.r * 0.003891051;\n" //Green = most significant
                           "if (v0 < 0.002)\n"
                           "    cg_color_out = vec4(0.2, 0.2, 0.2, 1.0);\n"
                           "else\n"
                           "    cg_color_out = texture2D(cg_sampler2, vec2(v1, 0.5));\n");
                           //"    cg_color_out = vec4(v1, 0.0, 0.0, 1.0);\n");
#endif
    cg_pipeline_add_snippet(data->depth_final, snippet);
    cg_object_unref(snippet);

    data->v4l_poll.data = data;
    uv_poll_init(loop, &data->v4l_poll, data->v4l_fd);
    uv_poll_start(&data->v4l_poll, UV_READABLE, v4l_ready_cb);
}

int
main(int argc, char **argv)
{
    struct data data;

    rut_init_tls_state();

#ifdef USE_GSTREAMER
    gst_init(&argc, &argv);
#endif

    data.depth_buf0 = NULL;

    data.shell = rut_shell_new(shell_redraw_cb, &data);
    rut_shell_set_on_run_callback(data.shell, on_run_cb, &data);

    rut_shell_main(data.shell);

    return 0;
}

/* 480 x 320 */
