
#include <glib.h>

#include <cogl/cogl.h>

#include "rig.h"
#include "rig-camera-private.h"
#include "rig-transform-private.h"
#include "rig-shell.h"

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include <glib-android/glib-android.h>
#elif defined (USE_SDL)
#include "rig-sdl-keysyms.h"
#endif

struct _RigShell
{
  RigObjectProps _parent;

  int ref_count;

  CoglBool quit;

#ifdef __ANDROID__
  struct android_app* app;
#endif

  RigContext *rig_ctx;

  RigShellInitCallback init_cb;
  RigShellFiniCallback fini_cb;
  RigShellPaintCallback paint_cb;
  void *user_data;

  RigInputCallback input_cb;
  void *input_data;
  GList *input_cameras;

  GList *input_regions;
  RigInputCallback grab_cb;
  void *grab_data;

  CoglBool redraw_queued;
};

/* PRIVATE */
typedef enum _RigShapeType
{
  RIG_SHAPE_TYPE_RECTANGLE,
  RIG_SHAPE_TYPE_CIRCLE
} RigShapeType;

/* PRIVATE */
typedef struct _RigShapeRectangle
{
  RigShapeType type;
  float x0, y0, x1, y1;
} RigShapeRectange;

/* PRIVATE */
typedef struct _RigShapeCircle
{
  RigShapeType type;
  float x, y;
  float r_squared;
} RigShapeCircle;

/* PRIVATE */
typedef struct _RigShapeAny
{
  RigShapeType type;
} RigShapeAny;

/* PRIVATE */
typedef union _RigShape
{
  RigShapeAny any;
  RigShapeRectange rectangle;
  RigShapeCircle circle;
} RigShape;

typedef enum _RigInputTransformType
{
  RIG_INPUT_TRANSFORM_TYPE_NONE,
  RIG_INPUT_TRANSFORM_TYPE_MATRIX,
  RIG_INPUT_TRANSFORM_TYPE_GRAPHABLE
} RigInputTransformType;

typedef struct _RigInputTransformAny
{
  RigInputTransformType type;
} RigInputTransformAny;

typedef struct _RigInputTransformMatrix
{
  RigInputTransformType type;
  CoglMatrix *matrix;
} RigInputTransformMatrix;

typedef struct _RigInputTransformGraphable
{
  RigInputTransformType type;
  RigObject *graphable;
} RigInputTransformGraphable;

typedef union _RigInputTransform
{
  RigInputTransformAny any;
  RigInputTransformMatrix matrix;
  RigInputTransformGraphable graphable;
} RigInputTransform;

struct _RigInputRegion
{
  RigObjectProps _parent;

  int ref_count;

  /* PRIVATE */
  RigInputTransform transform;

  RigShape shape;

  RigGraphableProps graphable;

  RigInputRegionCallback callback;
  void *user_data;
};


static void
_rig_scroll_bar_init_type (void);

static void
_rig_slider_init_type (void);

static void
_rig_input_region_init_type (void);

RigContext *
rig_shell_get_context (RigShell *shell)
{
  return shell->rig_ctx;
}

static void
_rig_shell_fini (RigShell *shell)
{
  shell->fini_cb (shell, shell->user_data);

  rig_ref_countable_simple_unref (shell->rig_ctx);
}

struct _RigInputEvent
{
  void *native;
  const CoglMatrix *input_transform;
};

/* XXX: The vertices must be 4 components: [x, y, z, w] */
static void
fully_transform_points (const CoglMatrix *modelview,
                        const CoglMatrix *projection,
                        const float *viewport,
                        float *verts,
                        int n_verts)
{
  int i;

  cogl_matrix_transform_points (modelview,
                                2, /* n_components */
                                sizeof (float) * 4, /* stride_in */
                                verts, /* points_in */
                                /* strideout */
                                sizeof (float) * 4,
                                verts, /* points_out */
                                4 /* n_points */);

  cogl_matrix_project_points (projection,
                              3, /* n_components */
                              sizeof (float) * 4, /* stride_in */
                              verts, /* points_in */
                              /* strideout */
                              sizeof (float) * 4,
                              verts, /* points_out */
                              4 /* n_points */);

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

  /* Scale from normalized device coordinates (in range [-1,1]) to
   * window coordinates ranging [0,window-size] ... */
  for (i = 0; i < n_verts; i++)
    {
      float w = verts[4 * i + 3];

      /* Perform perspective division */
      verts[4 * i] /= w;
      verts[4 * i + 1] /= w;

      /* Apply viewport transform */
      verts[4 * i] = VIEWPORT_TRANSFORM_X (verts[4 * i],
                                           viewport[0], viewport[2]);
      verts[4 * i + 1] = VIEWPORT_TRANSFORM_Y (verts[4 * i + 1],
                                               viewport[1], viewport[3]);
    }

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y
}

/* Given an (x0,y0) (x1,y1) rectangle this transforms it into
 * a polygon in window coordinates that can be intersected
 * with input coordinates for picking.
 */
static void
rect_to_screen_polygon (RigShapeRectange *rectangle,
                        const CoglMatrix *modelview,
                        const CoglMatrix *projection,
                        const float *viewport,
                        float *poly)
{
  poly[0] = rectangle->x0;
  poly[1] = rectangle->y0;
  poly[2] = 0;
  poly[3] = 1;

  poly[4] = rectangle->x0;
  poly[5] = rectangle->y1;
  poly[6] = 0;
  poly[7] = 1;

  poly[8] = rectangle->x1;
  poly[9] = rectangle->y1;
  poly[10] = 0;
  poly[11] = 1;

  poly[12] = rectangle->x1;
  poly[13] = rectangle->y0;
  poly[14] = 0;
  poly[15] = 1;

  fully_transform_points (modelview,
                          projection,
                          viewport,
                          poly,
                          4);
}

/* This is a replacement for the nearbyint function which always
   rounds to the nearest integer. nearbyint is apparently a C99
   function so it might not always be available but also it seems in
   glibc it is defined as a function call so this macro could end up
   faster anyway. We can't just add 0.5f because it will break for
   negative numbers. */
#define UTIL_NEARBYINT(x) ((int) ((x) < 0.0f ? (x) - 0.5f : (x) + 0.5f))

