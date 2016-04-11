/*
 * Rig C
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _RIG_C_
#define _RIG_C_

#include <stdbool.h>
#include <stdint.h>

typedef struct _RModule RModule;

typedef struct _RInputEvent RInputEvent;

typedef struct _RObject RObject;

typedef enum {
    R_INPUT_EVENT_TYPE_MOTION = 1,
    R_INPUT_EVENT_TYPE_KEY,
    R_INPUT_EVENT_TYPE_TEXT,
} RInputEventType;

typedef enum {
    R_KEY_EVENT_ACTION_UP = 1,
    R_KEY_EVENT_ACTION_DOWN
} RKeyEventAction;

typedef enum _r_motion_event_action_t {
    R_MOTION_EVENT_ACTION_UP = 1,
    R_MOTION_EVENT_ACTION_DOWN,
    R_MOTION_EVENT_ACTION_MOVE,
} RMotionEventAction;

typedef enum {
    R_BUTTON_STATE_1 = 1 << 1,
    R_BUTTON_STATE_2 = 1 << 2,
    R_BUTTON_STATE_3 = 1 << 3,
} RButtonState;

typedef enum {
    R_MODIFIER_SHIFT_ON = 1 << 0,
    R_MODIFIER_CTRL_ON = 1 << 1,
    R_MODIFIER_ALT_ON = 1 << 2,
    R_MODIFIER_NUM_LOCK_ON = 1 << 3,
    R_MODIFIER_CAPS_LOCK_ON = 1 << 4
} RModifierState;

typedef enum {
    R_INPUT_EVENT_STATUS_UNHANDLED,
    R_INPUT_EVENT_STATUS_HANDLED,
} RInputEventStatus;


typedef struct {
    double progress;
} RUpdateState;

RInputEventType r_input_event_get_type(RInputEvent *event);

RKeyEventAction r_key_event_get_action(RInputEvent *event);

int32_t r_key_event_get_keysym(RInputEvent *event);

RModifierState r_key_event_get_modifier_state(RInputEvent *event);

RMotionEventAction r_motion_event_get_action(RInputEvent *event);

RButtonState r_motion_event_get_button(RInputEvent *event);

RButtonState r_motion_event_get_button_state(RInputEvent *event);

RModifierState r_motion_event_get_modifier_state(RInputEvent *event);

float r_motion_event_get_x(RInputEvent *event);
float r_motion_event_get_y(RInputEvent *event);

void r_debug(RModule *module,
             const char *format,
             ...);

typedef struct _RColor {
    float red;
    float green;
    float blue;
    float alpha;
} RColor;

#define r_color_str(module, str) ({ \
    RColor *color = alloca(sizeof(RColor)); \
    r_color_init_from_string(module, color, str); \
    color; \
})

RColor *r_color_init_from_string(RModule *module, RColor *color, const char *str);

typedef struct {
    float heading;
    float pitch;
    float roll;
} REuler;

typedef struct {
    float w;

    float x;
    float y;
    float z;
} RQuaternion;

RQuaternion r_quaternion_identity(void);
RQuaternion r_quaternion(float angle, float x, float y, float z);

RQuaternion r_quaternion_from_angle_vector(float angle,
                                           const float *axis3f);
RQuaternion r_quaternion_from_array(const float *array);
RQuaternion r_quaternion_from_x_rotation(float angle);
RQuaternion r_quaternion_from_y_rotation(float angle);
RQuaternion r_quaternion_from_z_rotation(float angle);
RQuaternion r_quaternion_from_euler(const REuler *euler);
//RQuaternion r_quaternion_from_matrix(const RMatrix *matrix);

bool r_quaternion_equal(const RQuaternion *a, const RQuaternion *b);

float r_quaternion_get_rotation_angle(const RQuaternion *quaternion);
void r_quaternion_get_rotation_axis(const RQuaternion *quaternion,
                                    float *vector3);

void r_quaternion_normalize(RQuaternion *quaternion);
void r_quaternion_invert(RQuaternion *quaternion);
RQuaternion r_quaternion_multiply(const RQuaternion *left,
                                  const RQuaternion *right);

void r_quaternion_rotate_x_axis(RQuaternion *quaternion, float x_angle);
void r_quaternion_rotate_y_axis(RQuaternion *quaternion, float y_angle);
void r_quaternion_rotate_z_axis(RQuaternion *quaternion, float z_angle);

void r_quaternion_pow(RQuaternion *quaternion, float exponent);

float r_quaternion_dot_product(const RQuaternion *a,
                               const RQuaternion *b);

RQuaternion r_quaternion_slerp(const RQuaternion *a,
                               const RQuaternion *b,
                               float t);
RQuaternion r_quaternion_nlerp(const RQuaternion *a,
                               const RQuaternion *b,
                               float t);
RQuaternion r_quaternion_squad(const RQuaternion *prev,
                               const RQuaternion *a,
                               const RQuaternion *b,
                               const RQuaternion *next,
                               float t);

RObject *r_find(RModule *module, const char *name);

RObject *r_entity_new(RModule *module, RObject *parent);
RObject *r_entity_clone(RModule *module, RObject *entity);

void r_entity_translate(RModule *module, RObject *entity, float tx, float tz, float ty);

void r_entity_rotate_x_axis(RModule *module, RObject *entity, float x_angle);
void r_entity_rotate_y_axis(RModule *module, RObject *entity, float y_angle);
void r_entity_rotate_z_axis(RModule *module, RObject *entity, float z_angle);

void r_entity_delete(RModule *module, RObject *entity);
void r_component_delete(RModule *module, RObject *component);

RObject *r_view_new(RModule *module);
void r_view_delete(RModule *module, RObject *view);

RObject *r_controller_new(RModule *module, const char *name);
void r_controller_delete(RModule *module, RObject *controller);
void r_controller_bind(RModule *module, RObject *controller,
                       RObject *dst_obj, const char *dst_prop_name,
                       RObject *src_obj, const char *src_prop_name);

void r_request_animation_frame(RModule *module);

typedef enum {
    R_PROJECTION_PERSPECTIVE = 0,
    R_PROJECTION_ORTHOGRAPHIC = 2,
} RProjection;

RObject *r_camera_new(RModule *module);

void r_open_view(RModule *module, RObject *camera_entity);

RObject *r_light_new(RModule *module);

RObject *r_shape_new(RModule *module, float width, float height);
RObject *r_nine_slice_new(RModule *module,
                          float top, float right, float bottom, float left,
                          float width, float height);
RObject *r_diamond_new(RModule *module, float size);
RObject *r_pointalism_grid_new(RModule *module, float size);

RObject *r_material_new(RModule *module);
RObject *r_source_new(RModule *module, const char *url);

RObject *r_button_input_new(RModule *module);

RObject *r_text_new(RModule *module);

void r_add_component(RModule *module, RObject *entity, RObject *component);

void r_set_integer_by_name(RModule *module, RObject *object, const char *name, int value);
void r_set_integer(RModule *module, RObject *object, int id, int value);

void r_set_float_by_name(RModule *module, RObject *object, const char *name, float value);
void r_set_float(RModule *module, RObject *object, int id, float value);

void r_set_boolean_by_name(RModule *module, RObject *object, const char *name, bool value);
void r_set_boolean(RModule *module, RObject *object, int id, bool value);

void r_set_enum_by_name(RModule *module, RObject *object, const char *name, int value);
void r_set_enum(RModule *module, RObject *object, int id, int value);

void r_set_vec3(RModule *module, RObject *object, int id, const float value[3]);
void r_set_vec3_by_name(RModule *module, RObject *object, const char *name, const float value[3]);
void r_set_vec4(RModule *module, RObject *object, int id, const float value[4]);
void r_set_vec4_by_name(RModule *module, RObject *object, const char *name, const float value[4]);

void r_set_color(RModule *module, RObject *object, int id, const RColor *value);
void r_set_color_by_name(RModule *module, RObject *object, const char *name, const RColor *value);

void r_set_quaternion(RModule *module, RObject *object, int id, const RQuaternion *value);
void r_set_quaternion_by_name(RModule *module, RObject *object, const char *name, const RQuaternion *value);

void r_set_text_by_name(RModule *module, RObject *object, const char *name, const char *value);
void r_set_text(RModule *module, RObject *object, int id, const char *value);

void r_set_object_by_name(RModule *module, RObject *object, const char *name, RObject *value);
void r_set_object(RModule *module, RObject *object, int id, RObject *value);

typedef struct _r_engine r_engine_t;

typedef struct {
    bool require_vr_hmd;
} REngineConfig;

r_engine_t *r_engine_new(REngineConfig *config);

#define R_ABI_1         1
#define R_ABI_LATEST    R_ABI_1

bool r_engine_add_self_as_native_component(r_engine_t *stub_engine,
                                           int abi_version,
                                           const char *symbol_prefix);

void r_engine_run(r_engine_t *stub_engine);

void r_engine_ref(r_engine_t *stub_engine);
void r_engine_unref(r_engine_t *stub_engine);
#endif /* _RIG_C_ */
