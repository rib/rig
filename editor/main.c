#include <glib.h>
#include <gio/gio.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include <cogl/cogl.h>

#include "rig.h"
#include "rig-inspector.h"

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

#define INDENT_LEVEL 2

typedef struct _Data Data;

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

typedef struct _UndoJournal
{
  Data *data;
  GQueue ops;
  GList *pos;
  GQueue redo_ops;
} UndoJournal;

typedef struct _UndoRedoOpImpl
{
  void (*apply) (UndoJournal *journal, UndoRedo *undo_redo);
  UndoRedo *(*invert) (UndoRedo *src);
  void (*free) (UndoRedo *undo_redo);
} UndoRedoOpImpl;

typedef struct _Node
{
  float t;
} Node;

typedef struct _NodeFloat
{
  float t;
  float value;
} NodeFloat;

typedef struct _NodeVec3
{
  float t;
  float value[3];
} NodeVec3;

typedef struct _NodeQuaternion
{
  float t;
  CoglQuaternion value;
} NodeQuaternion;

typedef struct _Path
{
  RigContext *ctx;
  RigProperty *progress_prop;
  RigProperty *prop;
  GQueue nodes;
  GList *pos;
} Path;

typedef struct _DiamondSlice
{
  RigObjectProps _parent;
  int ref_count;

  CoglMatrix rotate_matrix;

  CoglTexture *texture;

  float width;
  float height;

  CoglPipeline *pipeline;
  CoglPrimitive *primitive;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

} DiamondSlice;

static uint8_t _diamond_slice_indices_data[] = {
    0,4,5,   0,5,1,  1,5,6,   1,6,2,   2,6,7,    2,7,3,
    4,8,9,   4,9,5,  5,9,10,  5,10,6,  6,10,11,  6,11,7,
    8,12,13, 8,13,9, 9,13,14, 9,14,10, 10,14,15, 10,15,11
};

enum {
  TRANSITION_PROP_PROGRESS,
  TRANSITION_N_PROPS
};

typedef struct _Transition
{
  RigObjectProps _parent;

  Data *data;

  uint32_t id;

  float progress;

  GList *paths;

  RigProperty props[TRANSITION_N_PROPS];
  RigSimpleIntrospectableProps introspectable;

} Transition;

static RigPropertySpec _transition_prop_specs[] = {
  {
    .name = "progress",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (Transition, progress)
  },
  { 0 }
};

typedef struct _TestPaintContext
{
  RigPaintContext _parent;

  Data *data;

  GList *camera_stack;

  gboolean shadow_pass;

} TestPaintContext;

typedef enum _State
{
  STATE_NONE
} State;

enum {
  DATA_PROP_WIDTH,
  DATA_PROP_HEIGHT,
  //DATA_PROP_PATH_T,

  DATA_N_PROPS
};

struct _Data
{
  RigCamera *camera;
  RigObject *root;
  RigObject *scene;

  CoglMatrix identity;

  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  CoglPipeline *root_pipeline;
  CoglPipeline *default_pipeline;

  State state;

  RigShell *shell;
  RigContext *ctx;
  CoglOnscreen *onscreen;

  UndoJournal *undo_journal;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture *shadow_map;
  RigCamera *shadow_map_camera;

  CoglIndices *diamond_slice_indices;
  CoglTexture *circle_texture;

  CoglTexture *light_icon;
  CoglTexture *clip_plane_icon;

  //float width;
  //RigProperty width_property;
  //float height;
  //RigProperty height_property;

  RigTransform *top_bar_transform;
  RigTransform *left_bar_transform;
  RigTransform *right_bar_transform;
  RigTransform *main_transform;
  RigTransform *bottom_bar_transform;

  //RigTransform *screen_area_transform;

  CoglPrimitive *grid_prim;
  CoglAttribute *circle_node_attribute;
  int circle_node_n_verts;

  //RigTransform *slider_transform;
  //RigSlider *slider;
  //RigProperty *slider_progress;
  RigRectangle *rect;
  float width;
  float height;
  float top_bar_height;
  float left_bar_width;
  float right_bar_width;
  float bottom_bar_height;
  float grab_margin;
  float main_width;
  float main_height;
  float screen_area_width;
  float screen_area_height;

  RigRectangle *top_bar_rect;
  RigRectangle *left_bar_rect;
  RigRectangle *right_bar_rect;
  RigRectangle *bottom_bar_rect;

  RigUIViewport *assets_vp;
  RigGraph *assets_list;
  GList *asset_input_closures;

  RigUIViewport *tool_vp;
  RigObject *inspector;

  RigCamera *timeline_camera;
  RigInputRegion *timeline_input_region;
  float timeline_width;
  float timeline_height;
  float timeline_len;
  float timeline_scale;

  RigUIViewport *timeline_vp;

  float grab_timeline_vp_t;
  float grab_timeline_vp_y;

  CoglMatrix main_view;
  float z_2d;

  RigEntity *main_camera_to_origin; /* move to origin */
  RigEntity *main_camera_rotate; /* armature rotate rotate */
  RigEntity *main_camera_origin_offset; /* negative offset */
  RigEntity *main_camera_armature; /* armature length */
  RigEntity *main_camera_dev_scale; /* scale to fit device coords */
  RigEntity *main_camera_screen_pos; /* position screen in edit view */
  RigEntity *main_camera_2d_view; /* setup 2d view, origin top-left */

  RigEntity *main_camera;
  RigCamera *main_camera_component;
  float main_camera_z;
  RigInputRegion *main_input_region;

  RigEntity *plane;
  RigEntity *cubes[N_CUBES];
  RigEntity *light;

  RigArcball arcball;
  CoglQuaternion saved_rotation;
  float origin[3];
  float saved_origin[3];

  //RigTransform *screen_area_transform;
  RigTransform *device_transform;

  RigTimeline *timeline;
  RigProperty *timeline_elapsed;
  RigProperty *timeline_progress;

  float grab_x;
  float grab_y;
  float entity_grab_pos[3];
  RigInputCallback key_focus_callback;

  GList *assets;

  uint32_t entity_next_id;
  GList *entities;
  GList *lights;
  GList *transitions;

  RigEntity *selected_entity;
  Transition *selected_transition;

  RigTool *tool;

  /* picking ray */
  CoglPipeline *picking_ray_color;
  CoglPrimitive *picking_ray;
  CoglBool debug_pick_ray;

  //Path *path;
  //float path_t;
  //RigProperty path_property;

  RigProperty properties[DATA_N_PROPS];

};

static RigPropertySpec data_propert_specs[] = {
  {
    .name = "width",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (Data, width)
  },
  {
    .name = "height",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (Data, height)
  },
#if 0
  {
    .name = "t",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (Data, path_t)
  },
#endif
  { 0 }
};


#ifndef __ANDROID__

static gboolean _rig_handset_in_device_mode = FALSE;
static char **_rig_handset_remaining_args = NULL;

static const GOptionEntry rig_handset_entries[] =
{
  { "device-mode", 'd', 0, 0,
    &_rig_handset_in_device_mode, "Run in Device Mode" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
    &_rig_handset_remaining_args, "Project" },
  { 0 }
};

static char *_rig_project_dir = NULL;

#endif /* __ANDROID__ */

static CoglBool
undo_journal_insert (UndoJournal *journal, UndoRedo *undo_redo);

static void
undo_redo_apply (UndoJournal *journal, UndoRedo *undo_redo);

static UndoRedo *
undo_redo_invert (UndoRedo *undo_redo);

static void
undo_redo_free (UndoRedo *undo_redo);

static void
save (Data *data);

static void
load (Data *data, const char *file);

static Path *
transition_get_path (Transition *transition,
                     RigObject *object,
                     const char *property_name);

static void
path_lerp_property (Path *path, float t);

void
node_float_lerp (NodeFloat *a,
                 NodeFloat *b,
                 float t,
                 float *value)
{
  float range = b->t - a->t;
  float offset = t - a->t;
  float factor = offset / range;

  *value = a->value + (b->value - a->value) * factor;
}

void
node_vec3_lerp (NodeVec3 *a,
                NodeVec3 *b,
                float t,
                float value[3])
{
  float range = b->t - a->t;
  float offset = t - a->t;
  float factor = offset / range;

  value[0] = a->value[0] + (b->value[0] - a->value[0]) * factor;
  value[1] = a->value[1] + (b->value[1] - a->value[1]) * factor;
  value[2] = a->value[2] + (b->value[2] - a->value[2]) * factor;
}

void
node_quaternion_lerp (NodeQuaternion *a,
                      NodeQuaternion *b,
                      float t,
                      CoglQuaternion *value)
{
  float range = b->t - a->t;
  float offset = t - a->t;
  float factor = offset / range;

  cogl_quaternion_nlerp (value, &a->value, &b->value, factor);
}

void
node_free (void *node, void *user_data)
{
  RigPropertyType type = GPOINTER_TO_UINT (user_data);

  switch (type)
    {
      case RIG_PROPERTY_TYPE_FLOAT:
	g_slice_free (NodeFloat, node);
	break;

      case RIG_PROPERTY_TYPE_VEC3:
	g_slice_free (NodeVec3, node);
	break;

      case RIG_PROPERTY_TYPE_QUATERNION:
	g_slice_free (NodeQuaternion, node);
	break;

      default:
        g_warn_if_reached ();
    }
}

NodeFloat *
node_new_for_float (float t, float value)
{
  NodeFloat *node = g_slice_new (NodeFloat);
  node->t = t;
  node->value = value;
  return node;
}

NodeVec3 *
node_new_for_vec3 (float t, const float value[3])
{
  NodeVec3 *node = g_slice_new (NodeVec3);
  node->t = t;
  memcpy (node->value, value, sizeof (value));
  return node;
}


NodeQuaternion *
node_new_for_quaternion (float t, float angle, float x, float y, float z)
{
  NodeQuaternion *node = g_slice_new (NodeQuaternion);
  node->t = t;
  cogl_quaternion_init (&node->value, angle, x, y, z);

  return node;
}

void
path_free (Path *path)
{
  g_queue_foreach (&path->nodes, node_free, GUINT_TO_POINTER (path->prop->spec->type));
  g_queue_clear (&path->nodes);
  rig_ref_countable_unref (path->ctx);
  g_slice_free (Path, path);
}


static void
update_path_property_cb (RigProperty *path_property, void *user_data)
{
  Path *path = user_data;
  float progress = rig_property_get_float (path->progress_prop);

  path_lerp_property (path, progress);
}

Path *
path_new_for_property (RigContext *ctx,
                       RigProperty *progress_prop,
                       RigProperty *path_prop)
{
  Path *path = g_slice_new (Path);

  path->ctx = rig_ref_countable_ref (ctx);

  path->progress_prop = progress_prop;
  path->prop = path_prop;

  g_queue_init (&path->nodes);
  path->pos = NULL;

  rig_property_set_binding (path_prop,
                            update_path_property_cb,
                            path,
                            progress_prop,
                            NULL);

  return path;
}

static GList *
nodes_find_less_than (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->prev)
    {
      Node *node = l->data;
      if (node->t < t)
        return l;
    }

  return NULL;
}

static GList *
nodes_find_less_than_equal (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->prev)
    {
      Node *node = l->data;
      if (node->t <= t)
        return l;
    }

  return NULL;
}

static GList *
nodes_find_greater_than (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->next)
    {
      Node *node = l->data;
      if (node->t > t)
        return l;
    }

  return NULL;
}

static GList *
nodes_find_first (GList *pos)
{
  GList *l;

  for (l = pos; l->prev; l = l->prev)
    ;

  return l;
}

static GList *
nodes_find_last (GList *pos)
{
  GList *l;

  for (l = pos; l->next; l = l->next)
    ;

  return l;
}

static GList *
nodes_find_greater_than_equal (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->next)
    {
      Node *node = l->data;
      if (node->t >= t)
        return l;
    }

  return NULL;
}

/* Finds 1 point either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node.
 */
static void
path_find_control_links2 (Path *path,
                          float t,
                          int direction,
                          GList **n0,
                          GList **n1)
{
  GList *pos;
  Node *pos_node;

  if (G_UNLIKELY (path->nodes.head == NULL))
    {
      *n0 = NULL;
      *n1= NULL;
      return;
    }

  if (G_UNLIKELY (path->pos == NULL))
    path->pos = path->nodes.head;

  pos = path->pos;
  pos_node = pos->data;

  /*
   * Note:
   *
   * A node with t exactly == t may only be considered as the first control
   * point moving in the current direction.
   */

  if (direction > 0)
    {
      if (pos_node->t > t)
        {
          /* > --- T -------- Pos ---- */
          GList *tmp = nodes_find_less_than_equal (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = nodes_find_first (pos);
              return;
            }
          pos = tmp;
        }
      else
        {
          /* > --- Pos -------- T ---- */
          GList *tmp = nodes_find_greater_than (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = nodes_find_last (pos);
              return;
            }
          pos = tmp->prev;
        }

      *n0 = pos;
      *n1 = pos->next;
    }
  else
    {
      if (pos_node->t > t)
        {
          /* < --- T -------- Pos ---- */
          GList *tmp = nodes_find_less_than (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = nodes_find_first (pos);
              return;
            }
          pos = tmp->next;
        }
      else
        {
          /* < --- Pos -------- T ---- */
          GList *tmp = nodes_find_greater_than_equal (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = nodes_find_last (pos);
              return;
            }
          pos = tmp;
        }

      *n0 = pos;
      *n1 = pos->prev;
    }

  path->pos = pos;
}