/* We've made a notable change to the original algorithm referenced
 * above to make sure we have reliable results for screen aligned
 * rectangles even though there may be some numerical in-precision in
 * how the vertices of the polygon were calculated.
 *
 * We've avoided introducing an epsilon factor to the comparisons
 * since we feel there's a risk of changing some semantics in ways that
 * might not be desirable. One of those is that if you transform two
 * polygons which share an edge and test a point close to that edge
 * then this algorithm will currently give a positive result for only
 * one polygon.
 *
 * Another concern is the way this algorithm resolves the corner case
 * where the horizontal ray being cast to count edge crossings may
 * cross directly through a vertex. The solution is based on the "idea
 * of Simulation of Simplicity" and "pretends to shift the ray
 * infinitesimally down so that it either clearly intersects, or
 * clearly doesn't touch". I'm not familiar with the idea myself so I
 * expect a misplaced epsilon is likely to break that aspect of the
 * algorithm.
 *
 * The simple solution we've gone for is to pixel align the polygon
 * vertices which should eradicate most noise due to in-precision.
 */
static int
point_in_screen_poly (float point_x,
                      float point_y,
                      void *vertices,
                      size_t stride,
                      int n_vertices)
{
  int i, j, c = 0;

  for (i = 0, j = n_vertices - 1; i < n_vertices; j = i++)
    {
      float vert_xi = *(float *)((uint8_t *)vertices + i * stride);
      float vert_xj = *(float *)((uint8_t *)vertices + j * stride);
      float vert_yi = *(float *)((uint8_t *)vertices + i * stride +
                                 sizeof (float));
      float vert_yj = *(float *)((uint8_t *)vertices + j * stride +
                                 sizeof (float));

      vert_xi = UTIL_NEARBYINT (vert_xi);
      vert_xj = UTIL_NEARBYINT (vert_xj);
      vert_yi = UTIL_NEARBYINT (vert_yi);
      vert_yj = UTIL_NEARBYINT (vert_yj);

      if (((vert_yi > point_y) != (vert_yj > point_y)) &&
           (point_x < (vert_xj - vert_xi) * (point_y - vert_yi) /
            (vert_yj - vert_yi) + vert_xi) )
         c = !c;
    }

  return c;
}

static CoglBool
rig_camera_pick_input_region (RigCamera *camera,
                              RigInputRegion *region,
                              float x,
                              float y)
{
  CoglMatrix matrix;
  CoglMatrix *modelview;
  float poly[16];

  switch (region->transform.any.type)
    {
    case RIG_INPUT_TRANSFORM_TYPE_GRAPHABLE:
      rig_graphable_get_transform (region->transform.graphable.graphable, &matrix);
      modelview = &matrix;
      break;
    case RIG_INPUT_TRANSFORM_TYPE_MATRIX:
      if (region->transform.matrix.matrix)
        {
          cogl_matrix_multiply (&matrix,
                                region->transform.matrix.matrix,
                                &camera->view);
          modelview = &matrix;
        }
      else
        modelview = &camera->view;
      break;
    case RIG_INPUT_TRANSFORM_TYPE_NONE:
      cogl_matrix_init_identity (&matrix);
      modelview = &matrix;
    }

  switch (region->shape.any.type)
    {
    case RIG_SHAPE_TYPE_RECTANGLE:
      {
        rect_to_screen_polygon (&region->shape.rectangle,
                                modelview,
                                &camera->projection,
                                camera->viewport,
                                poly);

#if 0
        g_print ("transformed input region\n");
        for (i = 0; i < 4; i++)
          {
            float *p = poly + 4 * i;
            g_print ("poly[].x=%f\n", p[0]);
            g_print ("poly[].y=%f\n", p[1]);
          }
        g_print ("x=%f y=%f\n", x, y);
#endif
        if (point_in_screen_poly (x, y, poly, sizeof (float) * 4, 4))
          return TRUE;
        else
          return FALSE;
      }
    case RIG_SHAPE_TYPE_CIRCLE:
      {
        RigShapeCircle *circle = &region->shape.circle;
        float center_x = circle->x;
        float center_y = circle->y;
        float z = 0;
        float w = 1;
        float a;
        float b;
        float c2;

        /* Note the circle hit regions are billboarded, such that only the center
         * point is transformed but the raius of the circle stays constant.
         */

        cogl_matrix_transform_point (modelview, &center_x, &center_y, &z, &w);

        a = x - center_x;
        b = y - center_y;
        c2 = a * a + b * b;

        if (c2 < circle->r_squared)
          return TRUE;
        else
          return FALSE;
      }
    }

  g_warn_if_reached ();
  return FALSE;
}

static void
_rig_input_region_free (void *object)
{
  RigInputRegion *region = object;

  switch (region->transform.any.type)
    {
    case RIG_INPUT_TRANSFORM_TYPE_GRAPHABLE:
      rig_ref_countable_simple_unref (region->transform.graphable.graphable);
      break;
    default:
      break;
    }

  g_slice_free (RigInputRegion, region);
}

static RigRefCountableVTable _rig_input_region_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_input_region_free
};

static void
_rig_input_region_graphable_child_removed (RigObject *self,
                                           RigObject *child)
{
  /* nop */
}

static void
_rig_input_region_graphable_child_added (RigObject *self,
                                         RigObject *child)
{
  /* nop */
}

static void
_rig_input_region_graphable_parent_changed (RigObject *self,
                                            RigObject *old_parent,
                                            RigObject *new_parent)
{
  /* nop */
}

static RigGraphableVTable _rig_input_region_graphable_vtable = {
  _rig_input_region_graphable_child_removed,
  _rig_input_region_graphable_child_added,
  _rig_input_region_graphable_parent_changed
};

RigType rig_input_region_type;

static void
_rig_input_region_init_type (void)
{
  rig_type_init (&rig_input_region_type);
  rig_type_add_interface (&rig_input_region_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigInputRegion, ref_count),
                          &_rig_input_region_ref_countable_vtable);
  rig_type_add_interface (&rig_input_region_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigInputRegion, graphable),
                          &_rig_input_region_graphable_vtable);
}

