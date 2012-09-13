#include <glib.h>
#include <gio/gio.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include <cogl/cogl.h>

#include "rig.h"
#include "rig-inspector.h"

#include "rig-data.h"
#include "rig-transition.h"
#include "rig-load-save.h"

//#define DEVICE_WIDTH 480.0
//#define DEVICE_HEIGHT 800.0
#define DEVICE_WIDTH 720.0
#define DEVICE_HEIGHT 1280.0

/*
 * Note: The size and padding for this circle texture have been carefully
 * chosen so it has a power of two size and we have enough padding to scale
 * down the circle to a size of 2 pixels and still have a 1 texel transparent
 * border which we rely on for anti-aliasing.
 */
#define CIRCLE_TEX_RADIUS 16
#define CIRCLE_TEX_PADDING 16

#define N_CUBES 5

typedef enum _UndoRedoOp
{
  UNDO_REDO_PROPERTY_CHANGE_OP,
  UNDO_REDO_N_OPS
} UndoRedoOp;

typedef struct _UndoRedoPropertyChange
{
  RigEntity *entity;
  RigProperty *property;
  RigBoxed value0;
  RigBoxed value1;
} UndoRedoPropertyChange;

typedef struct _UndoRedo
{
  UndoRedoOp op;
  CoglBool mergable;
  union
    {
      UndoRedoPropertyChange prop_change;
    } d;
} UndoRedo;

struct _RigUndoJournal
{
  RigData *data;
  GQueue ops;
  GList *pos;
  GQueue redo_ops;
};

typedef struct _UndoRedoOpImpl
{
  void (*apply) (RigUndoJournal *journal, UndoRedo *undo_redo);
  UndoRedo *(*invert) (UndoRedo *src);
  void (*free) (UndoRedo *undo_redo);
} UndoRedoOpImpl;

typedef enum _Pass
{
  PASS_COLOR,
  PASS_SHADOW,
  PASS_DOF_DEPTH
} Pass;

typedef struct _PaintContext
{
  RigPaintContext _parent;

  RigData *data;

  GList *camera_stack;

  Pass pass;

} PaintContext;

static RigPropertySpec rig_data_property_specs[] = {
  {
    .name = "width",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigData, width)
  },
  {
    .name = "height",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigData, height)
  },
  { 0 }
};

#ifndef __ANDROID__

CoglBool _rig_in_device_mode = FALSE;
static char **_rig_handset_remaining_args = NULL;

static const GOptionEntry rig_handset_entries[] =
{
  { "device-mode", 'd', 0, 0,
    &_rig_in_device_mode, "Run in Device Mode" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_handset_remaining_args, "Project" },
  { 0 }
};

static char *_rig_project_dir = NULL;

#endif /* __ANDROID__ */

static CoglBool
undo_journal_insert (RigUndoJournal *journal, UndoRedo *undo_redo);

static void
undo_redo_apply (RigUndoJournal *journal, UndoRedo *undo_redo);

static UndoRedo *
undo_redo_invert (UndoRedo *undo_redo);

static void
undo_redo_free (UndoRedo *undo_redo);

static UndoRedo *
undo_journal_find_recent_property_change (RigUndoJournal *journal,
                                          RigProperty *property)
{
  if (journal->pos &&
      journal->pos == journal->ops.tail)
    {
      UndoRedo *recent = journal->pos->data;
      if (recent->d.prop_change.property == property &&
          recent->mergable)
        return recent;
    }

  return NULL;
}

static void
undo_journal_log_move (RigUndoJournal *journal,
                       CoglBool mergable,
                       RigEntity *entity,
                       float prev_x,
                       float prev_y,
                       float prev_z,
                       float x,
                       float y,
                       float z)
{
  RigProperty *position =
    rig_introspectable_lookup_property (entity, "position");
  UndoRedo *undo_redo;
  UndoRedoPropertyChange *prop_change;

  if (mergable)
    {
      undo_redo = undo_journal_find_recent_property_change (journal, position);
      if (undo_redo)
        {
          prop_change = &undo_redo->d.prop_change;
          /* NB: when we are merging then the existing operation is an
           * inverse of a normal move operation so the new move
           * location goes into value0... */
          prop_change->value0.d.vec3_val[0] = x;
          prop_change->value0.d.vec3_val[1] = y;
          prop_change->value0.d.vec3_val[2] = z;
        }
    }

  undo_redo = g_slice_new (UndoRedo);

  undo_redo->op = UNDO_REDO_PROPERTY_CHANGE_OP;
  undo_redo->mergable = mergable;

  prop_change = &undo_redo->d.prop_change;
  prop_change->entity = rig_ref_countable_ref (entity);
  prop_change->property = position;

  prop_change->value0.type = RIG_PROPERTY_TYPE_VEC3;
  prop_change->value0.d.vec3_val[0] = prev_x;
  prop_change->value0.d.vec3_val[1] = prev_y;
  prop_change->value0.d.vec3_val[2] = prev_z;

  prop_change->value1.type = RIG_PROPERTY_TYPE_VEC3;
  prop_change->value1.d.vec3_val[0] = x;
  prop_change->value1.d.vec3_val[1] = y;
  prop_change->value1.d.vec3_val[2] = z;

  undo_journal_insert (journal, undo_redo);
}

static void
undo_journal_copy_property_and_log (RigUndoJournal *journal,
                                    CoglBool mergable,
                                    RigEntity *entity,
                                    RigProperty *target_prop,
                                    RigProperty *source_prop)
{
  UndoRedo *undo_redo;
  UndoRedoPropertyChange *prop_change;

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       undo_journal_find_recent_property_change (journal, target_prop)))
    {
      prop_change = &undo_redo->d.prop_change;
      /* NB: when we are merging then the existing operation is an
       * inverse of a normal move operation so the new move location
       * goes into value0... */
      rig_boxed_destroy (&prop_change->value0);
      rig_property_box (source_prop, &prop_change->value0);
      rig_property_set_boxed (&journal->data->ctx->property_ctx,
                              target_prop,
                              &prop_change->value0);
    }
  else
    {
      undo_redo = g_slice_new (UndoRedo);

      undo_redo->op = UNDO_REDO_PROPERTY_CHANGE_OP;
      undo_redo->mergable = mergable;

      prop_change = &undo_redo->d.prop_change;

      rig_property_box (target_prop, &prop_change->value0);
      rig_property_box (source_prop, &prop_change->value1);

      prop_change = &undo_redo->d.prop_change;
      prop_change->entity = rig_ref_countable_ref (entity);
      prop_change->property = target_prop;

      rig_property_set_boxed (&journal->data->ctx->property_ctx,
                              target_prop,
                              &prop_change->value1);

      undo_journal_insert (journal, undo_redo);
    }
}

static void
undo_redo_prop_change_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedoPropertyChange *prop_change = &undo_redo->d.prop_change;

  g_print ("Property change APPLY\n");

  rig_property_set_boxed (&journal->data->ctx->property_ctx,
                          prop_change->property, &prop_change->value1);
}

static UndoRedo *
undo_redo_prop_change_invert (UndoRedo *undo_redo_src)
{
  UndoRedoPropertyChange *src = &undo_redo_src->d.prop_change;
  UndoRedo *undo_redo_inverse = g_slice_new (UndoRedo);
  UndoRedoPropertyChange *inverse = &undo_redo_inverse->d.prop_change;

  undo_redo_inverse->op = undo_redo_src->op;
  undo_redo_inverse->mergable = FALSE;

  inverse->entity = rig_ref_countable_ref (src->entity);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_prop_change_free (UndoRedo *undo_redo)
{
  UndoRedoPropertyChange *prop_change = &undo_redo->d.prop_change;
  rig_ref_countable_unref (prop_change->entity);
  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedoOpImpl undo_redo_ops[] =
  {
      {
        undo_redo_prop_change_apply,
        undo_redo_prop_change_invert,
        undo_redo_prop_change_free
      }
  };

static void
undo_redo_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  g_return_if_fail (undo_redo->op < UNDO_REDO_N_OPS);

  undo_redo_ops[undo_redo->op].apply (journal, undo_redo);
}

static UndoRedo *
undo_redo_invert (UndoRedo *undo_redo)
{
  g_return_val_if_fail (undo_redo->op < UNDO_REDO_N_OPS, NULL);

  return undo_redo_ops[undo_redo->op].invert (undo_redo);
}

static void
undo_redo_free (UndoRedo *undo_redo)
{
  g_return_if_fail (undo_redo->op < UNDO_REDO_N_OPS);

  undo_redo_ops[undo_redo->op].free (undo_redo);
}

static void
undo_journal_flush_redos (RigUndoJournal *journal)
{
  UndoRedo *redo;
  while ((redo = g_queue_pop_head (&journal->redo_ops)))
    g_queue_push_tail (&journal->ops, redo);
  journal->pos = journal->ops.tail;
}

static CoglBool
undo_journal_insert (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedo *inverse = undo_redo_invert (undo_redo);

  g_return_val_if_fail (inverse != NULL, FALSE);

  undo_journal_flush_redos (journal);

  /* Purely for testing purposes we now redundantly apply
   * the inverse of the operation followed by the operation
   * itself which should leave us where we started and
   * if not we should hopefully notice quickly!
   */
  undo_redo_apply (journal, inverse);
  undo_redo_apply (journal, undo_redo);

  undo_redo_free (undo_redo);

  g_queue_push_tail (&journal->ops, inverse);
  journal->pos = journal->ops.tail;

  return TRUE;
}

static CoglBool
undo_journal_undo (RigUndoJournal *journal)
{
  g_print ("UNDO\n");
  if (journal->pos)
    {
      UndoRedo *redo = undo_redo_invert (journal->pos->data);
      if (!redo)
        {
          g_warning ("Not allowing undo of operation that can't be inverted");
          return FALSE;
        }
      g_queue_push_tail (&journal->redo_ops, redo);

      undo_redo_apply (journal, journal->pos->data);
      journal->pos = journal->pos->prev;

      rig_shell_queue_redraw (journal->data->shell);
      return TRUE;
    }
  else
    return FALSE;
}

static CoglBool
undo_journal_redo (RigUndoJournal *journal)
{
  UndoRedo *redo = g_queue_pop_tail (&journal->redo_ops);

  if (!redo)
    return FALSE;

  g_print ("REDO\n");

  undo_redo_apply (journal, redo);

  if (journal->pos)
    journal->pos = journal->pos->next;
  else
    journal->pos = journal->ops.head;

  rig_shell_queue_redraw (journal->data->shell);

  return TRUE;
}

static RigUndoJournal *
undo_journal_new (RigData *data)
{
  RigUndoJournal *journal = g_new0 (RigUndoJournal, 1);

  g_queue_init (&journal->ops);
  journal->data = data;
  journal->pos = NULL;
  g_queue_init (&journal->redo_ops);

  return journal;
}

typedef struct _VertexP2T2T2
{
  float x, y, s0, t0, s1, t1;
} VertexP2T2T2;

CoglPrimitive *
create_grid (RigContext *ctx,
             float width,
             float height,
             float x_space,
             float y_space)
{
  GArray *lines = g_array_new (FALSE, FALSE, sizeof (CoglVertexP2));
  float x, y;
  int n_lines = 0;

  for (x = 0; x < width; x += x_space)
    {
      CoglVertexP2 p[2] = {
        { .x = x, .y = 0 },
        { .x = x, .y = height }
      };
      g_array_append_vals (lines, p, 2);
      n_lines++;
    }

  for (y = 0; y < height; y += y_space)
    {
      CoglVertexP2 p[2] = {
        { .x = 0, .y = y },
        { .x = width, .y = y }
      };
      g_array_append_vals (lines, p, 2);
      n_lines++;
    }

  return cogl_primitive_new_p2 (ctx->cogl_context,
                                COGL_VERTICES_MODE_LINES,
                                n_lines * 2,
                                (CoglVertexP2 *)lines->data);
}

static const float jitter_offsets[32] =
{
  0.375f, 0.4375f,
  0.625f, 0.0625f,
  0.875f, 0.1875f,
  0.125f, 0.0625f,

  0.375f, 0.6875f,
  0.875f, 0.4375f,
  0.625f, 0.5625f,
  0.375f, 0.9375f,

  0.625f, 0.3125f,
  0.125f, 0.5625f,
  0.125f, 0.8125f,
  0.375f, 0.1875f,

  0.875f, 0.9375f,
  0.875f, 0.6875f,
  0.125f, 0.3125f,
  0.625f, 0.8125f
};

/* XXX: This assumes that the primitive is being drawn in pixel coordinates,
 * since we jitter the modelview not the projection.
 */
static void
draw_jittered_primitive4f (RigData *data,
                           CoglFramebuffer *fb,
                           CoglPrimitive *prim,
                           float red,
                           float green,
                           float blue)
{
  CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
  int i;

  cogl_pipeline_set_color4f (pipeline,
                             red / 16.0f,
                             green / 16.0f,
                             blue / 16.0f,
                             1.0f / 16.0f);

  for (i = 0; i < 16; i++)
    {
      const float *offset = jitter_offsets + 2 * i;

      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_translate (fb, offset[0], offset[1], 0);
      cogl_framebuffer_draw_primitive (fb, pipeline, prim);
      cogl_framebuffer_pop_matrix (fb);
    }

  cogl_object_unref (pipeline);
}

static void
camera_update_view (RigData *data, RigEntity *camera, Pass pass)
{
  RigCamera *camera_component =
    rig_entity_get_component (camera, RIG_COMPONENT_TYPE_CAMERA);
  CoglMatrix transform;
  CoglMatrix inverse_transform;
  CoglMatrix view;

  /* translate to z_2d and scale */
  if (pass != PASS_SHADOW)
    view = data->main_view;
  else
    view = data->identity;

  /* apply the camera viewing transform */
  rig_graphable_get_transform (camera, &transform);
  cogl_matrix_get_inverse (&transform, &inverse_transform);
  cogl_matrix_multiply (&view, &view, &inverse_transform);

  if (pass == PASS_SHADOW)
    {
      CoglMatrix flipped_view;
      cogl_matrix_init_identity (&flipped_view);
      cogl_matrix_scale (&flipped_view, 1, -1, 1);
      cogl_matrix_multiply (&flipped_view, &flipped_view, &view);
      rig_camera_set_view_transform (camera_component, &flipped_view);
    }
  else
    rig_camera_set_view_transform (camera_component, &view);
}

static void
get_normal_matrix (const CoglMatrix *matrix,
                   float *normal_matrix)
{
  CoglMatrix inverse_matrix;

  /* Invert the matrix */
  cogl_matrix_get_inverse (matrix, &inverse_matrix);

  /* Transpose it while converting it to 3x3 */
  normal_matrix[0] = inverse_matrix.xx;
  normal_matrix[1] = inverse_matrix.xy;
  normal_matrix[2] = inverse_matrix.xz;

  normal_matrix[3] = inverse_matrix.yx;
  normal_matrix[4] = inverse_matrix.yy;
  normal_matrix[5] = inverse_matrix.yz;

  normal_matrix[6] = inverse_matrix.zx;
  normal_matrix[7] = inverse_matrix.zy;
  normal_matrix[8] = inverse_matrix.zz;
}

static void
set_focal_parameters (CoglPipeline *pipeline,
                      float focal_distance,
                      float depth_of_field)
{
  int location;
  float distance;

  /* I want to have the focal distance as positive when it's in front of the
   * camera (it seems more natural, but as, in OpenGL, the camera is facing
   * the negative Ys, the actual value to give to the shader has to be
   * negated */
  distance = -focal_distance;

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "dof_focal_distance");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */, 1 /* count */,
                                   &distance);

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "dof_depth_of_field");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */, 1 /* count */,
                                   &depth_of_field);
}