void
path_find_control_points2 (Path *path,
                           float t,
                           int direction,
                           Node **n0,
                           Node **n1)
{
  GList *l0, *l1;
  path_find_control_links2 (path, t, direction, &l0, &l1);
  *n0 = l0->data;
  *n1 = l1->data;
}

/* Finds 2 points either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node. */
void
path_find_control_points4 (Path *path,
                           float t,
                           int direction,
                           Node **n0,
                           Node **n1,
                           Node **n2,
                           Node **n3)
{
  GList *l1, *l2;

  path_find_control_links2 (path, t, direction, &l1, &l2);

  if (direction > 0)
    {
      *n0 = l1->prev->data;
      *n3 = l2->next->data;
    }
  else
    {
      *n0 = l1->next->data;
      *n3 = l2->prev->data;
    }

  *n1 = l1->data;
  *n2 = l2->data;
}

void
node_print (void *node, void *user_data)
{
  RigPropertyType type = GPOINTER_TO_UINT (user_data);
  switch (type)
    {
      case RIG_PROPERTY_TYPE_FLOAT:
	{
	  NodeFloat *f_node = (NodeFloat *)node;

	  g_print (" t = %f value = %f\n", f_node->t, f_node->value);
          break;
	}

      case RIG_PROPERTY_TYPE_VEC3:
	{
	  NodeVec3 *vec3_node = (NodeVec3 *)node;

	  g_print (" t = %f value.x = %f .y = %f .z = %f\n",
                   vec3_node->t,
                   vec3_node->value[0],
                   vec3_node->value[1],
                   vec3_node->value[2]);
          break;
	}

      case RIG_PROPERTY_TYPE_QUATERNION:
	{
	  NodeQuaternion *q_node = (NodeQuaternion *)node;
	  const CoglQuaternion *q = &q_node->value;
	  g_print (" t = %f [%f (%f, %f, %f)]\n",
                   q_node->t,
                   q->w, q->x, q->y, q->z);
	  break;
	}

      default:
        g_warn_if_reached ();
    }
}

void
path_print (Path *path)
{
  g_print ("path=%p\n", path);
  g_queue_foreach (&path->nodes, node_print, GUINT_TO_POINTER (path->prop->spec->type));
}

static int
path_find_t_cb (gconstpointer a, gconstpointer b)
{
  const Node *node = a;
  const float *t = b;

  if (node->t == *t)
    return 0;

  return 1;
}

static int
path_node_sort_t_func (const Node *a,
                       const Node *b,
                       void *user_data)
{
  if (a->t == b->t)
    return 0;
  else if (a->t < b->t)
    return -1;
  else
    return 1;
}

void
path_insert_float (Path *path,
                   float t,
                   float value)
{
  GList *link;
  NodeFloat *node;

#if 0
  g_print ("BEFORE:\n");
  path_print (path);
#endif

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = value;
    }
  else
    {
      node = node_new_for_float (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }

#if 0
  g_print ("AFTER:\n");
  path_print (path);
#endif
}

void
path_insert_vec3 (Path *path,
                  float t,
                  const float value[3])
{
  GList *link;
  NodeVec3 *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      memcpy (node->value, value, sizeof (value));
    }
  else
    {
      node = node_new_for_vec3 (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }
}


void
path_insert_quaternion (Path *path,
                        float t,
                        float angle,
                        float x,
                        float y,
                        float z)
{
  GList *link;
  NodeQuaternion *node;

#if 0
  g_print ("BEFORE:\n");
  path_print (path);
#endif

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      cogl_quaternion_init (&node->value, angle, x, y, z);
    }
  else
    {
      node = node_new_for_quaternion (t, angle, x, y, z);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func, NULL);
    }

#if 0
  g_print ("AFTER:\n");
  path_print (path);
#endif
}

static void
path_lerp_property (Path *path, float t)
{
  Node *n0, *n1;

  path_find_control_points2 (path, t, 1,
                             &n0,
                             &n1);

  switch (path->prop->spec->type)
    {
    case RIG_PROPERTY_TYPE_FLOAT:
      {
        float value;

        node_float_lerp ((NodeFloat *)n0, (NodeFloat *)n1, t, &value);
        rig_property_set_float (&path->ctx->property_ctx, path->prop, value);
        break;
      }
    case RIG_PROPERTY_TYPE_VEC3:
      {
        float value[3];
        node_vec3_lerp ((NodeVec3 *)n0, (NodeVec3 *)n1, t, value);
        rig_property_set_vec3 (&path->ctx->property_ctx, path->prop, value);
        break;
      }
    case RIG_PROPERTY_TYPE_QUATERNION:
      {
        CoglQuaternion value;
        node_quaternion_lerp ((NodeQuaternion *)n0,
                              (NodeQuaternion *)n1, t, &value);
        rig_property_set_quaternion (&path->ctx->property_ctx,
                                     path->prop, &value);
        break;
      }
    }
}