RigInputRegion *
rig_input_region_new_rectangle (float x0,
                                float y0,
                                float x1,
                                float y1,
                                RigInputRegionCallback callback,
                                void *user_data)
{
  RigInputRegion *region = g_slice_new0 (RigInputRegion);

  rig_object_init (&region->_parent, &rig_input_region_type);

  region->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (region));

  region->transform.any.type = RIG_INPUT_TRANSFORM_TYPE_MATRIX;
  region->transform.matrix.matrix = NULL;

  region->shape.any.type = RIG_SHAPE_TYPE_RECTANGLE;
  region->shape.rectangle.x0 = x0;
  region->shape.rectangle.y0 = y0;
  region->shape.rectangle.x1 = x1;
  region->shape.rectangle.y1 = y1;
  region->callback = callback;
  region->user_data = user_data;

  return region;
}

void
rig_input_region_set_rectangle (RigInputRegion *region,
                                float x0,
                                float y0,
                                float x1,
                                float y1)
{
  region->shape.any.type = RIG_SHAPE_TYPE_RECTANGLE;
  region->shape.rectangle.x0 = x0;
  region->shape.rectangle.y0 = y0;
  region->shape.rectangle.x1 = x1;
  region->shape.rectangle.y1 = y1;
}

void
rig_input_region_set_graphable (RigInputRegion *region,
                                RigObject *graphable)
{
  region->transform.any.type = RIG_INPUT_TRANSFORM_TYPE_GRAPHABLE;
  region->transform.graphable.graphable = rig_ref_countable_simple_ref (graphable);
}

void
rig_input_region_set_transform (RigInputRegion *region,
                                CoglMatrix *matrix)
{
  region->transform.any.type = RIG_INPUT_TRANSFORM_TYPE_MATRIX;
  region->transform.matrix.matrix = matrix;
}

void
rig_shell_add_input_region (RigShell *shell,
                            RigInputRegion *region)
{
  shell->input_regions = g_list_prepend (shell->input_regions, region);
}

void
rig_shell_remove_input_region (RigShell *shell,
                               const RigInputRegion *region)
{
  shell->input_regions = g_list_remove (shell->input_regions, region);
}

void
rig_shell_set_input_callback (RigShell *shell,
                              RigInputCallback callback,
                              void *user_data)
{
  shell->input_cb = callback;
  shell->input_data = user_data;
}

void
rig_shell_add_input_camera (RigShell *shell,
                            RigCamera *camera)
{
  shell->input_cameras = g_list_prepend (shell->input_cameras, camera);
}

void
rig_shell_remove_input_camera (RigShell *shell,
                               RigCamera *camera)
{
  shell->input_cameras = g_list_remove (shell->input_cameras, camera);
}

RigInputEventType
rig_input_event_get_type (RigInputEvent *event)
{
#ifdef __ANDROID__
  AInputEvent *android_event = event->native;
  switch (AInputEvent_getType (android_event))
    {
    case AINPUT_EVENT_TYPE_MOTION:
      return RIG_INPUT_EVENT_TYPE_MOTION;
    case AINPUT_EVENT_TYPE_KEY:
      return RIG_INPUT_EVENT_TYPE_KEY;
    default:
      g_warn_if_reached (); /* Un-supported input type */
      return 0;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEMOTION:
      return RIG_INPUT_EVENT_TYPE_MOTION;
    case SDL_KEYUP:
    case SDL_KEYDOWN:
      return RIG_INPUT_EVENT_TYPE_KEY;
    default:
      g_warn_if_reached (); /* Un-supported input type */
      return 0;
    }
#else
#error "Unknown input system"
#endif
}

int32_t
rig_key_event_get_keysym (RigInputEvent *event)
{
#ifdef __ANDROID__
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return _rig_keysym_from_sdl_keysym (sdl_event->key.keysym.sym);
#else
#error "Unknown input system"
#endif
}

RigKeyEventAction
rig_key_event_get_action (RigInputEvent *event)
{
#ifdef __ANDROID__
  switch (AKeyEvent_getAction (event->native))
    {
    case AKEY_EVENT_ACTION_DOWN:
      return RIG_KEY_EVENT_ACTION_DOWN;
    case AKEY_EVENT_ACTION_UP:
      return RIG_KEY_EVENT_ACTION_UP;
    case AKEY_EVENT_ACTION_MULTIPLE:
      g_warn_if_reached ();
      /* TODO: Expand these out in android_handle_event into multiple distinct events,
       * it seems odd to require app developers to have to have special code for this
       * and key events are surely always low frequency enough that we don't need
       * this for optimization purposes.
       */
      return RIG_KEY_EVENT_ACTION_UP;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_KEYUP:
      return RIG_KEY_EVENT_ACTION_UP;
    case SDL_KEYDOWN:
      return RIG_KEY_EVENT_ACTION_DOWN;
    default:
      g_warn_if_reached ();
      return RIG_KEY_EVENT_ACTION_UP;
    }
#endif
}

RigMotionEventAction
rig_motion_event_get_action (RigInputEvent *event)
{
#ifdef __ANDROID__
  switch (AMotionEvent_getAction (event->native))
    {
    case AMOTION_EVENT_ACTION_DOWN:
      return RIG_MOTION_EVENT_ACTION_DOWN;
    case AMOTION_EVENT_ACTION_UP:
      return RIG_MOTION_EVENT_ACTION_UP;
    case AMOTION_EVENT_ACTION_MOVE:
      return RIG_MOTION_EVENT_ACTION_MOVE;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
      return RIG_MOTION_EVENT_ACTION_DOWN;
    case SDL_MOUSEBUTTONUP:
      return RIG_MOTION_EVENT_ACTION_UP;
    case SDL_MOUSEMOTION:
      return RIG_MOTION_EVENT_ACTION_MOVE;
    default:
      g_warn_if_reached (); /* Not a motion event */
      return RIG_MOTION_EVENT_ACTION_MOVE;
    }
#else
#error "Unknown input system"
#endif
}

