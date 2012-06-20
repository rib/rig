#ifndef _RIG_SHELL_H_
#define _RIG_SHELL_H_

#include "rig.h"
#include "rig-keysyms.h"
#include "rig-types.h"

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#endif

typedef struct _RigShell RigShell;

typedef void (*RigShellInitCallback) (RigShell *shell, void *user_data);
typedef void (*RigShellFiniCallback) (RigShell *shell, void *user_data);
typedef CoglBool (*RigShellPaintCallback) (RigShell *shell, void *user_data);

RigType rig_shell_type;

RigShell *
rig_shell_new (RigShellInitCallback init,
               RigShellFiniCallback fini,
               RigShellPaintCallback paint,
               void *user_data);

#ifdef __ANDROID__
RigShell *
rig_android_shell_new (struct android_app* application,
                       RigShellInitCallback init,
                       RigShellFiniCallback fini,
                       RigShellPaintCallback paint,
                       void *user_data);
#endif

/* PRIVATE */
void
_rig_shell_associate_context (RigShell *shell,
                              RigContext *context);

void
_rig_shell_init (RigShell *shell);

RigContext *
rig_shell_get_context (RigShell *shell);

void
rig_shell_main (RigShell *shell);

void
rig_shell_add_input_camera (RigShell *shell,
                            RigCamera *camera);

void
rig_shell_remove_input_camera (RigShell *shell,
                               RigCamera *camera);

typedef enum _RigInputEventType
{
  RIG_INPUT_EVENT_TYPE_MOTION = 1,
  RIG_INPUT_EVENT_TYPE_KEY
} RigInputEventType;

typedef enum _RigKeyEventAction
{
  RIG_KEY_EVENT_ACTION_UP = 1,
  RIG_KEY_EVENT_ACTION_DOWN
} RigKeyEventAction;

typedef enum _RigMotionEventAction
{
  RIG_MOTION_EVENT_ACTION_UP = 1,
  RIG_MOTION_EVENT_ACTION_DOWN,
  RIG_MOTION_EVENT_ACTION_MOVE,
} RigMotionEventAction;

typedef enum _RigButtonState
{
  RIG_BUTTON_STATE_1         = 1<<0,
  RIG_BUTTON_STATE_2         = 1<<1,
  RIG_BUTTON_STATE_3         = 1<<2,
  RIG_BUTTON_STATE_WHEELUP   = 1<<3,
  RIG_BUTTON_STATE_WHEELDOWN = 1<<4
} RigButtonState;

typedef enum _RigModifierState
{
  RIG_MODIFIER_LEFT_ALT_ON = 1<<0,
  RIG_MODIFIER_RigHT_ALT_ON = 1<<1,

  RIG_MODIFIER_LEFT_SHIFT_ON = 1<<2,
  RIG_MODIFIER_RigHT_SHIFT_ON = 1<<3,

  RIG_MODIFIER_LEFT_CTRL_ON = 1<<4,
  RIG_MODIFIER_RigHT_CTRL_ON = 1<<5,

  RIG_MODIFIER_LEFT_META_ON = 1<<6,
  RIG_MODIFIER_RigHT_META_ON = 1<<7,

  RIG_MODIFIER_NUM_LOCK_ON = 1<<8,
  RIG_MODIFIER_CAPS_LOCK_ON = 1<<9

} RigModifierState;

#define RIG_MODIFIER_ALT_ON (RIG_MODIFIER_LEFT_ALT_ON|RIG_MODIFIER_RigHT_ALT_ON)
#define RIG_MODIFIER_SHIFT_ON (RIG_MODIFIER_LEFT_SHIFT_ON|RIG_MODIFIER_RigHT_SHIFT_ON)
#define RIG_MODIFIER_CTRL_ON (RIG_MODIFIER_LEFT_CTRL_ON|RIG_MODIFIER_RigHT_CTRL_ON)
#define RIG_MODIFIER_META_ON (RIG_MODIFIER_LEFT_META_ON|RIG_MODIFIER_RigHT_META_ON)