static UndoRedo *
undo_journal_find_recent_property_change (UndoJournal *journal,
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
undo_journal_log_move (UndoJournal *journal,
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
undo_journal_copy_property_and_log (UndoJournal *journal,
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
      rig_boxed_destroy (&prop_change->value1);
      /* NB: when we are merging then the existing operation is an
       * inverse of a normal move operation so the new move location
       * goes into value0... */
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
undo_redo_prop_change_apply (UndoJournal *journal, UndoRedo *undo_redo)
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
undo_redo_apply (UndoJournal *journal, UndoRedo *undo_redo)
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
undo_journal_flush_redos (UndoJournal *journal)
{
  UndoRedo *redo;
  while ((redo = g_queue_pop_head (&journal->redo_ops)))
    g_queue_push_tail (&journal->ops, redo);
  journal->pos = journal->ops.tail;
}

static CoglBool
undo_journal_insert (UndoJournal *journal, UndoRedo *undo_redo)
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
undo_journal_undo (UndoJournal *journal)
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
undo_journal_redo (UndoJournal *journal)
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

static UndoJournal *
undo_journal_new (Data *data)
{
  UndoJournal *journal = g_new0 (UndoJournal, 1);

  g_queue_init (&journal->ops);
  journal->data = data;
  journal->pos = NULL;
  g_queue_init (&journal->redo_ops);

  return journal;
}

#if 0
static UIViewport *
ui_viewport_new (float width,
                 float height,
                 float doc_x,
                 float doc_y,
                 float doc_x_scale,
                 float doc_y_scale)
{
  UIViewport *vp = g_slice_new (UIViewport);

  vp->width = width;
  vp->height = height;
  vp->doc_x = doc_x;
  vp->doc_y = doc_y;
  vp->doc_x_scale = doc_x_scale;
  vp->doc_y_scale = doc_y_scale;

  return vp;
}

static void
ui_viewport_free (UIViewport *vp)
{
  g_slice_free (UIViewport, vp);
}
#endif

static void
_diamond_slice_free (void *object)
{
  DiamondSlice *diamond_slice = object;

  cogl_object_unref (diamond_slice->texture);

  cogl_object_unref (diamond_slice->pipeline);
  cogl_object_unref (diamond_slice->primitive);

  g_slice_free (DiamondSlice, object);
}

RigRefCountableVTable _diamond_slice_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _diamond_slice_free
};

static RigGraphableVTable _diamond_slice_graphable_vtable = {
    0
};

static void
_diamond_slice_paint (RigObject *object,
                       RigPaintContext *paint_ctx)
{
  DiamondSlice *diamond_slice = object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

  cogl_framebuffer_draw_primitive (fb,
                                   diamond_slice->pipeline,
                                   diamond_slice->primitive);
}

static RigPaintableVTable _diamond_slice_paintable_vtable = {
  _diamond_slice_paint
};

RigType _diamond_slice_type;

static void
_diamond_slice_init_type (void)
{
  rig_type_init (&_diamond_slice_type);
  rig_type_add_interface (&_diamond_slice_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (DiamondSlice, ref_count),
                           &_diamond_slice_ref_countable_vtable);
  rig_type_add_interface (&_diamond_slice_type,
                           RIG_INTERFACE_ID_GRAPHABLE,
                           offsetof (DiamondSlice, graphable),
                           &_diamond_slice_graphable_vtable);
  rig_type_add_interface (&_diamond_slice_type,
                           RIG_INTERFACE_ID_PAINTABLE,
                           offsetof (DiamondSlice, paintable),
                           &_diamond_slice_paintable_vtable);
}

typedef struct _VertexP2T2T2
{
  float x, y, s0, t0, s1, t1;
} VertexP2T2T2;

static CoglPrimitive *
primitive_new_p2t2t2 (CoglContext *ctx,
                      CoglVerticesMode mode,
                      int n_vertices,
                      const VertexP2T2T2 *data)
{
  CoglAttributeBuffer *attribute_buffer =
    cogl_attribute_buffer_new (ctx, n_vertices * sizeof (VertexP2T2T2), data);
  CoglAttribute *attributes[3];
  CoglPrimitive *primitive;
  int i;

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (VertexP2T2T2),
                                      offsetof (VertexP2T2T2, x),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord0_in",
                                      sizeof (VertexP2T2T2),
                                      offsetof (VertexP2T2T2, s0),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] = cogl_attribute_new (attribute_buffer,
                                      "cogl_tex_coord1_in",
                                      sizeof (VertexP2T2T2),
                                      offsetof (VertexP2T2T2, s1),
                                      2,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  cogl_object_unref (attribute_buffer);

  primitive = cogl_primitive_new_with_attributes (mode,
                                                  n_vertices,
                                                  attributes,
                                                  3);

  for (i = 0; i < 3; i++)
    cogl_object_unref (attributes[i]);

  return primitive;
}


DiamondSlice *
diamond_slice_new (Data *data,
                   CoglTexture *texture,
                   float size)
{
  RigContext *ctx = data->ctx;
  DiamondSlice *diamond_slice = g_slice_new (DiamondSlice);
  float width = size;
  float height = size;
#define DIAMOND_SLICE_CORNER_RADIUS 20
  CoglMatrix matrix;
  float tex_aspect;
  float t;
  float tex_width = cogl_texture_get_width (texture);
  float tex_height = cogl_texture_get_height (texture);

  rig_object_init (&diamond_slice->_parent, &_diamond_slice_type);

  diamond_slice->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (diamond_slice));

  diamond_slice->texture = cogl_object_ref (texture);

  diamond_slice->width = width;
  diamond_slice->height = height;

  diamond_slice->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_layer_texture (diamond_slice->pipeline, 0, data->circle_texture);
  cogl_pipeline_set_layer_texture (diamond_slice->pipeline, 1, texture);

    {
      //float tex_width = cogl_texture_get_width (rounded_texture);
      //float tex_height = cogl_texture_get_height (rounded_texture);

      /* x0,y0,x1,y1 and s0,t0,s1,t1 define the postion and texture
       * coordinates for the center rectangle... */
      float x0 = DIAMOND_SLICE_CORNER_RADIUS;
      float y0 = DIAMOND_SLICE_CORNER_RADIUS;
      float x1 = width - DIAMOND_SLICE_CORNER_RADIUS;
      float y1 = height - DIAMOND_SLICE_CORNER_RADIUS;

      /* The center region of the nine-slice can simply map to the
       * degenerate center of the circle */
      float s0 = 0.5;
      float t0 = 0.5;
      float s1 = 0.5;
      float t1 = 0.5;

#if 0
      float s0 = DIAMOND_SLICE_CORNER_RADIUS left / tex_width;
      float t0 = top / tex_height;
      float s1 = (tex_width - right) / tex_width;
      float t1 = (tex_height - bottom) / tex_height;
#endif

      int n_vertices;
      int i;

      /*
       * 0,0      x0,0      x1,0      width,0
       * 0,0      s0,0      s1,0      1,0
       * 0        1         2         3
       *
       * 0,y0     x0,y0     x1,y0     width,y0
       * 0,t0     s0,t0     s1,t0     1,t0
       * 4        5         6         7
       *
       * 0,y1     x0,y1     x1,y1     width,y1
       * 0,t1     s0,t1     s1,t1     1,t1
       * 8        9         10        11
       *
       * 0,height x0,height x1,height width,height
       * 0,1      s0,1      s1,1      1,1
       * 12       13        14        15
       */

      VertexP2T2T2 vertices[] =
        {
          { 0,  0, 0, 0, 0, 0 },
          { x0, 0, s0, 0, x0, 0},
          { x1, 0, s1, 0, x1, 0},
          { width, 0, 1, 0, width, 0},

          { 0, y0, 0, t0, 0, y0},
          { x0, y0, s0, t0, x0, y0},
          { x1, y0, s1, t0, x1, y0},
          { width, y0, 1, t0, width, y0},

          { 0, y1, 0, t1, 0, y1},
          { x0, y1, s0, t1, x0, y1},
          { x1, y1, s1, t1, x1, y1},
          { width, y1, 1, t1, width, y1},

          { 0, height, 0, 1, 0, height},
          { x0, height, s0, 1, x0, height},
          { x1, height, s1, 1, x1, height},
          { width, height, 1, 1, width, height},
        };

      cogl_matrix_init_identity (&diamond_slice->rotate_matrix);
      cogl_matrix_rotate (&diamond_slice->rotate_matrix, 45, 0, 0, 1);
      cogl_matrix_translate (&diamond_slice->rotate_matrix, - width / 2.0, - height / 2.0, 0);
      //cogl_matrix_translate (&diamond_slice->rotate_matrix, width / 2.0, height / 2.0, 0);

      n_vertices = sizeof (vertices) / sizeof (VertexP2T2T2);
      for (i = 0; i < n_vertices; i++)
        {
          float z = 0, w = 1;

          cogl_matrix_transform_point (&diamond_slice->rotate_matrix,
                                       &vertices[i].x,
                                       &vertices[i].y,
                                       &z,
                                       &w);
        }

      cogl_matrix_init_identity (&matrix);
      tex_aspect = tex_width / tex_height;

      t = 0.5 / sinf (G_PI_4);

      /* FIXME: hack */
      cogl_matrix_translate (&matrix, 0.5, 0, 0);

      if (tex_aspect < 1) /* taller than it is wide */
        {
          float s_scale = (1.0 / width) * t;

          float t_scale = s_scale * (1.0 / tex_aspect);

          cogl_matrix_scale (&matrix, s_scale, t_scale, t_scale);
        }
      else /* wider than it is tall */
        {
          float t_scale = (1.0 / height) * t;

          float s_scale = t_scale * tex_aspect;

          cogl_matrix_scale (&matrix, s_scale, t_scale, t_scale);
        }

      cogl_matrix_rotate (&matrix, 45, 0, 0, 1);

      n_vertices = sizeof (vertices) / sizeof (VertexP2T2T2);
      for (i = 0; i < n_vertices; i++)
        {
          float z = 0, w = 1;

          cogl_matrix_transform_point (&matrix,
                                       &vertices[i].s1,
                                       &vertices[i].t1,
                                       &z,
                                       &w);
        }


      diamond_slice->primitive =
        primitive_new_p2t2t2 (ctx->cogl_context,
                              COGL_VERTICES_MODE_TRIANGLES,
                              n_vertices,
                              vertices);

      /* The vertices uploaded only map to the key intersection points of the
       * 9-slice grid which isn't a topology that GPUs can handle directly so
       * this specifies an array of indices that allow the GPU to interpret the
       * vertices as a list of triangles... */
      cogl_primitive_set_indices (diamond_slice->primitive,
                                  data->diamond_slice_indices,
                                  sizeof (_diamond_slice_indices_data) /
                                  sizeof (_diamond_slice_indices_data[0]));
    }

  return diamond_slice;
}

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
draw_jittered_primitive4f (Data *data,
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
camera_update_view (Data *data, RigEntity *camera, CoglBool shadow_map)
{
  RigCamera *camera_component =
    rig_entity_get_component (camera, RIG_COMPONENT_TYPE_CAMERA);
  CoglMatrix transform;
  CoglMatrix inverse_transform;
  CoglMatrix view;

  /* translate to z_2d and scale */
  view = data->main_view;

  /* apply the camera viewing transform */
  rig_graphable_get_transform (camera, &transform);
  cogl_matrix_get_inverse (&transform, &inverse_transform);
  cogl_matrix_multiply (&view, &view, &inverse_transform);

  if (shadow_map)
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

CoglPipeline *
get_entity_pipeline (Data *data,
                     RigEntity *entity,
                     RigComponent *geometry,
                     gboolean shadow_pass)
{
  CoglSnippet *snippet;
  CoglDepthState depth_state;
  RigMaterial *material =
    rig_entity_get_component (entity, RIG_COMPONENT_TYPE_MATERIAL);
  CoglPipeline *pipeline;

  pipeline = rig_entity_get_pipeline_cache (entity);
  if (pipeline)
    return cogl_object_ref (pipeline);

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

#if 0
  /* Vertex shader setup for shadow mapping */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat4 light_shadow_matrix;\n"
      "varying vec4 shadow_coords;\n",

      /* post */
      "shadow_coords = light_shadow_matrix * cogl_modelview_matrix *\n"
      "                cogl_position_in;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);
#endif

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


#if 0

  /* Handle shadow mapping */

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* declarations */
      "",

      /* post */
      "shadow_coords_d = shadow_coords / shadow_coords.w;\n"
      "cogl_texel7 =  cogl_texture_lookup7 (cogl_sampler7, cogl_tex_coord_in[0]);\n"
      "float distance_from_light = cogl_texel7.z + 0.0005;\n"
      "float shadow = 1.0;\n"
      "if (shadow_coords.w > 0.0 && distance_from_light < shadow_coords_d.z)\n"
      "    shadow = 0.5;\n"

      "cogl_color_out = shadow * cogl_color_out;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* Hook the shadow map sampling */

  cogl_pipeline_set_layer_texture (pipeline, 7, data->shadow_map);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              /* declarations */
                              "varying vec4 shadow_coords;\n"
                              "vec4 shadow_coords_d;\n",
                              /* post */
                              "");

  cogl_snippet_set_replace (snippet,
                            "cogl_texel = texture2D(cogl_sampler7, shadow_coords_d.st);\n");

  cogl_pipeline_add_layer_snippet (pipeline, 7, snippet);
  cogl_object_unref (snippet);
#endif

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

  return pipeline;
}

static RigTraverseVisitFlags
_rig_entitygraph_pre_paint_cb (RigObject *object,
                               int depth,
                               void *user_data)
{
  TestPaintContext *test_paint_ctx = user_data;
  RigPaintContext *paint_ctx = user_data;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      const CoglMatrix *matrix = rig_transformable_get_matrix (object);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rig_object_get_type (object) == &rig_entity_type)
    {
      RigComponent *geometry =
        rig_entity_get_component (object, RIG_COMPONENT_TYPE_GEOMETRY);
      CoglPipeline *pipeline;
      CoglPrimitive *primitive;
      CoglMatrix modelview_matrix;
      float normal_matrix[9];

      if (!geometry)
        {
          rig_entity_draw (object, fb);
          return RIG_TRAVERSE_VISIT_CONTINUE;
        }
#if 1
      pipeline = get_entity_pipeline (test_paint_ctx->data,
                                      object,
                                      geometry,
                                      test_paint_ctx->shadow_pass);
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
              TestPaintContext *test_paint_ctx = paint_ctx;
              Data *data = test_paint_ctx->data;
              RigDiamondSlice *slice = rig_diamond_get_slice (geometry);
              CoglPipeline *template = rig_diamond_slice_get_pipeline_template (slice);
              CoglPipeline *material_pipeline = rig_material_get_pipeline (material);
              CoglPipeline *pipeline = cogl_pipeline_copy (template);
              //CoglPipeline *pipeline = cogl_pipeline_copy (data->root_pipeline);
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
      RigPaintContext *paint_ctx = user_data;
      CoglFramebuffer *fb = rig_camera_get_framebuffer (paint_ctx->camera);
      cogl_framebuffer_pop_matrix (fb);
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

#if 0
static void
compute_light_shadow_matrix (Data       *data,
                             CoglMatrix *light_matrix,
                             CoglMatrix *light_projection,
                             RigEntity  *light)
{
  CoglMatrix *main_camera, *light_transform, light_view;
  /* Move the unit data from [-1,1] to [0,1], column major order */
  float bias[16] = {
    .5f, .0f, .0f, .0f,
    .0f, .5f, .0f, .0f,
    .0f, .0f, .5f, .0f,
    .5f, .5f, .5f, 1.f
  };

  main_camera = rig_entity_get_transform (data->main_camera);
  light_transform = rig_entity_get_transform (light);
  cogl_matrix_get_inverse (light_transform, &light_view);

  cogl_matrix_init_from_array (light_matrix, bias);
  cogl_matrix_multiply (light_matrix, light_matrix, light_projection);
  cogl_matrix_multiply (light_matrix, light_matrix, &light_view);
  cogl_matrix_multiply (light_matrix, light_matrix, main_camera);
}
#endif

#if 1
static void
paint_main_area_camera (RigEntity *camera, TestPaintContext *test_paint_ctx)
{
  RigCamera *camera_component =
    rig_entity_get_component (camera, RIG_COMPONENT_TYPE_CAMERA);
  Data *data = test_paint_ctx->data;
  CoglContext *ctx = data->ctx->cogl_context;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera_component);
  RigComponent *light;
  //CoglFramebuffer *shadow_fb;

  camera_update_view (data, camera, FALSE);

  rig_camera_flush (camera_component);

  light = rig_entity_get_component (camera, RIG_COMPONENT_TYPE_LIGHT);
  test_paint_ctx->shadow_pass = light ? TRUE : FALSE;

#if 0
  {
    CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
    CoglMatrix view;
    float fovy = 10; /* y-axis field of view */
    float aspect = (float)data->main_width/(float)data->main_height;
    float z_near = 10; /* distance to near clipping plane */
    float z_far = 100; /* distance to far clipping plane */
#if 1
    fovy = 60;
    z_near = 1.1;
    z_far = 100;
#endif

    g_assert (data->main_width == cogl_framebuffer_get_viewport_width (fb));
    g_assert (data->main_height == cogl_framebuffer_get_viewport_height (fb));

    cogl_matrix_init_identity (&view);
    cogl_matrix_view_2d_in_perspective (&view,
                                        fovy, aspect, z_near, data->z_2d,
                                        DEVICE_WIDTH,
                                        DEVICE_HEIGHT);
    cogl_framebuffer_set_modelview_matrix (fb, &view);

    cogl_framebuffer_draw_rectangle (fb, pipeline,
                                     1, 1, DEVICE_WIDTH - 2, DEVICE_HEIGHT - 2);
    cogl_object_unref (pipeline);
  }
#endif

#if 0
  cogl_framebuffer_transform (fb, rig_transform_get_matrix (data->screen_area_transform));
  cogl_framebuffer_transform (fb, rig_transform_get_matrix (data->device_transform));
#endif

  if (!test_paint_ctx->shadow_pass)
    {
      CoglPipeline *pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_color4f (pipeline, 0, 0, 0, 1.0);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, 0, DEVICE_WIDTH, DEVICE_HEIGHT);
                                       //0, 0, data->pane_width, data->pane_height);
      cogl_object_unref (pipeline);
    }

#if 0
  shadow_fb = COGL_FRAMEBUFFER (data->shadow_fb);

  /* update uniforms in materials */
  {
    CoglMatrix light_shadow_matrix, light_projection;
    CoglPipeline *pipeline;
    RigMaterial *material;
    const float *light_matrix;
    int location;

    cogl_framebuffer_get_projection_matrix (shadow_fb, &light_projection);
    compute_light_shadow_matrix (data,
                                 &light_shadow_matrix,
                                 &light_projection,
                                 data->light);
    light_matrix = cogl_matrix_get_array (&light_shadow_matrix);

    /* plane material */
    material = rig_entity_get_component (data->plane,
                                         RIG_COMPONENT_TYPE_MATERIAL);
    pipeline = rig_material_get_pipeline (material);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      light_matrix);

    /* cubes material */
    material = rig_entity_get_component (data->cubes[0],
                                         RIG_COMPONENT_TYPE_MATERIAL);
    pipeline = rig_material_get_pipeline (material);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      light_matrix);
  }
#endif



  rig_graphable_traverse (data->scene,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_entitygraph_pre_paint_cb,
                          _rig_entitygraph_post_paint_cb,
                          test_paint_ctx);

  if (!test_paint_ctx->shadow_pass)
    {
      if (data->debug_pick_ray && data->picking_ray)
      //if (data->picking_ray)
        {
          cogl_framebuffer_draw_primitive (fb,
                                           data->picking_ray_color,
                                           data->picking_ray);
        }
#if 0
      for (l = data->entities; l; l = l->next)
        {
          RigEntity *entity = l->data;
          const CoglMatrix *transform;
          cogl_framebuffer_push_matrix (fb);

          transform = rig_entity_get_transform (entity);
          cogl_framebuffer_transform (fb, transform);

          rig_entity_draw (entity, fb);

          cogl_framebuffer_pop_matrix (fb);
        }
#endif

      draw_jittered_primitive4f (data, fb, data->grid_prim, 0.5, 0.5, 0.5);

      if (data->selected_entity)
        {
          rig_tool_update (data->tool, data->selected_entity);
          rig_tool_draw (data->tool, fb);
        }
    }

  rig_camera_end_frame (camera_component);
}
#endif