RigButtonState
rig_button_state_for_sdl_state (SDL_Event *event,
                                uint8_t    sdl_state)
{
  RigButtonState rig_state = 0;
  if (sdl_state & SDL_BUTTON(1))
    rig_state |= RIG_BUTTON_STATE_1;
  if (sdl_state & SDL_BUTTON(2))
    rig_state |= RIG_BUTTON_STATE_2;
  if (sdl_state & SDL_BUTTON(3))
    rig_state |= RIG_BUTTON_STATE_3;
  if ((event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEBUTTONDOWN) &&
      event->button.button == SDL_BUTTON_WHEELUP)
    rig_state |= RIG_BUTTON_STATE_WHEELUP;
  if ((event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEBUTTONDOWN) &&
      event->button.button == SDL_BUTTON_WHEELDOWN)
    rig_state |= RIG_BUTTON_STATE_WHEELDOWN;

  return rig_state;
}

RigButtonState
rig_motion_event_get_button_state (RigInputEvent *event)
{
#ifdef __ANDROID__
  return 0;
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return rig_button_state_for_sdl_state (sdl_event,
                                         SDL_GetMouseState (NULL, NULL));
#if 0
  /* FIXME: we need access to the RigContext here so that
   * we can statefully track the changes to the button
   * mask because the button up and down events don't
   * tell you what other buttons are currently down they
   * only tell you what button changed.
   */

  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      switch (sdl_event->motion.button)
        {
        case 1:
          return RIG_BUTTON_STATE_1;
        case 2:
          return RIG_BUTTON_STATE_2;
        case 3:
          return RIG_BUTTON_STATE_3;
        case 4:
          return RIG_BUTTON_STATE_WHEELUP;
        case 5:
          return RIG_BUTTON_STATE_WHEELDOWN;
        default:
          g_warning ("Out of range SDL button number");
          return 0;
        }
    case SDL_MOUSEMOTION:
      return rig_button_state_for_sdl_state (sdl_event->button.state);
    default:
      g_warn_if_reached (); /* Not a motion event */
      return 0;
    }
#endif
#else
#error "Unknown input system"
#endif
}

#ifdef __ANDROID__
RigModifierState
rig_modifier_state_for_android_meta (int32_t meta)
{
  RigModifierState rig_state = 0;

  if (meta & AMETA_LEFT_ALT_ON)
    rig_state |= RIG_MODIFIER_LEFT_ALT_ON;
  if (meta & AMETA_RigHT_ALT_ON)
    rig_state |= RIG_MODIFIER_RigHT_ALT_ON;

  if (meta & AMETA_LEFT_SHIFT_ON)
    rig_state |= RIG_MODIFIER_LEFT_SHIFT_ON;
  if (meta & AMETA_RigHT_SHIFT_ON)
    rig_state |= RIG_MODIFIER_RigHT_SHIFT_ON;

  return rig_state;
}
#endif

RigModifierState
rig_motion_event_get_modifier_state (RigInputEvent *event)
{
#ifdef __ANDROID__
  int32_t meta = AMotionEvent_getMetaState (event->native);
  return rig_modifier_state_for_android_meta (meta);
#elif defined (USE_SDL)
  SDLMod mod = SDL_GetModState ();
  RigModifierState rig_state = 0;

  if (mod & KMOD_LSHIFT)
    rig_state |= RIG_MODIFIER_LEFT_SHIFT_ON;
  if (mod & KMOD_RSHIFT)
    rig_state |= RIG_MODIFIER_RigHT_SHIFT_ON;
  if (mod & KMOD_LCTRL)
    rig_state |= RIG_MODIFIER_LEFT_CTRL_ON;
  if (mod & KMOD_RCTRL)
    rig_state |= RIG_MODIFIER_RigHT_CTRL_ON;
  if (mod & KMOD_LALT)
    rig_state |= RIG_MODIFIER_LEFT_ALT_ON;
  if (mod & KMOD_RALT)
    rig_state |= RIG_MODIFIER_RigHT_ALT_ON;
  if (mod & KMOD_LMETA)
    rig_state |= RIG_MODIFIER_LEFT_META_ON;
  if (mod & KMOD_RMETA)
    rig_state |= RIG_MODIFIER_RigHT_META_ON;
  if (mod & KMOD_NUM)
    rig_state |= RIG_MODIFIER_NUM_LOCK_ON;
  if (mod & KMOD_CAPS)
    rig_state |= RIG_MODIFIER_CAPS_LOCK_ON;
#else
#error "Unknown input system"
#endif
  return rig_state;
}

static void
rig_motion_event_get_transformed_xy (RigInputEvent *event,
                                     float *x,
                                     float *y)
{
  const CoglMatrix *transform = event->input_transform;

#ifdef __ANDROID__
  *x = AMotionEvent_getX (event->native, 0);
  *y = AMotionEvent_getY (event->native, 0);
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      *x = sdl_event->button.x;
      *y = sdl_event->button.y;
      break;
    case SDL_MOUSEMOTION:
      *x = sdl_event->motion.x;
      *y = sdl_event->motion.y;
      break;
    default:
      g_warn_if_reached (); /* Not a motion event */
      return;
    }
#else
#error "Unknown input system"
#endif

  if (transform)
    {
      *x = transform->xx * *x + transform->xy * *y + transform->xw;
      *y = transform->yx * *x + transform->yy * *y + transform->yw;
    }
}

float
rig_motion_event_get_x (RigInputEvent *event)
{
  float x, y;

  rig_motion_event_get_transformed_xy (event, &x, &y);

  return x;
}

float
rig_motion_event_get_y (RigInputEvent *event)
{
  float x, y;

  rig_motion_event_get_transformed_xy (event, &x, &y);

  return y;
}

typedef struct _CameraPickState
{
  RigCamera *camera;
  RigInputEvent *event;
  float x, y;
} CameraPickState;