static void
get_light_modelviewprojection (const CoglMatrix *model_transform,
                               RigEntity  *light,
                               const CoglMatrix *light_projection,
                               CoglMatrix *light_mvp)
{
  const CoglMatrix *light_transform;
  CoglMatrix light_view;

  /* TODO: cache the bias * light_projection * light_view matrix! */

  /* Move the unit data from [-1,1] to [0,1], column major order */
  float bias[16] = {
    .5f, .0f, .0f, .0f,
    .0f, .5f, .0f, .0f,
    .0f, .0f, .5f, .0f,
    .5f, .5f, .5f, 1.f
  };

  light_transform = rig_entity_get_transform (light);
  cogl_matrix_get_inverse (light_transform, &light_view);

  cogl_matrix_init_from_array (light_mvp, bias);
  cogl_matrix_multiply (light_mvp, light_mvp, light_projection);
  cogl_matrix_multiply (light_mvp, light_mvp, &light_view);

  cogl_matrix_multiply (light_mvp, light_mvp, model_transform);
}

CoglPipeline *
get_entity_pipeline (RigData *data,
                     RigEntity *entity,
                     RigComponent *geometry,
                     Pass pass)
{
  CoglSnippet *snippet;
  CoglDepthState depth_state;
  RigMaterial *material =
    rig_entity_get_component (entity, RIG_COMPONENT_TYPE_MATERIAL);
  CoglPipeline *pipeline;
  CoglFramebuffer *shadow_fb;

  if (pass == PASS_COLOR)
    {
      pipeline = rig_entity_get_pipeline_cache (entity);
      if (pipeline)
        {
          cogl_object_ref (pipeline);
          goto FOUND;
        }
    }
  else if (pass == PASS_DOF_DEPTH || pass == PASS_SHADOW)
    {
      if (!data->dof_pipeline_template)
        {
          CoglPipeline *pipeline;
          CoglDepthState depth_state;
          CoglSnippet *snippet;

          pipeline = cogl_pipeline_new (data->ctx->cogl_context);

          cogl_pipeline_set_color_mask (pipeline, COGL_COLOR_MASK_ALPHA);

          cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

          cogl_depth_state_init (&depth_state);
          cogl_depth_state_set_test_enabled (&depth_state, TRUE);
          cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

          snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

              /* definitions */
              "uniform float dof_focal_distance;\n"
              "uniform float dof_depth_of_field;\n"

              "varying float dof_blur;\n",
              //"varying vec4 world_pos;\n",

              /* compute the amount of bluriness we want */
              "vec4 world_pos = cogl_modelview_matrix * cogl_position_in;\n"
              //"world_pos = cogl_modelview_matrix * cogl_position_in;\n"
              "dof_blur = 1.0 - clamp (abs (world_pos.z - dof_focal_distance) /\n"
              "                  dof_depth_of_field, 0.0, 1.0);\n"
          );

          cogl_pipeline_add_snippet (pipeline, snippet);
          cogl_object_unref (snippet);

          /* This was used to debug the focal distance and bluriness amount in the DoF
           * effect: */
#if 0
          cogl_pipeline_set_color_mask (pipeline, COGL_COLOR_MASK_ALL);
          snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
              "varying vec4 world_pos;\n"
              "varying float dof_blur;",

             "cogl_color_out = vec4(dof_blur,0,0,1);\n"
             //"cogl_color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
             //"if (world_pos.z < -30.0) cogl_color_out = vec4(0,1,0,1);\n"
             //"if (abs (world_pos.z + 30.f) < 0.1) cogl_color_out = vec4(0,1,0,1);\n"
             "cogl_color_out.a = dof_blur;\n"
             //"cogl_color_out.a = 1.0;\n"
          );

          cogl_pipeline_add_snippet (pipeline, snippet);
          cogl_object_unref (snippet);
#endif

          data->dof_pipeline_template = pipeline;
        }

      if (rig_object_get_type (geometry) == &rig_diamond_type)
        {
          if (!data->dof_diamond_pipeline)
            {
              CoglPipeline *dof_diamond_pipeline =
                cogl_pipeline_copy (data->dof_pipeline_template);
              CoglSnippet *snippet;

              rig_diamond_apply_mask (RIG_DIAMOND (geometry), dof_diamond_pipeline);

              snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                          /* declarations */
                                          "varying float dof_blur;",

                                          /* post */
                                          "if (cogl_color_out.a <= 0.0)\n"
                                          "  discard;\n"
                                          "\n"
                                          "cogl_color_out.a = dof_blur;\n");

              cogl_pipeline_add_snippet (dof_diamond_pipeline, snippet);
              cogl_object_unref (snippet);

              set_focal_parameters (dof_diamond_pipeline, 30.f, 3.0f);

              data->dof_diamond_pipeline = dof_diamond_pipeline;
            }

          return cogl_object_ref (data->dof_diamond_pipeline);
        }
      else
        {
          if (!data->dof_pipeline)
            {
              CoglPipeline *dof_pipeline =
                cogl_pipeline_copy (data->dof_pipeline_template);
              CoglSnippet *snippet;

              /* store the bluriness in the alpha channel */
              snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                  "varying float dof_blur;",

                  "cogl_color_out.a = dof_blur;\n"
              );
              cogl_pipeline_add_snippet (dof_pipeline, snippet);
              cogl_object_unref (snippet);

              set_focal_parameters (dof_pipeline, 30.f, 3.0f);

              data->dof_pipeline = dof_pipeline;
            }

          return cogl_object_ref (data->dof_pipeline);
        }
    }

  pipeline = cogl_pipeline_new (data->ctx->cogl_context);

#if 0
  /* NB: Our texture colours aren't premultiplied */
  cogl_pipeline_set_blend (pipeline,
                           "RGB = ADD(SRC_COLOR*(SRC_COLOR[A]), DST_COLOR*(1-SRC_COLOR[A]))"
                           "A   = ADD(SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))",
                           NULL);
#endif

#if 0
  if (rig_object_get_type (geometry) == &rig_diamond_type)
    rig_geometry_component_update_pipeline (geometry, pipeline);

  for (l = data->lights; l; l = l->next)
    light_update_pipeline (l->data, pipeline);

  pipeline = cogl_pipeline_new (rig_cogl_context);
#endif

  cogl_pipeline_set_color4f (pipeline, 0.8f, 0.8f, 0.8f, 1.f);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  /* Vertex shader setup for lighting */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat3 normal_matrix;\n"
      "varying vec3 normal_direction, eye_direction;\n",

      /* post */
      "normal_direction = normalize(normal_matrix * cogl_normal_in);\n"
      //"normal_direction = cogl_normal_in;\n"
      "eye_direction    = -vec3(cogl_modelview_matrix * cogl_position_in);\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* Vertex shader setup for shadow mapping */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat4 light_shadow_matrix;\n"
      "varying vec4 shadow_coords;\n",

      /* post */
      "shadow_coords = light_shadow_matrix * cogl_position_in;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* and fragment shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* definitions */
      //"varying vec3 normal_direction;\n",
      "varying vec3 normal_direction, eye_direction;\n",
      /* post */
      "");
  //cogl_snippet_set_pre (snippet, "cogl_color_out = cogl_color_in;\n");

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* definitions */
      "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
      "uniform vec3 light0_direction_norm;\n",

      /* post */
      "vec4 final_color;\n"

      "vec3 L = light0_direction_norm;\n"
      "vec3 N = normalize(normal_direction);\n"

      "if (cogl_color_out.a <= 0.0)\n"
      "  discard;\n"

      "final_color = light0_ambient * cogl_color_out;\n"
      "float lambert = dot(N, L);\n"
      //"float lambert = 1.0;\n"

      "if (lambert > 0.0)\n"
      "{\n"
      "  final_color += cogl_color_out * light0_diffuse * lambert;\n"
      //"  final_color +=  vec4(1.0, 0.0, 0.0, 1.0) * light0_diffuse * lambert;\n"

      "  vec3 E = normalize(eye_direction);\n"
      "  vec3 R = reflect (-L, N);\n"
      "  float specular = pow (max(dot(R, E), 0.0),\n"
      "                        2.);\n"
      "  final_color += light0_specular * vec4(.6, .6, .6, 1.0) * specular;\n"
      "}\n"

      "cogl_color_out = final_color;\n"
      //"cogl_color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);


  /* Hook the shadow map sampling */

  cogl_pipeline_set_layer_texture (pipeline, 7, data->shadow_map);
  /* For debugging the shadow mapping... */
  //cogl_pipeline_set_layer_texture (pipeline, 7, data->shadow_color);
  //cogl_pipeline_set_layer_texture (pipeline, 7, data->gradient);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              /* declarations */
                              "varying vec4 shadow_coords;\n",
                              /* post */
                              "");

  cogl_snippet_set_replace (snippet,
                            "cogl_texel = texture2D(cogl_sampler7, cogl_tex_coord.st);\n");

  cogl_pipeline_add_layer_snippet (pipeline, 7, snippet);
  cogl_object_unref (snippet);

  /* Handle shadow mapping */

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* declarations */
      "",

      /* post */
      "cogl_texel7 =  cogl_texture_lookup7 (cogl_sampler7, shadow_coords);\n"
      "float distance_from_light = cogl_texel7.z + 0.0005;\n"
      "float shadow = 1.0;\n"
      "if (distance_from_light < shadow_coords.z)\n"
      "  shadow = 0.5;\n"

      "cogl_color_out = shadow * cogl_color_out;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

#if 1
  {
    RigLight *light = rig_entity_get_component (data->light, RIG_COMPONENT_TYPE_LIGHT);
    rig_light_set_uniforms (light, pipeline);
  }
#endif

#if 1
  if (rig_object_get_type (geometry) == &rig_diamond_type)
    {
      //pipeline = cogl_pipeline_new (data->ctx->cogl_context);

      //cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

      rig_diamond_apply_mask (RIG_DIAMOND (geometry), pipeline);

      //cogl_pipeline_set_color4f (pipeline, 1, 0, 0, 1);

#if 1
      if (material)
        {
          RigAsset *asset = rig_material_get_asset (material);
          CoglTexture *texture;
          if (asset)
            texture = rig_asset_get_texture (asset);
          else
            texture = NULL;

          if (texture)
            cogl_pipeline_set_layer_texture (pipeline, 1, texture);
        }
#endif
    }
#endif

  rig_entity_set_pipeline_cache (entity, pipeline);

FOUND:

  /* FIXME: there's lots to optimize about this! */
#if 1
  shadow_fb = COGL_FRAMEBUFFER (data->shadow_fb);

  /* update uniforms in pipelines */
  {
    CoglMatrix light_shadow_matrix, light_projection;
    CoglMatrix model_transform;
    const float *light_matrix;
    int location;

    cogl_framebuffer_get_projection_matrix (shadow_fb, &light_projection);

    /* XXX: This is pretty bad that we are having to do this. It would
     * be nicer if cogl exposed matrix-stacks publicly so we could
     * maintain the entity model_matrix incrementally as we traverse
     * the scenegraph. */
    rig_graphable_get_transform (entity, &model_transform);

    get_light_modelviewprojection (&model_transform,
                                   data->light,
                                   &light_projection,
                                   &light_shadow_matrix);

    light_matrix = cogl_matrix_get_array (&light_shadow_matrix);

    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      light_matrix);
  }
#endif

  return pipeline;
}

static void
draw_entity_camera_frustum (RigData *data,
                            RigEntity *entity,
                            CoglFramebuffer *fb)
{
  RigCamera *camera =
    rig_entity_get_component (entity, RIG_COMPONENT_TYPE_CAMERA);
  CoglPrimitive *primitive = rig_camera_create_frustum_primitive (camera);
  CoglPipeline *pipeline = cogl_pipeline_new (rig_cogl_context);
  CoglDepthState depth_state;

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  cogl_framebuffer_draw_primitive (fb, pipeline, primitive);

  cogl_object_unref (primitive);
  cogl_object_unref (pipeline);
}

static RigTraverseVisitFlags
_rig_entitygraph_pre_paint_cb (RigObject *object,
                               int depth,
                               void *user_data)
{
  PaintContext *paint_ctx = user_data;
  RigPaintContext *rig_paint_ctx = user_data;
  RigCamera *camera = rig_paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      const CoglMatrix *matrix = rig_transformable_get_matrix (object);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rig_object_get_type (object) == &rig_entity_type)
    {
      RigEntity *entity = RIG_ENTITY (object);
      RigComponent *geometry;
      CoglPipeline *pipeline;
      CoglPrimitive *primitive;
      CoglMatrix modelview_matrix;
      float normal_matrix[9];

      if (!rig_entity_get_visible (entity))
        return RIG_TRAVERSE_VISIT_CONTINUE;

      geometry =
        rig_entity_get_component (object, RIG_COMPONENT_TYPE_GEOMETRY);
      if (!geometry)
        {
          if (!paint_ctx->data->play_mode &&
              object == paint_ctx->data->light)
            draw_entity_camera_frustum (paint_ctx->data, object, fb);
          return RIG_TRAVERSE_VISIT_CONTINUE;
        }

#if 1
      pipeline = get_entity_pipeline (paint_ctx->data,
                                      object,
                                      geometry,
                                      paint_ctx->pass);
#endif

      primitive = rig_primable_get_primitive (geometry);

#if 1
      cogl_framebuffer_get_modelview_matrix (fb, &modelview_matrix);
      get_normal_matrix (&modelview_matrix, normal_matrix);

      {
        int location = cogl_pipeline_get_uniform_location (pipeline, "normal_matrix");
        cogl_pipeline_set_uniform_matrix (pipeline,
                                          location,
                                          3, /* dimensions */
                                          1, /* count */
                                          FALSE, /* don't transpose again */
                                          normal_matrix);
      }
#endif

      cogl_framebuffer_draw_primitive (fb,
                                       pipeline,
                                       primitive);

      /* FIXME: cache the pipeline with the entity */
      cogl_object_unref (pipeline);

#if 0
      geometry = rig_entity_get_component (object, RIG_COMPONENT_TYPE_GEOMETRY);
      material = rig_entity_get_component (object, RIG_COMPONENT_TYPE_MATERIAL);
      if (geometry && material)
        {
          if (rig_object_get_type (geometry) == &rig_diamond_type)
            {
              PaintContext *paint_ctx = rig_paint_ctx;
              RigData *data = paint_ctx->data;
              RigDiamondSlice *slice = rig_diamond_get_slice (geometry);
              CoglPipeline *template = rig_diamond_slice_get_pipeline_template (slice);
              CoglPipeline *material_pipeline = rig_material_get_pipeline (material);
              CoglPipeline *pipeline = cogl_pipeline_copy (template);
              //CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);

              /* FIXME: we should be combining the material and
               * diamond slice state together before now! */
              cogl_pipeline_set_layer_texture (pipeline, 1,
                                               cogl_pipeline_get_layer_texture (material_pipeline, 0));

              cogl_framebuffer_draw_primitive (fb,
                                               pipeline,
                                               slice->primitive);

              cogl_object_unref (pipeline);
            }
        }
#endif
      return RIG_TRAVERSE_VISIT_CONTINUE;
    }

  /* XXX:
   * How can we maintain state between the pre and post stages?  Is it
   * ok to just "sub-class" the paint context and maintain a stack of
   * state that needs to be shared with the post paint code.
   */

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static RigTraverseVisitFlags
_rig_entitygraph_post_paint_cb (RigObject *object,
                                int depth,
                                void *user_data)
{
  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      RigPaintContext *rig_paint_ctx = user_data;
      CoglFramebuffer *fb = rig_camera_get_framebuffer (rig_paint_ctx->camera);
      cogl_framebuffer_pop_matrix (fb);
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static void
paint_scene (PaintContext *paint_ctx)
{
  RigPaintContext *rig_paint_ctx = &paint_ctx->_parent;
  RigData *data = paint_ctx->data;
  CoglContext *ctx = data->ctx->cogl_context;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (rig_paint_ctx->camera);

  if (paint_ctx->pass == PASS_COLOR)
    {
      CoglPipeline *pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_color4f (pipeline, 0, 0, 0, 1.0);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0, DEVICE_WIDTH, DEVICE_HEIGHT);
                                       //0, 0, data->pane_width, data->pane_height);
      cogl_object_unref (pipeline);
    }

  rig_graphable_traverse (data->scene,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_entitygraph_pre_paint_cb,
                          _rig_entitygraph_post_paint_cb,
                          paint_ctx);

}

