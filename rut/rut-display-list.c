#include "rut-display-list.h"

/* Given the @head and @tail for a sub-list contained within another
 * @list this unlinks the sub-list from @list and returns the new
 * head of @list if it has changed. */
static GList *
list_unsplice (GList *list, GList *head, GList *tail)
{
  GList *after;

  if (tail->next)
    {
      after = tail->next;
      after->prev = NULL;
      tail->next = NULL;
    }
  else
    after = NULL;

  if (head->prev)
    {
      head->prev->next = after;

      if (after)
        after->prev = head->prev;

      head->prev = NULL;
      return list;
    }
  else
    {
      g_return_val_if_fail (head == list, list);
      return after;
    }
}

/* Given the @head and @tail for a sub-list this links the sub-list
 * into @list after the @after link and returns the new list head
 * if it has changed.
 *
 * If @after is NULL the sub-list will be linked in-front of @list. This
 * would have the same result as using g_list_concat (@head, @list)
 * although in this case there is no need to traverse the first list to
 * find its @tail. If @after is NULL then @list can also be NULL and
 * in that case the function will return @head.
 *
 * Note: this function doesn't implicitly unsplice the sub-list before
 * splicing so it's the callers responsibility to unsplice the list
 * if necessary and this function will assert that head->prev == NULL
 * and tail->next == NULL.
 */
static GList *
list_splice (GList *list, GList *after, GList *head, GList *tail)
{
  g_return_val_if_fail (head->prev == NULL, NULL);
  g_return_val_if_fail (tail->next == NULL, NULL);

  if (after)
    {
      GList *remainder = after->next;
      after->next = head;
      head->prev = after;
      if (remainder)
        {
          tail->next = remainder;
          remainder->prev = tail;
        }
      return list;
    }
  else
    {
      if (list)
        {
          tail->next = list;
          list->prev = tail;
        }
      return head;
    }
}

#if 0
static void
_assert_list_is (GList *list, unsigned int length, unsigned int value)
{
  GList *l;

  g_assert (list->prev == NULL);
  g_assert_cmpuint (g_list_length (list), ==, length);

  for (l = g_list_last (list); l; l = l->prev)
    {
      g_assert_cmpuint (GPOINTER_TO_UINT (l->data), ==, value % 10);
      value /= 10;
    }
}

static void
_rut_test_list_splice (void)
{
  GList *l0 = NULL, *l1 = NULL, *l2 = NULL;
  GList *l0_tail, *l1_tail, *l2_tail;
  GList *list = NULL;

  l0 = g_list_append (l0, GUINT_TO_POINTER (1));
  l0 = g_list_append (l0, GUINT_TO_POINTER (2));
  l0 = g_list_append (l0, GUINT_TO_POINTER (3));
  l0_tail = g_list_last (l0);
  _assert_list_is (l0, 3, 123);

  l1 = g_list_append (l1, GUINT_TO_POINTER (4));
  l1 = g_list_append (l1, GUINT_TO_POINTER (5));
  l1 = g_list_append (l1, GUINT_TO_POINTER (6));
  l1_tail = g_list_last (l1);
  _assert_list_is (l1, 3, 456);

  l2 = g_list_append (l2, GUINT_TO_POINTER (7));
  l2 = g_list_append (l2, GUINT_TO_POINTER (8));
  l2_tail = g_list_last (l2);
  _assert_list_is (l2, 2, 78);

  list = l0;

  /* splice on the end */
  list = list_splice (list, l0_tail, l1, l1_tail);
  _assert_list_is (list, 6, 123456);

  /* splice in the middle */
  list = list_splice (list, l0_tail, l2, l2_tail);
  _assert_list_is (list, 8, 12378456);

  /* unsplice from the middle */
  list = list_unsplice (list, l2, l2_tail);
  _assert_list_is (list, 6, 123456);
  _assert_list_is (l2, 2, 78);

  /* unsplice the end */
  list = list_unsplice (list, l1, l1_tail);
  _assert_list_is (list, 3, 123);
  _assert_list_is (l1, 3, 456);

  /* splice at the beginning */
  list = list_splice (list, NULL, l2, l2_tail);
  _assert_list_is (list, 5, 78123);

  /* unsplice from the beginning */
  list = list_unsplice (list, l2, l2_tail);
  _assert_list_is (list, 3, 123);
  _assert_list_is (l2, 2, 78);
}
#endif