static RigTraverseVisitFlags
camera_pick_region_cb (RigObject *object,
                       int depth,
                       void *user_data)
{
  if (rig_object_get_type (object) == &rig_input_region_type)
    {
      CameraPickState *state = user_data;
      RigInputRegion *region = (RigInputRegion *)object;

      //g_print ("cam pick: cap=%p region=%p\n", state->camera, object);
      if (rig_camera_pick_input_region (state->camera,
                                        region, state->x, state->y))
        {
          if (region->callback (region, state->event, region->user_data) ==
              RIG_INPUT_EVENT_STATUS_HANDLED)
            return RIG_TRAVERSE_VISIT_BREAK;
        }
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static RigInputEventStatus
_rig_shell_handle_input (RigShell *shell, RigInputEvent *event)
{
  RigInputEventStatus status = RIG_INPUT_EVENT_STATUS_UNHANDLED;
  GList *l;

  if (shell->input_cb)
    status = shell->input_cb (event, shell->input_data);

  if (status == RIG_INPUT_EVENT_STATUS_HANDLED)
    return status;

  if (shell->grab_cb)
    return shell->grab_cb (event, shell->grab_data);

  for (l = shell->input_cameras; l; l = l->next)
    {
      RigCamera *camera = l->data;
      GList *l2;

#if 0
      for (l2 = camera->input_callbacks; l2; l2 = l2->next)
        {
          status = _rig_camera_input_callback_wrapper (l2->data, event);
          if (status == RIG_INPUT_EVENT_STATUS_HANDLED)
            return status;
        }
#endif

      event->input_transform = &camera->input_transform;

      if (rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
        {
          float x = rig_motion_event_get_x (event);
          float y = rig_motion_event_get_y (event);
          CameraPickState state;

          for (l2 = camera->input_regions; l2; l2 = l2->next)
            {
              RigInputRegion *region = l2->data;

              if (rig_camera_pick_input_region (camera, region, x, y))
                {
                  status = region->callback (region, event, region->user_data);

                  if (status == RIG_INPUT_EVENT_STATUS_HANDLED)
                    return status;
                }
            }

          state.camera = camera;
          state.event = event;
          state.x = x;
          state.y = y;

          rig_graphable_traverse (RIG_OBJECT (camera),
                                  RIG_TRAVERSE_DEPTH_FIRST,
                                  camera_pick_region_cb,
                                  NULL,
                                  &state);
        }
    }

  event->input_transform = NULL;

  return status;
}


#ifdef __ANDROID__

/**
 * Process the next input event.
 */
static int32_t
android_handle_input (struct android_app* app, AInputEvent *event)
{
  RigInputEvent rig_event;
  RigShell *shell = (Data *)app->userData;

  rig_event.native = event;
  rig_event.input_transform = NULL;

  if (_rig_shell_handle_input (shell, &rig_event) ==
      RIG_INPUT_EVENT_STATUS_HANDLED)
    return 1;

  return 0;
}

static int
android_init (RigShell *shell)
{
  cogl_android_set_native_window (shell->app->window);

  shell->init_cb (shell, shell->user_data);
  return 0;
}

/**
 * Process the next main command.
 */
static void
android_handle_cmd (struct android_app *app,
                    int32_t cmd)
{
  Data *shell = (Data *) app->userData;

  switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
      /* The window is being shown, get it ready */
      g_message ("command: INIT_WINDOW");
      if (shell->app->window != NULL)
        {
          android_init (shell);
          shell->redraw_queued = shell->paint_cb (shell, shell->user_data);
        }
      break;

    case APP_CMD_TERM_WINDOW:
      /* The window is being hidden or closed, clean it up */
      g_message ("command: TERM_WINDOW");
      _rig_shell_fini (shell);
      break;

    case APP_CMD_GAINED_FOCUS:
      g_message ("command: GAINED_FOCUS");
      break;

    case APP_CMD_LOST_FOCUS:
      g_message ("command: LOST_FOCUS");
      shell->redraw_queued = shell->paint_cb (shell, shell->user_data);
      break;
    }
}
#endif

RigType rig_shell_type;

static void
_rig_shell_free (void *object)
{
  RigShell *shell = object;
  GList *l;

  for (l = shell->input_regions; l; l = l->next)
    rig_ref_countable_unref (l->data);
  g_list_free (shell->input_regions);

  _rig_shell_fini (shell);

  g_free (shell);
}

RigRefCountableVTable _rig_shell_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_shell_free
};


static void
_rig_shell_init_types (void)
{
  rig_type_init (&rig_shell_type);
  rig_type_add_interface (&rig_shell_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigShell, ref_count),
                          &_rig_shell_ref_countable_vtable);

  _rig_scroll_bar_init_type ();
  _rig_slider_init_type ();
  _rig_input_region_init_type ();
}

RigShell *
rig_shell_new (RigShellInitCallback init,
               RigShellFiniCallback fini,
               RigShellPaintCallback paint,
               void *user_data)
{
  static CoglBool initialized = FALSE;
  RigShell *shell;

  /* Make sure core types are registered */
  _rig_init ();

  if (G_UNLIKELY (initialized == FALSE))
    _rig_shell_init_types ();

  shell = g_new0 (RigShell, 1);

  shell->ref_count = 1;

  rig_object_init (&shell->_parent, &rig_shell_type);

  shell->init_cb = init;
  shell->fini_cb = fini;
  shell->paint_cb = paint;
  shell->user_data = user_data;

  return shell;
}

/* Note: we don't take a reference on the context so we don't
 * introduce a circular reference. */
void
_rig_shell_associate_context (RigShell *shell,
                              RigContext *context)
{
  shell->rig_ctx = context;
}

void
_rig_shell_init (RigShell *shell)
{
#ifndef __ANDROID__
  shell->init_cb (shell, shell->user_data);
#endif
}

#ifdef __ANDROID__

RigShell *
rig_android_shell_new (struct android_app* application,
                       RigShellInitCallback init,
                       RigShellFiniCallback fini,
                       RigShellPaintCallback paint,
                       void *user_data)
{
  RigShell *shell = rig_shell_new (init, fini, paint, user_data);

  application->userData = shell;
  application->onAppCmd = android_handle_cmd;
  application->onInputEvent = android_handle_input;
}

#endif

static CoglBool
_rig_shell_paint (RigShell *shell)
{
  GSList *l;
  //CoglBool status;

  for (l = shell->rig_ctx->timelines; l; l = l->next)
    _rig_timeline_update (l->data);

  if (shell->paint_cb (shell, shell->user_data))
    return TRUE;

  for (l = shell->rig_ctx->timelines; l; l = l->next)
    if (rig_timeline_is_running (l->data))
      return TRUE;

  return FALSE;
}

#ifdef USE_SDL