static void
paint_timeline_camera (RigCamera *camera, void *user_data)
{
  Data *data = user_data;
  CoglContext *ctx = data->ctx->cogl_context;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);
  GList *l;

  rig_camera_flush (camera);

  //cogl_framebuffer_push_matrix (fb);
  //cogl_framebuffer_transform (fb, rig_transformable_get_matrix (camera));

  if (data->selected_entity)
    {
      //CoglContext *ctx = data->ctx->cogl_context;
      RigEntity *entity = data->selected_entity;
      //int i;

      float viewport_x = 0;
      float viewport_y = 0;

      float viewport_t_scale =
        rig_ui_viewport_get_doc_scale_x (data->timeline_vp) *
        data->timeline_scale;

      float viewport_y_scale =
        rig_ui_viewport_get_doc_scale_y (data->timeline_vp) *
        data->timeline_scale;

      float viewport_t_offset = rig_ui_viewport_get_doc_x (data->timeline_vp);
      float viewport_y_offset = rig_ui_viewport_get_doc_y (data->timeline_vp);
      CoglPipeline *pipeline = cogl_pipeline_new (data->ctx->cogl_context);
      //NodeFloat *next;

      CoglPrimitive *prim;

      for (l = data->selected_transition->paths; l; l = l->next)
        {
          Path *path = l->data;
          GArray *points;
          GList *l;
          GList *next;
          float red, green, blue;

          if (path == NULL ||
              path->prop->object != entity ||
              path->prop->spec->type != RIG_PROPERTY_TYPE_FLOAT)
            continue;

          if (strcmp (path->prop->spec->name, "x") == 0)
            red = 1.0, green = 0.0, blue = 0.0;
          else if (strcmp (path->prop->spec->name, "y") == 0)
            red = 0.0, green = 1.0, blue = 0.0;
          else if (strcmp (path->prop->spec->name, "z") == 0)
            red = 0.0, green = 0.0, blue = 1.0;
          else
            continue;

          points = g_array_new (FALSE, FALSE, sizeof (CoglVertexP2));

          for (l = path->nodes.head; l; l = next)
            {
              NodeFloat *f_node = l->data;
              CoglVertexP2 p;

              next = l->next;

              /* FIXME: This clipping wasn't working... */
#if 0
              /* Only draw the nodes within the current viewport */
              if (next)
                {
                  float max_t = viewport_t_offset + data->timeline_vp->width * viewport_t_scale;
                  if (next->t < viewport_t_offset)
                    continue;
                  if (node->t > max_t && next->t > max_t)
                    break;
                }
#endif

#define HANDLE_HALF_SIZE 4
              p.x = viewport_x + (f_node->t - viewport_t_offset) * viewport_t_scale;

              cogl_pipeline_set_color4f (pipeline, red, green, blue, 1);

              p.y = viewport_y + (f_node->value - viewport_y_offset) * viewport_y_scale;
#if 1
#if 1
              cogl_framebuffer_push_matrix (fb);
              cogl_framebuffer_translate (fb, p.x, p.y, 0);
              cogl_framebuffer_scale (fb, HANDLE_HALF_SIZE, HANDLE_HALF_SIZE, 0);
              cogl_framebuffer_draw_attributes (fb,
                                                pipeline,
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

          prim = cogl_primitive_new_p2 (ctx, COGL_VERTICES_MODE_LINE_STRIP,
                                        points->len, (CoglVertexP2 *)points->data);
          draw_jittered_primitive4f (data, fb, prim, red, green, blue);
          cogl_object_unref (prim);

          g_array_free (points, TRUE);
        }

      cogl_object_unref (pipeline);

      {
        double progress;
        float progress_x;
        float progress_line[4];

        progress = rig_timeline_get_progress (data->timeline);

        progress_x = -viewport_t_offset * viewport_t_scale + data->timeline_width * data->timeline_scale * progress;
        progress_line[0] = progress_x;
        progress_line[1] = 0;
        progress_line[2] = progress_x;
        progress_line[3] = data->timeline_height;

        prim = cogl_primitive_new_p2 (ctx, COGL_VERTICES_MODE_LINE_STRIP, 2, (CoglVertexP2 *)progress_line);
        draw_jittered_primitive4f (data, fb, prim, 0, 1, 0);
        cogl_object_unref (prim);
      }
    }

  //cogl_framebuffer_pop_matrix (fb);

  rig_camera_end_frame (camera);
}

static RigTraverseVisitFlags
_rig_scenegraph_pre_paint_cb (RigObject *object,
                              int depth,
                              void *user_data)
{
  RigPaintContext *paint_ctx = user_data;
  RigCamera *camera = paint_ctx->camera;
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
    vtable->paint (object, paint_ctx);

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
  RigPaintContext *paint_ctx = user_data;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (paint_ctx->camera);

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
test_paint (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (data->camera);
  TestPaintContext test_paint_ctx;
  RigPaintContext *paint_ctx = &test_paint_ctx._parent;

  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                            0.5, 0.5, 0.5, 1);

  test_paint_ctx.data = data;
  test_paint_ctx.shadow_pass = FALSE;

  paint_ctx->camera = data->camera;

  //g_print ("Data camera = %p\n", data->camera);
  rig_camera_flush (data->camera);
  rig_graphable_traverse (data->root,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_scenegraph_pre_paint_cb,
                          _rig_scenegraph_post_paint_cb,
                          &test_paint_ctx);
  rig_camera_end_frame (data->camera);

  paint_main_area_camera (data->main_camera, &test_paint_ctx);

  paint_timeline_camera (data->timeline_camera, data);

#if 0
  paint_ctx->camera = data->main_camera;

  rig_graphable_traverse (data->main_camera,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_scenegraph_pre_paint_cb,
                          _rig_scenegraph_post_paint_cb,
                          &test_paint_ctx);
#endif

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));

  return FALSE;
}

#if 0
static void
path_t_update_cb (RigProperty *property, void *user_data)
{
  Data *data = user_data;
  double elapsed = rig_timeline_get_elapsed (data->timeline);
  Node *n0, *n1;
  float x, y, z;

  path_find_control_points2 (data->path, elapsed, 1, &n0, &n1);

  lerp_node_pos (n0, n1, elapsed, &x, &y, &z);

  g_print ("e=%f: 0(%f,%f,%f) 1(%f,%f,%f) lerp=(%f, %f, %f)\n", elapsed,
           n0->point[0], n0->point[1], n0->point[2],
           n1->point[0], n1->point[1], n1->point[2],
           x, y, z);
}
#endif

static void
update_transition_progress_cb (RigProperty *property, void *user_data)
{
  Data *data = user_data;
  double elapsed = rig_timeline_get_elapsed (data->timeline);
  Transition *transition = property->object;

  transition->progress = elapsed;
  rig_property_dirty (&data->ctx->property_ctx,
                      &transition->props[TRANSITION_PROP_PROGRESS]);
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
  Data *data;

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
  Data *data = closure->data;

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
  Data *data = user_data;

  undo_journal_copy_property_and_log (data->undo_journal,
                                      TRUE, /* mergable */
                                      data->selected_entity,
                                      target_property,
                                      source_property);
}

static void
update_inspector (Data *data)
{
  RigObject *doc_node;

  if (data->inspector)
    {
      rig_graphable_remove_child (data->inspector);
      data->inspector = NULL;
    }

  if (data->selected_entity)
    {
      float width, height;

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
    }
}

static RigIntrospectableVTable _transition_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

static RigType _transition_type;

static void
_transition_type_init (void)
{
  rig_type_init (&_transition_type);
  rig_type_add_interface (&_transition_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_transition_introspectable_vtable);
  rig_type_add_interface (&_transition_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (Transition, introspectable),
                          NULL); /* no implied vtable */
}

Transition *
transition_new (Data *data,
                uint32_t id)
{
  //GError *error = NULL;
  Transition *transition;

  transition = g_slice_new0 (Transition);

  transition->id = id;

  rig_object_init (&transition->_parent, &_transition_type);

  rig_simple_introspectable_init (transition, _transition_prop_specs, transition->props);

  transition->data = data;

  transition->progress = 0;
  transition->paths = NULL;

  rig_property_set_binding (&transition->props[TRANSITION_PROP_PROGRESS],
                            update_transition_progress_cb,
                            data,
                            data->timeline_elapsed,
                            NULL);

  return transition;
}

static void
transition_free (Transition *transition)
{
  GList *l;

  rig_simple_introspectable_destroy (transition);

  for (l = transition->paths; l; l = l->next)
    {
      Path *path = l->data;
      path_free (path);
    }

  g_slice_free (Transition, transition);
}

void
transition_add_path (Transition *transition,
                     Path *path)
{
  transition->paths = g_list_prepend (transition->paths, path);
}

static Path *
transition_find_path (Transition *transition,
                      RigProperty *property)
{
  GList *l;

  for (l = transition->paths; l; l = l->next)
    {
      Path *path = l->data;

      if (path->prop == property)
        return path;
    }

  return NULL;
}

static Path *
transition_get_path (Transition *transition,
                     RigObject *object,
                     const char *property_name)
{
  RigProperty *property =
    rig_introspectable_lookup_property (object, property_name);
  Path *path;

  if (!property)
    return NULL;

  path = transition_find_path (transition, property);
  if (path)
    return path;

  path = path_new_for_property (transition->data->ctx,
                                &transition->props[TRANSITION_PROP_PROGRESS],
                                property);

  transition_add_path (transition, path);

  return path;
}

#if 0
static void
update_slider_pos_cb (RigProperty *property,
                      void *user_data)
{
  Data *data = user_data;
  double progress = rig_timeline_get_progress (data->timeline);

  //g_print ("update_slider_pos_cb %f\n", progress);

  rig_slider_set_progress (data->slider, progress);
}

static void
update_timeline_progress_cb (RigProperty *property,
                             void *user_data)
{
  Data *data = user_data;
  double progress = rig_property_get_float (data->slider_progress);

  //g_print ("update_timeline_progress_cb %f\n", progress);

  rig_timeline_set_progress (data->timeline, progress);
}
#endif