/* A display-list is a list of sequential drawing commands including
 * transformation commands and primitive drawing commands.
 *
 * A display list is currently represented as a linked list of GList nodes
 * although the api we want is a cross between the g_list_ api and the
 * g_queue_ api so we have a wrapper api instead to make display list
 * manipulation less confusing and error prone.
 *
 * Two common manipulations to do on display lists are "splicing" and
 * "unsplicing" which means to insert a linked sub-list into a certain
 * position within another display list or to unlink a linked sub-list
 * from a larger display list respectively.
 *
 * A notable requirement for these operations though is that you don't
 * need access to the head pointer for the larger list being spliced
 * into or being unspliced from. This is unlike the above list_splice
 * and list_unsplice functions. Below you'll see that we essentially
 * pass in dummy head arguments to these functions and assert that
 * modification of the head wasn't required.
 *   Note: to make this work it requires that the outermost display
 *   list owned by a RutCamera must add at least one link into the
 *   display list before allowing any splice or unsplice operations.
 *
 * A complete command sequence is created from a scene-graph by
 * traversing in a depth first order and asking the children of each
 * node to splice their commands into a given position of the
 * display list. Once a node has spliced in its own list of commands
 * then it associates each child with a position within that list
 * and recursively asks the child to splice its commands into that
 * position.
 */

void
rut_display_list_unsplice (RutDisplayList *list)
{
  if (list->head->prev)
    g_assert (list_unsplice ((void *)0xDEADBEEF, list->head, list->tail)
              == (GList *)0xDEADBEEF);
}

void
rut_display_list_splice (GList *after, RutDisplayList *sub_list)
{
  rut_display_list_unsplice (sub_list);
  g_assert (list_splice (after, after, sub_list->head, sub_list->tail)
            == after);
}

void
rut_display_list_append (RutDisplayList *list, void *data)
{
  GList *link = g_list_alloc ();
  link->data = data;
  link->prev = list->tail;
  link->next = NULL;

  if (list->tail)
    list->tail->next = link;
  else
    {
      g_assert (list->head == NULL);
      list->head = link;
    }
  list->tail = link;
}

/* Note: unlike the similar GList api this returns the newly inserted
 * link not the head of the list. */
GList *
rut_display_list_insert_before (GList *sibling,
                                 void *data)
{
  GList *link = g_list_alloc ();
  link->data = data;
  link->next = sibling;
  link->prev = sibling->prev;
  link->prev->next = link;
  sibling->prev = link;

  return link;
}

void
rut_display_list_delete_link (GList *link)
{
  link->prev->next = link->next;
  link->next->prev = link->prev;
  g_list_free_1 (link);
}

void
rut_display_list_init (RutDisplayList *list)
{
  list->head = NULL;
  list->tail = NULL;
}

void
rut_display_list_destroy (RutDisplayList *list)
{
  rut_display_list_unsplice (list);
  g_list_free (list->head);
  list->head = NULL;
  list->tail = NULL;
}

void
rut_display_list_paint (RutDisplayList *display_list,
                        CoglFramebuffer *fb)
{
  GList *l;

  for (l = display_list->head; l; l = l->next)
    {
      RutCmd *cmd = l->data;

      if (!cmd)
        continue;

      switch (cmd->type)
        {
        case RUT_CMD_TYPE_NOP:
          continue;
        case RUT_CMD_TYPE_TRANSFORM_PUSH:
          cogl_framebuffer_push_matrix (fb);
          break;
        case RUT_CMD_TYPE_TRANSFORM_POP:
          cogl_framebuffer_pop_matrix (fb);
          break;
        case RUT_CMD_TYPE_TRANSFORM:
          {
            RutTransformCmd *transform_cmd = RUT_TRANSFORM_CMD (cmd);
            cogl_framebuffer_transform (fb,
                                        &transform_cmd->matrix);
            break;
          }
        case RUT_CMD_TYPE_PRIMITIVE:
          {
            RutPrimitiveCmd *prim_cmd = RUT_PRIMITIVE_CMD (cmd);
            cogl_framebuffer_draw_primitive (fb,
                                             prim_cmd->pipeline,
                                             prim_cmd->primitive);
            break;
          }
        case RUT_CMD_TYPE_TEXT:
          {
            RutTextCmd *text_cmd = RUT_TEXT_CMD (cmd);
            cogl_pango_show_layout (fb,
                                    text_cmd->layout,
                                    text_cmd->x, text_cmd->y,
                                    &text_cmd->color);
            break;
          }
        case RUT_CMD_TYPE_RECTANGLE:
          {
            RutRectangleCmd *rect_cmd = RUT_RECTANGLE_CMD (cmd);
            cogl_framebuffer_draw_rectangle (fb,
                                             rect_cmd->pipeline,
                                             0,
                                             0,
                                             rect_cmd->width,
                                             rect_cmd->height);
            break;
          }
        }
    }
}


