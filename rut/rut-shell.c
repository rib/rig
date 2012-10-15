
#include <glib.h>

#include <cogl/cogl.h>

/* FIXME: we should have a config.h where things like USE_SDL would
 * be defined instead of defining that in rut.h */
#include "rut.h"

#include "rut-camera-private.h"
#include "rut-transform-private.h"
#include "rut-shell.h"

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include <glib-android/glib-android.h>
#elif defined (USE_SDL)
#include "rut-sdl-keysyms.h"
#endif

typedef struct
{
  RutInputCallback callback;
  RutCamera *camera;
  void *user_data;
} RutShellGrab;

struct _RutShell
{
  RutObjectProps _parent;

  int ref_count;

  CoglBool quit;

#ifdef __ANDROID__
  struct android_app* app;
#endif

  RutContext *rut_ctx;

  RutShellInitCallback init_cb;
  RutShellFiniCallback fini_cb;
  RutShellPaintCallback paint_cb;
  void *user_data;

  RutList input_cb_list;
  GList *input_cameras;

  /* Use to handle input events in window coordinates */
  RutCamera *window_camera;

  GList *input_regions;

  /* List of grabs that are currently in place. This are in order from
   * highest to lowest priority. */
  GList *grabs;

  RutObject *keyboard_focus_object;
  GDestroyNotify keyboard_ungrab_cb;

  CoglBool redraw_queued;
};

/* PRIVATE */
typedef enum _RutShapeType
{
  RUT_SHAPE_TYPE_RECTANGLE,
  RUT_SHAPE_TYPE_CIRCLE
} RutShapeType;

/* PRIVATE */
typedef struct _RutShapeRectangle
{
  RutShapeType type;
  float x0, y0, x1, y1;
} RutShapeRectange;

/* PRIVATE */
typedef struct _RutShapeCircle
{
  RutShapeType type;
  float x, y;
  float r_squared;
} RutShapeCircle;

/* PRIVATE */
typedef struct _RutShapeAny
{
  RutShapeType type;
} RutShapeAny;

/* PRIVATE */
typedef union _RutShape
{
  RutShapeAny any;
  RutShapeRectange rectangle;
  RutShapeCircle circle;
} RutShape;

typedef enum _RutInputTransformType
{
  RUT_INPUT_TRANSFORM_TYPE_NONE,
  RUT_INPUT_TRANSFORM_TYPE_MATRIX,
  RUT_INPUT_TRANSFORM_TYPE_GRAPHABLE
} RutInputTransformType;

typedef struct _RutInputTransformAny
{
  RutInputTransformType type;
} RutInputTransformAny;

typedef struct _RutInputTransformMatrix
{
  RutInputTransformType type;
  CoglMatrix *matrix;
} RutInputTransformMatrix;

typedef struct _RutInputTransformGraphable
{
  RutInputTransformType type;
} RutInputTransformGraphable;

typedef union _RutInputTransform
{
  RutInputTransformAny any;
  RutInputTransformMatrix matrix;
  RutInputTransformGraphable graphable;
} RutInputTransform;

struct _RutInputRegion
{
  RutObjectProps _parent;

  int ref_count;

  RutShape shape;

  RutGraphableProps graphable;
  RutInputableProps inputable;

  CoglBool has_transform;
  CoglMatrix transform;
  CoglBool hud_mode;

  RutInputRegionCallback callback;
  void *user_data;
};


static void
_rut_slider_init_type (void);

static void
_rut_input_region_init_type (void);

RutContext *
rut_shell_get_context (RutShell *shell)
{
  return shell->rut_ctx;
}

static void
_rut_shell_fini (RutShell *shell)
{
  rut_refable_simple_unref (shell->rut_ctx);
}