#if 1
static void
paint_camera_entity (RigEntity *camera, PaintContext *paint_ctx)
{
  RigPaintContext *rig_paint_ctx = &paint_ctx->_parent;
  RigCamera *save_camera = rig_paint_ctx->camera;
  RigCamera *camera_component =
    rig_entity_get_component (camera, RIG_COMPONENT_TYPE_CAMERA);
  RigData *data = paint_ctx->data;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera_component);
  //CoglFramebuffer *shadow_fb;

  rig_paint_ctx->camera = camera_component;

  if (rig_entity_get_component (camera, RIG_COMPONENT_TYPE_LIGHT))
    paint_ctx->pass = PASS_SHADOW;
  else
    paint_ctx->pass = PASS_COLOR;

  camera_update_view (data, camera, paint_ctx->pass);

  if (paint_ctx->pass != PASS_SHADOW &&
      data->enable_dof)
    {
      const float *viewport = rig_camera_get_viewport (camera_component);
      int width = viewport[2];
      int height = viewport[3];
      int save_viewport_x = viewport[0];
      int save_viewport_y = viewport[1];
      Pass save_pass = paint_ctx->pass;
      CoglFramebuffer *pass_fb;

      rig_camera_set_viewport (camera_component, 0, 0, width, height);

      rig_dof_effect_set_framebuffer_size (data->dof, width, height);

      pass_fb = rig_dof_effect_get_depth_pass_fb (data->dof);
      rig_camera_set_framebuffer (camera_component, pass_fb);

      rig_camera_flush (camera_component);
      cogl_framebuffer_clear4f (pass_fb,
                                COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                                1, 1, 1, 1);

      paint_ctx->pass = PASS_DOF_DEPTH;
      paint_scene (paint_ctx);
      paint_ctx->pass = save_pass;

      rig_camera_end_frame (camera_component);

      pass_fb = rig_dof_effect_get_color_pass_fb (data->dof);
      rig_camera_set_framebuffer (camera_component, pass_fb);

      rig_camera_flush (camera_component);
      cogl_framebuffer_clear4f (pass_fb,
                                COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                                0.22, 0.22, 0.22, 1);

      paint_ctx->pass = PASS_COLOR;
      paint_scene (paint_ctx);
      paint_ctx->pass = save_pass;

      rig_camera_end_frame (camera_component);

      rig_camera_set_framebuffer (camera_component, fb);
      rig_camera_set_clear (camera_component, FALSE);

      rig_camera_flush (camera_component);

      rig_camera_end_frame (camera_component);

      rig_camera_set_viewport (camera_component,
                               save_viewport_x,
                               save_viewport_y,
                               width, height);
      rig_paint_ctx->camera = save_camera;
      rig_camera_flush (save_camera);
      rig_dof_effect_draw_rectangle (data->dof,
                                     rig_camera_get_framebuffer (save_camera),
                                     data->main_x,
                                     data->main_y,
                                     data->main_x + data->main_width,
                                     data->main_y + data->main_height);
      rig_camera_end_frame (save_camera);
    }
  else
    {
      rig_camera_set_framebuffer (camera_component, fb);

      rig_camera_flush (camera_component);

      paint_scene (paint_ctx);

      rig_camera_end_frame (camera_component);
    }

  if (paint_ctx->pass == PASS_COLOR)
    {
      rig_camera_flush (camera_component);

      /* Use this to visualize the depth-of-field alpha buffer... */
#if 0
      CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
      cogl_pipeline_set_layer_texture (pipeline, 0, data->dof.depth_pass);
      cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0,
                                       200, 200);
#endif

      /* Use this to visualize the shadow_map */
#if 0
      CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
      cogl_pipeline_set_layer_texture (pipeline, 0, data->shadow_map);
      //cogl_pipeline_set_layer_texture (pipeline, 0, data->shadow_color);
      cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0,
                                       200, 200);
#endif

      if (data->debug_pick_ray && data->picking_ray)
      //if (data->picking_ray)
        {
          cogl_framebuffer_draw_primitive (fb,
                                           data->picking_ray_color,
                                           data->picking_ray);
        }

      if (!_rig_in_device_mode)
        {
          draw_jittered_primitive4f (data, fb, data->grid_prim, 0.5, 0.5, 0.5);

          if (data->selected_entity)
            {
              rig_tool_update (data->tool, data->selected_entity);
              rig_tool_draw (data->tool, fb);
            }
        }

      rig_camera_end_frame (camera_component);
    }

  rig_paint_ctx->camera = save_camera;
}
#endif

typedef struct
{
  CoglPipeline *pipeline;
  RigEntity *entity;
  PaintContext *paint_ctx;

  float viewport_x, viewport_y;
  float viewport_t_scale;
  float viewport_y_scale;
  float viewport_t_offset;
  float viewport_y_offset;
} PaintTimelineData;

static void
paint_timeline_path_cb (RigProperty *property,
                        RigPath *path,
                        const RigBoxed *constant_value,
                        void *user_data)
{
  PaintTimelineData *paint_data = user_data;
  RigPaintContext *rig_paint_ctx = &paint_data->paint_ctx->_parent;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (rig_paint_ctx->camera);
  RigData *data = paint_data->paint_ctx->data;
  RigContext *ctx = data->ctx;
  CoglPrimitive *prim;
  GArray *points;
  GList *l;
  GList *next;
  float red, green, blue;

  if (path == NULL ||
      property->object != paint_data->entity ||
      property->spec->type != RIG_PROPERTY_TYPE_FLOAT)
    return;

  if (strcmp (property->spec->name, "x") == 0)
    red = 1.0, green = 0.0, blue = 0.0;
  else if (strcmp (property->spec->name, "y") == 0)
    red = 0.0, green = 1.0, blue = 0.0;
  else if (strcmp (property->spec->name, "z") == 0)
    red = 0.0, green = 0.0, blue = 1.0;
  else
    return;

  points = g_array_new (FALSE, FALSE, sizeof (CoglVertexP2));

  for (l = path->nodes.head; l; l = next)
    {
      RigNodeFloat *f_node = l->data;
      CoglVertexP2 p;

      next = l->next;

      /* FIXME: This clipping wasn't working... */
#if 0
      /* Only draw the nodes within the current viewport */
      if (next)
        {
          float max_t = (viewport_t_offset +
                         data->timeline_vp->width * viewport_t_scale);
          if (next->t < viewport_t_offset)
            continue;
          if (node->t > max_t && next->t > max_t)
            break;
        }
#endif

#define HANDLE_HALF_SIZE 4
      p.x = (paint_data->viewport_x +
             (f_node->t - paint_data->viewport_t_offset) *
             paint_data->viewport_t_scale);

      cogl_pipeline_set_color4f (paint_data->pipeline, red, green, blue, 1);

      p.y = (paint_data->viewport_y +
             (f_node->value - paint_data->viewport_y_offset) *
             paint_data->viewport_y_scale);
#if 1
#if 1
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_translate (fb, p.x, p.y, 0);
      cogl_framebuffer_scale (fb, HANDLE_HALF_SIZE, HANDLE_HALF_SIZE, 0);
      cogl_framebuffer_draw_attributes (fb,
                                        paint_data->pipeline,
                                        COGL_VERTICES_MODE_LINE_STRIP,
                                        1,
                                        data->circle_node_n_verts - 1,
                                        &data->circle_node_attribute,
                                        1);
      cogl_framebuffer_pop_matrix (fb);
#else
#if 0
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       p.x - HANDLE_HALF_SIZE,
                                       p.y - HANDLE_HALF_SIZE,
                                       p.x + HANDLE_HALF_SIZE,
                                       p.y + HANDLE_HALF_SIZE);
#endif
#endif
#endif
      g_array_append_val (points, p);
    }

  prim = cogl_primitive_new_p2 (ctx->cogl_context,
                                COGL_VERTICES_MODE_LINE_STRIP,
                                points->len, (CoglVertexP2 *)points->data);
  draw_jittered_primitive4f (data, fb, prim, red, green, blue);
  cogl_object_unref (prim);

  g_array_free (points, TRUE);
}

static void
paint_timeline_camera (PaintContext *paint_ctx)
{
  RigPaintContext *rig_paint_ctx = &paint_ctx->_parent;
  RigData *data = paint_ctx->data;
  RigContext *ctx = data->ctx;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (rig_paint_ctx->camera);

  //cogl_framebuffer_push_matrix (fb);
  //cogl_framebuffer_transform (fb, rig_transformable_get_matrix (camera));

  if (data->selected_entity)
    {
      PaintTimelineData paint_data;

      paint_data.paint_ctx = paint_ctx;

      paint_data.entity = data->selected_entity;
      paint_data.viewport_x = 0;
      paint_data.viewport_y = 0;

      paint_data.viewport_t_scale =
        rig_ui_viewport_get_doc_scale_x (data->timeline_vp) *
        data->timeline_scale;

      paint_data.viewport_y_scale =
        rig_ui_viewport_get_doc_scale_y (data->timeline_vp) *
        data->timeline_scale;

      paint_data.viewport_t_offset =
        rig_ui_viewport_get_doc_x (data->timeline_vp);
      paint_data.viewport_y_offset =
        rig_ui_viewport_get_doc_y (data->timeline_vp);
      paint_data.pipeline = cogl_pipeline_new (data->ctx->cogl_context);

      rig_transition_foreach_property (data->selected_transition,
                                       paint_timeline_path_cb,
                                       &paint_data);

      cogl_object_unref (paint_data.pipeline);

      {
        CoglPrimitive *prim;
        double progress;
        float progress_x;
        float progress_line[4];

        progress = rig_timeline_get_progress (data->timeline);

        progress_x =
          -paint_data.viewport_t_offset *
          paint_data.viewport_t_scale +
          data->timeline_width *
          data->timeline_scale * progress;

        progress_line[0] = progress_x;
        progress_line[1] = 0;
        progress_line[2] = progress_x;
        progress_line[3] = data->timeline_height;

        prim = cogl_primitive_new_p2 (ctx->cogl_context,
                                      COGL_VERTICES_MODE_LINE_STRIP,
                                      2,
                                      (CoglVertexP2 *)progress_line);
        draw_jittered_primitive4f (data, fb, prim, 0, 1, 0);
        cogl_object_unref (prim);
      }
    }

  //cogl_framebuffer_pop_matrix (fb);
}

static RigTraverseVisitFlags
_rig_scenegraph_pre_paint_cb (RigObject *object,
                              int depth,
                              void *user_data)
{
  RigPaintContext *rig_paint_ctx = user_data;
  RigCamera *camera = rig_paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);
  RigPaintableVTable *vtable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_PAINTABLE);

#if 0
  if (rig_object_get_type (object) == &rig_camera_type)
    {
      g_print ("%*sCamera = %p\n", depth, "", object);
      rig_camera_flush (RIG_CAMERA (object));
      return RIG_TRAVERSE_VISIT_CONTINUE;
    }
  else
#endif

  if (rig_object_get_type (object) == &rig_ui_viewport_type)
    {
      RigUIViewport *ui_viewport = RIG_UI_VIEWPORT (object);
#if 0
      g_print ("%*sPushing clip = %f %f\n",
               depth, "",
               rig_ui_viewport_get_width (ui_viewport),
               rig_ui_viewport_get_height (ui_viewport));
#endif
      cogl_framebuffer_push_rectangle_clip (fb,
                                            0, 0,
                                            rig_ui_viewport_get_width (ui_viewport),
                                            rig_ui_viewport_get_height (ui_viewport));
    }

  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      //g_print ("%*sTransformable = %p\n", depth, "", object);
      const CoglMatrix *matrix = rig_transformable_get_matrix (object);
      //cogl_debug_matrix_print (matrix);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rig_object_is (object, RIG_INTERFACE_ID_PAINTABLE))
    vtable->paint (object, rig_paint_ctx);

  /* XXX:
   * How can we maintain state between the pre and post stages?  Is it
   * ok to just "sub-class" the paint context and maintain a stack of
   * state that needs to be shared with the post paint code.
   */

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static RigTraverseVisitFlags
_rig_scenegraph_post_paint_cb (RigObject *object,
                               int depth,
                               void *user_data)
{
  RigPaintContext *rig_paint_ctx = user_data;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (rig_paint_ctx->camera);

#if 0
  if (rig_object_get_type (object) == &rig_camera_type)
    {
      rig_camera_end_frame (RIG_CAMERA (object));
      return RIG_TRAVERSE_VISIT_CONTINUE;
    }
  else
#endif

  if (rig_object_get_type (object) == &rig_ui_viewport_type)
    {
      cogl_framebuffer_pop_clip (fb);
    }

  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      cogl_framebuffer_pop_matrix (fb);
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static CoglBool
paint (RigShell *shell, void *user_data)
{
  RigData *data = user_data;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (data->camera);
  PaintContext paint_ctx;
  RigPaintContext *rig_paint_ctx = &paint_ctx._parent;

  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                            0.22, 0.22, 0.22, 1);

  paint_ctx.data = data;
  paint_ctx.pass = PASS_COLOR;

  rig_paint_ctx->camera = data->camera;

  if (!_rig_in_device_mode)
    {
      rig_camera_flush (data->camera);
      rig_graphable_traverse (data->root,
                              RIG_TRAVERSE_DEPTH_FIRST,
                              _rig_scenegraph_pre_paint_cb,
                              _rig_scenegraph_post_paint_cb,
                              &paint_ctx);
      /* FIXME: this should be moved to the end of this function but we
       * currently get warnings about unbalanced _flush()/_end_frame()
       * pairs. */
      rig_camera_end_frame (data->camera);
    }

  rig_paint_ctx->camera = data->camera;
  paint_camera_entity (data->light, &paint_ctx);

  rig_paint_ctx->camera = data->camera;
  paint_camera_entity (data->editor_camera, &paint_ctx);

  if (!_rig_in_device_mode)
    {
      rig_paint_ctx->camera = data->timeline_camera;
      rig_camera_flush (rig_paint_ctx->camera);
      paint_timeline_camera (&paint_ctx);
      rig_camera_end_frame (rig_paint_ctx->camera);
    }

#if 0
  rig_paint_ctx->camera = data->editor_camera;

  rig_graphable_traverse (data->editor_camera,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_scenegraph_pre_paint_cb,
                          _rig_scenegraph_post_paint_cb,
                          &paint_ctx);
#endif

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));

  return FALSE;
}

