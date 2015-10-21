#include <config.h>

#include <cglib/cglib.h>

#include <string.h>

#include "test-cg-fixtures.h"

/* Non-power-of-two sized texture that should cause slicing */
#define TEXTURE_SIZE        384
/* Number of times to split the texture up on each axis */
#define PARTS               2
/* The texture is split into four parts, each with a different colour */
#define PART_SIZE           (TEXTURE_SIZE / PARTS)

/* Amount of pixels to skip off the top, bottom, left and right of the
   texture when reading back the stage */
#define TEST_INSET          4

/* Size to actually render the texture at */
#define TEXTURE_RENDER_SIZE TEXTURE_SIZE
/* The size of a part once rendered */
#define PART_RENDER_SIZE    (TEXTURE_RENDER_SIZE / PARTS)

static const uint32_t corner_colors[PARTS * PARTS] =
  {
    /* Top left     - red */    0xff0000ff,
    /* Top right    - green */  0x00ff00ff,
    /* Bottom left  - blue */   0x0000ffff,
    /* Bottom right - yellow */ 0xffff00ff
  };

static void
validate_part (int xnum,
               int ynum,
               uint32_t color)
{
  test_cg_check_region (test_fb,
                           xnum * PART_RENDER_SIZE + TEST_INSET,
                           ynum * PART_RENDER_SIZE + TEST_INSET,
                           PART_RENDER_SIZE - TEST_INSET * 2,
                           PART_RENDER_SIZE - TEST_INSET * 2,
                           color);
}

static void
validate_result (void)
{
  /* Validate that all four corners of the texture are drawn in the
     right color */
  validate_part (0, 0, corner_colors[0]);
  validate_part (1, 0, corner_colors[1]);
  validate_part (0, 1, corner_colors[2]);
  validate_part (1, 1, corner_colors[3]);
}

static cg_texture_t *
make_texture (void)
{
  void *tex_data;
  uint32_t *p;
  cg_texture_t *tex;
  int partx, party, width, height;

  p = tex_data = c_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  /* Make a texture with a different color for each part */
  for (party = 0; party < PARTS; party++)
    {
      height = (party < PARTS - 1
                ? PART_SIZE
                : TEXTURE_SIZE - PART_SIZE * (PARTS - 1));

      for (partx = 0; partx < PARTS; partx++)
        {
          uint32_t color = corner_colors[party * PARTS + partx];
          width = (partx < PARTS - 1
                   ? PART_SIZE
                   : TEXTURE_SIZE - PART_SIZE * (PARTS - 1));

          while (width-- > 0)
            *(p++) = C_UINT32_TO_BE (color);
        }

      while (--height > 0)
        {
          memcpy (p, p - TEXTURE_SIZE, TEXTURE_SIZE * 4);
          p += TEXTURE_SIZE;
        }
    }

  tex = test_cg_texture_new_from_data (test_dev,
                                          TEXTURE_SIZE,
                                          TEXTURE_SIZE,
                                          TEST_CG_TEXTURE_NO_ATLAS,
                                          CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                          TEXTURE_SIZE * 4,
                                          tex_data);

  c_free (tex_data);

  if (test_verbose ())
    {
      if (cg_texture_is_sliced (tex))
        c_print ("Texture is sliced\n");
      else
        c_print ("Texture is not sliced\n");
    }

  /* The texture should be sliced unless NPOTs are supported */
  c_assert (cg_has_feature (test_dev, CG_FEATURE_ID_TEXTURE_NPOT)
            ? !cg_texture_is_sliced (tex)
            : cg_texture_is_sliced (tex));

  return tex;
}

static void
paint (void)
{
  cg_pipeline_t *pipeline = cg_pipeline_new (test_dev);
  cg_texture_t *texture = make_texture ();
  int y, x;

  cg_pipeline_set_layer_texture (pipeline, 0, texture);

  /* Just render the texture in the top left corner */
  /* Render the texture using four separate rectangles */
  for (y = 0; y < 2; y++)
    for (x = 0; x < 2; x++)
      cg_framebuffer_draw_textured_rectangle (test_fb,
                                                pipeline,
                                                x * TEXTURE_RENDER_SIZE / 2,
                                                y * TEXTURE_RENDER_SIZE / 2,
                                                (x + 1) *
                                                TEXTURE_RENDER_SIZE / 2,
                                                (y + 1) *
                                                TEXTURE_RENDER_SIZE / 2,
                                                x / 2.0f,
                                                y / 2.0f,
                                                (x + 1) / 2.0f,
                                                (y + 1) / 2.0f);

  cg_object_unref (pipeline);
  cg_object_unref (texture);
}

void
test_npot_texture (void)
{
  if (test_verbose ())
    {
      if (cg_has_feature (test_dev, CG_FEATURE_ID_TEXTURE_NPOT))
        c_print ("NPOT textures are supported\n");
      else
        c_print ("NPOT textures are not supported\n");
    }

  cg_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cg_framebuffer_get_width (test_fb),
                                 cg_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint ();
  validate_result ();

  if (test_verbose ())
    c_print ("OK\n");
}