struct _RutInputEvent
{
  void *native;
  RutCamera *camera;
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

static void
rectangle_poly_init (RutShapeRectange *rectangle,
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
}

/* Given an (x0,y0) (x1,y1) rectangle this transforms it into
 * a polygon in window coordinates that can be intersected
 * with input coordinates for picking.
 */
static void
rect_to_screen_polygon (RutShapeRectange *rectangle,
                        const CoglMatrix *modelview,
                        const CoglMatrix *projection,
                        const float *viewport,
                        float *poly)
{
  rectangle_poly_init (rectangle, poly);

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

CoglBool
rut_camera_pick_input_region (RutCamera *camera,
                              RutInputRegion *region,
                              float x,
                              float y)
{
  CoglMatrix matrix;
  const CoglMatrix *modelview = NULL;
  float poly[16];
  RutObject *parent = rut_graphable_get_parent (region);
  const CoglMatrix *view = rut_camera_get_view_transform (camera);

  if (parent)
    {
      RutObject *parent = rut_graphable_get_parent (region);
      matrix = *view;
      rut_graphable_apply_transform (parent, &matrix);
      modelview = &matrix;
    }
  else if (region->has_transform)
    {
      cogl_matrix_multiply (&matrix, &region->transform, view);
      modelview = &matrix;
    }
  else if (region->hud_mode)
    modelview = &camera->ctx->identity_matrix;
  else
    modelview = view;

  switch (region->shape.any.type)
    {
    case RUT_SHAPE_TYPE_RECTANGLE:
      {
        if (!region->hud_mode)
          {
            rect_to_screen_polygon (&region->shape.rectangle,
                                    modelview,
                                    &camera->projection,
                                    camera->viewport,
                                    poly);
          }
        else
          rectangle_poly_init (&region->shape.rectangle, poly);

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
    case RUT_SHAPE_TYPE_CIRCLE:
      {
        RutShapeCircle *circle = &region->shape.circle;
        float center_x = circle->x;
        float center_y = circle->y;
        float z = 0;
        float w = 1;
        float a;
        float b;
        float c2;

        /* Note the circle hit regions are billboarded, such that only the
         * center point is transformed but the raius of the circle stays
         * constant. */

        /* XXX: This is a hack to use input regions in the tool example */
        if (camera)
          cogl_matrix_transform_point (modelview,
                                       &center_x, &center_y, &z, &w);

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
_rut_input_region_free (void *object)
{
  RutInputRegion *region = object;

#if 0
  switch (region->transform.any.type)
    {
    case RUT_INPUT_TRANSFORM_TYPE_GRAPHABLE:
      rut_refable_simple_unref (region->transform.graphable.graphable);
      break;
    default:
      break;
    }
#endif

  rut_graphable_destroy (region);

  g_slice_free (RutInputRegion, region);
}

static RutRefCountableVTable _rut_input_region_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_input_region_free
};

static RutGraphableVTable _rut_input_region_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

RutType rut_input_region_type;

static void
_rut_input_region_init_type (void)
{
  rut_type_init (&rut_input_region_type);
  rut_type_add_interface (&rut_input_region_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutInputRegion, ref_count),
                          &_rut_input_region_ref_countable_vtable);
  rut_type_add_interface (&rut_input_region_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutInputRegion, graphable),
                          &_rut_input_region_graphable_vtable);
  rut_type_add_interface (&rut_input_region_type,
                          RUT_INTERFACE_ID_INPUTABLE,
                          offsetof (RutInputRegion, inputable),
                          NULL /* no vtable */);
}

static RutInputRegion *
rut_input_region_new_common (RutInputRegionCallback  callback,
                             void                   *user_data)
{
  RutInputRegion *region = g_slice_new0 (RutInputRegion);

  rut_object_init (&region->_parent, &rut_input_region_type);

  region->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (region));
  region->inputable.input_region = region;

  region->callback = callback;
  region->user_data = user_data;

  return region;
}

RutInputRegion *
rut_input_region_new_rectangle (float x0,
                                float y0,
                                float x1,
                                float y1,
                                RutInputRegionCallback callback,
                                void *user_data)
{
  RutInputRegion *region;

  region = rut_input_region_new_common (callback, user_data);

  //region->transform.any.type = RUT_INPUT_TRANSFORM_TYPE_MATRIX;
  //region->transform.matrix.matrix = NULL;
  region->has_transform = FALSE;

  rut_input_region_set_rectangle (region, x0, y0, x1, y1);

  return region;
}

RutInputRegion *
rut_input_region_new_circle (float x0,
                             float y0,
                             float radius,
                             RutInputRegionCallback callback,
                             void *user_data)
{
  RutInputRegion *region;

  region = rut_input_region_new_common (callback, user_data);

  region->has_transform = FALSE;

  rut_input_region_set_circle (region, x0, y0, radius);

  return region;

}

void
rut_input_region_set_rectangle (RutInputRegion *region,
                                float x0,
                                float y0,
                                float x1,
                                float y1)
{
  region->shape.any.type = RUT_SHAPE_TYPE_RECTANGLE;
  region->shape.rectangle.x0 = x0;
  region->shape.rectangle.y0 = y0;
  region->shape.rectangle.x1 = x1;
  region->shape.rectangle.y1 = y1;
}

void
rut_input_region_set_circle (RutInputRegion *region,
                             float x,
                             float y,
                             float radius)
{
  region->shape.any.type = RUT_SHAPE_TYPE_CIRCLE;
  region->shape.circle.x = x;
  region->shape.circle.y = y;
  region->shape.circle.r_squared = radius * radius;
}

#if 0
void
rut_input_region_set_graphable (RutInputRegion *region,
                                RutObject *graphable)
{
  region->transform.any.type = RUT_INPUT_TRANSFORM_TYPE_GRAPHABLE;
  region->transform.graphable.graphable = rut_refable_simple_ref (graphable);
}
#endif

void
rut_input_region_set_transform (RutInputRegion *region,
                                CoglMatrix *matrix)
{
  if (cogl_matrix_is_identity (matrix))
    {
      region->has_transform = FALSE;
      return;
    }

  region->transform = *matrix;
  region->has_transform = TRUE;
}

void
rut_input_region_set_hud_mode (RutInputRegion *region,
                               CoglBool hud_mode)
{
  region->hud_mode = hud_mode;
}

void
rut_shell_add_input_region (RutShell *shell,
                            RutInputRegion *region)
{
  shell->input_regions = g_list_prepend (shell->input_regions, region);
}

void
rut_shell_remove_input_region (RutShell *shell,
                               const RutInputRegion *region)
{
  shell->input_regions = g_list_remove (shell->input_regions, region);
}

RutClosure *
rut_shell_add_input_callback (RutShell *shell,
                              RutInputCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&shell->input_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

typedef struct _InputCamera
{
  RutCamera *camera;
  RutObject *scenegraph;
} InputCamera;

void
rut_shell_add_input_camera (RutShell *shell,
                            RutCamera *camera,
                            RutObject *scenegraph)
{
  InputCamera *input_camera = g_slice_new (InputCamera);

  input_camera->camera = rut_refable_ref (camera);

  if (scenegraph)
    input_camera->scenegraph = rut_refable_ref (scenegraph);
  else
    input_camera->scenegraph = NULL;

  shell->input_cameras = g_list_prepend (shell->input_cameras, input_camera);
}

static void
input_camera_free (InputCamera *input_camera)
{
  rut_refable_unref (input_camera->camera);
  if (input_camera->scenegraph)
    rut_refable_unref (input_camera->scenegraph);
  g_slice_free (InputCamera, input_camera);
}

void
rut_shell_remove_input_camera (RutShell *shell,
                               RutCamera *camera,
                               RutObject *scenegraph)
{
  GList *l;

  for (l = shell->input_cameras; l; l = l->next)
    {
      InputCamera *input_camera = l->data;
      if (input_camera->camera == camera &&
          input_camera->scenegraph == scenegraph)
        {
          input_camera_free (input_camera);
          shell->input_cameras = g_list_delete_link (shell->input_cameras, l);
          return;
        }
    }

  g_warning ("Failed to find input camera to remove from shell");
}

static void
_rut_shell_remove_all_input_cameras (RutShell *shell)
{
  GList *l;

  for (l = shell->input_cameras; l; l = l->next)
    input_camera_free (l->data);
  g_list_free (shell->input_cameras);
  shell->input_cameras = NULL;
}

RutCamera *
rut_input_event_get_camera (RutInputEvent *event)
{
  return event->camera;
}

RutInputEventType
rut_input_event_get_type (RutInputEvent *event)
{
#ifdef __ANDROID__
  AInputEvent *android_event = event->native;
  switch (AInputEvent_getType (android_event))
    {
    case AINPUT_EVENT_TYPE_MOTION:
      return RUT_INPUT_EVENT_TYPE_MOTION;
    case AINPUT_EVENT_TYPE_KEY:
      return RUT_INPUT_EVENT_TYPE_KEY;
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
      return RUT_INPUT_EVENT_TYPE_MOTION;
    case SDL_KEYUP:
    case SDL_KEYDOWN:
      return RUT_INPUT_EVENT_TYPE_KEY;
    default:
      g_warn_if_reached (); /* Un-supported input type */
      return 0;
    }
#else
#error "Unknown input system"
#endif
}

int32_t
rut_key_event_get_keysym (RutInputEvent *event)
{
#ifdef __ANDROID__
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return _rut_keysym_from_sdl_keysym (sdl_event->key.keysym.sym);
#else
#error "Unknown input system"
#endif
}

uint32_t
rut_key_event_get_unicode (RutInputEvent *event)
{
#ifdef __ANDROID__
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return sdl_event->key.keysym.unicode;
#else
#error "Unknown input system"
#endif
}

RutKeyEventAction
rut_key_event_get_action (RutInputEvent *event)
{
#ifdef __ANDROID__
  switch (AKeyEvent_getAction (event->native))
    {
    case AKEY_EVENT_ACTION_DOWN:
      return RUT_KEY_EVENT_ACTION_DOWN;
    case AKEY_EVENT_ACTION_UP:
      return RUT_KEY_EVENT_ACTION_UP;
    case AKEY_EVENT_ACTION_MULTIPLE:
      g_warn_if_reached ();
      /* TODO: Expand these out in android_handle_event into multiple distinct events,
       * it seems odd to require app developers to have to have special code for this
       * and key events are surely always low frequency enough that we don't need
       * this for optimization purposes.
       */
      return RUT_KEY_EVENT_ACTION_UP;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_KEYUP:
      return RUT_KEY_EVENT_ACTION_UP;
    case SDL_KEYDOWN:
      return RUT_KEY_EVENT_ACTION_DOWN;
    default:
      g_warn_if_reached ();
      return RUT_KEY_EVENT_ACTION_UP;
    }
#endif
}

RutMotionEventAction
rut_motion_event_get_action (RutInputEvent *event)
{
#ifdef __ANDROID__
  switch (AMotionEvent_getAction (event->native))
    {
    case AMOTION_EVENT_ACTION_DOWN:
      return RUT_MOTION_EVENT_ACTION_DOWN;
    case AMOTION_EVENT_ACTION_UP:
      return RUT_MOTION_EVENT_ACTION_UP;
    case AMOTION_EVENT_ACTION_MOVE:
      return RUT_MOTION_EVENT_ACTION_MOVE;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
      return RUT_MOTION_EVENT_ACTION_DOWN;
    case SDL_MOUSEBUTTONUP:
      return RUT_MOTION_EVENT_ACTION_UP;
    case SDL_MOUSEMOTION:
      return RUT_MOTION_EVENT_ACTION_MOVE;
    default:
      g_warn_if_reached (); /* Not a motion event */
      return RUT_MOTION_EVENT_ACTION_MOVE;
    }
#else
#error "Unknown input system"
#endif
}

#ifdef USE_SDL
static RutButtonState
rut_button_state_for_sdl_state (SDL_Event *event,
                                uint8_t    sdl_state)
{
  RutButtonState rut_state = 0;
  if (sdl_state & SDL_BUTTON(1))
    rut_state |= RUT_BUTTON_STATE_1;
  if (sdl_state & SDL_BUTTON(2))
    rut_state |= RUT_BUTTON_STATE_2;
  if (sdl_state & SDL_BUTTON(3))
    rut_state |= RUT_BUTTON_STATE_3;

#if SDL_MAJOR_VERSION < 2

  if ((event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEBUTTONDOWN) &&
      event->button.button == SDL_BUTTON_WHEELUP)
    rut_state |= RUT_BUTTON_STATE_WHEELUP;
  if ((event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEBUTTONDOWN) &&
      event->button.button == SDL_BUTTON_WHEELDOWN)
    rut_state |= RUT_BUTTON_STATE_WHEELDOWN;

#else /* SDL_MAJOR_VERSION < 2 */

  if (event->type == SDL_MOUSEWHEEL)
    {
      if (event->wheel.y < 0)
        rut_state |= RUT_BUTTON_STATE_WHEELUP;
      else if (event->wheel.y > 0)
        rut_state |= RUT_BUTTON_STATE_WHEELDOWN;
    }

#endif /* SDL_MAJOR_VERSION < 2 */

  return rut_state;
}
#endif /* USE_SDL */

RutButtonState
rut_motion_event_get_button_state (RutInputEvent *event)
{
#ifdef __ANDROID__
  return 0;
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return rut_button_state_for_sdl_state (sdl_event,
                                         SDL_GetMouseState (NULL, NULL));
#if 0
  /* FIXME: we need access to the RutContext here so that
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
          return RUT_BUTTON_STATE_1;
        case 2:
          return RUT_BUTTON_STATE_2;
        case 3:
          return RUT_BUTTON_STATE_3;
        case 4:
          return RUT_BUTTON_STATE_WHEELUP;
        case 5:
          return RUT_BUTTON_STATE_WHEELDOWN;
        default:
          g_warning ("Out of range SDL button number");
          return 0;
        }
    case SDL_MOUSEMOTION:
      return rut_button_state_for_sdl_state (sdl_event->button.state);
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
RutModifierState
rut_modifier_state_for_android_meta (int32_t meta)
{
  RutModifierState rut_state = 0;

  if (meta & AMETA_ALT_LEFT_ON)
    rut_state |= RUT_MODIFIER_LEFT_ALT_ON;
  if (meta & AMETA_ALT_RIGHT_ON)
    rut_state |= RUT_MODIFIER_RIGHT_ALT_ON;

  if (meta & AMETA_SHIFT_LEFT_ON)
    rut_state |= RUT_MODIFIER_LEFT_SHIFT_ON;
  if (meta & AMETA_SHIFT_RIGHT_ON)
    rut_state |= RUT_MODIFIER_RIGHT_SHIFT_ON;

  return rut_state;
}
#endif

#ifdef USE_SDL
static RutModifierState
rut_sdl_get_modifier_state (void)
{
  RutModifierState rut_state = 0;
#if SDL_MAJOR_VERSION < 2
  SDLMod mod;
#else
  SDL_Keymod mod;
#endif

  mod = SDL_GetModState ();

  if (mod & KMOD_LSHIFT)
    rut_state |= RUT_MODIFIER_LEFT_SHIFT_ON;
  if (mod & KMOD_RSHIFT)
    rut_state |= RUT_MODIFIER_RIGHT_SHIFT_ON;
  if (mod & KMOD_LCTRL)
    rut_state |= RUT_MODIFIER_LEFT_CTRL_ON;
  if (mod & KMOD_RCTRL)
    rut_state |= RUT_MODIFIER_RIGHT_CTRL_ON;
  if (mod & KMOD_LALT)
    rut_state |= RUT_MODIFIER_LEFT_ALT_ON;
  if (mod & KMOD_RALT)
    rut_state |= RUT_MODIFIER_RIGHT_ALT_ON;
  if (mod & KMOD_NUM)
    rut_state |= RUT_MODIFIER_NUM_LOCK_ON;
  if (mod & KMOD_CAPS)
    rut_state |= RUT_MODIFIER_CAPS_LOCK_ON;
#if SDL_MAJOR_VERSION < 2
  if (mod & KMOD_LMETA)
    rut_state |= RUT_MODIFIER_LEFT_META_ON;
  if (mod & KMOD_RMETA)
    rut_state |= RUT_MODIFIER_RIGHT_META_ON;
#endif /* SDL_MAJOR_VERSION < 2 */

  return rut_state;
}
#endif

RutModifierState
rut_key_event_get_modifier_state (RutInputEvent *event)
{
 #ifdef __ANDROID__
  int32_t meta = AKeyEvent_getMetaState (event->native);
  return rut_modifier_state_for_android_meta (meta);
#elif defined (USE_SDL)
  return rut_sdl_get_modifier_state ();
#else
#error "Unknown input system"
  return 0;
#endif
}

RutModifierState
rut_motion_event_get_modifier_state (RutInputEvent *event)
{
#ifdef __ANDROID__
  int32_t meta = AMotionEvent_getMetaState (event->native);
  return rut_modifier_state_for_android_meta (meta);
#elif defined (USE_SDL)
  return rut_sdl_get_modifier_state ();
#else
#error "Unknown input system"
  return 0;
#endif
}

static void
rut_motion_event_get_transformed_xy (RutInputEvent *event,
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
rut_motion_event_get_x (RutInputEvent *event)
{
  float x, y;

  rut_motion_event_get_transformed_xy (event, &x, &y);

  return x;
}

float
rut_motion_event_get_y (RutInputEvent *event)
{
  float x, y;

  rut_motion_event_get_transformed_xy (event, &x, &y);

  return y;
}

CoglBool
rut_motion_event_unproject (RutInputEvent *event,
                            RutObject *graphable,
                            float *x,
                            float *y)
{
  CoglMatrix transform;
  CoglMatrix inverse_transform;
  RutCamera *camera = rut_input_event_get_camera (event);

  rut_graphable_get_modelview (graphable, camera, &transform);

  if (!cogl_matrix_get_inverse (&transform, &inverse_transform))
    return FALSE;

  *x = rut_motion_event_get_x (event);
  *y = rut_motion_event_get_y (event);
  rut_camera_unproject_coord (camera,
                              &transform,
                              &inverse_transform,
                              0, /* object_coord_z */
                              x,
                              y);

  return TRUE;
}


typedef struct _CameraPickState
{
  RutCamera *camera;
  RutInputEvent *event;
  float x, y;
} CameraPickState;

static RutTraverseVisitFlags
camera_pick_region_cb (RutObject *object,
                       int depth,
                       void *user_data)
{
  CameraPickState *state = user_data;

  if (rut_object_get_type (object) == &rut_input_region_type)
    {
      RutInputRegion *region = (RutInputRegion *)object;

      //g_print ("cam pick: cap=%p region=%p\n", state->camera, object);
      if (rut_camera_pick_input_region (state->camera,
                                        region, state->x, state->y))
        {
          if (region->callback (region, state->event, region->user_data) ==
              RUT_INPUT_EVENT_STATUS_HANDLED)
            return RUT_TRAVERSE_VISIT_BREAK;
        }
    }
  else if (rut_object_get_type (object) == &rut_ui_viewport_type)
    {
      RutUIViewport *ui_viewport = RUT_UI_VIEWPORT (object);
      CoglMatrix transform;
      RutShapeRectange rect;
      float poly[16];
      RutObject *parent = rut_graphable_get_parent (object);
      const CoglMatrix *view = rut_camera_get_view_transform (state->camera);

      transform = *view;
      rut_graphable_apply_transform (parent, &transform);

      rect.x0 = 0;
      rect.y0 = 0;
      rect.x1 = rut_ui_viewport_get_width (ui_viewport);
      rect.y1 = rut_ui_viewport_get_height (ui_viewport);

      rect_to_screen_polygon (&rect,
                              &transform,
                              &state->camera->projection,
                              state->camera->viewport,
                              poly);

      if (!point_in_screen_poly (state->x, state->y,
                                 poly, sizeof (float) * 4, 4))
        return RUT_TRAVERSE_VISIT_SKIP_CHILDREN;
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutInputEventStatus
_rut_shell_handle_input (RutShell *shell, RutInputEvent *event)
{
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
  GList *l, *next;
  RutClosure *c, *tmp;

  event->camera = shell->window_camera;

  rut_list_for_each_safe (c, tmp, &shell->input_cb_list, list_node)
    {
      RutInputCallback cb = c->function;

      status = cb (event, c->user_data);
      if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
        return status;
    }

  for (l = shell->grabs; l; l = next)
    {
      RutShellGrab *grab = l->data;
      RutCamera *old_camera = event->camera;
      RutInputEventStatus grab_status;

      next = l->next;

      if (grab->camera)
        event->camera = grab->camera;

      grab_status = grab->callback (event, grab->user_data);

      event->camera = old_camera;

      if (grab_status == RUT_INPUT_EVENT_STATUS_HANDLED)
        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  if (shell->keyboard_focus_object &&
      rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY)
    {
      RutInputRegion *region =
        rut_inputable_get_input_region (shell->keyboard_focus_object);

      status = region->callback (region, event, region->user_data);
      if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
        return status;
    }

  /* XXX: remove this when the rotation tool works with RutCamera */
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);

      for (l = shell->input_regions; l; l = l->next)
        {
          RutInputRegion *region = l->data;

          if (rut_camera_pick_input_region (NULL, region, x, y))
            {
              status = region->callback (region, event, region->user_data);

              if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
                return status;
            }
        }
    }


  for (l = shell->input_cameras; l; l = l->next)
    {
      InputCamera *input_camera = l->data;
      RutCamera *camera = input_camera->camera;
      RutObject *scenegraph = input_camera->scenegraph;
      GList *l2;

      event->camera = camera;

      event->input_transform = &camera->input_transform;

      if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          CameraPickState state;

          for (l2 = camera->input_regions; l2; l2 = l2->next)
            {
              RutInputRegion *region = l2->data;

              if (rut_camera_pick_input_region (camera, region, x, y))
                {
                  status = region->callback (region, event, region->user_data);

                  if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
                    return status;
                }
            }

          if (scenegraph)
            {
              RutTraverseVisitFlags flags;
              state.camera = camera;
              state.event = event;
              state.x = x;
              state.y = y;

              flags = rut_graphable_traverse (scenegraph,
                                              RUT_TRAVERSE_DEPTH_FIRST,
                                              camera_pick_region_cb,
                                              NULL,
                                              &state);
              if (flags & RUT_TRAVERSE_VISIT_BREAK)
                return RUT_INPUT_EVENT_STATUS_HANDLED;
            }
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
  RutInputEvent rut_event;
  RutShell *shell = (RutShell *)app->userData;

  rut_event.native = event;
  rut_event.input_transform = NULL;

  if (_rut_shell_handle_input (shell, &rut_event) ==
      RUT_INPUT_EVENT_STATUS_HANDLED)
    return 1;

  return 0;
}

static int
android_init (RutShell *shell)
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
  RutShell *shell = (RutShell *) app->userData;

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
      _rut_shell_fini (shell);
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

static void
_rut_shell_remove_grab_link (RutShell *shell,
                             GList *link)
{
  RutShellGrab *grab = link->data;

  if (grab->camera)
    rut_refable_unref (grab->camera);
  g_slice_free (RutShellGrab, grab);
  shell->grabs = g_list_delete_link (shell->grabs, link);
}

RutType rut_shell_type;

static void
_rut_shell_free (void *object)
{
  RutShell *shell = object;
  GList *l;

  rut_closure_list_disconnect_all (&shell->input_cb_list);

  for (l = shell->input_regions; l; l = l->next)
    rut_refable_unref (l->data);
  g_list_free (shell->input_regions);

  while (shell->grabs)
    _rut_shell_remove_grab_link (shell, shell->grabs);

  _rut_shell_remove_all_input_cameras (shell);

  _rut_shell_fini (shell);

  g_free (shell);
}

RutRefCountableVTable _rut_shell_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_shell_free
};


static void
_rut_shell_init_types (void)
{
  rut_type_init (&rut_shell_type);
  rut_type_add_interface (&rut_shell_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutShell, ref_count),
                          &_rut_shell_ref_countable_vtable);

  _rut_slider_init_type ();
  _rut_input_region_init_type ();
}

RutShell *
rut_shell_new (RutShellInitCallback init,
               RutShellFiniCallback fini,
               RutShellPaintCallback paint,
               void *user_data)
{
  static CoglBool initialized = FALSE;
  RutShell *shell;

  /* Make sure core types are registered */
  _rut_init ();

  if (G_UNLIKELY (initialized == FALSE))
    _rut_shell_init_types ();

  shell = g_new0 (RutShell, 1);

  shell->ref_count = 1;

  rut_object_init (&shell->_parent, &rut_shell_type);

  rut_list_init (&shell->input_cb_list);

  shell->init_cb = init;
  shell->fini_cb = fini;
  shell->paint_cb = paint;
  shell->user_data = user_data;

  return shell;
}

/* Note: we don't take a reference on the context so we don't
 * introduce a circular reference. */
void
_rut_shell_associate_context (RutShell *shell,
                              RutContext *context)
{
  shell->rut_ctx = context;
}

void
_rut_shell_init (RutShell *shell)
{
#if defined(USE_SDL) && SDL_MAJOR_VERSION < 2
  SDL_EnableUNICODE (1);
#endif
}

void
rut_shell_set_window_camera (RutShell *shell, RutCamera *window_camera)
{
  shell->window_camera = window_camera;
}

#ifdef __ANDROID__

RutShell *
rut_android_shell_new (struct android_app* application,
                       RutShellInitCallback init,
                       RutShellFiniCallback fini,
                       RutShellPaintCallback paint,
                       void *user_data)
{
  RutShell *shell = rut_shell_new (init, fini, paint, user_data);

  shell->app = application;
  application->userData = shell;
  application->onAppCmd = android_handle_cmd;
  application->onInputEvent = android_handle_input;
}

#endif

void
rut_shell_grab_key_focus (RutShell *shell,
                          RutObject *inputable,
                          GDestroyNotify ungrab_callback)
{
  g_return_if_fail (rut_object_is (inputable, RUT_INTERFACE_ID_INPUTABLE));

  /* If something tries to set the keyboard focus to the same object
   * then we probably do still want to call the keyboard ungrab
   * callback for the last object that set it. The code may be
   * assuming that when this function is called it definetely has the
   * keyboard focus and that the callback will definetely called at
   * some point. Otherwise this function is more like a request and it
   * should have a way of reporting whether the request succeeded */

  if (rut_object_is (inputable, RUT_INTERFACE_ID_REF_COUNTABLE))
    rut_refable_ref (inputable);

  rut_shell_ungrab_key_focus (shell);

  shell->keyboard_focus_object = inputable;
  shell->keyboard_ungrab_cb = ungrab_callback;
}

void
rut_shell_ungrab_key_focus (RutShell *shell)
{
  if (shell->keyboard_focus_object)
    {
      if (shell->keyboard_ungrab_cb)
        shell->keyboard_ungrab_cb (shell->keyboard_focus_object);

      if (rut_object_is (shell->keyboard_focus_object,
                         RUT_INTERFACE_ID_REF_COUNTABLE))
        rut_refable_unref (shell->keyboard_focus_object);

      shell->keyboard_focus_object = NULL;
      shell->keyboard_ungrab_cb = NULL;
    }
}

static CoglBool
_rut_shell_paint (RutShell *shell)
{
  GSList *l;
  //CoglBool status;

  for (l = shell->rut_ctx->timelines; l; l = l->next)
    _rut_timeline_update (l->data);

  if (shell->paint_cb (shell, shell->user_data))
    return TRUE;

  for (l = shell->rut_ctx->timelines; l; l = l->next)
    if (rut_timeline_is_running (l->data))
      return TRUE;

  return FALSE;
}

#ifdef USE_SDL

static void
sdl_handle_event (RutShell *shell, SDL_Event *event)
{
  switch (event->type)
    {
#if SDL_MAJOR_VERSION < 2
    case SDL_VIDEOEXPOSE:
      shell->redraw_queued = TRUE;
      break;
#else /* SDL_MAJOR_VERSION < 2 */
    case SDL_WINDOWEVENT:
      switch (event->window.event)
        {
        case SDL_WINDOWEVENT_EXPOSED:
          shell->redraw_queued = TRUE;
          break;

        case SDL_WINDOWEVENT_CLOSE:
          shell->quit = TRUE;
          break;
        }
      break;
#endif /* SDL_MAJOR_VERSION < 2 */

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_KEYUP:
    case SDL_KEYDOWN:
      {
        RutInputEvent rut_event;

        rut_event.native = event;
        rut_event.input_transform = NULL;

        _rut_shell_handle_input (shell, &rut_event);
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
  RutShell *shell = user_data;

  shell->redraw_queued = _rut_shell_paint (shell);

  /* If the driver can deliver swap complete events then we can remove
   * the idle paint callback until we next get a swap complete event
   * otherwise we keep the idle paint callback installed and simply
   * paint as fast as the driver will allow... */
  if (cogl_has_feature (shell->rut_ctx->cogl_context, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    return FALSE; /* remove the callback */
  else
    return TRUE;
}

static void
swap_complete_cb (CoglFramebuffer *framebuffer, void *user_data)
{
  RutShell *shell = user_data;

  if (shell->redraw_queued)
    g_idle_add (glib_paint_cb, user_data);
}
#endif

void
rut_shell_main (RutShell *shell)
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
              shell->fini_cb (shell, shell->user_data);
              return;
            }

          if (source != NULL)
            source->process (shell->app, source);
        }

      shell->redraw_queued = _rut_shell_paint (shell);
    }

#elif defined(USE_SDL)

  shell->init_cb (shell, shell->user_data);

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

              cogl_sdl_idle (shell->rut_ctx->cogl_context);
              if (!SDL_WaitEvent (&event))
                g_error ("Error waiting for SDL events");
            }

          sdl_handle_event (shell, &event);
          cogl_sdl_handle_event (shell->rut_ctx->cogl_context, &event);
        }

      shell->redraw_queued = _rut_shell_paint (shell);
    }

  shell->fini_cb (shell, shell->user_data);

#elif defined (USE_GLIB)
  GSource *cogl_source;
  GMainLoop *loop;

  shell->init_cb (shell, shell->user_data);

  cogl_source = cogl_glib_source_new (shell->ctx, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  if (cogl_has_feature (shell->ctx, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    cogl_onscreen_add_swap_buffers_callback (COGL_ONSCREEN (shell->fb),
                                             swap_complete_cb, shell);

  g_idle_add (glib_paint_cb, shell);

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);

  shell->fini_cb (shell, shell->user_data);

#else

#error "No platform mainloop provided"

#endif
}

void
rut_shell_grab_input (RutShell *shell,
                      RutCamera *camera,
                      RutInputCallback callback,
                      void *user_data)
{
  RutShellGrab *grab = g_slice_new (RutShellGrab);

  grab->callback = callback;
  grab->user_data = user_data;

  if (camera)
    grab->camera = rut_refable_ref (camera);
  else
    grab->camera = NULL;

  shell->grabs = g_list_prepend (shell->grabs, grab);
}

void
rut_shell_ungrab_input (RutShell *shell,
                        RutInputCallback callback,
                        void *user_data)
{
  GList *l;

  for (l = shell->grabs; l; l = l->next)
    {
      RutShellGrab *grab = l->data;

      if (grab->callback == callback && grab->user_data == user_data)
        {
          _rut_shell_remove_grab_link (shell, l);
          break;
        }
    }
}

void
rut_shell_queue_redraw (RutShell *shell)
{
  shell->redraw_queued = TRUE;
}

enum {
  RUT_SLIDER_PROP_PROGRESS,
  RUT_SLIDER_N_PROPS
};

struct _RutSlider
{
  RutObjectProps _parent;
  int ref_count;

  /* FIXME: It doesn't seem right that we should have to save a
   * pointer to the context for input here... */
  RutContext *ctx;

  /* FIXME: It also doesn't seem right to have to save a pointer
   * to the camera here so we can queue a redraw */
  //RutCamera *camera;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
  RutSimpleWidgetProps simple_widget;
  RutSimpleIntrospectableProps introspectable;

  RutNineSlice *background;
  RutNineSlice *handle;
  RutTransform *handle_transform;

  RutInputRegion *input_region;
  float grab_x;
  float grab_y;
  float grab_progress;

  RutAxis axis;
  float range_min;
  float range_max;
  float length;
  float progress;

  RutProperty properties[RUT_SLIDER_N_PROPS];
};

static RutPropertySpec _rut_slider_prop_specs[] = {
  {
    .name = "progress",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutSlider, progress),
    .setter = rut_slider_set_progress
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_slider_free (void *object)
{
  RutSlider *slider = object;

  rut_refable_simple_unref (slider->input_region);

  rut_graphable_remove_child (slider->handle_transform);

  rut_refable_simple_unref (slider->handle_transform);
  rut_refable_simple_unref (slider->handle);
  rut_refable_simple_unref (slider->background);

  rut_simple_introspectable_destroy (slider);

  rut_graphable_destroy (slider);

  g_slice_free (RutSlider, object);
}

RutRefCountableVTable _rut_slider_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_slider_free
};

static RutGraphableVTable _rut_slider_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_slider_paint (RutObject *object, RutPaintContext *paint_ctx)
{
  RutSlider *slider = RUT_SLIDER (object);
  RutPaintableVTable *bg_paintable =
    rut_object_get_vtable (slider->background, RUT_INTERFACE_ID_PAINTABLE);

  bg_paintable->paint (RUT_OBJECT (slider->background), paint_ctx);
}

static RutPaintableVTable _rut_slider_paintable_vtable = {
  _rut_slider_paint
};

static RutIntrospectableVTable _rut_slider_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

#if 0
static void
_rut_slider_set_camera (RutObject *object,
                        RutCamera *camera)
{
  RutSlider *slider = RUT_SLIDER (object);

  if (camera)
    rut_camera_add_input_region (camera, slider->input_region);

  slider->camera = camera;
}
#endif

RutSimpleWidgetVTable _rut_slider_simple_widget_vtable = {
  0,
};

RutType rut_slider_type;

static void
_rut_slider_init_type (void)
{
  rut_type_init (&rut_slider_type);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutSlider, ref_count),
                          &_rut_slider_ref_countable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutSlider, graphable),
                          &_rut_slider_graphable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutSlider, paintable),
                          &_rut_slider_paintable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RutSlider, simple_widget),
                          &_rut_slider_simple_widget_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_slider_introspectable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutSlider, introspectable),
                          NULL); /* no implied vtable */
}

