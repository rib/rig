#ifndef _RIG_CONTEXT_H_
#define _RIG_CONTEXT_H_

#include <cogl-pango/cogl-pango.h>

#include "rig-object.h"
#include "rig-shell.h"
#include "rig-display-list.h"
#include "rig-property.h"

/* TODO: This header needs to be split up, since most of the APIs here
 * don't relate directly to the RigContext type. */

#define RIG_UINT32_RED_AS_FLOAT(COLOR)   (((COLOR & 0xff000000) >> 24) / 255.0)
#define RIG_UINT32_GREEN_AS_FLOAT(COLOR) (((COLOR & 0xff0000) >> 16) / 255.0)
#define RIG_UINT32_BLUE_AS_FLOAT(COLOR)  (((COLOR & 0xff00) >> 8) / 255.0)
#define RIG_UINT32_ALPHA_AS_FLOAT(COLOR) ((COLOR & 0xff) / 255.0)


extern uint8_t _rig_nine_slice_indices_data[54];

/* PRIVATE */
void
_rig_init (void);

typedef struct _RigPaintableProps
{
  int padding;
} RigPaintableProps;

typedef struct _RigPaintContext
{
  RigCamera *camera;
} RigPaintContext;

#define RIG_PAINT_CONTEXT(X) ((RigPaintContext *)X)

typedef struct _RigPaintableVtable
{
  void (*paint) (RigObject *object, RigPaintContext *paint_ctx);
} RigPaintableVTable;

void
rig_paintable_init (RigObject *object);

void
rig_paintable_paint (RigObject *object, RigPaintContext *paint_ctx);

typedef struct _RigSettings RigSettings;

/* TODO Make internals private */
struct _RigContext
{
  RigObjectProps _parent;
  int ref_count;

  RigShell *shell;

  RigSettings *settings;

  CoglContext *cogl_context;

  CoglMatrix identity_matrix;

  GHashTable *texture_cache;

  CoglIndices *nine_slice_indices;

  CoglTexture *circle_texture;

  CoglPangoFontMap *pango_font_map;
  PangoContext *pango_context;
  PangoFontDescription *pango_font_desc;

  RigPropertyContext property_ctx;

  GSList *timelines;
};

RigContext *
rig_context_new (RigShell *shell /* optional */);

void
rig_context_init (RigContext *context);

typedef void (*RigSettingsChangedCallback) (RigSettings *settings,
                                            void *user_data);

void
rig_settings_add_changed_callback (RigSettings *settings,
                                   RigSettingsChangedCallback callback,
                                   GDestroyNotify destroy_notify,
                                   void *user_data);

void
rig_settings_remove_changed_callback (RigSettings *settings,
                                      RigSettingsChangedCallback callback);

unsigned int
rig_settings_get_password_hint_time (RigSettings *settings);

char *
rig_settings_get_font_name (RigSettings *settings);

CoglTexture *
rig_load_texture (RigContext *ctx, const char *filename, GError **error);

typedef struct _RigGraph RigGraph;
#define RIG_GRAPH(X) ((RigGraph *)X)

extern RigType rig_graph_type;

RigGraph *
rig_graph_new (RigContext *ctx, ...);


RigTransform *
rig_transform_new (RigContext *ctx, ...) G_GNUC_NULL_TERMINATED;

void
rig_transform_translate (RigTransform *transform,
                         float x,
                         float y,
                         float z);

void
rig_transform_rotate (RigTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z);

void
rig_transform_quaternion_rotate (RigTransform *transform,
                                 const CoglQuaternion *quaternion);

void
rig_transform_scale (RigTransform *transform,
                     float x,
                     float y,
                     float z);

void
rig_transform_init_identity (RigTransform *transform);

const CoglMatrix *
rig_transform_get_matrix (RigTransform *transform);

typedef struct _RigNineSlice RigNineSlice;
#define RIG_NINE_SLICE(X) ((RigNineSlice *)X)

extern RigType rig_nine_slice_type;

RigNineSlice *
rig_nine_slice_new (RigContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height);

typedef struct _RigSimpleWidgetProps
{
  RigDisplayList display_list;
} RigSimpleWidgetProps;