static RigInputEventStatus
timeline_grab_input_cb (RigInputEvent *event, void *user_data)
{
  Data *data = user_data;

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
  Data *data = user_data;

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
create_picking_ray (Data            *data,
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
      float distance, min_distance = G_MAXFLOAT;
      bool hit;
      float transformed_ray_origin[3];
      float transformed_ray_direction[3];
      CoglMatrix transform;

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

      /* to compare intersection distances we need to re-transform it back
       * to the world space, */
      cogl_vector3_normalize (transformed_ray_direction);
      transformed_ray_direction[0] *= distance;
      transformed_ray_direction[1] *= distance;
      transformed_ray_direction[2] *= distance;

      rig_util_transform_normal (&transform,
                                 &transformed_ray_direction[0],
                                 &transformed_ray_direction[1],
                                 &transformed_ray_direction[2]);
      distance = cogl_vector3_magnitude (transformed_ray_direction);

      if (hit && distance < min_distance)
        {
          min_distance = distance;
          pick_ctx->selected_entity = entity;
          pick_ctx->selected_distance = distance;
          pick_ctx->selected_index = index;
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
pick (Data *data,
      CoglFramebuffer *fb,
      float ray_origin[3],
      float ray_direction[3])
{
  PickContext pick_ctx;

  pick_ctx.fb = fb;
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
update_camera_position (Data *data)
{
  rig_entity_set_position (data->main_camera_to_origin,
                           data->origin);

  rig_entity_set_translate (data->main_camera_armature, 0, 0, data->main_camera_z);

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
translate_grab_entity (Data *data,
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
entity_translate_done_cb (RigEntity *entity,
                          float start[3],
                          float rel[3],
                          void *user_data)
{
  Data *data = user_data;
  Transition *transition = data->selected_transition;
  float elapsed = rig_timeline_get_elapsed (data->timeline);
  Path *path_position = transition_get_path (transition,
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

  path_insert_vec3 (path_position, elapsed,
                    rig_entity_get_position (entity));

  rig_shell_queue_redraw (data->ctx->shell);
}

static void
entity_translate_cb (RigEntity *entity,
                     float start[3],
                     float rel[3],
                     void *user_data)
{
  Data *data = user_data;

  rig_entity_set_translate (entity,
                            start[0] + rel[0],
                            start[1] + rel[1],
                            start[2] + rel[2]);

  rig_shell_queue_redraw (data->ctx->shell);
}

static void
scene_translate_cb (RigEntity *entity,
                    float start[3],
                    float rel[3],
                    void *user_data)
{
  Data *data = user_data;

  data->origin[0] = start[0] - rel[0];
  data->origin[1] = start[1] - rel[1];
  data->origin[2] = start[2] - rel[2];

  update_camera_position (data);
}

static RigInputEventStatus
main_input_cb (RigInputEvent *event,
               void *user_data)
{
  Data *data = user_data;

  g_print ("Main Input Callback\n");

  if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      RigMotionEventAction action = rig_motion_event_get_action (event);
      RigModifierState modifiers = rig_motion_event_get_modifier_state (event);
      float x = rig_motion_event_get_x (event);
      float y = rig_motion_event_get_y (event);
      RigButtonState state;

      if (rig_camera_transform_window_coordinate (data->main_camera_component, &x, &y))
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

          camera = rig_entity_get_component (data->main_camera,
                                             RIG_COMPONENT_TYPE_CAMERA);
          viewport = rig_camera_get_viewport (RIG_CAMERA (camera));
          z_near = rig_camera_get_near_plane (RIG_CAMERA (camera));
          z_far = rig_camera_get_far_plane (RIG_CAMERA (camera));
          inverse_projection =
            rig_camera_get_inverse_projection (RIG_CAMERA (camera));

#if 0
          camera_transform = rig_entity_get_transform (data->main_camera);
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
                                        rig_camera_get_framebuffer (camera),
                                        ray_position,
                                        ray_direction);

          rig_shell_queue_redraw (data->ctx->shell);
          if (data->selected_entity == NULL)
            rig_tool_update (data->tool, NULL);

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
          //data->saved_rotation = *rig_entity_get_rotation (data->main_camera);
          data->saved_rotation = *rig_entity_get_rotation (data->main_camera_rotate);

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
                                      data->main_camera_to_origin,
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

          rig_entity_get_transformed_position (data->main_camera,
                                               origin);
          rig_entity_get_transformed_position (data->main_camera,
                                               unit_x);
          rig_entity_get_transformed_position (data->main_camera,
                                               unit_y);

          x_vec[0] = origin[0] - unit_x[0];
          x_vec[1] = origin[1] - unit_x[1];
          x_vec[2] = origin[2] - unit_x[2];

            {
              CoglMatrix transform;
              rig_graphable_get_transform (data->main_camera, &transform);
              cogl_debug_matrix_print (&transform);
            }
          g_print (" =========================== x_vec = %f, %f, %f\n",
                   x_vec[0], x_vec[1], x_vec[2]);

          y_vec[0] = origin[0] - unit_y[0];
          y_vec[1] = origin[1] - unit_y[1];
          y_vec[2] = origin[2] - unit_y[2];

          //dx = (x - data->grab_x) * (data->main_camera_z / 100.0f);
          //dy = -(y - data->grab_y) * (data->main_camera_z / 100.0f);
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

          //rig_entity_set_rotation (data->main_camera, &new_rotation);
          rig_entity_set_rotation (data->main_camera_rotate, &new_rotation);

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
  else if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_KEY &&
           rig_key_event_get_action (event) == RIG_KEY_EVENT_ACTION_UP)
    {
      switch (rig_key_event_get_keysym (event))
        {
        case RIG_KEY_s:
          save (data);
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
          if (data->main_camera_z)
            data->main_camera_z *= 1.2f;
          else
            data->main_camera_z = 0.1;

          update_camera_position (data);
          break;
        case RIG_KEY_equal:
          data->main_camera_z *= 0.8;
          update_camera_position (data);
          break;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
main_input_region_cb (RigInputRegion *region,
                      RigInputEvent *event,
                      void *user_data)
{
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
allocate (Data *data)
{
  float screen_aspect;
  float main_aspect;
  float device_scale;
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
  data->main_width = data->width - data->left_bar_width - data->right_bar_width;
  data->main_height = data->height - data->top_bar_height - data->bottom_bar_height;


  /* Update the window camera */
  rig_camera_set_projection_mode (data->camera, RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (data->camera,
                                           0, 0, data->width, data->height);
  rig_camera_set_near_plane (data->camera, -1);
  rig_camera_set_far_plane (data->camera, 100);

  rig_camera_set_viewport (data->camera, 0, 0, data->width, data->height);

  screen_aspect = DEVICE_WIDTH / DEVICE_HEIGHT;
  main_aspect = data->main_width / data->main_height;

  //rig_transform_init_identity (data->screen_area_transform);

  if (screen_aspect < main_aspect) /* screen is slimmer and taller than the main area */
    {
      data->screen_area_height = data->main_height;
      data->screen_area_width = data->screen_area_height * screen_aspect;

      //rig_transform_translate (data->screen_area_transform,
      //                         (data->main_width / 2.0) - (data->screen_area_width / 2.0),
      //                         0, 0);
      rig_entity_set_translate (data->main_camera_screen_pos,
                                -(data->main_width / 2.0) + (data->screen_area_width / 2.0),
                                0, 0);
    }
  else
    {
      data->screen_area_width = data->main_width;
      data->screen_area_height = data->screen_area_width / screen_aspect;

#if 0
      rig_transform_translate (data->screen_area_transform,
                               0,
                               (data->main_height / 2.0) - (data->screen_area_height / 2.0),
                               0);
#endif
      rig_entity_set_translate (data->main_camera_screen_pos,
                                0,
                                -(data->main_height / 2.0) + (data->screen_area_height / 2.0),
                                0);
    }

  /* NB: We know the screen area matches the device aspect ratio so we can use
   * a uniform scale here... */
  device_scale = data->screen_area_width / DEVICE_WIDTH;

#if 0
  rig_transform_init_identity (data->device_transform);
  rig_transform_scale (data->device_transform,
                       device_scale,
                       device_scale,
                       device_scale);
#endif

  rig_entity_set_scale (data->main_camera_dev_scale, 1.0 / device_scale);

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
#if 0
    rig_camera_set_view_transform (data->main_camera_component, &data->main_view);
#endif

    rig_camera_set_projection_mode (data->main_camera_component,
                                    RIG_PROJECTION_PERSPECTIVE);
    rig_camera_set_field_of_view (data->main_camera_component, fovy);
    rig_camera_set_near_plane (data->main_camera_component, z_near);
    rig_camera_set_far_plane (data->main_camera_component, z_far);

#if 0
    cogl_matrix_init_identity (&data->main_view);
    rig_camera_set_projection_mode (data->main_camera_component,
                                    RIG_PROJECTION_ORTHOGRAPHIC);
    rig_camera_set_orthographic_coordinates (data->main_camera_component,
                                             0, 0, data->main_width, data->main_height);
    rig_camera_set_near_plane (data->main_camera_component, -1);
    rig_camera_set_far_plane (data->main_camera_component, 100);
#endif

    rig_camera_set_viewport (data->main_camera_component,
                             data->left_bar_width,
                             data->top_bar_height,
                             data->main_width,
                             data->main_height);

    rig_input_region_set_rectangle (data->main_input_region,
                                    data->left_bar_width,
                                    data->top_bar_height,
                                    data->left_bar_width + data->main_width,
                                    data->top_bar_height + data->main_height);

    /* Handle the z_2d translation by changing the length of the
     * camera's armature.
     */
    cogl_matrix_get_inverse (&data->main_view,
                             &inverse);
    cogl_matrix_transform_point (&inverse,
                                 &x, &y, &z_2d, &w);

    data->main_camera_z = z_2d / device_scale;
    rig_entity_set_translate (data->main_camera_armature, 0, 0, data->main_camera_z);
    //rig_entity_set_translate (data->main_camera_armature, 0, 0, 0);

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

      rig_entity_set_translate (data->main_camera_2d_view, -dx, -dy, -dz);
      rig_entity_set_rotation (data->main_camera_2d_view, &rotation);
      rig_entity_set_scale (data->main_camera_2d_view, 1.0 / scale);
    }
  }

#if 0
  data->origin[0] = data->main_width / 2;
  data->origin[1] = data->main_height / 2;
  data->origin[2] = 0;
  //data->origin[2] = data->z_2d;
#endif

  /* Setup projection for the timeline view */
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
  }

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


#if 0
  {
    CoglMatrix input_transform;
    cogl_matrix_init_identity (&input_transform);
    cogl_matrix_translate (&input_transform, 0, -data->top_bar_height, 0);

    //rig_camera_set_input_transform (data->main_camera_component, &input_transform);
  }
#endif

  rig_rectangle_set_width (data->top_bar_rect, data->width);
  rig_rectangle_set_height (data->top_bar_rect, data->top_bar_height - data->grab_margin);

  {
    float left_bar_height = data->height - data->top_bar_height - data->bottom_bar_height;

    rig_rectangle_set_width (data->left_bar_rect, data->left_bar_width - data->grab_margin);
    rig_rectangle_set_height (data->left_bar_rect,
                              data->height - data->top_bar_height - data->bottom_bar_height);

    rig_transform_init_identity (data->left_bar_transform);
    rig_transform_translate (data->left_bar_transform,
                             0, data->top_bar_height, 0);

    rig_ui_viewport_set_width (data->assets_vp, data->left_bar_width);
    rig_ui_viewport_set_height (data->assets_vp, left_bar_height);
  }

  {
    float right_bar_height = data->height - data->top_bar_height;

    rig_rectangle_set_width (data->right_bar_rect, data->right_bar_width - data->grab_margin);
    rig_rectangle_set_height (data->right_bar_rect, right_bar_height);

    rig_transform_init_identity (data->right_bar_transform);
    rig_transform_translate (data->right_bar_transform,
                             0, data->top_bar_height, 0);

    rig_ui_viewport_set_width (data->tool_vp, data->right_bar_width);
    rig_ui_viewport_set_height (data->tool_vp, right_bar_height);
  }

  rig_rectangle_set_width (data->bottom_bar_rect, data->width - data->right_bar_width);
  rig_rectangle_set_height (data->bottom_bar_rect, data->bottom_bar_height - data->grab_margin);


  rig_transform_init_identity (data->right_bar_transform);
  rig_transform_translate (data->right_bar_transform,
                           data->width - data->right_bar_width + data->grab_margin,
                           data->top_bar_height, 0);

  rig_transform_init_identity (data->main_transform);
  rig_transform_translate (data->main_transform, 0, data->top_bar_height, 0);

  rig_transform_init_identity (data->bottom_bar_transform);
  rig_transform_translate (data->bottom_bar_transform, 0, data->height - data->bottom_bar_height + data->grab_margin, 0);

  //rig_transform_init_identity (data->slider_transform);
  //rig_transform_translate (data->slider_transform, 0, data->bottom_bar_height - 20, 0);

  //rig_transform_init_identity (data->screen_area_transform);
  //rig_transform_translate (data->screen_area_transform, data->main_width / 3, 0, 0);

#if 0
  rig_transform_init_identity (data->pane1_transform);
  rig_transform_translate (data->pane1_transform, data->main_width / 3, 0, 0);

  rig_transform_init_identity (data->pane2_transform);
  rig_transform_translate (data->pane2_transform, (data->main_width / 3) * 2, 0, 0);
#endif

  //rig_slider_set_length (data->slider, data->width);

  rig_arcball_init (&data->arcball,
                    data->main_width / 2,
                    data->main_height / 2,
                    sqrtf (data->main_width * data->main_width + data->main_height * data->main_height) / 2);

  /* picking ray */
  data->picking_ray_color = cogl_pipeline_new (data->ctx->cogl_context);
  cogl_pipeline_set_color4f (data->picking_ray_color, 1.0, 0.0, 0.0, 1.0);
}

static void
data_onscreen_resize (CoglOnscreen *onscreen,
                      int width,
                      int height,
                      void *user_data)
{
  Data *data = user_data;

  data->width = width;
  data->height = height;

  rig_property_dirty (&data->ctx->property_ctx, &data->properties[DATA_PROP_WIDTH]);
  rig_property_dirty (&data->ctx->property_ctx, &data->properties[DATA_PROP_HEIGHT]);

  allocate (data);
}

CoglPipeline *
create_diffuse_specular_material (void)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglDepthState depth_state;

  pipeline = cogl_pipeline_new (rig_cogl_context);
  cogl_pipeline_set_color4f (pipeline, 0.8f, 0.8f, 0.8f, 1.f);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  /* set up our vertex shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat4 light_shadow_matrix;\n"
      "uniform mat3 normal_matrix;\n"
      "varying vec3 normal_direction, eye_direction;\n"
      "varying vec4 shadow_coords;\n",

      "normal_direction = normalize(normal_matrix * cogl_normal_in);\n"
      "eye_direction    = -vec3(cogl_modelview_matrix * cogl_position_in);\n"

      "shadow_coords = light_shadow_matrix * cogl_modelview_matrix *\n"
      "                cogl_position_in;\n"
);

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* and fragment shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* definitions */
      "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
      "uniform vec3 light0_direction_norm;\n"
      "varying vec3 normal_direction, eye_direction;\n",

      /* post */
      NULL);

  cogl_snippet_set_replace (snippet,
      "vec4 final_color = light0_ambient * cogl_color_in;\n"

      " vec3 L = light0_direction_norm;\n"
      " vec3 N = normalize(normal_direction);\n"

      "float lambert = dot(N, L);\n"

      "if (lambert > 0.0)\n"
      "{\n"
      "  final_color += cogl_color_in * light0_diffuse * lambert;\n"

      "  vec3 E = normalize(eye_direction);\n"
      "  vec3 R = reflect (-L, N);\n"
      "  float specular = pow (max(dot(R, E), 0.0),\n"
      "                        2.);\n"
      "  final_color += light0_specular * vec4(.6, .6, .6, 1.0) * specular;\n"
      "}\n"

      "shadow_coords_d = shadow_coords / shadow_coords.w;\n"
      "cogl_texel7 =  cogl_texture_lookup7 (cogl_sampler7, cogl_tex_coord_in[0]);\n"
      "float distance_from_light = cogl_texel7.z + 0.0005;\n"
      "float shadow = 1.0;\n"
      "if (shadow_coords.w > 0.0 && distance_from_light < shadow_coords_d.z)\n"
      "    shadow = 0.5;\n"

      "cogl_color_out = shadow * final_color;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  return pipeline;
}

static void
test_init (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  CoglFramebuffer *fb;
  float vector3[3];
  int i;
  char *full_path;
  GError *error = NULL;
  CoglTexture2D *color_buffer;
  CoglPipeline *root_pipeline;
  CoglSnippet *snippet;
  CoglColor color;
  RigMeshRenderer *mesh;
  RigMaterial *material;
  RigLight *light;
  RigCamera *camera;

  /* A unit test for the list_splice/list_unsplice functions */
#if 0
  _rig_test_list_splice ();
#endif

  cogl_matrix_init_identity (&data->identity);

  data->debug_pick_ray = 1;

  for (i = 0; i < DATA_N_PROPS; i++)
    rig_property_init (&data->properties[i],
                       &data_propert_specs[i],
                       data);

  data->onscreen = cogl_onscreen_new (data->ctx->cogl_context, 880, 660);
  cogl_onscreen_show (data->onscreen);

  /* FIXME: On SDL this isn't taking affect if set before allocating
   * the framebuffer. */
  cogl_onscreen_set_resizable (data->onscreen, TRUE);
  cogl_onscreen_add_resize_handler (data->onscreen, data_onscreen_resize, data);

  fb = COGL_FRAMEBUFFER (data->onscreen);
  data->width = cogl_framebuffer_get_width (fb);
  data->height  = cogl_framebuffer_get_height (fb);


  data->undo_journal = undo_journal_new (data);
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
  cogl_framebuffer_enable_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb),
                                         TRUE);
  /* FIXME: It doesn't seem right that we can query back the texture before
   * the framebuffer has been allocated. */
  data->shadow_map =
    cogl_framebuffer_get_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb));

  if (data->shadow_fb == NULL)
    g_critical ("could not create offscreen buffer");

  /* Hook the shadow sampling */
  root_pipeline = create_diffuse_specular_material ();
  cogl_pipeline_set_layer_texture (root_pipeline, 7, data->shadow_map);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              /* declarations */
                              "varying vec4 shadow_coords;\n"
                              "vec4 shadow_coords_d;\n",
                              /* post */
                              "");

  cogl_snippet_set_replace (snippet,
                            "cogl_texel = texture2D(cogl_sampler7, shadow_coords_d.st);\n");

  cogl_pipeline_add_layer_snippet (root_pipeline, 7, snippet);
  cogl_object_unref (snippet);

  data->root_pipeline = root_pipeline;
  data->default_pipeline = cogl_pipeline_new (data->ctx->cogl_context);

  data->diamond_slice_indices =
    cogl_indices_new (data->ctx->cogl_context,
                      COGL_INDICES_TYPE_UNSIGNED_BYTE,
                      _diamond_slice_indices_data,
                      sizeof (_diamond_slice_indices_data) /
                      sizeof (_diamond_slice_indices_data[0]));

  data->circle_texture = rig_create_circle_texture (data->ctx,
                                                    CIRCLE_TEX_RADIUS /* radius */,
                                                    CIRCLE_TEX_PADDING /* padding */);

  data->grid_prim = create_grid (data->ctx,
                                 DEVICE_WIDTH,
                                 DEVICE_HEIGHT,
                                 100,
                                 100);

  data->circle_node_attribute =
    rig_create_circle_fan_p2 (data->ctx, 20, &data->circle_node_n_verts);

  full_path = g_build_filename (RIG_SHARE_DIR, "light-bulb.png", NULL);
  //full_path = g_build_filename (RIG_HANDSET_SHARE_DIR, "nine-slice-test.png", NULL);
  data->light_icon = rig_load_texture (data->ctx, full_path, &error);
  g_free (full_path);
  if (!data->light_icon)
    {
      g_warning ("Failed to load light-bulb texture: %s", error->message);
      g_error_free (error);
    }

  data->timeline_vp = rig_ui_viewport_new (data->ctx, 0, 0, NULL);

  //data->screen_area_transform = rig_transform_new (data->ctx, NULL);

  data->device_transform = rig_transform_new (data->ctx, NULL);

  data->camera = rig_camera_new (data->ctx, COGL_FRAMEBUFFER (data->onscreen));
  rig_camera_set_clear (data->camera, FALSE);

  /* XXX: Basically just a hack for now. We should have a
   * RigShellWindow type that internally creates a RigCamera that can
   * be used when handling input events in device coordinates.
   */
  rig_shell_set_window_camera (shell, data->camera);

  data->timeline_camera = rig_camera_new (data->ctx, fb);
  rig_camera_set_clear (data->timeline_camera, FALSE);
  rig_shell_add_input_camera (shell, data->timeline_camera, NULL);
  data->timeline_scale = 1;
  data->timeline_len = 20;

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

  data->main_camera_to_origin = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->scene, data->main_camera_to_origin);
  rig_entity_set_label (data->main_camera_to_origin, "rig:camera_to_origin");

  data->main_camera_rotate = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->main_camera_to_origin, data->main_camera_rotate);
  rig_entity_set_label (data->main_camera_rotate, "rig:camera_rotate");

  data->main_camera_armature = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->main_camera_rotate, data->main_camera_armature);
  rig_entity_set_label (data->main_camera_armature, "rig:camera_armature");

  data->main_camera_origin_offset = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->main_camera_armature, data->main_camera_origin_offset);
  rig_entity_set_label (data->main_camera_origin_offset, "rig:camera_origin_offset");

  data->main_camera_dev_scale = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->main_camera_origin_offset, data->main_camera_dev_scale);
  rig_entity_set_label (data->main_camera_dev_scale, "rig:camera_dev_scale");

  data->main_camera_screen_pos = rig_entity_new (data->ctx, data->entity_next_id++);
  rig_graphable_add_child (data->main_camera_dev_scale, data->main_camera_screen_pos);
  rig_entity_set_label (data->main_camera_screen_pos, "rig:camera_screen_pos");

  data->main_camera_2d_view = rig_entity_new (data->ctx, data->entity_next_id++);
  //rig_graphable_add_child (data->main_camera_screen_pos, data->main_camera_2d_view); FIXME
  rig_entity_set_label (data->main_camera_2d_view, "rig:camera_2d_view");

  data->main_camera = rig_entity_new (data->ctx, data->entity_next_id++);
  //rig_graphable_add_child (data->main_camera_2d_view, data->main_camera); FIXME
  rig_graphable_add_child (data->main_camera_screen_pos, data->main_camera);
  rig_entity_set_label (data->main_camera, "rig:camera");

  data->origin[0] = DEVICE_WIDTH / 2;
  data->origin[1] = DEVICE_HEIGHT / 2;
  data->origin[2] = 0;

  rig_entity_translate (data->main_camera_to_origin,
                        data->origin[0],
                        data->origin[1],
                        data->origin[2]);
                        //DEVICE_WIDTH / 2, (DEVICE_HEIGHT / 2), 0);

  //rig_entity_rotate_z_axis (data->main_camera_to_origin, 45);

  rig_entity_translate (data->main_camera_origin_offset,
                        -DEVICE_WIDTH / 2, -(DEVICE_HEIGHT / 2), 0);

  /* FIXME: currently we also do a z translation due to using
   * cogl_matrix_view_2d_in_perspective, we should stop using that api so we can
   * do our z_2d translation here...
   *
   * XXX: should the camera_z transform be done for the negative translate?
   */
  //device scale = 0.389062494
  data->main_camera_z = 0.f;
  rig_entity_translate (data->main_camera_armature, 0, 0, data->main_camera_z);

