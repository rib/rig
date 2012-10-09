#ifndef _RUT_CONTEXT_H_
#define _RUT_CONTEXT_H_

#include <cogl-pango/cogl-pango.h>

#include "rut-object.h"
#include "rut-shell.h"
#include "rut-display-list.h"
#include "rut-property.h"
#include "rut-closure.h"

/* TODO: This header needs to be split up, since most of the APIs here
 * don't relate directly to the RutContext type. */

#define RUT_UINT32_RED_AS_FLOAT(COLOR)   (((COLOR & 0xff000000) >> 24) / 255.0)
#define RUT_UINT32_GREEN_AS_FLOAT(COLOR) (((COLOR & 0xff0000) >> 16) / 255.0)
#define RUT_UINT32_BLUE_AS_FLOAT(COLOR)  (((COLOR & 0xff00) >> 8) / 255.0)
#define RUT_UINT32_ALPHA_AS_FLOAT(COLOR) ((COLOR & 0xff) / 255.0)


extern uint8_t _rut_nine_slice_indices_data[54];

/* PRIVATE */
void
_rut_init (void);

/*
 * Note: The size and padding for this circle texture have been carefully
 * chosen so it has a power of two size and we have enough padding to scale
 * down the circle to a size of 2 pixels and still have a 1 texel transparent
 * border which we rely on for anti-aliasing.
 */
#define CIRCLE_TEX_RADIUS 16
#define CIRCLE_TEX_PADDING 16


typedef struct _RutSettings RutSettings;

/* TODO Make internals private */
struct _RutContext
{
  RutObjectProps _parent;
  int ref_count;

  RutShell *shell;

  RutSettings *settings;

  CoglContext *cogl_context;

  CoglMatrix identity_matrix;

  char *assets_location;

  GHashTable *texture_cache;

  CoglIndices *nine_slice_indices;

  CoglTexture *circle_texture;

  GHashTable *colors_hash;

  CoglPangoFontMap *pango_font_map;
  PangoContext *pango_context;
  PangoFontDescription *pango_font_desc;

  RutPropertyContext property_ctx;

  GSList *timelines;
};

RutContext *
rut_context_new (RutShell *shell /* optional */);

void
rut_context_init (RutContext *context);

void
rut_set_assets_location (RutContext *context,
                         const char *assets_location);

typedef void (*RutSettingsChangedCallback) (RutSettings *settings,
                                            void *user_data);

void
rut_settings_add_changed_callback (RutSettings *settings,
                                   RutSettingsChangedCallback callback,
                                   GDestroyNotify destroy_notify,
                                   void *user_data);

void
rut_settings_remove_changed_callback (RutSettings *settings,
                                      RutSettingsChangedCallback callback);

unsigned int
rut_settings_get_password_hint_time (RutSettings *settings);

char *
rut_settings_get_font_name (RutSettings *settings);

CoglTexture *
rut_load_texture (RutContext *ctx, const char *filename, CoglError **error);

typedef struct _RutGraph RutGraph;
#define RUT_GRAPH(X) ((RutGraph *)X)

extern RutType rut_graph_type;

RutGraph *
rut_graph_new (RutContext *ctx, ...);


RutTransform *
rut_transform_new (RutContext *ctx, ...) G_GNUC_NULL_TERMINATED;

void
rut_transform_translate (RutTransform *transform,
                         float x,
                         float y,
                         float z);

void
rut_transform_rotate (RutTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z);

void
rut_transform_quaternion_rotate (RutTransform *transform,
                                 const CoglQuaternion *quaternion);

void
rut_transform_scale (RutTransform *transform,
                     float x,
                     float y,
                     float z);

void
rut_transform_init_identity (RutTransform *transform);

const CoglMatrix *
rut_transform_get_matrix (RutTransform *transform);

typedef struct _RutNineSlice RutNineSlice;
#define RUT_NINE_SLICE(X) ((RutNineSlice *)X)

extern RutType rut_nine_slice_type;

RutNineSlice *
rut_nine_slice_new (RutContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height);

typedef struct _RutSimpleWidgetProps
{
  RutDisplayList display_list;
} RutSimpleWidgetProps;

typedef struct _RutSimpleWidgetVTable
{
  void (*set_camera) (RutObject *widget,
                      RutCamera *camera);
} RutSimpleWidgetVTable;

void
rut_simple_widget_graphable_parent_changed (RutObject *self,
                                            RutObject *old_parent,
                                            RutObject *new_parent);

/* Use for widget that can't have children */
void
rut_simple_widget_graphable_child_removed_warn (RutObject *self,
                                                RutObject *child);

void
rut_simple_widget_graphable_child_added_warn (RutObject *self,
                                              RutObject *child);

typedef struct _RutRectangle RutRectangle;
#define RUT_RECTANGLE(X) ((RutRectangle *)X)

extern RutType rut_rectangle_type;

RutRectangle *
rut_rectangle_new4f (RutContext *ctx,
                     float width,
                     float height,
                     float red,
                     float green,
                     float blue,
                     float alpha);

void
rut_rectangle_set_width (RutRectangle *rectangle, float width);

void
rut_rectangle_set_height (RutRectangle *rectangle, float height);

void
rut_rectangle_set_size (RutRectangle *rectangle,
                        float width,
                        float height);

void
rut_rectangle_get_size (RutRectangle *rectangle,
                        float *width,
                        float *height);

typedef struct _RutButton RutButton;
#define RUT_BUTTON(X) ((RutButton *)X)

extern RutType rut_button_type;

RutButton *
rut_button_new (RutContext *ctx,
                const char *label);

typedef void (*RutButtonClickCallback) (RutButton *button, void *user_data);

RutClosure *
rut_button_add_on_click_callback (RutButton *button,
                                  RutButtonClickCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb);

CoglTexture *
_rut_load_texture (RutContext *ctx, const char *filename, CoglError **error);

void
rut_color_init_from_uint32 (RutColor *color, uint32_t value);

#endif /* _RUT_CONTEXT_H_ */