static void
update_transition_progress_cb (RigProperty *target_property,
                               RigProperty *source_property,
                               void *user_data)
{
  RigData *data = user_data;
  double elapsed = rig_timeline_get_elapsed (data->timeline);
  RigTransition *transition = target_property->object;

  rig_transition_set_progress (transition, elapsed);
}

RigTransition *
rig_create_transition (RigData *data,
                       uint32_t id)
{
  RigTransition *transition = rig_transition_new (data->ctx, id);

  /* FIXME: this should probably only update the progress for the
   * current transition */
  rig_property_set_binding (&transition->props[RIG_TRANSITION_PROP_PROGRESS],
                            update_transition_progress_cb,
                            data,
                            data->timeline_elapsed,
                            NULL);

  return transition;
}

static void
unproject_window_coord (RigCamera *camera,
                        const CoglMatrix *modelview,
                        const CoglMatrix *inverse_modelview,
                        float object_coord_z,
                        float *x,
                        float *y)
{
  const CoglMatrix *projection = rig_camera_get_projection (camera);
  const CoglMatrix *inverse_projection =
    rig_camera_get_inverse_projection (camera);
  //float z;
  float ndc_x, ndc_y, ndc_z, ndc_w;
  float eye_x, eye_y, eye_z, eye_w;
  const float *viewport = rig_camera_get_viewport (camera);

  /* Convert object coord z into NDC z */
  {
    float tmp_x, tmp_y, tmp_z;
    const CoglMatrix *m = modelview;
    float z, w;

    tmp_x = m->xz * object_coord_z + m->xw;
    tmp_y = m->yz * object_coord_z + m->yw;
    tmp_z = m->zz * object_coord_z + m->zw;

    m = projection;
    z = m->zx * tmp_x + m->zy * tmp_y + m->zz * tmp_z + m->zw;
    w = m->wx * tmp_x + m->wy * tmp_y + m->wz * tmp_z + m->ww;

    ndc_z = z / w;
  }

  /* Undo the Viewport transform, putting us in Normalized Device Coords */
  ndc_x = (*x - viewport[0]) * 2.0f / viewport[2] - 1.0f;
  ndc_y = ((viewport[3] - 1 + viewport[1] - *y) * 2.0f / viewport[3] - 1.0f);

  /* Undo the Projection, putting us in Eye Coords. */
  ndc_w = 1;
  cogl_matrix_transform_point (inverse_projection,
                               &ndc_x, &ndc_y, &ndc_z, &ndc_w);
  eye_x = ndc_x / ndc_w;
  eye_y = ndc_y / ndc_w;
  eye_z = ndc_z / ndc_w;
  eye_w = 1;

  /* Undo the Modelview transform, putting us in Object Coords */
  cogl_matrix_transform_point (inverse_modelview,
                               &eye_x,
                               &eye_y,
                               &eye_z,
                               &eye_w);

  *x = eye_x;
  *y = eye_y;
  //*z = eye_z;
}

typedef void (*EntityTranslateCallback)(RigEntity *entity,
                                        float start[3],
                                        float rel[3],
                                        void *user_data);

typedef void (*EntityTranslateDoneCallback)(RigEntity *entity,
                                            float start[3],
                                            float rel[3],
                                            void *user_data);

typedef struct _EntityTranslateGrabClosure
{
  RigData *data;

  /* pointer position at start of grab */
  float grab_x;
  float grab_y;

  /* entity position at start of grab */
  float entity_grab_pos[3];
  RigEntity *entity;

  float x_vec[3];
  float y_vec[3];

  EntityTranslateCallback entity_translate_cb;
  EntityTranslateDoneCallback entity_translate_done_cb;
  void *user_data;
} EntityTranslateGrabClosure;

static RigInputEventStatus
entity_translate_grab_input_cb (RigInputEvent *event,
                                void *user_data)

{
  EntityTranslateGrabClosure *closure = user_data;
  RigEntity *entity = closure->entity;
  RigData *data = closure->data;

  g_print ("Entity grab event\n");

  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      float x = rig_motion_event_get_x (event);
      float y = rig_motion_event_get_y (event);
      float move_x, move_y;
      float rel[3];
      float *x_vec = closure->x_vec;
      float *y_vec = closure->y_vec;

      move_x = x - closure->grab_x;
      move_y = y - closure->grab_y;

      rel[0] = x_vec[0] * move_x;
      rel[1] = x_vec[1] * move_x;
      rel[2] = x_vec[2] * move_x;

      rel[0] += y_vec[0] * move_y;
      rel[1] += y_vec[1] * move_y;
      rel[2] += y_vec[2] * move_y;

      if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_UP)
        {
          if (closure->entity_translate_done_cb)
            closure->entity_translate_done_cb (entity,
                                               closure->entity_grab_pos,
                                               rel,
                                               closure->user_data);

          rig_shell_ungrab_input (data->ctx->shell,
                                  entity_translate_grab_input_cb,
                                  user_data);

          g_slice_free (EntityTranslateGrabClosure, user_data);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_MOVE)
        {
          closure->entity_translate_cb (entity,
                                        closure->entity_grab_pos,
                                        rel,
                                        closure->user_data);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
inspector_property_changed_cb (RigProperty *target_property,
                               RigProperty *source_property,
                               void *user_data)
{
  RigData *data = user_data;

  undo_journal_copy_property_and_log (data->undo_journal,
                                      TRUE, /* mergable */
                                      data->selected_entity,
                                      target_property,
                                      source_property);
}

typedef struct _AddComponentState
{
  RigData *data;
  int y_offset;
} AddComponentState;

static void
add_component_inspector_cb (RigComponent *component,
                            void *user_data)
{
  AddComponentState *state = user_data;
  RigData *data = state->data;
  RigInspector *inspector = rig_inspector_new (data->ctx,
                                               component,
                                               inspector_property_changed_cb,
                                               data);
  RigTransform *transform = rig_transform_new (data->ctx, inspector, NULL);
  float width, height;
  RigObject *doc_node;

  rig_ref_countable_unref (inspector);

  rig_sizable_get_preferred_width (inspector,
                                   -1, /* for height */
                                   NULL, /* min_width */
                                   &width);
  rig_sizable_get_preferred_height (inspector,
                                    -1, /* for width */
                                    NULL, /* min_height */
                                    &height);
  rig_sizable_set_size (inspector, width, height);

  doc_node = rig_ui_viewport_get_doc_node (data->tool_vp);

  rig_transform_translate (transform, 0, state->y_offset, 0);
  state->y_offset += height;
  rig_graphable_add_child (doc_node, transform);
  rig_ref_countable_unref (transform);

  data->component_inspectors =
    g_list_prepend (data->component_inspectors, inspector);
}

static void
update_inspector (RigData *data)
{
  RigObject *doc_node;

  if (data->inspector)
    {
      rig_graphable_remove_child (data->inspector);
      data->inspector = NULL;

      if (data->component_inspectors)
        {
          GList *l;

          for (l = data->component_inspectors; l; l = l->next)
            rig_graphable_remove_child (l->data);
          g_list_free (data->component_inspectors);
          data->component_inspectors = NULL;
        }
    }

  if (data->selected_entity)
    {
      float width, height;
      AddComponentState component_add_state;

      data->inspector = rig_inspector_new (data->ctx,
                                           data->selected_entity,
                                           inspector_property_changed_cb,
                                           data);

      rig_sizable_get_preferred_width (data->inspector,
                                       -1, /* for height */
                                       NULL, /* min_width */
                                       &width);
      rig_sizable_get_preferred_height (data->inspector,
                                        -1, /* for width */
                                        NULL, /* min_height */
                                        &height);
      rig_sizable_set_size (data->inspector, width, height);

      doc_node = rig_ui_viewport_get_doc_node (data->tool_vp);
      rig_graphable_add_child (doc_node, data->inspector);
      rig_ref_countable_unref (data->inspector);

      component_add_state.data = data;
      component_add_state.y_offset = height + 10;
      rig_entity_foreach_component (data->selected_entity,
                                    add_component_inspector_cb,
                                    &component_add_state);
    }
}

static RigInputEventStatus
timeline_grab_input_cb (RigInputEvent *event, void *user_data)
{
  RigData *data = user_data;

  if (rig_input_event_get_type (event) != RIG_INPUT_EVENT_TYPE_MOTION)
    return RIG_INPUT_EVENT_STATUS_UNHANDLED;

  if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_MOVE)
    {
      RigButtonState state = rig_motion_event_get_button_state (event);
      float x = rig_motion_event_get_x (event);
      float y = rig_motion_event_get_y (event);

      if (state & RIG_BUTTON_STATE_1)
        {
          RigCamera *camera = data->timeline_camera;
          const CoglMatrix *view = rig_camera_get_view_transform (camera);
          CoglMatrix inverse_view;
          float progress;

          if (!cogl_matrix_get_inverse (view, &inverse_view))
            g_error ("Failed to get inverse transform");

          unproject_window_coord (camera,
                                  view,
                                  &inverse_view,
                                  0, /* z in entity coordinates */
                                  &x, &y);

          progress = x / data->timeline_width;
          //g_print ("x = %f, y = %f progress=%f\n", x, y, progress);

          rig_timeline_set_progress (data->timeline, progress);
          rig_shell_queue_redraw (data->ctx->shell);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (state & RIG_BUTTON_STATE_2)
        {
          float dx = data->grab_x - x;
          float dy = data->grab_y - y;
          float t_scale =
            rig_ui_viewport_get_doc_scale_x (data->timeline_vp) *
            data->timeline_scale;

          float y_scale =
            rig_ui_viewport_get_doc_scale_y (data->timeline_vp) *
            data->timeline_scale;

          float inv_t_scale = 1.0 / t_scale;
          float inv_y_scale = 1.0 / y_scale;


          rig_ui_viewport_set_doc_x (data->timeline_vp,
                                     data->grab_timeline_vp_t + (dx * inv_t_scale));
          rig_ui_viewport_set_doc_y (data->timeline_vp,
                                     data->grab_timeline_vp_y + (dy * inv_y_scale));

          rig_shell_queue_redraw (data->ctx->shell);
        }
    }
  else if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_UP)
    {
      rig_shell_ungrab_input (data->ctx->shell,
                              timeline_grab_input_cb,
                              user_data);
      return RIG_INPUT_EVENT_STATUS_HANDLED;
    }
  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
timeline_input_cb (RigInputEvent *event,
                   void *user_data)
{
  RigData *data = user_data;

  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      data->key_focus_callback = timeline_input_cb;

      switch (rig_motion_event_get_action (event))
        {
        case RIG_MOTION_EVENT_ACTION_DOWN:
          {
            data->grab_x = rig_motion_event_get_x (event);
            data->grab_y = rig_motion_event_get_y (event);
            data->grab_timeline_vp_t = rig_ui_viewport_get_doc_x (data->timeline_vp);
            data->grab_timeline_vp_y = rig_ui_viewport_get_doc_y (data->timeline_vp);
            /* TODO: Add rig_shell_implicit_grab_input() that handles releasing
             * the grab for you */
            g_print ("timeline input grab\n");
            rig_shell_grab_input (data->ctx->shell,
                                  rig_input_event_get_camera (event),
                                  timeline_grab_input_cb, data);
            return RIG_INPUT_EVENT_STATUS_HANDLED;
          }
	default:
	  break;
        }
    }
  else if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_KEY &&
           rig_key_event_get_action (event) == RIG_KEY_EVENT_ACTION_UP)
    {
      switch (rig_key_event_get_keysym (event))
        {
        case RIG_KEY_equal:
          data->timeline_scale += 0.2;
          rig_shell_queue_redraw (data->ctx->shell);
          break;
        case RIG_KEY_minus:
          data->timeline_scale -= 0.2;
          rig_shell_queue_redraw (data->ctx->shell);
          break;
        case RIG_KEY_Home:
          data->timeline_scale = 1;
          rig_shell_queue_redraw (data->ctx->shell);
        }
      g_print ("Key press in timeline area\n");
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
timeline_region_input_cb (RigInputRegion *region,
                          RigInputEvent *event,
                          void *user_data)
{
  return timeline_input_cb (event, user_data);
}

static CoglPrimitive *
create_line_primitive (float a[3], float b[3])
{
  CoglVertexP3 data[2];
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[1];
  CoglPrimitive *primitive;

  data[0].x = a[0];
  data[0].y = a[1];
  data[0].z = a[2];
  data[1].x = b[0];
  data[1].y = b[1];
  data[1].z = b[2];

  attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
                                                2 * sizeof (CoglVertexP3),
                                                data);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (CoglVertexP3),
                                      offsetof (CoglVertexP3, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_LINES,
                                                  2, attributes, 1);

  cogl_object_unref (attribute_buffer);
  cogl_object_unref (attributes[0]);

  return primitive;
}

static void
transform_ray (CoglMatrix *transform,
               bool        inverse_transform,
               float       ray_origin[3],
               float       ray_direction[3])
{
  CoglMatrix inverse, normal_matrix, *m;

  m = transform;
  if (inverse_transform)
    {
      cogl_matrix_get_inverse (transform, &inverse);
      m = &inverse;
    }

  cogl_matrix_transform_points (m,
                                3, /* num components for input */
                                sizeof (float) * 3, /* input stride */
                                ray_origin,
                                sizeof (float) * 3, /* output stride */
                                ray_origin,
                                1 /* n_points */);

  cogl_matrix_get_inverse (m, &normal_matrix);
  cogl_matrix_transpose (&normal_matrix);

  rig_util_transform_normal (&normal_matrix,
                             &ray_direction[0],
                             &ray_direction[1],
                             &ray_direction[2]);
}

static CoglPrimitive *
create_picking_ray (RigData            *data,
                    CoglFramebuffer *fb,
                    float            ray_position[3],
                    float            ray_direction[3],
                    float            length)
{
  CoglPrimitive *line;
  float points[6];

  points[0] = ray_position[0];
  points[1] = ray_position[1];
  points[2] = ray_position[2];
  points[3] = ray_position[0] + length * ray_direction[0];
  points[4] = ray_position[1] + length * ray_direction[1];
  points[5] = ray_position[2] + length * ray_direction[2];

  line = create_line_primitive (points, points + 3);

  return line;
}

typedef struct _PickContext
{
  RigCamera *camera;
  CoglFramebuffer *fb;
  float *ray_origin;
  float *ray_direction;
  RigEntity *selected_entity;
  float selected_distance;
  int selected_index;
} PickContext;