static void
sdl_handle_event (RigShell *shell, SDL_Event *event)
{
  switch (event->type)
    {
    case SDL_VIDEOEXPOSE:
      shell->redraw_queued = TRUE;
      break;

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_KEYUP:
    case SDL_KEYDOWN:
      {
        RigInputEvent rig_event;

        rig_event.native = event;
        rig_event.input_transform = NULL;

        _rig_shell_handle_input (shell, &rig_event);
        break;
      }
    case SDL_QUIT:
      shell->quit = TRUE;
      break;
    }
}

#elif defined (USE_GLIB)

static CoglBool
glib_paint_cb (void *user_data)
{
  RigShell *shell = user_data;

  shell->redraw_queued = _rig_shell_paint (shell);

  /* If the driver can deliver swap complete events then we can remove
   * the idle paint callback until we next get a swap complete event
   * otherwise we keep the idle paint callback installed and simply
   * paint as fast as the driver will allow... */
  if (cogl_has_feature (shell->rig_ctx->cogl_context, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    return FALSE; /* remove the callback */
  else
    return TRUE;
}

static void
swap_complete_cb (CoglFramebuffer *framebuffer, void *user_data)
{
  RigShell *shell = user_data;

  if (shell->redraw_queued)
    g_idle_add (glib_paint_cb, user_data);
}
#endif

void
rig_shell_main (RigShell *shell)
{
#ifdef __ANDROID__
  while (!shell->quit)
    {
      int events;
      struct android_poll_source* source;

      while (!shell->quit)
        {
          int status;

          status = ALooper_pollAll (0, NULL, &events, (void**)&source);
          if (status == ALOOPER_POLL_TIMEOUT)
            {
              if (shell->redraw_queued)
                break;

              /* Idle now
               * FIXME: cogl_android_idle (shell->ctx)
               */

              status = ALooper_pollAll (-1, NULL, &events, (void**)&source);
            }

          if (status == ALOOPER_POLL_ERROR)
            {
              g_error ("Error waiting for polling for events");
              return;
            }

          if (shell->app->destroyRequested != 0)
            {
              flip_fini (shell);
              return;
            }

          if (source != NULL)
            source->process (shell->app, source);
        }

      shell->redraw_queued = paint (shell);
    }

#elif defined(USE_SDL)

  shell->quit = FALSE;
  shell->redraw_queued = TRUE;
  while (!shell->quit)
    {
      SDL_Event event;

      while (!shell->quit)
        {
          if (!SDL_PollEvent (&event))
            {
              if (shell->redraw_queued)
                break;

              cogl_sdl_idle (shell->rig_ctx->cogl_context);
              if (!SDL_WaitEvent (&event))
                g_error ("Error waiting for SDL events");
            }

          sdl_handle_event (shell, &event);
          cogl_sdl_handle_event (shell->rig_ctx->cogl_context, &event);
        }

      shell->redraw_queued = _rig_shell_paint (shell);
    }

#elif defined (USE_GLIB)
  GSource *cogl_source;
  GMainLoop *loop;

  cogl_source = cogl_glib_source_new (shell->ctx, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  if (cogl_has_feature (shell->ctx, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    cogl_onscreen_add_swap_buffers_callback (COGL_ONSCREEN (shell->fb),
                                             swap_complete_cb, shell);

  g_idle_add (glib_paint_cb, shell);

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);

#else

#error "No platform mainloop provided"

#endif
}

void
rig_shell_grab_input (RigShell *shell,
                      RigInputCallback callback,
                      void *user_data)
{
  g_return_if_fail (shell->grab_cb == NULL);

  shell->grab_cb = callback;
  shell->grab_data = user_data;
}

void
rig_shell_ungrab_input (RigShell *shell)
{
  shell->grab_cb = NULL;
}

void
rig_shell_queue_redraw (RigShell *shell)
{
  shell->redraw_queued = TRUE;
}

struct _RigScrollBar
{
  RigObjectProps _parent;
  int ref_count;

  RigNineSlice *background;
  RigNineSlice *handle;

  RigGraphableProps graphable;
  RigPaintableProps paintable;
  RigSimpleWidgetProps simple_widget;

  RigInputRegion *input_region;

  RigAxis axis;
  float virtual_length;
  float viewport_length;
  float offset;
};

static void
_rig_scroll_bar_free (void *object)
{
  RigScrollBar *scroll_bar = object;

  rig_ref_countable_simple_unref (scroll_bar->background);
  rig_ref_countable_simple_unref (scroll_bar->handle);

  g_slice_free (RigScrollBar, object);
}

static RigRefCountableVTable _rig_scroll_bar_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_scroll_bar_free
};

static void
_rig_scroll_bar_paint (RigObject *object, RigPaintContext *paint_ctx)
{
  RigScrollBar *scroll_bar = RIG_SCROLL_BAR (object);
  RigPaintableVTable *bg_paintable =
    rig_object_get_vtable (scroll_bar->background, RIG_INTERFACE_ID_PAINTABLE);
  RigPaintableVTable *handle_paintable =
    rig_object_get_vtable (scroll_bar->handle, RIG_INTERFACE_ID_PAINTABLE);

  bg_paintable->paint (RIG_OBJECT (scroll_bar->background), paint_ctx);
  handle_paintable->paint (RIG_OBJECT (scroll_bar->handle), paint_ctx);
}

static RigPaintableVTable _rig_scroll_bar_paintable_vtable = {
  _rig_scroll_bar_paint
};

RigGraphableVTable _rig_scroll_bar_graphable_vtable = {
  rig_simple_widget_graphable_child_removed_warn,
  rig_simple_widget_graphable_child_added_warn,
  rig_simple_widget_graphable_parent_changed
};

#if 0
static void
_rig_scroll_bar_set_camera (RigObject *object,
                            RigCamera *camera)
{
  RigScrollBar *scroll_bar = RIG_SCROLL_BAR (object);
  //if (old_camera)
  //  rig_camera_remove_input_region (old_camera, scroll_bar->input_region);

  /* FIXME: we don't have a way of cleaning up state associated
   * with any previous camera we may have been associated with.
   *
   * Adding a changed_camera vfunc would be quite awkward considering
   * that the scroll bar may belong to a graph containing nodes that
   * aren't widgets which and so when we reparent them we don't
   * get an opportunity to hook into that and notify that the camera
   * has changed.
   *
   * TODO: Add a RigAugmentable interface that lets us attach private
   * data to objects implementing that interface. This would let
   * us associate a camera pointer with scroll_bar->input_region and
   * so we could remove the region from the previous camera.
   */
  if (camera)
    rig_camera_add_input_region (camera, scroll_bar->input_region);
}
#endif

RigSimpleWidgetVTable _rig_scroll_bar_simple_widget_vtable = {
  0,
};

RigType rig_scroll_bar_type;

static void
_rig_scroll_bar_init_type (void)
{
  rig_type_init (&rig_scroll_bar_type);
  rig_type_add_interface (&rig_scroll_bar_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigScrollBar, ref_count),
                          &_rig_scroll_bar_ref_countable_vtable);
  rig_type_add_interface (&rig_scroll_bar_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigScrollBar, graphable),
                          &_rig_scroll_bar_graphable_vtable);
  rig_type_add_interface (&rig_scroll_bar_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigScrollBar, paintable),
                          &_rig_scroll_bar_paintable_vtable);
  rig_type_add_interface (&rig_scroll_bar_type,
                          RIG_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RigScrollBar, simple_widget),
                          &_rig_scroll_bar_simple_widget_vtable);
}

