#ifndef _RUT_DISPLAY_LIST_H_
#define _RUT_DISPLAY_LIST_H_

#include <glib.h>

#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

typedef enum _RutCmdType
{
  RUT_CMD_TYPE_NOP,
  RUT_CMD_TYPE_TRANSFORM_PUSH,
  RUT_CMD_TYPE_TRANSFORM_POP,
  RUT_CMD_TYPE_TRANSFORM,
  RUT_CMD_TYPE_PRIMITIVE,
  RUT_CMD_TYPE_TEXT,
  RUT_CMD_TYPE_RECTANGLE
} RutCmdType;

typedef struct _RutCmd
{
  RutCmdType type;
} RutCmd;
#define RUT_CMD(x) ((RutCmd *)X)

typedef struct _RutTransformCmd
{
  RutCmd _parent;
  CoglMatrix matrix;
} RutTransformCmd;
#define RUT_TRANSFORM_CMD(X) ((RutTransformCmd *)X)

typedef struct _RutPrimitiveCmd
{
  RutCmd _parent;
  CoglPipeline *pipeline;
  CoglPrimitive *primitive;
} RutPrimitiveCmd;
#define RUT_PRIMITIVE_CMD(X) ((RutPrimitiveCmd *)X)

typedef struct _RutTextCmd
{
  RutCmd _parent;
  PangoLayout *layout;
  CoglColor color;
  int x;
  int y;
} RutTextCmd;
#define RUT_TEXT_CMD(X) ((RutTextCmd *)X)

typedef struct _RutRectangleCmd
{
  RutCmd _parent;
  CoglPipeline *pipeline;
  float width, height;
} RutRectangleCmd;
#define RUT_RECTANGLE_CMD(X) ((RutRectangleCmd *)X)

typedef struct _RutDisplayList
{
  /* PRIVATE */
  GList *head;
  GList *tail;
} RutDisplayList;

void
rut_display_list_unsplice (RutDisplayList *list);

void
rut_display_list_splice (GList *after, RutDisplayList *sub_list);

void
rut_display_list_append (RutDisplayList *list, void *data);

GList *
rut_display_list_insert_before (GList *sibling,
                                 void *data);

void
rut_display_list_delete_link (GList *link);

void
rut_display_list_init (RutDisplayList *list);

void
rut_display_list_destroy (RutDisplayList *list);

void
rut_display_list_paint (RutDisplayList *display_list,
                        CoglFramebuffer *fb);

#endif /* _RUT_DISPLAY_LIST_H_ */