static RigTraverseVisitFlags
_rig_entitygraph_pre_pick_cb (RigObject *object,
                              int depth,
                              void *user_data)
{
  PickContext *pick_ctx = user_data;
  CoglFramebuffer *fb = pick_ctx->fb;

  /* XXX: It could be nice if Cogl exposed matrix stacks directly, but for now
   * we just take advantage of an arbitrary framebuffer matrix stack so that
   * we can avoid repeated accumulating the transform of ancestors when
   * traversing between scenegraph nodes that have common ancestors.
   */
  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      const CoglMatrix *matrix = rig_transformable_get_matrix (object);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rig_object_get_type (object) == &rig_entity_type)
    {
      RigEntity *entity = object;
      RigComponent *geometry;
      uint8_t *vertex_data;
      int n_vertices;
      size_t stride;
      int index;
      float distance;
      bool hit;
      float transformed_ray_origin[3];
      float transformed_ray_direction[3];
      CoglMatrix transform;

      if (!rig_entity_get_visible (entity))
        return RIG_TRAVERSE_VISIT_CONTINUE;

      geometry = rig_entity_get_component (entity, RIG_COMPONENT_TYPE_GEOMETRY);

      /* Get a mesh we can pick against */
      if (!(geometry &&
            rig_object_is (geometry, RIG_INTERFACE_ID_PICKABLE) &&
            (vertex_data = rig_pickable_get_vertex_data (geometry,
                                                         &stride,
                                                         &n_vertices))))
        return RIG_TRAVERSE_VISIT_CONTINUE;

      /* transform the ray into the model space */
      memcpy (transformed_ray_origin,
              pick_ctx->ray_origin, 3 * sizeof (float));
      memcpy (transformed_ray_direction,
              pick_ctx->ray_direction, 3 * sizeof (float));

      cogl_framebuffer_get_modelview_matrix (fb, &transform);

      transform_ray (&transform,
                     TRUE, /* inverse of the transform */
                     transformed_ray_origin,
                     transformed_ray_direction);

      /* intersect the transformed ray with the mesh data */
      hit = rig_util_intersect_mesh (vertex_data,
                                     n_vertices,
                                     stride,
                                     transformed_ray_origin,
                                     transformed_ray_direction,
                                     &index,
                                     &distance);

      if (hit)
        {
          const CoglMatrix *view = rig_camera_get_view_transform (pick_ctx->camera);
          float w = 1;

          /* to compare intersection distances we find the actual point of ray
           * intersection in model coordinates and transform that into eye
           * coordinates */

          transformed_ray_direction[0] *= distance;
          transformed_ray_direction[1] *= distance;
          transformed_ray_direction[2] *= distance;

          transformed_ray_direction[0] += transformed_ray_origin[0];
          transformed_ray_direction[1] += transformed_ray_origin[1];
          transformed_ray_direction[2] += transformed_ray_origin[2];

          cogl_matrix_transform_point (&transform,
                                       &transformed_ray_direction[0],
                                       &transformed_ray_direction[1],
                                       &transformed_ray_direction[2],
                                       &w);
          cogl_matrix_transform_point (view,
                                       &transformed_ray_direction[0],
                                       &transformed_ray_direction[1],
                                       &transformed_ray_direction[2],
                                       &w);
          distance = transformed_ray_direction[2];

          if (distance > pick_ctx->selected_distance)
            {
              pick_ctx->selected_entity = entity;
              pick_ctx->selected_distance = distance;
              pick_ctx->selected_index = index;
            }
        }
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static RigTraverseVisitFlags
_rig_entitygraph_post_pick_cb (RigObject *object,
                               int depth,
                               void *user_data)
{
  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      PickContext *pick_ctx = user_data;
      cogl_framebuffer_pop_matrix (pick_ctx->fb);
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static RigEntity *
pick (RigData *data,
      RigCamera *camera,
      CoglFramebuffer *fb,
      float ray_origin[3],
      float ray_direction[3])
{
  PickContext pick_ctx;

  pick_ctx.camera = camera;
  pick_ctx.fb = fb;
  pick_ctx.selected_distance = -G_MAXFLOAT;
  pick_ctx.selected_entity = NULL;
  pick_ctx.ray_origin = ray_origin;
  pick_ctx.ray_direction = ray_direction;

  rig_graphable_traverse (data->scene,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_entitygraph_pre_pick_cb,
                          _rig_entitygraph_post_pick_cb,
                          &pick_ctx);

  if (pick_ctx.selected_entity)
    {
      g_message ("Hit entity, triangle #%d, distance %.2f",
                 pick_ctx.selected_index, pick_ctx.selected_distance);
    }

  return pick_ctx.selected_entity;
}

static void
update_camera_position (RigData *data)
{
  rig_entity_set_position (data->editor_camera_to_origin,
                           data->origin);

  rig_entity_set_translate (data->editor_camera_armature, 0, 0, data->editor_camera_z);

  rig_shell_queue_redraw (data->ctx->shell);
}

static void
print_quaternion (const CoglQuaternion *q,
                  const char *label)
{
  float angle = cogl_quaternion_get_rotation_angle (q);
  float axis[3];
  cogl_quaternion_get_rotation_axis (q, axis);
  g_print ("%s: [%f (%f, %f, %f)]\n", label, angle, axis[0], axis[1], axis[2]);
}

static CoglBool
translate_grab_entity (RigData *data,
                       RigCamera *camera,
                       RigEntity *entity,
                       float grab_x,
                       float grab_y,
                       EntityTranslateCallback translate_cb,
                       EntityTranslateDoneCallback done_cb,
                       void *user_data)
{
  EntityTranslateGrabClosure *closure =
    g_slice_new (EntityTranslateGrabClosure);
  RigEntity *parent = rig_graphable_get_parent (entity);
  CoglMatrix parent_transform;
  CoglMatrix inverse_transform;
  float origin[3] = {0, 0, 0};
  float unit_x[3] = {1, 0, 0};
  float unit_y[3] = {0, 1, 0};
  float x_vec[3];
  float y_vec[3];
  float entity_x, entity_y, entity_z;
  float w;

  if (!parent)
    return FALSE;

  rig_graphable_get_modelview (parent, camera, &parent_transform);

  if (!cogl_matrix_get_inverse (&parent_transform, &inverse_transform))
    {
      g_warning ("Failed to get inverse transform of entity");
      return FALSE;
    }

  /* Find the z of our selected entity in eye coordinates */
  entity_x = 0;
  entity_y = 0;
  entity_z = 0;
  w = 1;
  cogl_matrix_transform_point (&parent_transform,
                               &entity_x, &entity_y, &entity_z, &w);

  //g_print ("Entity origin in eye coords: %f %f %f\n", entity_x, entity_y, entity_z);

  /* Convert unit x and y vectors in screen coordinate
   * into points in eye coordinates with the same z depth
   * as our selected entity */

  unproject_window_coord (camera,
                          &data->identity, &data->identity,
                          entity_z, &origin[0], &origin[1]);
  origin[2] = entity_z;
  //g_print ("eye origin: %f %f %f\n", origin[0], origin[1], origin[2]);

  unproject_window_coord (camera,
                          &data->identity, &data->identity,
                          entity_z, &unit_x[0], &unit_x[1]);
  unit_x[2] = entity_z;
  //g_print ("eye unit_x: %f %f %f\n", unit_x[0], unit_x[1], unit_x[2]);

  unproject_window_coord (camera,
                          &data->identity, &data->identity,
                          entity_z, &unit_y[0], &unit_y[1]);
  unit_y[2] = entity_z;
  //g_print ("eye unit_y: %f %f %f\n", unit_y[0], unit_y[1], unit_y[2]);


  /* Transform our points from eye coordinates into entity
   * coordinates and convert into input mapping vectors */

  w = 1;
  cogl_matrix_transform_point (&inverse_transform,
                               &origin[0], &origin[1], &origin[2], &w);
  w = 1;
  cogl_matrix_transform_point (&inverse_transform,
                               &unit_x[0], &unit_x[1], &unit_x[2], &w);
  w = 1;
  cogl_matrix_transform_point (&inverse_transform,
                               &unit_y[0], &unit_y[1], &unit_y[2], &w);


  x_vec[0] = unit_x[0] - origin[0];
  x_vec[1] = unit_x[1] - origin[1];
  x_vec[2] = unit_x[2] - origin[2];

  //g_print (" =========================== Entity coords: x_vec = %f, %f, %f\n",
  //         x_vec[0], x_vec[1], x_vec[2]);

  y_vec[0] = unit_y[0] - origin[0];
  y_vec[1] = unit_y[1] - origin[1];
  y_vec[2] = unit_y[2] - origin[2];

  //g_print (" =========================== Entity coords: y_vec = %f, %f, %f\n",
  //         y_vec[0], y_vec[1], y_vec[2]);

  closure->data = data;
  closure->grab_x = grab_x;
  closure->grab_y = grab_y;

  memcpy (closure->entity_grab_pos,
          rig_entity_get_position (entity),
          sizeof (float) * 3);

  closure->entity = entity;
  closure->entity_translate_cb = translate_cb;
  closure->entity_translate_done_cb = done_cb;
  closure->user_data = user_data;

  memcpy (closure->x_vec, x_vec, sizeof (float) * 3);
  memcpy (closure->y_vec, y_vec, sizeof (float) * 3);

  rig_shell_grab_input (data->ctx->shell,
                        camera,
                        entity_translate_grab_input_cb,
                        closure);

  return TRUE;
}

static void
reload_position_inspector (RigData *data,
                           RigEntity *entity)
{
  if (data->inspector)
    {
      RigProperty *property =
        rig_introspectable_lookup_property (entity, "position");

      rig_inspector_reload_property (data->inspector, property);
    }
}

static void
entity_translate_done_cb (RigEntity *entity,
                          float start[3],
                          float rel[3],
                          void *user_data)
{
  RigData *data = user_data;
  RigTransition *transition = data->selected_transition;
  float elapsed = rig_timeline_get_elapsed (data->timeline);
  RigPath *path_position = rig_transition_get_path (transition,
                                                    entity,
                                                    "position");

  undo_journal_log_move (data->undo_journal,
                         FALSE,
                         entity,
                         start[0],
                         start[1],
                         start[2],
                         start[0] + rel[0],
                         start[1] + rel[1],
                         start[2] + rel[2]);

  rig_path_insert_vec3 (path_position, elapsed,
                        rig_entity_get_position (entity));

  reload_position_inspector (data, entity);

  rig_shell_queue_redraw (data->ctx->shell);
}

static void
entity_translate_cb (RigEntity *entity,
                     float start[3],
                     float rel[3],
                     void *user_data)
{
  RigData *data = user_data;

  rig_entity_set_translate (entity,
                            start[0] + rel[0],
                            start[1] + rel[1],
                            start[2] + rel[2]);

  reload_position_inspector (data, entity);

  rig_shell_queue_redraw (data->ctx->shell);
}

static void
scene_translate_cb (RigEntity *entity,
                    float start[3],
                    float rel[3],
                    void *user_data)
{
  RigData *data = user_data;

  data->origin[0] = start[0] - rel[0];
  data->origin[1] = start[1] - rel[1];
  data->origin[2] = start[2] - rel[2];

  update_camera_position (data);
}

static void
set_play_mode_enabled (RigData *data, CoglBool enabled)
{
  data->play_mode = enabled;

  if (data->play_mode)
    {
      data->enable_dof = TRUE;
      data->debug_pick_ray = 0;
    }
  else
    {
      data->enable_dof = FALSE;
      data->debug_pick_ray = 1;
    }

  rig_shell_queue_redraw (data->ctx->shell);
}

static RigInputEventStatus
main_input_cb (RigInputEvent *event,
               void *user_data)
{
  RigData *data = user_data;

  g_print ("Main Input Callback\n");

  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      RigMotionEventAction action = rig_motion_event_get_action (event);
      RigModifierState modifiers = rig_motion_event_get_modifier_state (event);
      float x = rig_motion_event_get_x (event);
      float y = rig_motion_event_get_y (event);
      RigButtonState state;

      if (rig_camera_transform_window_coordinate (data->editor_camera_component, &x, &y))
        data->key_focus_callback = main_input_cb;

      state = rig_motion_event_get_button_state (event);

      if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
          state == RIG_BUTTON_STATE_1)
        {
          /* pick */
          RigCamera *camera;
          float ray_position[3], ray_direction[3], screen_pos[2],
                z_far, z_near;
          const float *viewport;
          const CoglMatrix *inverse_projection;
          //CoglMatrix *camera_transform;
          const CoglMatrix *camera_view;
          CoglMatrix camera_transform;

          camera = rig_entity_get_component (data->editor_camera,
                                             RIG_COMPONENT_TYPE_CAMERA);
          viewport = rig_camera_get_viewport (RIG_CAMERA (camera));
          z_near = rig_camera_get_near_plane (RIG_CAMERA (camera));
          z_far = rig_camera_get_far_plane (RIG_CAMERA (camera));
          inverse_projection =
            rig_camera_get_inverse_projection (RIG_CAMERA (camera));

#if 0
          camera_transform = rig_entity_get_transform (data->editor_camera);
#else
          camera_view = rig_camera_get_view_transform (camera);
          cogl_matrix_get_inverse (camera_view, &camera_transform);
#endif

          screen_pos[0] = x;
          screen_pos[1] = y;

          rig_util_create_pick_ray (viewport,
                                    inverse_projection,
                                    &camera_transform,
                                    screen_pos,
                                    ray_position,
                                    ray_direction);

          if (data->debug_pick_ray)
            {
              float x1 = 0, y1 = 0, z1 = z_near, w1 = 1;
              float x2 = 0, y2 = 0, z2 = z_far, w2 = 1;
              float len;

              if (data->picking_ray)
                cogl_object_unref (data->picking_ray);

              /* FIXME: This is a hack, we should intersect the ray with
               * the far plane to decide how long the debug primitive
               * should be */
              cogl_matrix_transform_point (&camera_transform,
                                           &x1, &y1, &z1, &w1);
              cogl_matrix_transform_point (&camera_transform,
                                           &x2, &y2, &z2, &w2);
              len = z2 - z1;

              data->picking_ray = create_picking_ray (data,
                                                      rig_camera_get_framebuffer (camera),
                                                      ray_position,
                                                      ray_direction,
                                                      len);
            }

          data->selected_entity = pick (data,
                                        camera,
                                        rig_camera_get_framebuffer (camera),
                                        ray_position,
                                        ray_direction);

          rig_shell_queue_redraw (data->ctx->shell);
          if (data->selected_entity == NULL)
            rig_tool_update (data->tool, NULL);
          else if (data->selected_entity == data->light_handle)
            data->selected_entity = data->light;

          update_inspector (data);

          /* If we have selected an entity then initiate a grab so the
           * entity can be moved with the mouse...
           */
          if (data->selected_entity)
            {
              if (!translate_grab_entity (data,
                                          rig_input_event_get_camera (event),
                                          data->selected_entity,
                                          rig_motion_event_get_x (event),
                                          rig_motion_event_get_y (event),
                                          entity_translate_cb,
                                          entity_translate_done_cb,
                                          data))
                return RIG_INPUT_EVENT_STATUS_UNHANDLED;
            }

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
               state == RIG_BUTTON_STATE_2 &&
               ((modifiers & RIG_MODIFIER_SHIFT_ON) == 0))
        {
          //data->saved_rotation = *rig_entity_get_rotation (data->editor_camera);
          data->saved_rotation = *rig_entity_get_rotation (data->editor_camera_rotate);

          cogl_quaternion_init_identity (&data->arcball.q_drag);

          //rig_arcball_mouse_down (&data->arcball, data->width - x, y);
          rig_arcball_mouse_down (&data->arcball, data->main_width - x, data->main_height - y);
          g_print ("Arcball init, mouse = (%d, %d)\n", (int)(data->width - x), (int)(data->height - y));

          print_quaternion (&data->saved_rotation, "Saved Quaternion");
          print_quaternion (&data->arcball.q_drag, "Arcball Initial Quaternion");
          //data->button_down = TRUE;

          data->grab_x = x;
          data->grab_y = y;
          memcpy (data->saved_origin, data->origin, sizeof (data->origin));

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (action == RIG_MOTION_EVENT_ACTION_MOVE &&
               state == RIG_BUTTON_STATE_2 &&
               modifiers & RIG_MODIFIER_SHIFT_ON)
        {
          if (!translate_grab_entity (data,
                                      rig_input_event_get_camera (event),
                                      data->editor_camera_to_origin,
                                      rig_motion_event_get_x (event),
                                      rig_motion_event_get_y (event),
                                      scene_translate_cb,
                                      NULL,
                                      data))
            return RIG_INPUT_EVENT_STATUS_UNHANDLED;
#if 0
          float origin[3] = {0, 0, 0};
          float unit_x[3] = {1, 0, 0};
          float unit_y[3] = {0, 1, 0};
          float x_vec[3];
          float y_vec[3];
          float dx;
          float dy;
          float translation[3];

          rig_entity_get_transformed_position (data->editor_camera,
                                               origin);
          rig_entity_get_transformed_position (data->editor_camera,
                                               unit_x);
          rig_entity_get_transformed_position (data->editor_camera,
                                               unit_y);

          x_vec[0] = origin[0] - unit_x[0];
          x_vec[1] = origin[1] - unit_x[1];
          x_vec[2] = origin[2] - unit_x[2];

            {
              CoglMatrix transform;
              rig_graphable_get_transform (data->editor_camera, &transform);
              cogl_debug_matrix_print (&transform);
            }
          g_print (" =========================== x_vec = %f, %f, %f\n",
                   x_vec[0], x_vec[1], x_vec[2]);

          y_vec[0] = origin[0] - unit_y[0];
          y_vec[1] = origin[1] - unit_y[1];
          y_vec[2] = origin[2] - unit_y[2];

          //dx = (x - data->grab_x) * (data->editor_camera_z / 100.0f);
          //dy = -(y - data->grab_y) * (data->editor_camera_z / 100.0f);
          dx = (x - data->grab_x);
          dy = -(y - data->grab_y);

          translation[0] = dx * x_vec[0];
          translation[1] = dx * x_vec[1];
          translation[2] = dx * x_vec[2];

          translation[0] += dy * y_vec[0];
          translation[1] += dy * y_vec[1];
          translation[2] += dy * y_vec[2];

          data->origin[0] = data->saved_origin[0] + translation[0];
          data->origin[1] = data->saved_origin[1] + translation[1];
          data->origin[2] = data->saved_origin[2] + translation[2];

          update_camera_position (data);

          g_print ("Translate %f %f dx=%f, dy=%f\n",
                   x - data->grab_x,
                   y - data->grab_y,
                   dx, dy);
#endif
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (action == RIG_MOTION_EVENT_ACTION_MOVE &&
               state == RIG_BUTTON_STATE_2 &&
               ((modifiers & RIG_MODIFIER_SHIFT_ON) == 0))
        {
          CoglQuaternion new_rotation;

          //if (!data->button_down)
          //  break;

          //rig_arcball_mouse_motion (&data->arcball, data->width - x, y);
          rig_arcball_mouse_motion (&data->arcball, data->main_width - x, data->main_height - y);
          g_print ("Arcball motion, center=%f,%f mouse = (%f, %f)\n",
                   data->arcball.center[0],
                   data->arcball.center[1],
                   x, y);

          cogl_quaternion_multiply (&new_rotation,
                                    &data->saved_rotation,
                                    &data->arcball.q_drag);

          //rig_entity_set_rotation (data->editor_camera, &new_rotation);
          rig_entity_set_rotation (data->editor_camera_rotate, &new_rotation);

          print_quaternion (&new_rotation, "New Rotation");

          print_quaternion (&data->arcball.q_drag, "Arcball Quaternion");

          g_print ("rig entity set rotation\n");

          rig_shell_queue_redraw (data->ctx->shell);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }

#if 0
      switch (rig_motion_event_get_action (event))
        {
        case RIG_MOTION_EVENT_ACTION_DOWN:
          /* TODO: Add rig_shell_implicit_grab_input() that handles releasing
           * the grab for you */
          rig_shell_grab_input (data->ctx->shell, timeline_grab_input_cb, data);
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
#endif
    }
  else if (!_rig_in_device_mode &&
           rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_KEY &&
           rig_key_event_get_action (event) == RIG_KEY_EVENT_ACTION_UP)
    {
      switch (rig_key_event_get_keysym (event))
        {
        case RIG_KEY_s:
          rig_save (data);
          break;
        case RIG_KEY_z:
          if (rig_key_event_get_modifier_state (event) & RIG_MODIFIER_CTRL_ON)
            undo_journal_undo (data->undo_journal);
          break;
        case RIG_KEY_y:
          if (rig_key_event_get_modifier_state (event) & RIG_MODIFIER_CTRL_ON)
            undo_journal_redo (data->undo_journal);
          break;
        case RIG_KEY_minus:
          if (data->editor_camera_z)
            data->editor_camera_z *= 1.2f;
          else
            data->editor_camera_z = 0.1;

          update_camera_position (data);
          break;
        case RIG_KEY_equal:
          data->editor_camera_z *= 0.8;
          update_camera_position (data);
          break;
        case RIG_KEY_p:
          set_play_mode_enabled (data, !data->play_mode);
          break;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
editor_input_region_cb (RigInputRegion *region,
                      RigInputEvent *event,
                      void *user_data)
{
  if (_rig_in_device_mode)
    return RIG_INPUT_EVENT_STATUS_UNHANDLED;
  else
    return main_input_cb (event, user_data);
}

void
matrix_view_2d_in_frustum (CoglMatrix *matrix,
                           float left,
                           float right,
                           float bottom,
                           float top,
                           float z_near,
                           float z_2d,
                           float width_2d,
                           float height_2d)
{
  float left_2d_plane = left / z_near * z_2d;
  float right_2d_plane = right / z_near * z_2d;
  float bottom_2d_plane = bottom / z_near * z_2d;
  float top_2d_plane = top / z_near * z_2d;

  float width_2d_start = right_2d_plane - left_2d_plane;
  float height_2d_start = top_2d_plane - bottom_2d_plane;

  /* Factors to scale from framebuffer geometry to frustum
   * cross-section geometry. */
  float width_scale = width_2d_start / width_2d;
  float height_scale = height_2d_start / height_2d;

  //cogl_matrix_translate (matrix,
  //                       left_2d_plane, top_2d_plane, -z_2d);
  cogl_matrix_translate (matrix,
                         left_2d_plane, top_2d_plane, 0);

  cogl_matrix_scale (matrix, width_scale, -height_scale, width_scale);
}

/* Assuming a symmetric perspective matrix is being used for your
 * projective transform then for a given z_2d distance within the
 * projective frustrum this convenience function determines how
 * we can use an entity transform to move from a normalized coordinate
 * space with (0,0) in the center of the screen to a non-normalized
 * 2D coordinate space with (0,0) at the top-left of the screen.
 *
 * Note: It assumes the viewport aspect ratio matches the desired
 * aspect ratio of the 2d coordinate space which is why we only
 * need to know the width of the 2d coordinate space.
 *
 */
void
get_entity_transform_for_2d_view (float fov_y,
                                  float aspect,
                                  float z_near,
                                  float z_2d,
                                  float width_2d,
                                  float *dx,
                                  float *dy,
                                  float *dz,
                                  CoglQuaternion *rotation,
                                  float *scale)
{
  float top = z_near * tan (fov_y * G_PI / 360.0);
  float left = -top * aspect;
  float right = top * aspect;

  float left_2d_plane = left / z_near * z_2d;
  float right_2d_plane = right / z_near * z_2d;
  float top_2d_plane = top / z_near * z_2d;

  float width_2d_start = right_2d_plane - left_2d_plane;

  *dx = left_2d_plane;
  *dy = top_2d_plane;
  *dz = 0;
  //*dz = -z_2d;

  /* Factors to scale from framebuffer geometry to frustum
   * cross-section geometry. */
  *scale = width_2d_start / width_2d;

  cogl_quaternion_init_from_z_rotation (rotation, 180);
}

static void
matrix_view_2d_in_perspective (CoglMatrix *matrix,
                               float fov_y,
                               float aspect,
                               float z_near,
                               float z_2d,
                               float width_2d,
                               float height_2d)
{
  float top = z_near * tan (fov_y * G_PI / 360.0);

  matrix_view_2d_in_frustum (matrix,
                             -top * aspect,
                             top * aspect,
                             -top,
                             top,
                             z_near,
                             z_2d,
                             width_2d,
                             height_2d);
}

static void
allocate_main_area (RigData *data)
{
  float screen_aspect;
  float main_aspect;
  float device_scale;

  if (_rig_in_device_mode)
    {
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (data->onscreen);
      data->main_width = cogl_framebuffer_get_width (fb);
      data->main_height = cogl_framebuffer_get_height (fb);
    }
  else
    {
      rig_bevel_get_size (data->main_area_bevel, &data->main_width, &data->main_height);
      if (data->main_width <= 0)
        data->main_width = 10;
      if (data->main_height <= 0)
        data->main_height = 10;
    }

  /* Update the window camera */
  rig_camera_set_projection_mode (data->camera, RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (data->camera,
                                           0, 0, data->width, data->height);
  rig_camera_set_near_plane (data->camera, -1);
  rig_camera_set_far_plane (data->camera, 100);

  rig_camera_set_viewport (data->camera, 0, 0, data->width, data->height);

  screen_aspect = DEVICE_WIDTH / DEVICE_HEIGHT;
  main_aspect = data->main_width / data->main_height;

  if (screen_aspect < main_aspect) /* screen is slimmer and taller than the main area */
    {
      data->screen_area_height = data->main_height;
      data->screen_area_width = data->screen_area_height * screen_aspect;

      rig_entity_set_translate (data->editor_camera_screen_pos,
                                -(data->main_width / 2.0) + (data->screen_area_width / 2.0),
                                0, 0);
    }
  else
    {
      data->screen_area_width = data->main_width;
      data->screen_area_height = data->screen_area_width / screen_aspect;

      rig_entity_set_translate (data->editor_camera_screen_pos,
                                0,
                                -(data->main_height / 2.0) + (data->screen_area_height / 2.0),
                                0);
    }

  /* NB: We know the screen area matches the device aspect ratio so we can use
   * a uniform scale here... */
  device_scale = data->screen_area_width / DEVICE_WIDTH;

  rig_entity_set_scale (data->editor_camera_dev_scale, 1.0 / device_scale);

  /* Setup projection for main content view */
  {
    float fovy = 10; /* y-axis field of view */
    float aspect = (float)data->main_width/(float)data->main_height;
    float z_near = 10; /* distance to near clipping plane */
    float z_far = 100; /* distance to far clipping plane */
    float x = 0, y = 0, z_2d = 30, w = 1;
    CoglMatrix inverse;

    data->z_2d = z_2d; /* position to 2d plane */

    cogl_matrix_init_identity (&data->main_view);
    matrix_view_2d_in_perspective (&data->main_view,
                                   fovy, aspect, z_near, data->z_2d,
                                   data->main_width,
                                   data->main_height);

    rig_camera_set_projection_mode (data->editor_camera_component,
                                    RIG_PROJECTION_PERSPECTIVE);
    rig_camera_set_field_of_view (data->editor_camera_component, fovy);
    rig_camera_set_near_plane (data->editor_camera_component, z_near);
    rig_camera_set_far_plane (data->editor_camera_component, z_far);

    /* Handle the z_2d translation by changing the length of the
     * camera's armature.
     */
    cogl_matrix_get_inverse (&data->main_view,
                             &inverse);
    cogl_matrix_transform_point (&inverse,
                                 &x, &y, &z_2d, &w);

    data->editor_camera_z = z_2d / device_scale;
    rig_entity_set_translate (data->editor_camera_armature, 0, 0, data->editor_camera_z);
    //rig_entity_set_translate (data->editor_camera_armature, 0, 0, 0);

    {
      float dx, dy, dz, scale;
      CoglQuaternion rotation;

      get_entity_transform_for_2d_view (fovy,
                                        aspect,
                                        z_near,
                                        data->z_2d,
                                        data->main_width,
                                        &dx,
                                        &dy,
                                        &dz,
                                        &rotation,
                                        &scale);

      rig_entity_set_translate (data->editor_camera_2d_view, -dx, -dy, -dz);
      rig_entity_set_rotation (data->editor_camera_2d_view, &rotation);
      rig_entity_set_scale (data->editor_camera_2d_view, 1.0 / scale);
    }
  }

  if (!_rig_in_device_mode)
    {
      rig_arcball_init (&data->arcball,
                        data->main_width / 2,
                        data->main_height / 2,
                        sqrtf (data->main_width *
                               data->main_width +
                               data->main_height *
                               data->main_height) / 2);
    }
}

static void
allocate (RigData *data)
{
  float vp_width;
  float vp_height;

  data->top_bar_height = 30;
  //data->top_bar_height = 0;
  data->left_bar_width = data->width * 0.2;
  //data->left_bar_width = 200;
  //data->left_bar_width = 0;
  data->right_bar_width = data->width * 0.2;
  //data->right_bar_width = 200;
  data->bottom_bar_height = data->height * 0.2;
  data->grab_margin = 5;
  //data->main_width = data->width - data->left_bar_width - data->right_bar_width;
  //data->main_height = data->height - data->top_bar_height - data->bottom_bar_height;

  if (!_rig_in_device_mode)
    rig_split_view_set_size (data->splits[0], data->width, data->height);

  allocate_main_area (data);

  /* Setup projection for the timeline view */
  if (!_rig_in_device_mode)
    {
      data->timeline_width = data->width - data->right_bar_width;
      data->timeline_height = data->bottom_bar_height;

      rig_camera_set_projection_mode (data->timeline_camera, RIG_PROJECTION_ORTHOGRAPHIC);
      rig_camera_set_orthographic_coordinates (data->timeline_camera,
                                               0, 0,
                                               data->timeline_width,
                                               data->timeline_height);
      rig_camera_set_near_plane (data->timeline_camera, -1);
      rig_camera_set_far_plane (data->timeline_camera, 100);
      rig_camera_set_background_color4f (data->timeline_camera, 1, 0, 0, 1);

      rig_camera_set_viewport (data->timeline_camera,
                               0,
                               data->height - data->bottom_bar_height,
                               data->timeline_width,
                               data->timeline_height);

      rig_input_region_set_rectangle (data->timeline_input_region,
                                      0, 0, data->timeline_width, data->timeline_height);

      vp_width = data->width - data->bottom_bar_height;
      rig_ui_viewport_set_width (data->timeline_vp,
                                 vp_width);
      vp_height = data->bottom_bar_height;
      rig_ui_viewport_set_height (data->timeline_vp,
                                  vp_height);
      rig_ui_viewport_set_doc_scale_x (data->timeline_vp,
                                       (vp_width / data->timeline_len));
      rig_ui_viewport_set_doc_scale_y (data->timeline_vp,
                                       (vp_height / DEVICE_HEIGHT));
    }
}

static void
data_onscreen_resize (CoglOnscreen *onscreen,
                      int width,
                      int height,
                      void *user_data)
{
  RigData *data = user_data;

  data->width = width;
  data->height = height;

  rig_property_dirty (&data->ctx->property_ctx, &data->properties[RIG_DATA_PROP_WIDTH]);
  rig_property_dirty (&data->ctx->property_ctx, &data->properties[RIG_DATA_PROP_HEIGHT]);

  allocate (data);
}

static void
camera_viewport_binding_cb (RigProperty *target_property,
                            RigProperty *source_property,
                            void *user_data)
{
  RigData *data = user_data;
  float x, y, z, width, height;

  x = y = z = 0;
  rig_graphable_fully_transform_point (data->main_area_bevel,
                                       data->camera,
                                       &x, &y, &z);

  data->main_x = x;
  data->main_y = y;

  x = RIG_UTIL_NEARBYINT (x);
  y = RIG_UTIL_NEARBYINT (y);

  rig_bevel_get_size (data->main_area_bevel, &width, &height);

  /* XXX: We round down here since that's currently what
   * rig-bevel.c:_rig_bevel_paint() does too. */
  width = (int)width;
  height = (int)height;

  rig_camera_set_viewport (data->editor_camera_component,
                           x, y, width, height);

  rig_input_region_set_rectangle (data->editor_input_region,
                                  x, y,
                                  x + width,
                                  y + height);

  allocate_main_area (data);
}

static void
init (RigShell *shell, void *user_data)
{
  RigData *data = user_data;
  CoglFramebuffer *fb;
  float vector3[3];
  int i;
  char *full_path;
  CoglError *error = NULL;
  CoglTexture2D *color_buffer;
  RigColor color;
  RigMeshRenderer *mesh;
  RigMaterial *material;
  RigLight *light;
  RigCamera *camera;
  RigColor top_bar_ref_color, main_area_ref_color, right_bar_ref_color;

  /* A unit test for the list_splice/list_unsplice functions */
#if 0
  _rig_test_list_splice ();
#endif

  cogl_matrix_init_identity (&data->identity);

  for (i = 0; i < RIG_DATA_N_PROPS; i++)
    rig_property_init (&data->properties[i],
                       &rig_data_property_specs[i],
                       data);

  if (_rig_in_device_mode)
    data->onscreen = cogl_onscreen_new (data->ctx->cogl_context,
                                        DEVICE_WIDTH / 2, DEVICE_HEIGHT / 2);
  else
    data->onscreen = cogl_onscreen_new (data->ctx->cogl_context, 1000, 700);
  cogl_onscreen_show (data->onscreen);

  if (!_rig_in_device_mode)
    {
      /* FIXME: On SDL this isn't taking affect if set before allocating
       * the framebuffer. */
      cogl_onscreen_set_resizable (data->onscreen, TRUE);
      cogl_onscreen_add_resize_handler (data->onscreen, data_onscreen_resize, data);
    }

  fb = COGL_FRAMEBUFFER (data->onscreen);
  data->width = cogl_framebuffer_get_width (fb);
  data->height  = cogl_framebuffer_get_height (fb);

  if (!_rig_in_device_mode)
    data->undo_journal = undo_journal_new (data);

  /* Create a color gradient texture that can be used for debugging
   * shadow mapping.
   *
   * XXX: This should probably simply be #ifdef DEBUG code.
   */
  if (!_rig_in_device_mode)
    {
      CoglVertexP2C4 quad[] = {
        { 0, 0, 0xff, 0x00, 0x00, 0xff },
        { 0, 200, 0x00, 0xff, 0x00, 0xff },
        { 200, 200, 0x00, 0x00, 0xff, 0xff },
        { 200, 0, 0xff, 0xff, 0xff, 0xff }
      };
      CoglOffscreen *offscreen;
      CoglPrimitive *prim =
        cogl_primitive_new_p2c4 (data->ctx->cogl_context, COGL_VERTICES_MODE_TRIANGLE_FAN, 4, quad);
      CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);

      data->gradient = COGL_TEXTURE (
        cogl_texture_2d_new_with_size (rig_cogl_context,
                                       200, 200,
                                       COGL_PIXEL_FORMAT_ANY,
                                       NULL));

      offscreen = cogl_offscreen_new_to_texture (data->gradient);

      cogl_framebuffer_orthographic (COGL_FRAMEBUFFER (offscreen),
                                     0, 0,
                                     200,
                                     200,
                                     -1,
                                     100);
      cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                                COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
                                0, 0, 0, 1);
      cogl_framebuffer_draw_primitive (COGL_FRAMEBUFFER (offscreen),
                                       pipeline,
                                       prim);
      cogl_object_unref (prim);
      cogl_object_unref (offscreen);
    }

  /*
   * Shadow mapping
   */

  /* Setup the shadow map */
  /* TODO: reallocate if the onscreen framebuffer is resized */
  color_buffer = cogl_texture_2d_new_with_size (rig_cogl_context,
                                                data->width, data->height,
                                                COGL_PIXEL_FORMAT_ANY,
                                                &error);
  if (error)
    g_critical ("could not create texture: %s", error->message);

  data->shadow_color = color_buffer;

  /* XXX: Right now there's no way to disable rendering to the color
   * buffer. */
  data->shadow_fb =
    cogl_offscreen_new_to_texture (COGL_TEXTURE (color_buffer));

  /* retrieve the depth texture */
  cogl_framebuffer_set_depth_texture_enabled (COGL_FRAMEBUFFER (data->shadow_fb),
                                              TRUE);
  /* FIXME: It doesn't seem right that we can query back the texture before
   * the framebuffer has been allocated. */
  data->shadow_map =
    cogl_framebuffer_get_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb));

  if (data->shadow_fb == NULL)
    g_critical ("could not create offscreen buffer");

  data->default_pipeline = cogl_pipeline_new (data->ctx->cogl_context);

  /*
   * Depth of Field
   */

  data->dof = rig_dof_effect_new (data->ctx);
  data->enable_dof = FALSE;

  data->circle_texture = rig_create_circle_texture (data->ctx,
                                                    CIRCLE_TEX_RADIUS /* radius */,
                                                    CIRCLE_TEX_PADDING /* padding */);

  if (!_rig_in_device_mode)
    {
      data->grid_prim = create_grid (data->ctx,
                                     DEVICE_WIDTH,
                                     DEVICE_HEIGHT,
                                     100,
                                     100);
    }

  data->circle_node_attribute =
    rig_create_circle_fan_p2 (data->ctx, 20, &data->circle_node_n_verts);

  if (!_rig_in_device_mode)
    {
      full_path = g_build_filename (RIG_SHARE_DIR, "light-bulb.png", NULL);
      //full_path = g_build_filename (RIG_HANDSET_SHARE_DIR, "nine-slice-test.png", NULL);
      data->light_icon = rig_load_texture (data->ctx, full_path, &error);
      g_free (full_path);
      if (!data->light_icon)
        {
          g_warning ("Failed to load light-bulb texture: %s", error->message);
          cogl_error_free (error);
        }

      data->timeline_vp = rig_ui_viewport_new (data->ctx, 0, 0, NULL);
    }

  data->device_transform = rig_transform_new (data->ctx, NULL);

  data->camera = rig_camera_new (data->ctx, COGL_FRAMEBUFFER (data->onscreen));
  rig_camera_set_clear (data->camera, FALSE);

  /* XXX: Basically just a hack for now. We should have a
   * RigShellWindow type that internally creates a RigCamera that can
   * be used when handling input events in device coordinates.
   */
  rig_shell_set_window_camera (shell, data->camera);

  if (!_rig_in_device_mode)
    {
      data->timeline_camera = rig_camera_new (data->ctx, fb);
      rig_camera_set_clear (data->timeline_camera, FALSE);
      rig_shell_add_input_camera (shell, data->timeline_camera, NULL);
      data->timeline_scale = 1;
      data->timeline_len = 20;
    }

  data->scene = rig_graph_new (data->ctx, NULL);

  /* Conceptually we rig the camera to an armature with a pivot fixed
   * at the current origin. This setup makes it straight forward to
   * model user navigation by letting us change the length of the
   * armature to handle zoom, rotating the armature to handle
   * middle-click rotating the scene with the mouse and moving the
   * position of the armature for shift-middle-click translations with
   * the mouse.
   *
   * It also simplifies things if all the viewport setup for the
   * camera is handled using entity transformations as opposed to
   * mixing entity transforms with manual camera view transforms.
   */

  data->editor_camera_to_origin = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->scene, data->editor_camera_to_origin);
  rig_entity_set_label (data->editor_camera_to_origin, "rig:camera_to_origin");

  data->editor_camera_rotate = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->editor_camera_to_origin, data->editor_camera_rotate);
  rig_entity_set_label (data->editor_camera_rotate, "rig:camera_rotate");

  data->editor_camera_armature = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->editor_camera_rotate, data->editor_camera_armature);
  rig_entity_set_label (data->editor_camera_armature, "rig:camera_armature");

  data->editor_camera_origin_offset = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->editor_camera_armature, data->editor_camera_origin_offset);
  rig_entity_set_label (data->editor_camera_origin_offset, "rig:camera_origin_offset");

  data->editor_camera_dev_scale = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->editor_camera_origin_offset, data->editor_camera_dev_scale);
  rig_entity_set_label (data->editor_camera_dev_scale, "rig:camera_dev_scale");

  data->editor_camera_screen_pos = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->editor_camera_dev_scale, data->editor_camera_screen_pos);
  rig_entity_set_label (data->editor_camera_screen_pos, "rig:camera_screen_pos");

  data->editor_camera_2d_view = rig_entity_new (data->ctx, data->entity_next_id++);
  //rig_graphable_add_child (data->editor_camera_screen_pos, data->editor_camera_2d_view); FIXME
  rig_entity_set_label (data->editor_camera_2d_view, "rig:camera_2d_view");

  data->editor_camera = rig_entity_new (data->ctx, data->entity_next_id++);
  //rig_graphable_add_child (data->editor_camera_2d_view, data->editor_camera); FIXME
  rig_graphable_add_child (data->editor_camera_screen_pos, data->editor_camera);
  rig_entity_set_label (data->editor_camera, "rig:camera");

  data->origin[0] = DEVICE_WIDTH / 2;
  data->origin[1] = DEVICE_HEIGHT / 2;
  data->origin[2] = 0;

  rig_entity_translate (data->editor_camera_to_origin,
                        data->origin[0],
                        data->origin[1],
                        data->origin[2]);
                        //DEVICE_WIDTH / 2, (DEVICE_HEIGHT / 2), 0);

  //rig_entity_rotate_z_axis (data->editor_camera_to_origin, 45);

  rig_entity_translate (data->editor_camera_origin_offset,
                        -DEVICE_WIDTH / 2, -(DEVICE_HEIGHT / 2), 0);

  /* FIXME: currently we also do a z translation due to using
   * cogl_matrix_view_2d_in_perspective, we should stop using that api so we can
   * do our z_2d translation here...
   *
   * XXX: should the camera_z transform be done for the negative translate?
   */
  //device scale = 0.389062494
  data->editor_camera_z = 0.f;
  rig_entity_translate (data->editor_camera_armature, 0, 0, data->editor_camera_z);