#if 0
  {
    float pos[3] = {0, 0, 0};
    rig_entity_set_position (data->main_camera_rig, pos);
    rig_entity_translate (data->main_camera_rig, 100, 100, 0);
  }
#endif

  //rig_entity_translate (data->main_camera, 100, 100, 0);

#if 0
  data->main_camera_z = 20.f;
  vector3[0] = 0.f;
  vector3[1] = 0.f;
  vector3[2] = data->main_camera_z;
  rig_entity_set_position (data->main_camera, vector3);
#else
  data->main_camera_z = 10.f;
#endif

  data->main_camera_component = rig_camera_new (data->ctx, fb);
  rig_camera_set_clear (data->main_camera_component, FALSE);
  rig_entity_add_component (data->main_camera, data->main_camera_component);
  rig_shell_add_input_camera (shell,
                              data->main_camera_component,
                              data->scene);
                              //data->screen_area_transform);

  data->main_input_region =
    rig_input_region_new_rectangle (0, 0, 0, 0, main_input_region_cb, data);
  rig_input_region_set_hud_mode (data->main_input_region, TRUE);
  //rig_camera_add_input_region (data->camera,
  rig_camera_add_input_region (data->main_camera_component,
                               data->main_input_region);

  update_camera_position (data);

  /* plane */
  data->plane = rig_entity_new (data->ctx, data->entity_next_id++);
  //data->entities = g_list_prepend (data->entities, data->plane);
  //data->pickables = g_list_prepend (data->pickables, data->plane);
  rig_entity_set_cast_shadow (data->plane, FALSE);
  rig_entity_set_y (data->plane, -1.f);

  mesh = rig_mesh_renderer_new_from_template (data->ctx, "plane");
  rig_entity_add_component (data->plane, mesh);
  material = rig_material_new (data->ctx, NULL, NULL);
  rig_entity_add_component (data->plane, material);

  rig_graphable_add_child (data->scene, data->plane);

  /* 5 cubes */
  cogl_color_init_from_4f (&color, 0.6f, 0.6f, 0.6f, 1.0f);
  for (i = 0; i < N_CUBES; i++)
    {
      data->cubes[i] = rig_entity_new (data->ctx, data->entity_next_id++);

      rig_entity_set_scale (data->cubes[i], 50);
      //data->entities = g_list_prepend (data->entities, data->cubes[i]);
      //data->pickables = g_list_prepend (data->pickables, data->cubes[i]);

      rig_entity_set_cast_shadow (data->cubes[i], TRUE);
      rig_entity_set_x (data->cubes[i], 50 + i * 125);
      rig_entity_set_y (data->cubes[i], 50 + i * 100);
      rig_entity_set_z (data->cubes[i], 50);
#if 0
      rig_entity_set_y (data->cubes[i], .5);
      rig_entity_set_z (data->cubes[i], 1);
      rig_entity_rotate_y_axis (data->cubes[i], 10);
#endif

      mesh = rig_mesh_renderer_new_from_template (data->ctx, "cube");
      rig_entity_add_component (data->cubes[i], mesh);
      material = rig_material_new (data->ctx, NULL, &color);
      rig_entity_add_component (data->cubes[i], material);

      rig_graphable_add_child (data->scene, data->cubes[i]);
    }

  data->light = rig_entity_new (data->ctx, data->entity_next_id++);
  data->entities = g_list_prepend (data->entities, data->light);

  vector3[0] = 0;
  vector3[1] = 0;
  vector3[2] = 200;
  rig_entity_set_position (data->light, vector3);

  rig_entity_rotate_x_axis (data->light, 20);
  rig_entity_rotate_y_axis (data->light, -20);

  light = rig_light_new ();
  cogl_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
  rig_light_set_ambient (light, &color);
  cogl_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
  rig_light_set_diffuse (light, &color);
  cogl_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
  rig_light_set_specular (light, &color);
  rig_light_add_pipeline (light, root_pipeline);

  rig_entity_add_component (data->light, light);

  camera = rig_camera_new (data->ctx, COGL_FRAMEBUFFER (data->shadow_fb));
  data->shadow_map_camera = camera;

  rig_camera_set_background_color4f (camera, 0.f, .3f, 0.f, 1.f);
  rig_camera_set_projection_mode (camera,
                                  RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (camera,
                                           0, 0, 400, 400);
  rig_camera_set_near_plane (camera, 1.1f);
  rig_camera_set_far_plane (camera, 1000.f);

  rig_entity_add_component (data->light, camera);

  rig_graphable_add_child (data->scene, data->light);


  data->root =
    rig_graph_new (data->ctx,
                   (data->top_bar_transform =
                    rig_transform_new (data->ctx,
                                       (data->top_bar_rect =
                                        rig_rectangle_new4f (data->ctx, 0, 0,
                                                             0.2, 0.2, 0.2, 1)),
                                       NULL)
                   ),
                   (data->left_bar_transform =
                    rig_transform_new (data->ctx,
                                       (data->left_bar_rect =
                                        rig_rectangle_new4f (data->ctx, 0, 0,
                                                             0.2, 0.2, 0.2, 1)),
                                       (data->assets_vp =
                                        rig_ui_viewport_new (data->ctx,
                                                             0, 0,
                                                             NULL)),
                                       NULL)
                   ),
                   (data->right_bar_transform =
                    rig_transform_new (data->ctx,
                                       (data->right_bar_rect =
                                        rig_rectangle_new4f (data->ctx, 0, 0,
                                                             0.2, 0.2, 0.2, 1)),
                                       (data->tool_vp =
                                        rig_ui_viewport_new (data->ctx,
                                                             0, 0,
                                                             NULL)),
                                       NULL)
                   ),
                   (data->main_transform = rig_transform_new (data->ctx, NULL)
                   ),
                   (data->bottom_bar_transform =
                    rig_transform_new (data->ctx,
                                       (data->bottom_bar_rect =
                                        rig_rectangle_new4f (data->ctx, 0, 0,
                                                             0.2, 0.2, 0.2, 1)),
                                       NULL)
                   ),
                   NULL);


  rig_shell_add_input_camera (shell, data->camera, data->root);

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

  data->timeline_input_region =
    rig_input_region_new_rectangle (0, 0, 0, 0,
                                    timeline_region_input_cb,
                                    data);
  rig_camera_add_input_region (data->timeline_camera,
                               data->timeline_input_region);

  data->timeline = rig_timeline_new (data->ctx, 20.0);
  rig_timeline_set_loop_enabled (data->timeline, TRUE);
  rig_timeline_stop (data->timeline);

  data->timeline_elapsed =
    rig_introspectable_lookup_property (data->timeline, "elapsed");
  data->timeline_progress =
    rig_introspectable_lookup_property (data->timeline, "progress");

#if 0
  rig_property_set_binding (data->slider_progress,
                            update_slider_pos_cb,
                            data,
                            data->timeline_elapsed,
                            NULL);

  rig_property_set_binding (data->timeline_progress,
                            update_timeline_progress_cb,
                            data,
                            data->slider_progress,
                            NULL);
#endif

  /* tool */
  data->tool = rig_tool_new (data->shell);
  rig_tool_set_camera (data->tool, data->main_camera);

  allocate (data);

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

      load (data, ui);
      g_free (ui);
    }