typedef struct _RigSimpleWidgetVTable
{
  void (*set_camera) (RigObject *widget,
                      RigCamera *camera);
} RigSimpleWidgetVTable;

void
rig_simple_widget_graphable_parent_changed (RigObject *self,
                                            RigObject *old_parent,
                                            RigObject *new_parent);

/* Use for widget that can't have children */
void
rig_simple_widget_graphable_child_removed_warn (RigObject *self,
                                                RigObject *child);

void
rig_simple_widget_graphable_child_added_warn (RigObject *self,
                                              RigObject *child);

typedef struct _RigRectangle RigRectangle;
#define RIG_RECTANGLE(X) ((RigRectangle *)X)

extern RigType rig_rectangle_type;

RigRectangle *
rig_rectangle_new4f (RigContext *ctx,
                     float width,
                     float height,
                     float red,
                     float green,
                     float blue,
                     float alpha);

void
rig_rectangle_set_width (RigRectangle *rectangle, float width);

void
rig_rectangle_set_height (RigRectangle *rectangle, float height);

typedef struct _RigUIViewport RigUIViewport;
#define RIG_UI_VIEWPORT(X) ((RigUIViewport *)X)

extern RigType rig_ui_viewport_type;

RigUIViewport *
rig_ui_viewport_new (RigContext *ctx,
                     float width,
                     float height,
                     ...);

void
rig_ui_viewport_set_width (RigUIViewport *ui_viewport, float width);

void
rig_ui_viewport_set_height (RigUIViewport *ui_viewport, float height);

void
rig_ui_viewport_set_doc_x (RigUIViewport *ui_viewport, float doc_x);

void
rig_ui_viewport_set_doc_y (RigUIViewport *ui_viewport, float doc_y);

void
rig_ui_viewport_set_doc_scale_x (RigUIViewport *ui_viewport, float doc_scale_x);

void
rig_ui_viewport_set_doc_scale_y (RigUIViewport *ui_viewport, float doc_scale_y);

float
rig_ui_viewport_get_width (RigUIViewport *ui_viewport);

float
rig_ui_viewport_get_height (RigUIViewport *ui_viewport);

float
rig_ui_viewport_get_doc_x (RigUIViewport *ui_viewport);

float
rig_ui_viewport_get_doc_y (RigUIViewport *ui_viewport);

float
rig_ui_viewport_get_doc_scale_x (RigUIViewport *ui_viewport);

float
rig_ui_viewport_get_doc_scale_y (RigUIViewport *ui_viewport);

const CoglMatrix *
rig_ui_viewport_get_doc_matrix (RigUIViewport *ui_viewport);

RigObject *
rig_ui_viewport_get_doc_node (RigUIViewport *ui_viewport);

typedef struct _RigButton RigButton;
#define RIG_BUTTON(X) ((RigButton *)X)

extern RigType rig_button_type;

RigButton *
rig_button_new (RigContext *ctx,
                const char *label);

typedef void (*RigButtonClickCallback) (RigButton *button, void *user_data);

void
rig_button_set_on_click_callback (RigButton *button,
                                  RigButtonClickCallback callback,
                                  void *user_data);

typedef struct _RigToggle RigToggle;
#define RIG_TOGGLE(X) ((RigToggle *)X)

extern RigType rig_toggle_type;

RigToggle *
rig_toggle_new (RigContext *ctx,
                const char *label);

typedef void (*RigToggleCallback) (RigToggle *toggle,
                                   CoglBool value,
                                   void *user_data);

void
rig_toggle_set_on_toggle_callback (RigToggle *toggle,
                                   RigToggleCallback callback,
                                   void *user_data);

void
rig_toggle_set_enabled (RigToggle *toggle,
                        CoglBool enabled);

void
rig_toggle_set_state (RigToggle *toggle,
                      CoglBool state);

RigProperty *
rig_toggle_get_enabled_property (RigToggle *toggle);

CoglTexture *
_rig_load_texture (RigContext *ctx, const char *filename, GError **error);

void
rig_color_init_from_uint32 (RigColor *color, uint32_t value);


#endif /* _RIG_CONTEXT_H_ */