#if 0
  {
    float pos[3] = {0, 0, 0};
    rig_entity_set_position (data->editor_camera_rig, pos);
    rig_entity_translate (data->editor_camera_rig, 100, 100, 0);
  }
#endif

  //rig_entity_translate (data->editor_camera, 100, 100, 0);

#if 0
  data->editor_camera_z = 20.f;
  vector3[0] = 0.f;
  vector3[1] = 0.f;
  vector3[2] = data->editor_camera_z;
  rig_entity_set_position (data->editor_camera, vector3);
#else
  data->editor_camera_z = 10.f;
#endif

  data->editor_camera_component = rig_camera_new (data->ctx, fb);
  rig_camera_set_clear (data->editor_camera_component, FALSE);
  rig_entity_add_component (data->editor_camera, data->editor_camera_component);
  rig_shell_add_input_camera (shell,
                              data->editor_camera_component,
                              data->scene);

  data->editor_input_region =
    rig_input_region_new_rectangle (0, 0, 0, 0, editor_input_region_cb, data);
  rig_input_region_set_hud_mode (data->editor_input_region, TRUE);
  //rig_camera_add_input_region (data->camera,
  rig_camera_add_input_region (data->editor_camera_component,
                               data->editor_input_region);


  update_camera_position (data);

  data->current_camera = data->editor_camera;

  data->light = rig_entity_new (data->ctx, data->entity_next_id++);
  data->entities = g_list_prepend (data->entities, data->light);