#endif
}

static void
test_fini (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  int i;

  rig_ref_countable_unref (data->camera);
  rig_ref_countable_unref (data->root);

  for (i = 0; i < DATA_N_PROPS; i++)
    rig_property_destroy (&data->properties[i]);

  rig_ref_countable_unref (data->timeline_vp);

  cogl_object_unref (data->circle_texture);

  cogl_object_unref (data->grid_prim);
  cogl_object_unref (data->circle_node_attribute);

  cogl_object_unref (data->light_icon);

  //cogl_object_unref (data->rounded_tex);
}

static RigInputEventStatus
test_input_handler (RigInputEvent *event, void *user_data)
{
  Data *data = user_data;

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

typedef struct _SaveState
{
  Data *data;
  FILE *file;
  int indent;
  RigEntity *current_entity;
  int next_id;
  GHashTable *id_map;
} SaveState;

static void
save_component_cb (RigComponent *component,
                   void *user_data)
{
  const RigType *type = rig_object_get_type (component);
  SaveState *state = user_data;

  state->indent += INDENT_LEVEL;

  if (type == &rig_light_type)
    {
      RigLight *light = RIG_LIGHT (component);
      const CoglColor *ambient = rig_light_get_ambient (light);
      const CoglColor *diffuse = rig_light_get_diffuse (light);
      const CoglColor *specular = rig_light_get_specular (light);

      fprintf (state->file,
               "%*s<light "
               "ambient=\"#%02x%02x%02x%02x\" "
               "diffuse=\"#%02x%02x%02x%02x\" "
               "specular=\"#%02x%02x%02x%02x\"/>\n",
               state->indent, "",
               cogl_color_get_red_byte (ambient),
               cogl_color_get_green_byte (ambient),
               cogl_color_get_blue_byte (ambient),
               cogl_color_get_alpha_byte (ambient),
               cogl_color_get_red_byte (diffuse),
               cogl_color_get_green_byte (diffuse),
               cogl_color_get_blue_byte (diffuse),
               cogl_color_get_alpha_byte (diffuse),
               cogl_color_get_red_byte (specular),
               cogl_color_get_green_byte (specular),
               cogl_color_get_blue_byte (specular),
               cogl_color_get_alpha_byte (specular));
    }
  else if (type == &rig_material_type)
    {
      RigMaterial *material = RIG_MATERIAL (component);
      RigAsset *asset = rig_material_get_asset (material);

      fprintf (state->file, "%*s<material>\n", state->indent, "");
      state->indent += INDENT_LEVEL;

      if (asset)
        {
          int id = GPOINTER_TO_INT (g_hash_table_lookup (state->id_map, asset));
          if (id)
            {
              fprintf (state->file, "%*s<texture asset=\"%d\"/>\n",
                       state->indent, "",
                       id);
            }
        }

      state->indent -= INDENT_LEVEL;
      fprintf (state->file, "%*s</material>\n", state->indent, "");
    }
  else if (type == &rig_diamond_type)
    {
      fprintf (state->file, "%*s<diamond size=\"%f\"/>\n",
               state->indent, "",
               rig_diamond_get_size (RIG_DIAMOND (component)));
    }
  else if (type == &rig_mesh_renderer_type)
    {
      RigMeshRenderer *mesh = RIG_MESH_RENDERER (component);

      fprintf (state->file, "%*s<mesh", state->indent, "");

      switch (rig_mesh_renderer_get_type (mesh))
        {
        case RIG_MESH_RENDERER_TYPE_TEMPLATE:
          fprintf (state->file, " type=\"template\" template=\"%s\"",
                   rig_mesh_renderer_get_path (mesh));
          break;
        case RIG_MESH_RENDERER_TYPE_FILE:
          fprintf (state->file, " type=\"file\" path=\"%s\"",
                   rig_mesh_renderer_get_path (mesh));
          break;
        default:
          g_warn_if_reached ();
        }

      fprintf (state->file, " />\n");
    }

  state->indent -= INDENT_LEVEL;
}

static RigTraverseVisitFlags
_rig_entitygraph_pre_save_cb (RigObject *object,
                              int depth,
                              void *user_data)
{
  SaveState *state = user_data;
  const RigType *type = rig_object_get_type (object);
  RigObject *parent = rig_graphable_get_parent (object);
  RigEntity *entity;
  CoglQuaternion *q;
  float angle;
  float axis[3];
  const char *label;

  if (type != &rig_entity_type)
    {
      g_warning ("Can't save non-entity graphables\n");
      return RIG_TRAVERSE_VISIT_CONTINUE;
    }

  entity = object;

  g_hash_table_insert (state->id_map, entity, GINT_TO_POINTER (state->next_id));

  state->indent += INDENT_LEVEL;
  fprintf (state->file, "%*s<entity id=\"%d\"\n",
           state->indent, "",
           state->next_id++);

  if (parent && rig_object_get_type (parent) == &rig_entity_type)
    {
      int id = GPOINTER_TO_INT (g_hash_table_lookup (state->id_map, parent));
      if (id)
        {
          fprintf (state->file, "%*s        parent=\"%d\"\n",
                   state->indent, "",
                   id);
        }
      else
        g_warning ("Failed to find id of parent entity\n");
    }

  /* NB: labels with a "rig:" prefix imply that this is an internal
   * entity that shouldn't be saved (such as the editing camera
   * entities) */
  label = rig_entity_get_label (entity);
  if (label && strncmp ("rig:", label, 4) != 0)
    fprintf (state->file, "%*s        label=\"%s\"\n",
             state->indent, "",
             label);

  q = rig_entity_get_rotation (entity);

  angle = cogl_quaternion_get_rotation_angle (q);
  cogl_quaternion_get_rotation_axis (q, axis);

  fprintf (state->file,
           "%*s        position=\"(%f, %f, %f)\"\n"
           "%*s        rotation=\"[%f (%f, %f, %f)]]\">\n",
           state->indent, "",
           rig_entity_get_x (entity),
           rig_entity_get_y (entity),
           rig_entity_get_z (entity),
           state->indent, "",
           angle, axis[0], axis[1], axis[2]);

  state->current_entity = entity;
  rig_entity_foreach_component (entity,
                                save_component_cb,
                                state);

  fprintf (state->file, "%*s</entity>\n", state->indent, "");
  state->indent -= INDENT_LEVEL;

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static void
save (Data *data)
{
  struct stat sb;
  char *path = g_build_filename (_rig_project_dir, "ui.xml", NULL);
  FILE *file;
  SaveState state;
  GList *l;

  if (stat (_rig_project_dir, &sb) == -1)
    mkdir (_rig_project_dir, 0777);

  file = fopen (path, "w");
  if (!file)
    {
      g_warning ("Failed to open %s file for saving\n", path);
      return;
    }

  state.data = data;
  state.id_map = g_hash_table_new (g_direct_hash, g_direct_equal);
  state.file = file;
  state.indent = 0;

  fprintf (file, "<ui>\n");

  /* NB: We have to reserve 0 here so we can tell if lookups into the
   * id_map fail. */
  state.next_id = 1;

  /* Assets */

  for (l = data->assets; l; l = l->next)
    {
      RigAsset *asset = l->data;

      if (rig_asset_get_type (asset) != RIG_ASSET_TYPE_TEXTURE)
        continue;

      g_hash_table_insert (state.id_map, asset, GINT_TO_POINTER (state.next_id));

      state.indent += INDENT_LEVEL;
      fprintf (file, "%*s<asset id=\"%d\" type=\"texture\" path=\"%s\" />\n",
               state.indent, "",
               state.next_id++,
               rig_asset_get_path (asset));
      state.indent -= INDENT_LEVEL;
    }

  rig_graphable_traverse (data->scene,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_entitygraph_pre_save_cb,
                          NULL,
                          &state);

  for (l = data->transitions; l; l = l->next)
    {
      Transition *transition = l->data;
      GList *l2;
      //int i;

      state.indent += INDENT_LEVEL;
      fprintf (file, "%*s<transition id=\"%d\">\n", state.indent, "", transition->id);

      for (l2 = transition->paths; l2; l2 = l2->next)
        {
          Path *path = l2->data;
          GList *l3;
          RigEntity *entity;

          if (path == NULL)
            continue;

          entity = path->prop->object;

          state.indent += INDENT_LEVEL;
          fprintf (file, "%*s<path entity=\"%d\" property=\"%s\">\n", state.indent, "",
                   rig_entity_get_id (entity),
                   path->prop->spec->name);

          state.indent += INDENT_LEVEL;
          for (l3 = path->nodes.head; l3; l3 = l3->next)
            {
              switch (path->prop->spec->type)
                {
                case RIG_PROPERTY_TYPE_FLOAT:
                  {
                    NodeFloat *node = l3->data;
                    fprintf (file, "%*s<node t=\"%f\" value=\"%f\" />\n", state.indent, "", node->t, node->value);
                    break;
                  }
                case RIG_PROPERTY_TYPE_VEC3:
                  {
                    NodeVec3 *node = l3->data;
                    fprintf (file, "%*s<node t=\"%f\" value=\"(%f, %f, %f)\" />\n",
                             state.indent, "", node->t,
                             node->value[0],
                             node->value[1],
                             node->value[2]);
                    break;
                  }
                case RIG_PROPERTY_TYPE_QUATERNION:
                  {
                    NodeQuaternion *node = l3->data;
                    CoglQuaternion *q = &node->value;
                    float angle;
                    float axis[3];

                    angle = cogl_quaternion_get_rotation_angle (q);
                    cogl_quaternion_get_rotation_axis (q, axis);

                    fprintf (file, "%*s<node t=\"%f\" value=\"[%f (%f, %f, %f)]\" />\n", state.indent, "",
                             node->t, angle, axis[0], axis[1], axis[2]);
                    break;
                  }
                default:
                  g_warn_if_reached ();
                }
            }
          state.indent -= INDENT_LEVEL;

          fprintf (file, "%*s</path>\n", state.indent, "");
          state.indent -= INDENT_LEVEL;
        }

      fprintf (file, "%*s</transition>\n", state.indent, "");
      fprintf (file, "</ui>\n");
    }

  fclose (file);

  g_print ("File Saved\n");

  g_hash_table_destroy (state.id_map);
}

typedef struct _AssetInputClosure
{
  RigAsset *asset;
  Data *data;
} AssetInputClosure;

static RigInputEventStatus
asset_input_cb (RigInputRegion *region,
                RigInputEvent *event,
                void *user_data)
{
  AssetInputClosure *closure = user_data;
  RigAsset *asset = closure->asset;
  Data *data = closure->data;

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
add_asset_icon (Data *data,
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
free_asset_input_closures (Data *data)
{
  GList *l;

  for (l = data->asset_input_closures; l; l = l->next)
    g_slice_free (AssetInputClosure, l->data);
  g_list_free (data->asset_input_closures);
  data->asset_input_closures = NULL;
}

static void
update_asset_list (Data *data)
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
    add_asset_icon (data, l->data, 10 + 110 * i);

  //add_asset_icon (data, data->light_icon, 10 + 110 * i++, add_light_cb, NULL);
}

enum {
  LOADER_STATE_NONE,
  LOADER_STATE_LOADING_ENTITY,
  LOADER_STATE_LOADING_MATERIAL_COMPONENT,
  LOADER_STATE_LOADING_MESH_COMPONENT,
  LOADER_STATE_LOADING_DIAMOND_COMPONENT,
  LOADER_STATE_LOADING_LIGHT_COMPONENT,
  LOADER_STATE_LOADING_CAMERA_COMPONENT,
  LOADER_STATE_LOADING_TRANSITION,
  LOADER_STATE_LOADING_PATH
};

typedef struct _Loader
{
  Data *data;
  GQueue state;
  uint32_t id;
  CoglBool texture_specified;
  uint32_t texture_asset_id;

  GList *assets;
  GList *entities;
  GList *lights;
  GList *transitions;


  float diamond_size;
  RigEntity *current_entity;
  CoglBool is_light;

  Transition *current_transition;
  Path *current_path;

  GHashTable *id_map;
} Loader;

static void
loader_push_state (Loader *loader, int state)
{
  g_queue_push_tail (&loader->state, GINT_TO_POINTER (state));
}

static int
loader_get_state (Loader *loader)
{
  void *state = g_queue_peek_tail (&loader->state);
  return GPOINTER_TO_INT (state);
}

static void
loader_pop_state (Loader *loader)
{
  g_queue_pop_tail (&loader->state);
}

static RigEntity *
loader_find_entity (Loader *loader, uint32_t id)
{
  RigObject *object =
    g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id));
  if (object == NULL || rig_object_get_type (object) != &rig_entity_type)
    return NULL;
  return RIG_ENTITY (object);
}

static RigAsset *
loader_find_asset (Loader *loader, uint32_t id)
{
  RigObject *object =
    g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id));
  if (object == NULL || rig_object_get_type (object) != &rig_asset_type)
    return NULL;
  return RIG_ASSET (object);
}

