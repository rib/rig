#include "config.h"

#include <cglib/cglib.h>

#include "test-cg-fixtures.h"

/* I'm writing this on the train after having dinner at a churrascuria. */
#define MAGIC_CHUNK_O_DATA ((void *) 0xdeadbeef)

static GMainLoop *loop;

gboolean
timeout (void *user_data)
{
  c_assert (!"timeout not reached");

  return false;
}

void
callback (cg_fence_t *fence,
          void *user_data)
{
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);

  test_cg_check_pixel (test_fb, fb_width - 1, fb_height - 1, 0x00ff0000);
  c_assert (user_data == MAGIC_CHUNK_O_DATA && "callback data not mangled");

  g_main_loop_quit (loop);
}

void
test_fence (void)
{
  GSource *cg_source;
  int fb_width = cg_framebuffer_get_width (test_fb);
  int fb_height = cg_framebuffer_get_height (test_fb);
  cg_fence_closure_t *closure;

  cg_source = cg_glib_source_new (test_dev, G_PRIORITY_DEFAULT);
  g_source_attach (cg_source, NULL);
  loop = g_main_loop_new (NULL, true);

  cg_framebuffer_orthographic (test_fb, 0, 0, fb_width, fb_height, -1, 100);
  cg_framebuffer_clear4f (test_fb, CG_BUFFER_BIT_COLOR,
                            0.0f, 1.0f, 0.0f, 0.0f);

  closure = cg_framebuffer_add_fence_callback (test_fb,
                                                 callback,
                                                 MAGIC_CHUNK_O_DATA);
  c_assert (closure != NULL);

  g_timeout_add_seconds (5, timeout, NULL);

  g_main_loop_run (loop);

  if (test_verbose ())
    c_print ("OK\n");
}