#if 1
  vector3[0] = 0;
  vector3[1] = 0;
  vector3[2] = 500;
  rig_entity_set_position (data->light, vector3);

  rig_entity_rotate_x_axis (data->light, 20);
  rig_entity_rotate_y_axis (data->light, -20);
#endif

  if (!_rig_in_device_mode)
    {
      RigMeshRenderer *mesh = rig_mesh_renderer_new_from_template (data->ctx, "cube");

      data->light_handle = rig_entity_new (data->ctx, data->entity_next_id++);
      rig_entity_add_component (data->light_handle, mesh);
      rig_graphable_add_child (data->light, data->light_handle);
      rig_entity_set_scale (data->light_handle, 100);
    }

  light = rig_light_new ();
  rig_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
  rig_light_set_ambient (light, &color);
  rig_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
  rig_light_set_diffuse (light, &color);
  rig_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
  rig_light_set_specular (light, &color);

  rig_entity_add_component (data->light, light);

  camera = rig_camera_new (data->ctx, COGL_FRAMEBUFFER (data->shadow_fb));
  data->shadow_map_camera = camera;

  rig_camera_set_background_color4f (camera, 0.f, .3f, 0.f, 1.f);
  rig_camera_set_projection_mode (camera,
                                  RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (camera,
                                           -1000, -1000, 1000, 1000);
  rig_camera_set_near_plane (camera, 1.1f);
  rig_camera_set_far_plane (camera, 1500.f);

  rig_entity_add_component (data->light, camera);

  rig_graphable_add_child (data->scene, data->light);

  data->root =
    rig_graph_new (data->ctx,
                   //(data->main_transform = rig_transform_new (data->ctx, NULL)),
                   NULL);

  if (!_rig_in_device_mode)
    {
      RigGraph *graph = rig_graph_new (data->ctx, NULL);
      RigTransform *transform;
      RigText *text;
      float x = 10;
      float width, height;

      rig_color_init_from_4f (&top_bar_ref_color, 0.41, 0.41, 0.41, 1.0);
      rig_color_init_from_4f (&main_area_ref_color, 0.22, 0.22, 0.22, 1.0);
      rig_color_init_from_4f (&right_bar_ref_color, 0.45, 0.45, 0.45, 1.0);

      data->splits[0] =
        rig_split_view_new (data->ctx,
                            RIG_SPLIT_VIEW_SPLIT_HORIZONTAL,
                            100,
                            100,
                            NULL);

      transform = rig_transform_new (data->ctx,
                                     (text = rig_text_new (data->ctx)), NULL);
      rig_transform_translate (transform, x, 5, 0);
      rig_text_set_text (text, "File");
      rig_graphable_add_child (graph, transform);
      rig_ref_countable_unref (transform);
      rig_sizable_get_size (text, &width, &height);
      x += width + 30;

      transform = rig_transform_new (data->ctx,
                                     (text = rig_text_new (data->ctx)), NULL);
      rig_transform_translate (transform, x, 5, 0);
      rig_text_set_text (text, "Edit");
      rig_graphable_add_child (graph, transform);
      rig_ref_countable_unref (transform);
      rig_sizable_get_size (text, &width, &height);
      x += width + 30;

      transform = rig_transform_new (data->ctx,
                                     (text = rig_text_new (data->ctx)), NULL);
      rig_transform_translate (transform, x, 5, 0);
      rig_text_set_text (text, "Help");
      rig_graphable_add_child (graph, transform);
      rig_ref_countable_unref (transform);
      rig_sizable_get_size (text, &width, &height);
      x += width + 30;

      data->top_bar_stack =
        rig_stack_new (data->ctx, 0, 0,
                       (data->top_bar_rect =
                        rig_rectangle_new4f (data->ctx, 0, 0,
                                             0.41, 0.41, 0.41, 1)),
                       graph,
                       rig_bevel_new (data->ctx, 0, 0, &top_bar_ref_color),
                       NULL);

      rig_graphable_add_child (data->root, data->splits[0]);

      data->splits[1] = rig_split_view_new (data->ctx,
                                            RIG_SPLIT_VIEW_SPLIT_VERTICAL,
                                            100,
                                            100,
                                            NULL);

      rig_split_view_set_child0 (data->splits[0], data->top_bar_stack);
      rig_split_view_set_child1 (data->splits[0], data->splits[1]);

      data->splits[2] = rig_split_view_new (data->ctx,
                                            RIG_SPLIT_VIEW_SPLIT_HORIZONTAL,
                                            100,
                                            100,
                                            NULL);

      data->splits[3] = rig_split_view_new (data->ctx,
                                            RIG_SPLIT_VIEW_SPLIT_HORIZONTAL,
                                            100,
                                            100,
                                            NULL);

      data->splits[4] = rig_split_view_new (data->ctx,
                                            RIG_SPLIT_VIEW_SPLIT_VERTICAL,
                                            100,
                                            100,
                                            NULL);

      data->icon_bar_stack = rig_stack_new (data->ctx, 0, 0,
                                            (data->icon_bar_rect =
                                             rig_rectangle_new4f (data->ctx, 0, 0,
                                                                  0.41, 0.41, 0.41, 1)),
                                            rig_bevel_new (data->ctx, 0, 0, &top_bar_ref_color),
                                            NULL);
      rig_split_view_set_child0 (data->splits[3], data->splits[4]);
      rig_split_view_set_child1 (data->splits[3], data->icon_bar_stack);

      data->left_bar_stack = rig_stack_new (data->ctx, 0, 0,
                                            (data->left_bar_rect =
                                             rig_rectangle_new4f (data->ctx, 0, 0,
                                                                  0.57, 0.57, 0.57, 1)),
                                            (data->assets_vp =
                                             rig_ui_viewport_new (data->ctx,
                                                                  0, 0,
                                                                  NULL)),
                                            rig_bevel_new (data->ctx, 0, 0, &top_bar_ref_color),
                                            NULL);

      rig_ui_viewport_set_x_pannable (data->assets_vp, FALSE);

        {
          RigEntry *entry;
          RigText *text;
          RigTransform *transform;
          transform = rig_transform_new (data->ctx,
                                         (entry = rig_entry_new (data->ctx)), NULL);
          rig_transform_translate (transform, 20, 10, 0);
          rig_graphable_add_child (data->assets_vp, transform);

          text = rig_entry_get_text (entry);
          rig_text_set_editable (text, TRUE);
          rig_text_set_text (text, "Search...");
        }


      data->main_area_bevel = rig_bevel_new (data->ctx, 0, 0, &main_area_ref_color),

      rig_split_view_set_child0 (data->splits[4], data->left_bar_stack);
      rig_split_view_set_child1 (data->splits[4], data->main_area_bevel);

      data->bottom_bar_stack = rig_stack_new (data->ctx, 0, 0,
                                              (data->bottom_bar_rect =
                                               rig_rectangle_new4f (data->ctx, 0, 0,
                                                                    0.57, 0.57, 0.57, 1)),
                                              NULL);

      rig_split_view_set_child0 (data->splits[2], data->splits[3]);
      rig_split_view_set_child1 (data->splits[2], data->bottom_bar_stack);

      data->right_bar_stack =
        rig_stack_new (data->ctx, 100, 100,
                       (data->right_bar_rect =
                        rig_rectangle_new4f (data->ctx, 0, 0,
                                             0.57, 0.57, 0.57, 1)),
                       (data->tool_vp =
                        rig_ui_viewport_new (data->ctx, 0, 0, NULL)),
                       rig_bevel_new (data->ctx, 0, 0, &right_bar_ref_color),
                       NULL);

      rig_ui_viewport_set_x_pannable (data->tool_vp, FALSE);

      rig_split_view_set_child0 (data->splits[1], data->splits[2]);
      rig_split_view_set_child1 (data->splits[1], data->right_bar_stack);

      rig_split_view_set_split_offset (data->splits[0], 30);
      rig_split_view_set_split_offset (data->splits[1], 850);
      rig_split_view_set_split_offset (data->splits[2], 500);
      rig_split_view_set_split_offset (data->splits[3], 470);
      rig_split_view_set_split_offset (data->splits[4], 150);
    }

  rig_shell_add_input_camera (shell, data->camera, data->root);

  if (_rig_in_device_mode)
    {
      int width = cogl_framebuffer_get_width (fb);
      int height = cogl_framebuffer_get_height (fb);

      rig_camera_set_viewport (data->editor_camera_component,
                               0, 0, width, height);

      rig_input_region_set_rectangle (data->editor_input_region,
                                      0, 0, width, height);

    }
  else
    {
      RigProperty *main_area_width =
        rig_introspectable_lookup_property (data->main_area_bevel, "width");
      RigProperty *main_area_height =
        rig_introspectable_lookup_property (data->main_area_bevel, "height");

      rig_property_set_binding_by_name (data->editor_camera_component,
                                        "viewport_x",
                                        camera_viewport_binding_cb,
                                        data,
                                        /* XXX: Hack: we are currently relying on
                                         * the bevel width being redundantly re-set
                                         * at times when bevel's position may have
                                         * also changed.
                                         *
                                         * FIXME: We need a proper allocation cycle
                                         * in Rig!
                                         */
                                        main_area_width,
                                        NULL);

      rig_property_set_binding_by_name (data->editor_camera_component,
                                        "viewport_y",
                                        camera_viewport_binding_cb,
                                        data,
                                        /* XXX: Hack: we are currently relying on
                                         * the bevel width being redundantly re-set
                                         * at times when bevel's position may have
                                         * also changed.
                                         *
                                         * FIXME: We need a proper allocation cycle
                                         * in Rig!
                                         */
                                        main_area_width,
                                        NULL);

      rig_property_set_binding_by_name (data->editor_camera_component,
                                        "viewport_width",
                                        camera_viewport_binding_cb,
                                        data,
                                        main_area_width,
                                        NULL);

      rig_property_set_binding_by_name (data->editor_camera_component,
                                        "viewport_height",
                                        camera_viewport_binding_cb,
                                        data,
                                        main_area_height,
                                        NULL);
    }

#if 0
  data->slider_transform =
    rig_transform_new (data->ctx,
                       data->slider = rig_slider_new (data->ctx,
                                                      RIG_AXIS_X,
                                                      0, 1,
                                                      data->main_width));
  rig_graphable_add_child (data->bottom_bar_transform, data->slider_transform);

  data->slider_progress =
    rig_introspectable_lookup_property (data->slider, "progress");
#endif

  if (!_rig_in_device_mode)
    {
      data->timeline_input_region =
        rig_input_region_new_rectangle (0, 0, 0, 0,
                                        timeline_region_input_cb,
                                        data);
      rig_camera_add_input_region (data->timeline_camera,
                                   data->timeline_input_region);
    }

  data->timeline = rig_timeline_new (data->ctx, 20.0);
  rig_timeline_set_loop_enabled (data->timeline, TRUE);
  rig_timeline_stop (data->timeline);

  data->timeline_elapsed =
    rig_introspectable_lookup_property (data->timeline, "elapsed");
  data->timeline_progress =
    rig_introspectable_lookup_property (data->timeline, "progress");

  /* tool */
  data->tool = rig_tool_new (data->shell);
  rig_tool_set_camera (data->tool, data->editor_camera);

  /* picking ray */
  data->picking_ray_color = cogl_pipeline_new (data->ctx->cogl_context);
  cogl_pipeline_set_color4f (data->picking_ray_color, 1.0, 0.0, 0.0, 1.0);

  allocate (data);

  if (_rig_in_device_mode)
    set_play_mode_enabled (data, TRUE);
  else
    set_play_mode_enabled (data, FALSE);

#ifndef __ANDROID__
  if (_rig_handset_remaining_args &&
      _rig_handset_remaining_args[0])
    {
      char *ui;
      struct stat st;

      stat (_rig_handset_remaining_args[0], &st);
      if (!S_ISDIR (st.st_mode))
        {
          g_error ("Could not find project directory %s",
                   _rig_handset_remaining_args[0]);
        }

      _rig_project_dir = _rig_handset_remaining_args[0];
      rig_set_assets_location (data->ctx, _rig_project_dir);

      ui = g_build_filename (_rig_handset_remaining_args[0], "ui.xml", NULL);

      rig_load (data, ui);
      g_free (ui);
    }
#endif
}