static RutInputEventStatus
_rut_slider_grab_input_cb (RutInputEvent *event,
                           void *user_data)
{
  RutSlider *slider = user_data;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = slider->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell,
                                  _rut_slider_grab_input_cb,
                                  user_data);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float diff;
          float progress;

          if (slider->axis == RUT_AXIS_X)
            diff = rut_motion_event_get_x (event) - slider->grab_x;
          else
            diff = rut_motion_event_get_y (event) - slider->grab_y;

          progress = CLAMP (slider->grab_progress + diff / slider->length, 0, 1);

          rut_slider_set_progress (slider, progress);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_slider_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  RutSlider *slider = user_data;

  g_print ("Slider input\n");

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = slider->ctx->shell;
      rut_shell_grab_input (shell,
                            rut_input_event_get_camera (event),
                            _rut_slider_grab_input_cb,
                            slider);
      slider->grab_x = rut_motion_event_get_x (event);
      slider->grab_y = rut_motion_event_get_y (event);
      slider->grab_progress = slider->progress;
      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutSlider *
rut_slider_new (RutContext *ctx,
                RutAxis axis,
                float min,
                float max,
                float length)
{
  RutSlider *slider = g_slice_new0 (RutSlider);
  CoglTexture *bg_texture;
  CoglTexture *handle_texture;
  CoglError *error = NULL;
  //PangoRectangle label_size;
  float width;
  float height;

  rut_object_init (&slider->_parent, &rut_slider_type);

  slider->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (slider));
  rut_paintable_init (RUT_OBJECT (slider));

  slider->ctx = ctx;

  slider->axis = axis;
  slider->range_min = min;
  slider->range_max = max;
  slider->length = length;
  slider->progress = 0;

  bg_texture = rut_load_texture (ctx, RIG_DATA_DIR "slider-background.png", &error);
  if (!bg_texture)
    {
      g_warning ("Failed to load slider-background.png: %s", error->message);
      g_error_free (error);
    }

  handle_texture = rut_load_texture (ctx, RIG_DATA_DIR "slider-handle.png", &error);
  if (!handle_texture)
    {
      g_warning ("Failed to load slider-handle.png: %s", error->message);
      g_error_free (error);
    }

  if (axis == RUT_AXIS_X)
    {
      width = length;
      height = 20;
    }
  else
    {
      width = 20;
      height = length;
    }

  slider->background = rut_nine_slice_new (ctx, bg_texture, 2, 3, 3, 3,
                                           width, height);

  if (axis == RUT_AXIS_X)
    width = 20;
  else
    height = 20;

  slider->handle_transform =
    rut_transform_new (ctx,
                       slider->handle = rut_nine_slice_new (ctx,
                                                            handle_texture,
                                                            4, 5, 6, 5,
                                                            width, height),
                       NULL);
  rut_graphable_add_child (slider, slider->handle_transform);

  slider->input_region =
    rut_input_region_new_rectangle (0, 0, width, height,
                                    _rut_slider_input_cb,
                                    slider);

  //rut_input_region_set_graphable (slider->input_region, slider->handle);
  rut_graphable_add_child (slider, slider->input_region);

  rut_simple_introspectable_init (slider,
                                  _rut_slider_prop_specs,
                                  slider->properties);

  return slider;
}

void
rut_slider_set_range (RutSlider *slider,
                      float min, float max)
{
  slider->range_min = min;
  slider->range_max = max;
}

void
rut_slider_set_length (RutSlider *slider,
                       float length)
{
  slider->length = length;
}

void
rut_slider_set_progress (RutSlider *slider,
                         float progress)
{
  float translation;

  if (slider->progress == progress)
    return;

  slider->progress = progress;
  rut_property_dirty (&slider->ctx->property_ctx,
                      &slider->properties[RUT_SLIDER_PROP_PROGRESS]);

  translation = (slider->length - 20) * slider->progress;

  rut_transform_init_identity (slider->handle_transform);

  if (slider->axis == RUT_AXIS_X)
    rut_transform_translate (slider->handle_transform, translation, 0, 0);
  else
    rut_transform_translate (slider->handle_transform, 0, translation, 0);

  rut_shell_queue_redraw (slider->ctx->shell);

  g_print ("progress = %f\n", slider->progress);
}