typedef enum _RigInputEventStatus
{
  RIG_INPUT_EVENT_STATUS_UNHANDLED,
  RIG_INPUT_EVENT_STATUS_HANDLED,
} RigInputEventStatus;

typedef struct _RigInputEvent RigInputEvent;

typedef RigInputEventStatus (*RigInputCallback) (RigInputEvent *event,
                                                 void *user_data);

typedef struct _RigInputEvent RigInputEvent;

void
rig_shell_grab_input (RigShell *shell,
                      RigInputCallback callback,
                      void *user_data);

void
rig_shell_ungrab_input (RigShell *shell);

void
rig_shell_queue_redraw (RigShell *shell);

RigInputEventType
rig_input_event_get_type (RigInputEvent *event);

RigKeyEventAction
rig_key_event_get_action (RigInputEvent *event);

int32_t
rig_key_event_get_keysym (RigInputEvent *event);

RigMotionEventAction
rig_motion_event_get_action (RigInputEvent *event);

RigButtonState
rig_motion_event_get_button_state (RigInputEvent *event);

RigModifierState
rig_motion_event_get_modifier_state (RigInputEvent *event);

float
rig_motion_event_get_x (RigInputEvent *event);

float
rig_motion_event_get_y (RigInputEvent *event);

typedef struct _RigInputRegion RigInputRegion;
#define RIG_INPUT_REGION(X) ((RigInputRegion *)X)

RigType rig_input_region_type;

typedef RigInputEventStatus (*RigInputRegionCallback) (RigInputRegion *region,
                                                       RigInputEvent *event,
                                                       void *user_data);

RigInputRegion *
rig_input_region_new_rectangle (float x0,
                                float y0,
                                float x1,
                                float y1,
                                RigInputRegionCallback callback,
                                void *user_data);
void
rig_input_region_set_transform (RigInputRegion *region,
                                CoglMatrix *matrix);

void
rig_input_region_set_graphable (RigInputRegion *region,
                                RigObject *graphable);

void
rig_input_region_set_rectangle (RigInputRegion *region,
                                float x0,
                                float y0,
                                float x1,
                                float y1);

void
rig_shell_add_input_region (RigShell *shell,
                            RigInputRegion *region);

void
rig_shell_remove_input_region (RigShell *shell,
                               const RigInputRegion *region);

typedef struct _RigScrollBar RigScrollBar;
#define RIG_SCROLL_BAR(X) ((RigScrollBar *)X)

RigType rig_scroll_bar_type;

typedef enum _RigAxis
{
  RIG_AXIS_X,
  RIG_AXIS_Y,
  RIG_AXIS_Z
} RigAxis;

RigScrollBar *
rig_scroll_bar_new (RigContext *ctx,
                    RigAxis axis,
                    float length,
                    float virtual_length,
                    float viewport_length);

/* How long is virtual length of the document being scrolled */
void
rig_scroll_bar_set_virtual_length (RigScrollBar *scroll_bar,
                                   float virtual_length);

/* What is the length of the viewport into the document being scrolled */
void
rig_scroll_bar_set_viewport_length (RigScrollBar *scroll_bar,
                                    float viewport_length);

typedef struct _RigSlider RigSlider;
#define RIG_SLIDER(X) ((RigSlider *)X)

RigType rig_slider_type;

RigSlider *
rig_slider_new (RigContext *ctx,
                RigAxis axis,
                float min,
                float max,
                float length);

void
rig_slider_set_range (RigSlider *slider,
                      float min, float max);

void
rig_slider_set_length (RigSlider *slider,
                       float length);

void
rig_slider_set_progress (RigSlider *slider,
                         float progress);
void
rig_shell_set_input_callback (RigShell *shell,
                              RigInputCallback callback,
                              void *user_data);

#endif /* _RIG_SHELL_H_ */
