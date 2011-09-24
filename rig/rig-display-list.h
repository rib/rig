#ifndef _RIG_DISPLAY_LIST_H_
#define _RIG_DISPLAY_LIST_H_

#include <glib.h>

#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

typedef enum _RigCmdType
{
  RIG_CMD_TYPE_NOP,
  RIG_CMD_TYPE_TRANSFORM_PUSH,
  RIG_CMD_TYPE_TRANSFORM_POP,
  RIG_CMD_TYPE_TRANSFORM,
  RIG_CMD_TYPE_PRIMITIVE,
  RIG_CMD_TYPE_TEXT,
  RIG_CMD_TYPE_RECTANGLE
} RigCmdType;

typedef struct _RigCmd
{
  RigCmdType type;
} RigCmd;
#define RIG_CMD(x) ((RigCmd *)X)

typedef struct _RigTransformCmd
{
  RigCmd _parent;
  CoglMatrix matrix;
} RigTransformCmd;
#define RIG_TRANSFORM_CMD(X) ((RigTransformCmd *)X)

typedef struct _RigPrimitiveCmd
{
  RigCmd _parent;
  CoglPipeline *pipeline;
  CoglPrimitive *primitive;
} RigPrimitiveCmd;
#define RIG_PRIMITIVE_CMD(X) ((RigPrimitiveCmd *)X)

typedef struct _RigTextCmd
{
  RigCmd _parent;
  PangoLayout *layout;
  CoglColor color;
  int x;
  int y;
} RigTextCmd;
#define RIG_TEXT_CMD(X) ((RigTextCmd *)X)

typedef struct _RigRectangleCmd
{
  RigCmd _parent;
  CoglPipeline *pipeline;
  float width, height;
} RigRectangleCmd;
#define RIG_RECTANGLE_CMD(X) ((RigRectangleCmd *)X)

typedef struct _RigDisplayList
{
  /* PRIVATE */
  GList *head;
  GList *tail;
} RigDisplayList;

void
rig_display_list_unsplice (RigDisplayList *list);

void
rig_display_list_splice (GList *after, RigDisplayList *sub_list);

void
rig_display_list_append (RigDisplayList *list, void *data);

GList *
rig_display_list_insert_before (GList *sibling,
                                 void *data);

void
rig_display_list_delete_link (GList *link);

void
rig_display_list_init (RigDisplayList *list);

void
rig_display_list_destroy (RigDisplayList *list);

void
rig_display_list_paint (RigDisplayList *display_list,
                        CoglFramebuffer *fb);

#endif /* _RIG_DISPLAY_LIST_H_ */