static RigInputEventStatus
_rig_scroll_bar_input_cb (RigInputRegion *region,
                          RigInputEvent *event,
                          void *user_data)
{
  //RigScrollBar *scroll_bar = user_data;

  g_print ("Scroll Bar input\n");

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

RigScrollBar *
rig_scroll_bar_new (RigContext *ctx,
                    RigAxis axis,
                    float length,
                    float virtual_length,
                    float viewport_length)
{
  RigScrollBar *scroll_bar = g_slice_new0 (RigScrollBar);
  CoglTexture *bg_texture;
  CoglTexture *handle_texture;
  GError *error = NULL;
  //PangoRectangle label_size;
  float width;
  float height;
  float handle_size;

  rig_object_init (&scroll_bar->_parent, &rig_scroll_bar_type);

  scroll_bar->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (scroll_bar));
  rig_paintable_init (RIG_OBJECT (scroll_bar));

  scroll_bar->axis = axis;
  scroll_bar->virtual_length = virtual_length;
  scroll_bar->viewport_length = viewport_length;
  scroll_bar->offset = 0;

  bg_texture = rig_load_texture (ctx, RIG_DATA_DIR "slider-background.png", &error);
  if (!bg_texture)
    {
      g_warning ("Failed to load slider-background.png: %s", error->message);
      g_error_free (error);
    }

  handle_texture = rig_load_texture (ctx, RIG_DATA_DIR "slider-handle.png", &error);
  if (!handle_texture)
    {
      g_warning ("Failed to load slider-handle.png: %s", error->message);
      g_error_free (error);
    }

  if (axis == RIG_AXIS_X)
    {
      width = length;
      height = 20;
    }
  else
    {
      width = 20;
      height = length;
    }

  scroll_bar->background = rig_nine_slice_new (ctx, bg_texture, 2, 3, 3, 3,
                                               width, height);

  scroll_bar->input_region =
    rig_input_region_new_rectangle (0, 0, width, height,
                                    _rig_scroll_bar_input_cb,
                                    scroll_bar);
  rig_input_region_set_graphable (scroll_bar->input_region,
                                  RIG_OBJECT (scroll_bar));
  rig_graphable_add_child (RIG_OBJECT (scroll_bar),
                           RIG_OBJECT (scroll_bar->input_region));

  handle_size = (viewport_length / virtual_length) * length;
  handle_size = MAX (20, handle_size);

  if (axis == RIG_AXIS_X)
    width = handle_size;
  else
    height = handle_size;

  scroll_bar->handle = rig_nine_slice_new (ctx, handle_texture, 4, 5, 6, 5,
                                           width, height);

  return scroll_bar;
}

void
rig_scroll_bar_set_virtual_length (RigScrollBar *scroll_bar,
                                   float virtual_length)
{
  scroll_bar->virtual_length = virtual_length;
}

void
rig_scroll_bar_set_viewport_length (RigScrollBar *scroll_bar,
                                    float viewport_length)
{
  scroll_bar->viewport_length = viewport_length;
}

enum {
  RIG_SLIDER_PROP_PROGRESS,
  RIG_SLIDER_N_PROPS
};

struct _RigSlider
{
  RigObjectProps _parent;
  int ref_count;

  /* FIXME: It doesn't seem right that we should have to save a
   * pointer to the context for input here... */
  RigContext *ctx;

  /* FIXME: It also doesn't seem right to have to save a pointer
   * to the camera here so we can queue a redraw */
  //RigCamera *camera;

  RigGraphableProps graphable;
  RigPaintableProps paintable;
  RigSimpleWidgetProps simple_widget;
  RigSimpleIntrospectableProps introspectable;

  RigNineSlice *background;
  RigNineSlice *handle;
  RigTransform *handle_transform;

  RigInputRegion *input_region;
  float grab_x;
  float grab_y;
  float grab_progress;

  RigAxis axis;
  float range_min;
  float range_max;
  float length;
  float progress;

  RigProperty properties[RIG_SLIDER_N_PROPS];
};

static RigPropertySpec _rig_slider_prop_specs[] = {
  {
    .name = "progress",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigSlider, progress),
    .setter = rig_slider_set_progress
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};


static void
_rig_slider_free (void *object)
{
  RigSlider *slider = object;

  rig_ref_countable_simple_unref (slider->input_region);

  rig_graphable_remove_child (slider, slider->handle_transform);

  rig_ref_countable_simple_unref (slider->handle_transform);
  rig_ref_countable_simple_unref (slider->handle);
  rig_ref_countable_simple_unref (slider->background);

  rig_simple_introspectable_destroy (slider);

  g_slice_free (RigSlider, object);
}

RigRefCountableVTable _rig_slider_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_slider_free
};

static void
_rig_slider_graphable_child_removed (RigObject *self,
                                    RigObject *child)
{
  /* nop */
}

static void
_rig_slider_graphable_child_added (RigObject *self,
                                  RigObject *child)
{
  /* nop */
}

static RigGraphableVTable _rig_slider_graphable_vtable = {
  _rig_slider_graphable_child_removed,
  _rig_slider_graphable_child_added,
  rig_simple_widget_graphable_parent_changed
};

