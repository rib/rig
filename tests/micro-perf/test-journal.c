#include <config.h>

#include <glib.h>
#include <cglib/cglib.h>
#include <math.h>

#include <clib.h>

#include <cglib/cg-profile.h>

#define FRAMEBUFFER_WIDTH 800
#define FRAMEBUFFER_HEIGHT 500

bool run_all = FALSE;

typedef struct _Data
{
  cg_device_t *dev;
  cg_framebuffer_t *fb;
  cg_pipeline_t *pipeline;
  cg_pipeline_t *alpha_pipeline;
  c_timer_t *timer;
  int frame;
} Data;

static void
test_rectangles (Data *data)
{
#define RECT_WIDTH 5
#define RECT_HEIGHT 5
  int x;
  int y;

  cg_framebuffer_clear4f (data->fb, CG_BUFFER_BIT_COLOR, 1, 1, 1, 1);

  cg_framebuffer_push_rectangle_clip (data->fb,
                                        10,
                                        10,
                                        FRAMEBUFFER_WIDTH - 10,
                                        FRAMEBUFFER_HEIGHT - 10);

  /* Should the rectangles be randomly positioned/colored/rotated?
   *
   * It could be good to develop equivalent GL and Cairo tests so we can
   * have a sanity check for our Cogl performance.
   *
   * The color should vary to check that we correctly batch color changes
   * The use of alpha should vary so we have a variation of which rectangles
   * require blending.
   *  Should this be a random variation?
   *  It could be good to experiment with focibly enabling blending for
   *  rectangles that don't technically need it for the sake of extending
   *  batching. E.g. if you a long run of interleved rectangles with every
   *  other rectangle needing blending then it may be worth enabling blending
   *  for all the rectangles to avoid the state changes.
   * The modelview should change between rectangles to check the software
   * transform codepath.
   *  Should we group some rectangles under the same modelview? Potentially
   *  we could avoid software transform for long runs of rectangles with the
   *  same modelview.
   *
   */
  for (y = 0; y < FRAMEBUFFER_HEIGHT; y += RECT_HEIGHT)
    {
      for (x = 0; x < FRAMEBUFFER_WIDTH; x += RECT_WIDTH)
        {
          cg_framebuffer_push_matrix (data->fb);
          cg_framebuffer_translate (data->fb, x, y, 0);
          cg_framebuffer_rotate (data->fb, 45, 0, 0, 1);

          cg_pipeline_set_color4ub(data->pipeline,
                                   0xff,
                                   (255 * y/FRAMEBUFFER_WIDTH),
                                   (255 * x/FRAMEBUFFER_WIDTH),
                                   0xff);
          cg_framebuffer_draw_rectangle (data->fb,
                                         data->pipeline,
                                         0, 0, RECT_WIDTH, RECT_HEIGHT);

          cg_framebuffer_pop_matrix (data->fb);
        }
    }

  for (y = 0; y < FRAMEBUFFER_HEIGHT; y += RECT_HEIGHT)
    {
      for (x = 0; x < FRAMEBUFFER_WIDTH; x += RECT_WIDTH)
        {
          cg_framebuffer_push_matrix (data->fb);
          cg_framebuffer_translate (data->fb, x, y, 0);

          cg_pipeline_set_color4ub(data->alpha_pipeline,
                                   0xff,
                                   (255 * y/FRAMEBUFFER_WIDTH),
                                   (255 * x/FRAMEBUFFER_WIDTH),
                                   (255 * x/FRAMEBUFFER_WIDTH));
          cg_framebuffer_draw_rectangle (data->fb,
                                         data->alpha_pipeline,
                                         0, 0, RECT_WIDTH, RECT_HEIGHT);

          cg_framebuffer_pop_matrix (data->fb);
        }
    }

  cg_framebuffer_pop_clip (data->fb);
}

static gboolean
paint_cb (void *user_data)
{
  Data *data = user_data;
  double elapsed;

  data->frame++;

  test_rectangles (data);

  cg_onscreen_swap_buffers (CG_ONSCREEN (data->fb));

  elapsed = c_timer_elapsed (data->timer, NULL);
  if (elapsed > 1.0)
    {
      c_print ("fps = %f\n", data->frame / elapsed);
      c_timer_start (data->timer);
      data->frame = 0;
    }

  return FALSE; /* remove the callback */
}

static void
frame_event_cb (cg_onscreen_t *onscreen,
                cg_frame_event_t event,
                cg_frame_info_t *info,
                void *user_data)
{
  if (event == CG_FRAME_EVENT_SYNC)
    paint_cb (user_data);
}

int
main (int argc, char **argv)
{
  Data data;
  cg_onscreen_t *onscreen;
  GSource *cg_source;
  GMainLoop *loop;
  CG_STATIC_TIMER (mainloop_timer,
                      NULL, //no parent
                      "Mainloop",
                      "The time spent in the glib mainloop",
                      0);  // no application private data

  data.dev = cg_device_new ();

  onscreen = cg_onscreen_new (data.dev,
                                FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
  cg_onscreen_set_swap_throttled (onscreen, FALSE);
  cg_onscreen_show (onscreen);

  data.fb = onscreen;
  cg_framebuffer_orthographic (data.fb,
                                 0, 0,
                                 FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT,
                                 -1,
                                 100);

  data.pipeline = cg_pipeline_new (data.dev);
  cg_pipeline_set_color4f (data.pipeline, 1, 1, 1, 1);
  data.alpha_pipeline = cg_pipeline_new (data.dev);
  cg_pipeline_set_color4f (data.alpha_pipeline, 1, 1, 1, 0.5);

  cg_source = cg_glib_source_new (data.dev, G_PRIORITY_DEFAULT);

  g_source_attach (cg_source, NULL);

  cg_onscreen_add_frame_callback (CG_ONSCREEN (data.fb),
                                    frame_event_cb,
                                    &data,
                                    NULL); /* destroy notify */

  g_idle_add (paint_cb, &data);

  data.frame = 0;
  data.timer = c_timer_new ();
  c_timer_start (data.timer);

  loop = g_main_loop_new (NULL, TRUE);
  CG_TIMER_START (uprof_get_mainloop_context (), mainloop_timer);
  g_main_loop_run (loop);
  CG_TIMER_STOP (uprof_get_mainloop_context (), mainloop_timer);

  return 0;
}