static void
fini (RigShell *shell, void *user_data)
{
  RigData *data = user_data;
  int i;

  rig_ref_countable_unref (data->camera);
  rig_ref_countable_unref (data->root);

  for (i = 0; i < RIG_DATA_N_PROPS; i++)
    rig_property_destroy (&data->properties[i]);

  cogl_object_unref (data->circle_texture);

  cogl_object_unref (data->circle_node_attribute);

  rig_dof_effect_free (data->dof);

  if (!_rig_in_device_mode)
    {
      rig_ref_countable_unref (data->timeline_vp);
      cogl_object_unref (data->grid_prim);
      cogl_object_unref (data->light_icon);
    }
}

static RigInputEventStatus
shell_input_handler (RigInputEvent *event, void *user_data)
{
  RigData *data = user_data;

  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      /* Anything that can claim the keyboard focus will do so during
       * motion events so we clear it before running other input
       * callbacks */
      data->key_focus_callback = NULL;
    }

  switch (rig_input_event_get_type (event))
    {
    case RIG_INPUT_EVENT_TYPE_MOTION:
#if 0
      switch (rig_motion_event_get_action (event))
        {
        case RIG_MOTION_EVENT_ACTION_DOWN:
          //g_print ("Press Down\n");
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        case RIG_MOTION_EVENT_ACTION_UP:
          //g_print ("Release\n");
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        case RIG_MOTION_EVENT_ACTION_MOVE:
          //g_print ("Move\n");
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
#endif
      break;

    case RIG_INPUT_EVENT_TYPE_KEY:
      {
        if (data->key_focus_callback)
          data->key_focus_callback (event, data);
      }
      break;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

typedef struct _AssetInputClosure
{
  RigAsset *asset;
  RigData *data;
} AssetInputClosure;

static RigInputEventStatus
asset_input_cb (RigInputRegion *region,
                RigInputEvent *event,
                void *user_data)
{
  AssetInputClosure *closure = user_data;
  RigAsset *asset = closure->asset;
  RigData *data = closure->data;

  g_print ("Asset input\n");

  if (rig_asset_get_type (asset) != RIG_ASSET_TYPE_TEXTURE)
    return RIG_INPUT_EVENT_STATUS_UNHANDLED;

  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN)
        {
          RigEntity *entity = rig_entity_new (data->ctx,
                                              data->entity_next_id++);
          CoglTexture *texture = rig_asset_get_texture (asset);
          RigMaterial *material = rig_material_new (data->ctx, asset, NULL);
          RigDiamond *diamond =
            rig_diamond_new (data->ctx,
                             400,
                             cogl_texture_get_width (texture),
                             cogl_texture_get_height (texture));
          rig_entity_add_component (entity, material);
          rig_entity_add_component (entity, diamond);

          data->selected_entity = entity;
          rig_graphable_add_child (data->scene, entity);

          update_inspector (data);

          rig_shell_queue_redraw (data->ctx->shell);
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

#if 0
static RigInputEventStatus
add_light_cb (RigInputRegion *region,
              RigInputEvent *event,
              void *user_data)
{
  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN)
        {
          g_print ("Add light!\n");
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}
#endif

static void
add_asset_icon (RigData *data,
                RigAsset *asset,
                float y_pos)
{
  AssetInputClosure *closure;
  CoglTexture *texture;
  RigNineSlice *nine_slice;
  RigInputRegion *region;
  RigTransform *transform;

  if (rig_asset_get_type (asset) != RIG_ASSET_TYPE_TEXTURE)
    return;

  closure = g_slice_new (AssetInputClosure);
  closure->asset = asset;
  closure->data = data;

  texture = rig_asset_get_texture (asset);

  transform =
    rig_transform_new (data->ctx,
                       (nine_slice = rig_nine_slice_new (data->ctx,
                                                         texture,
                                                         0, 0, 0, 0,
                                                         100, 100)),
                       (region =
                        rig_input_region_new_rectangle (0, 0, 100, 100,
                                                        asset_input_cb,
                                                        closure)),
                       NULL);
  rig_graphable_add_child (data->assets_list, transform);

  /* XXX: It could be nicer to have some form of weak pointer
   * mechanism to manage the lifetime of these closures... */
  data->asset_input_closures = g_list_prepend (data->asset_input_closures,
                                               closure);

  rig_transform_translate (transform, 10, y_pos, 0);

  //rig_input_region_set_graphable (region, nine_slice);

  rig_ref_countable_unref (transform);
  rig_ref_countable_unref (nine_slice);
  rig_ref_countable_unref (region);
}

static void
free_asset_input_closures (RigData *data)
{
  GList *l;

  for (l = data->asset_input_closures; l; l = l->next)
    g_slice_free (AssetInputClosure, l->data);
  g_list_free (data->asset_input_closures);
  data->asset_input_closures = NULL;
}

void
rig_update_asset_list (RigData *data)
{
  GList *l;
  int i = 0;
  RigObject *doc_node;

  if (data->assets_list)
    {
      rig_graphable_remove_child (data->assets_list);
      free_asset_input_closures (data);
    }

  data->assets_list = rig_graph_new (data->ctx, NULL);

  doc_node = rig_ui_viewport_get_doc_node (data->assets_vp);
  rig_graphable_add_child (doc_node, data->assets_list);
  rig_ref_countable_unref (data->assets_list);

  for (l = data->assets, i= 0; l; l = l->next, i++)
    add_asset_icon (data, l->data, 70 + 110 * i);

  //add_asset_icon (data, data->light_icon, 10 + 110 * i++, add_light_cb, NULL);
}

void
rig_free_ux (RigData *data)
{
  GList *l;

  for (l = data->transitions; l; l = l->next)
    rig_transition_free (l->data);
  g_list_free (data->transitions);
  data->transitions = NULL;

  for (l = data->assets; l; l = l->next)
    rig_ref_countable_unref (l->data);
  g_list_free (data->assets);
  data->assets = NULL;

  free_asset_input_closures (data);
}

static void
init_types (void)
{
}

#ifdef __ANDROID__

void
android_main (struct android_app *application)
{
  RigData data;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&data, 0, sizeof (RigData));
  data.app = application;

  init_types ();

  data.shell = rig_android_shell_new (application,
                                      init,
                                      fini,
                                      paint,
                                      &data);

  data.ctx = rig_context_new (data.shell);

  rig_context_init (data.ctx);

  rig_shell_set_input_callback (data.shell, shell_input_handler, &data);

  rig_shell_main (data.shell);
}

#else

int
main (int argc, char **argv)
{
  RigData data;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  g_option_context_add_main_entries (context, rig_handset_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("option parsing failed: %s\n", error->message);
      exit(EXIT_FAILURE);
    }

  memset (&data, 0, sizeof (RigData));

  init_types ();

  data.shell = rig_shell_new (init, fini, paint, &data);

  data.ctx = rig_context_new (data.shell);

  rig_context_init (data.ctx);

  rig_shell_add_input_callback (data.shell, shell_input_handler, &data, NULL);

  rig_shell_main (data.shell);

  return 0;
}

#endif