static void
_rig_slider_paint (RigObject *object, RigPaintContext *paint_ctx)
{
  RigSlider *slider = RIG_SLIDER (object);
  RigPaintableVTable *bg_paintable =
    rig_object_get_vtable (slider->background, RIG_INTERFACE_ID_PAINTABLE);

  bg_paintable->paint (RIG_OBJECT (slider->background), paint_ctx);
}

static RigPaintableVTable _rig_slider_paintable_vtable = {
  _rig_slider_paint
};

static RigIntrospectableVTable _rig_slider_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

#if 0
static void
_rig_slider_set_camera (RigObject *object,
                        RigCamera *camera)
{
  RigSlider *slider = RIG_SLIDER (object);

  if (camera)
    rig_camera_add_input_region (camera, slider->input_region);

  slider->camera = camera;
}
#endif

RigSimpleWidgetVTable _rig_slider_simple_widget_vtable = {
  0,
};

RigType rig_slider_type;

static void
_rig_slider_init_type (void)
{
  rig_type_init (&rig_slider_type);
  rig_type_add_interface (&rig_slider_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigSlider, ref_count),
                          &_rig_slider_ref_countable_vtable);
  rig_type_add_interface (&rig_slider_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigSlider, graphable),
                          &_rig_slider_graphable_vtable);
  rig_type_add_interface (&rig_slider_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigSlider, paintable),
                          &_rig_slider_paintable_vtable);
  rig_type_add_interface (&rig_slider_type,
                          RIG_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RigSlider, simple_widget),
                          &_rig_slider_simple_widget_vtable);
  rig_type_add_interface (&rig_slider_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_slider_introspectable_vtable);
  rig_type_add_interface (&rig_slider_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigSlider, introspectable),
                          NULL); /* no implied vtable */
}

static RigInputEventStatus
_rig_slider_grab_input_cb (RigInputEvent *event,
                           void *user_data)
{
  RigSlider *slider = user_data;

  if(rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      RigShell *shell = slider->ctx->shell;
      if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_UP)
        {
          rig_shell_ungrab_input (shell);
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_MOVE)
        {
          float diff;
          float progress;

          if (slider->axis == RIG_AXIS_X)
            diff = rig_motion_event_get_x (event) - slider->grab_x;
          else
            diff = rig_motion_event_get_y (event) - slider->grab_y;

          progress = CLAMP (slider->grab_progress + diff / slider->length, 0, 1);

          rig_slider_set_progress (slider, progress);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
_rig_slider_input_cb (RigInputRegion *region,
                      RigInputEvent *event,
                      void *user_data)
{
  RigSlider *slider = user_data;

  g_print ("Slider input\n");

  if(rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION &&
     rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN)
    {
      RigShell *shell = slider->ctx->shell;
      rig_shell_grab_input (shell, _rig_slider_grab_input_cb, slider);
      slider->grab_x = rig_motion_event_get_x (event);
      slider->grab_y = rig_motion_event_get_y (event);
      slider->grab_progress = slider->progress;
      return RIG_INPUT_EVENT_STATUS_HANDLED;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

RigSlider *
rig_slider_new (RigContext *ctx,
                RigAxis axis,
                float min,
                float max,
                float length)
{
  RigSlider *slider = g_slice_new0 (RigSlider);
  CoglTexture *bg_texture;
  CoglTexture *handle_texture;
  GError *error = NULL;
  //PangoRectangle label_size;
  float width;
  float height;

  rig_object_init (&slider->_parent, &rig_slider_type);

  slider->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (slider));
  rig_paintable_init (RIG_OBJECT (slider));

  slider->ctx = ctx;

  slider->axis = axis;
  slider->range_min = min;
  slider->range_max = max;
  slider->length = length;
  slider->progress = 0;

  bg_texture = rig_load_texture (ctx, RIG_DATA_DIR "slider-background.png", &error);
  if (!bg_texture)
    {
      g_warning ("Failed to load slider-background.png: %s", error->message);
      g_error_free (error);
    }

  handle_texture = rig_load_texture (ctx, RIG_DATA_DIR "slider-handle.png", &error);
  if (!handle_texture)
    {
      g_warning ("Failed to load slider-handle.png: %s", error->message);
      g_error_free (error);
    }

  if (axis == RIG_AXIS_X)
    {
      width = length;
      height = 20;
    }
  else
    {
      width = 20;
      height = length;
    }

  slider->background = rig_nine_slice_new (ctx, bg_texture, 2, 3, 3, 3,
                                           width, height);

  if (axis == RIG_AXIS_X)
    width = 20;
  else
    height = 20;

  slider->handle_transform =
    rig_transform_new (ctx,
                       slider->handle = rig_nine_slice_new (ctx,
                                                            handle_texture,
                                                            4, 5, 6, 5,
                                                            width, height),
                       NULL);
  rig_graphable_add_child (slider, slider->handle_transform);

  slider->input_region =
    rig_input_region_new_rectangle (0, 0, width, height,
                                    _rig_slider_input_cb,
                                    slider);

  rig_input_region_set_graphable (slider->input_region, slider->handle);
  rig_graphable_add_child (slider, slider->input_region);

  rig_simple_introspectable_init (slider,
                                  _rig_slider_prop_specs,
                                  slider->properties);

  return slider;
}

void
rig_slider_set_range (RigSlider *slider,
                      float min, float max)
{
  slider->range_min = min;
  slider->range_max = max;
}

void
rig_slider_set_length (RigSlider *slider,
                       float length)
{
  slider->length = length;
}

void
rig_slider_set_progress (RigSlider *slider,
                         float progress)
{
  float translation;

  if (slider->progress == progress)
    return;

  slider->progress = progress;
  rig_property_dirty (&slider->ctx->property_ctx,
                      &slider->properties[RIG_SLIDER_PROP_PROGRESS]);

  translation = (slider->length - 20) * slider->progress;

  rig_transform_init_identity (slider->handle_transform);

  if (slider->axis == RIG_AXIS_X)
    rig_transform_translate (slider->handle_transform, translation, 0, 0);
  else
    rig_transform_translate (slider->handle_transform, 0, translation, 0);

  rig_shell_queue_redraw (slider->ctx->shell);

  g_print ("progress = %f\n", slider->progress);
}