static void
parse_start_element (GMarkupParseContext *context,
                     const char *element_name,
                     const char **attribute_names,
                     const char **attribute_values,
                     void *user_data,
                     GError **error)
{
  Loader *loader = user_data;
  Data *data = loader->data;
  int state = loader_get_state (loader);

  if (state == LOADER_STATE_NONE &&
      strcmp (element_name, "asset") == 0)
    {
      const char *id_str;
      const char *type;
      const char *path;
      uint32_t id;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "type",
                                        &type,
                                        G_MARKUP_COLLECT_STRING,
                                        "path",
                                        &path,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      id = g_ascii_strtoull (id_str, NULL, 10);
      if (g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id)))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate id %d", id);
          return;
        }

      if (strcmp (type, "texture") == 0)
        {
          RigAsset *asset = rig_asset_new_texture (data->ctx, path);
          loader->assets = g_list_prepend (loader->assets, asset);
          g_hash_table_insert (loader->id_map, GUINT_TO_POINTER (id), asset);
        }
      else
        g_warning ("Ignoring unknown asset type: %s\n", type);
    }
  else if (state == LOADER_STATE_NONE &&
           strcmp (element_name, "entity") == 0)
    {
      const char *id_str;
      unsigned int id;
      RigEntity *entity;
      const char *parent_id_str;
      const char *position_str;
      const char *rotation_str;
      const char *scale_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "parent",
                                        &parent_id_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "position",
                                        &position_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "rotation",
                                        &rotation_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "scale",
                                        &scale_str,
                                        G_MARKUP_COLLECT_INVALID))
      {
        return;
      }

      id = g_ascii_strtoull (id_str, NULL, 10);
      if (g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id)))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate entity id %d", id);
          return;
        }

      entity = rig_entity_new (loader->data->ctx, loader->data->entity_next_id++);

      if (parent_id_str)
        {
          unsigned int parent_id = g_ascii_strtoull (parent_id_str, NULL, 10);
          RigEntity *parent = loader_find_entity (loader, parent_id);

          if (!parent)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid parent id referenced in entity element");
              rig_ref_countable_unref (entity);
              return;
            }

          rig_graphable_add_child (parent, entity);
        }

      if (position_str)
        {
          float pos[3];
          if (sscanf (position_str, "(%f, %f, %f)",
                      &pos[0], &pos[1], &pos[2]) != 3)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid entity position");
              return;
            }
          rig_entity_set_position (entity, pos);
        }
      if (rotation_str)
        {
          float angle;
          float axis[3];
          CoglQuaternion q;

          if (sscanf (rotation_str, "[%f (%f, %f, %f)]",
                      &angle, &axis[0], &axis[1], &axis[2]) != 4)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid entity rotation");
              return;
            }
          cogl_quaternion_init_from_angle_vector (&q, angle, axis);
          rig_entity_set_rotation (entity, &q);
        }
      if (scale_str)
        {
          double scale = g_ascii_strtod (scale_str, NULL);
          rig_entity_set_scale (entity, scale);
        }

      loader->current_entity = entity;
      loader->is_light = FALSE;
      g_hash_table_insert (loader->id_map, GUINT_TO_POINTER (id), entity);

      loader_push_state (loader, LOADER_STATE_LOADING_ENTITY);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "material") == 0)
    {
      loader->texture_specified = FALSE;
      loader_push_state (loader, LOADER_STATE_LOADING_MATERIAL_COMPONENT);
#if 0
      const char *texture_id;

      g_markup_collect_attributes (element_name,
                                   attribute_names,
                                   attribute_values,
                                   error,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "texture",
                                   &texture_id,
                                   G_MARKUP_COLLECT_INVALID);
#endif
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "light") == 0)
    {
      const char *ambient_str;
      const char *diffuse_str;
      const char *specular_str;
      CoglColor ambient;
      CoglColor diffuse;
      CoglColor specular;
      RigLight *light;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "ambient",
                                        &ambient_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "diffuse",
                                        &diffuse_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "specular",
                                        &specular_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      rig_util_parse_color (loader->data->ctx, ambient_str, &ambient);
      rig_util_parse_color (loader->data->ctx, diffuse_str, &diffuse);
      rig_util_parse_color (loader->data->ctx, specular_str, &specular);

      light = rig_light_new ();
      rig_light_set_ambient (light, &ambient);
      rig_light_set_diffuse (light, &diffuse);
      rig_light_set_specular (light, &specular);

      rig_entity_add_component (loader->current_entity, light);
      loader->is_light = TRUE;

      //loader_push_state (loader, LOADER_STATE_LOADING_LIGHT_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "diamond") == 0)
    {
      const char *size_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "size",
                                        &size_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->diamond_size = g_ascii_strtod (size_str, NULL);

      loader_push_state (loader, LOADER_STATE_LOADING_DIAMOND_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "mesh") == 0)
    {
      const char *type_str;
      const char *template_str;
      const char *path_str;
      RigMeshRenderer *mesh;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "type",
                                        &type_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "template",
                                        &template_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "path",
                                        &path_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      if (strcmp (type_str, "template") == 0)
        {
          if (!template_str)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Missing mesh template name");
              return;
            }
          mesh = rig_mesh_renderer_new_from_template (loader->data->ctx,
                                                      template_str);
        }
      else if (strcmp (type_str, "file") == 0)
        {
          if (!path_str)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Missing mesh path name");
              return;
            }
          mesh = rig_mesh_renderer_new_from_file (loader->data->ctx, path_str);
        }
      else
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid mesh type \"%s\"", type_str);
          return;
        }

      if (mesh)
        rig_entity_add_component (loader->current_entity, mesh);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "texture") == 0)
    {
      const char *id_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "asset",
                                        &id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->texture_specified = TRUE;
      loader->texture_asset_id = g_ascii_strtoull (id_str, NULL, 10);
    }
  else if (state == LOADER_STATE_NONE &&
           strcmp (element_name, "transition") == 0)
    {
      const char *id_str;
      uint32_t id;

      loader_push_state (loader, LOADER_STATE_LOADING_TRANSITION);

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      id = g_ascii_strtoull (id_str, NULL, 10);

      loader->current_transition = transition_new (loader->data, id);
      loader->transitions = g_list_prepend (loader->transitions, loader->current_transition);
    }
  else if (state == LOADER_STATE_LOADING_TRANSITION &&
           strcmp (element_name, "path") == 0)
    {
      const char *entity_id_str;
      uint32_t entity_id;
      RigEntity *entity;
      const char *property_name;
      RigProperty *prop;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "entity",
                                        &entity_id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "property",
                                        &property_name,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      entity_id = g_ascii_strtoull (entity_id_str, NULL, 10);

      entity = loader_find_entity (loader, entity_id);
      if (!entity)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid Entity id %d referenced in path element",
                       entity_id);
          return;
        }

      prop = rig_introspectable_lookup_property (entity, property_name);
      if (!prop)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid Entity property referenced in path element");
          return;
        }

      loader->current_path =
        path_new_for_property (data->ctx,
                               &loader->current_transition->props[TRANSITION_PROP_PROGRESS],
                               prop);

      loader_push_state (loader, LOADER_STATE_LOADING_PATH);
    }
  else if (state == LOADER_STATE_LOADING_PATH &&
           strcmp (element_name, "node") == 0)
    {
      const char *t_str;
      float t;
      const char *value_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "t",
                                        &t_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "value",
                                        &value_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      t = g_ascii_strtod (t_str, NULL);

      switch (loader->current_path->prop->spec->type)
        {
        case RIG_PROPERTY_TYPE_FLOAT:
          {
            float value = g_ascii_strtod (value_str, NULL);
            path_insert_float (loader->current_path, t, value);
            break;
          }
        case RIG_PROPERTY_TYPE_VEC3:
          {
            float value[3];
            if (sscanf (value_str, "(%f, %f, %f)",
                        &value[0], &value[1], &value[2]) != 3)
              {
                g_set_error (error,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "Invalid vec3 value");
                return;
              }
            path_insert_vec3 (loader->current_path, t, value);
            break;
          }
        case RIG_PROPERTY_TYPE_QUATERNION:
          {
            float angle, x, y, z;

            if (sscanf (value_str, "[%f (%f, %f, %f)]", &angle, &x, &y, &z) != 4)
              {
                g_set_error (error,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "Invalid rotation value");
                return;
              }

            path_insert_quaternion (loader->current_path, t, angle, x, y, z);
            break;
          }
        }
    }
}

static void
parse_end_element (GMarkupParseContext *context,
                   const char *element_name,
                   void *user_data,
                   GError **error)
{
  Loader *loader = user_data;
  int state = loader_get_state (loader);

  if (state == LOADER_STATE_LOADING_ENTITY &&
      strcmp (element_name, "entity") == 0)
    {
      if (loader->is_light)
        loader->lights = g_list_prepend (loader->entities, loader->current_entity);
      else
        loader->entities = g_list_prepend (loader->entities, loader->current_entity);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_DIAMOND_COMPONENT &&
           strcmp (element_name, "diamond") == 0)
    {
      RigMaterial *material =
        rig_entity_get_component (loader->current_entity,
                                  RIG_COMPONENT_TYPE_MATERIAL);
      RigAsset *asset = NULL;
      CoglTexture *texture = NULL;
      RigDiamond *diamond;

      /* We need to know the size of the texture before we can create
       * a diamond component */
      if (material)
        asset = rig_material_get_asset (material);

      if (asset)
        texture = rig_asset_get_texture (asset);

      if (!texture)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Can't add diamond component without a texture");
          return;
        }

      diamond = rig_diamond_new (loader->data->ctx,
                                 loader->diamond_size,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture));
      rig_entity_add_component (loader->current_entity,
                                diamond);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "material") == 0)
    {
      RigMaterial *material;
      RigAsset *texture_asset;

      if (loader->texture_specified)
        {
          texture_asset = loader_find_asset (loader, loader->texture_asset_id);
          if (!texture_asset)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid asset id");
              return;
            }
        }
      else
        texture_asset = NULL;

      material = rig_material_new (loader->data->ctx, texture_asset, NULL);
      rig_entity_add_component (loader->current_entity, material);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_TRANSITION &&
           strcmp (element_name, "transition") == 0)
    {
      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_PATH &&
           strcmp (element_name, "path") == 0)
    {
      transition_add_path (loader->current_transition, loader->current_path);
      loader_pop_state (loader);
    }
}

static void
parse_error (GMarkupParseContext *context,
             GError *error,
             void *user_data)
{

}

static void
free_ux (Data *data)
{
  GList *l;

  for (l = data->transitions; l; l = l->next)
    transition_free (l->data);
  g_list_free (data->transitions);
  data->transitions = NULL;

  for (l = data->assets; l; l = l->next)
    rig_ref_countable_unref (l->data);
  g_list_free (data->assets);
  data->assets = NULL;

  free_asset_input_closures (data);
}

static void
load (Data *data, const char *file)
{
  GMarkupParser parser = {
    .start_element = parse_start_element,
    .end_element = parse_end_element,
    .error = parse_error
  };
  Loader loader;
  char *contents;
  gsize len;
  GError *error = NULL;
  GMarkupParseContext *context;
  GList *l;

  memset (&loader, 0, sizeof (loader));
  loader.data = data;
  g_queue_init (&loader.state);
  loader_push_state (&loader, LOADER_STATE_NONE);
  g_queue_push_tail (&loader.state, LOADER_STATE_NONE);

  loader.id_map = g_hash_table_new (g_direct_hash, g_direct_equal);

  if (!g_file_get_contents (file,
                            &contents,
                            &len,
                            &error))
    {
      g_warning ("Failed to load ui description: %s", error->message);
      return;
    }

  context = g_markup_parse_context_new (&parser, 0, &loader, NULL);

  if (!g_markup_parse_context_parse (context, contents, len, &error))
    {
      g_warning ("Failed to parse ui description: %s", error->message);
      g_markup_parse_context_free (context);
    }

  g_queue_clear (&loader.state);

  free_ux (data);

  for (l = loader.entities; l; l = l->next)
    {
      if (rig_graphable_get_parent (l->data) == NULL)
        rig_graphable_add_child (data->scene, l->data);
    }

  g_list_free (data->lights);
  data->lights = loader.lights;

  data->transitions = loader.transitions;
  if (data->transitions)
    data->selected_transition = loader.transitions->data;

  data->assets = loader.assets;

  update_asset_list (data);

  rig_shell_queue_redraw (data->ctx->shell);

  g_hash_table_destroy (loader.id_map);

  g_print ("File Loaded\n");
}

static void
init_types (void)
{
  _transition_type_init ();
  _diamond_slice_init_type ();
}

#ifdef __ANDROID__

void
android_main (struct android_app *application)
{
  Data data;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&data, 0, sizeof (Data));
  data.app = application;

  init_types ();

  data.shell = rig_android_shell_new (application,
                                      test_init,
                                      test_fini,
                                      test_paint,
                                      &data);

  data.ctx = rig_context_new (data.shell);

  rig_context_init (data.ctx);

  rig_shell_set_input_callback (data.shell, test_input_handler, &data);

  rig_shell_main (data.shell);
}

#else

int
main (int argc, char **argv)
{
  Data data;
  GOptionContext *context = g_option_context_new (NULL);
  GError *error = NULL;

  g_option_context_add_main_entries (context, rig_handset_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("option parsing failed: %s\n", error->message);
      exit(EXIT_FAILURE);
    }

  memset (&data, 0, sizeof (Data));

  init_types ();

  data.shell = rig_shell_new (test_init, test_fini, test_paint, &data);

  data.ctx = rig_context_new (data.shell);

  rig_context_init (data.ctx);

  rig_shell_add_input_callback (data.shell, test_input_handler, &data, NULL);

  rig_shell_main (data.shell);

  return 0;
}

#endif
