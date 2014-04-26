/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2009,2010,2012 Intel Corporation.
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

#include <config.h>

#include <cogl/cogl.h>

#include "rut-context.h"
#include "rut-object.h"
#include "rut-magazine.h"
#include "rut-matrix-stack.h"

static RutMagazine *rut_matrix_stack_magazine;
static RutMagazine *rut_matrix_stack_matrices_magazine;

/* XXX: Note: this leaves entry->parent uninitialized! */
static RutMatrixEntry *
_rut_matrix_entry_new (CoglMatrixOp operation)
{
  RutMatrixEntry *entry =
    rut_magazine_chunk_alloc (rut_matrix_stack_magazine);

  entry->ref_count = 1;
  entry->op = operation;

#ifdef RUT_DEBUG_ENABLED
  entry->composite_gets = 0;
#endif

  return entry;
}

static void *
_rut_matrix_stack_push_entry (RutMatrixStack *stack,
                               RutMatrixEntry *entry)
{
  /* NB: The initial reference of the entry is transferred to the
   * stack here.
   *
   * The stack only maintains a reference to the top of the stack (the
   * last entry pushed) and each entry in-turn maintains a reference
   * to its parent.
   *
   * We don't need to take a reference to the parent from the entry
   * here because the we are stealing the reference that was held by
   * the stack while that parent was previously the top of the stack.
   */
  entry->parent = stack->last_entry;
  stack->last_entry = entry;

  return entry;
}

static void *
_rut_matrix_stack_push_operation (RutMatrixStack *stack,
                                   CoglMatrixOp operation)
{
  RutMatrixEntry *entry = _rut_matrix_entry_new (operation);

  _rut_matrix_stack_push_entry (stack, entry);

  return entry;
}

static void *
_rut_matrix_stack_push_replacement_entry (RutMatrixStack *stack,
                                          CoglMatrixOp operation)
{
  RutMatrixEntry *old_top = stack->last_entry;
  RutMatrixEntry *new_top;

  /* This would only be called for operations that completely replace
   * the matrix. In that case we don't need to keep a reference to
   * anything up to the last save entry. This optimisation could be
   * important for applications that aren't using the stack but
   * instead just perform their own matrix manipulations and load a
   * new stack every frame. If this optimisation isn't done then the
   * stack would just grow endlessly. See the comments
   * rut_matrix_stack_pop for a description of how popping works. */
  for (new_top = old_top;
       new_top->op != RUT_MATRIX_OP_SAVE && new_top->parent;
       new_top = new_top->parent)
    ;

  rut_matrix_entry_ref (new_top);
  rut_matrix_entry_unref (old_top);
  stack->last_entry = new_top;

  return _rut_matrix_stack_push_operation (stack, operation);
}

void
_rut_matrix_entry_identity_init (RutMatrixEntry *entry)
{
  entry->ref_count = 1;
  entry->op = RUT_MATRIX_OP_LOAD_IDENTITY;
  entry->parent = NULL;
#ifdef RUT_DEBUG_ENABLED
  entry->composite_gets = 0;
#endif
}

void
rut_matrix_stack_load_identity (RutMatrixStack *stack)
{
  _rut_matrix_stack_push_replacement_entry (stack,
                                             RUT_MATRIX_OP_LOAD_IDENTITY);
}

void
rut_matrix_stack_translate (RutMatrixStack *stack,
                              float x,
                              float y,
                              float z)
{
  RutMatrixEntryTranslate *entry;

  entry = _rut_matrix_stack_push_operation (stack, RUT_MATRIX_OP_TRANSLATE);

  entry->x = x;
  entry->y = y;
  entry->z = z;
}

void
rut_matrix_stack_rotate (RutMatrixStack *stack,
                         float angle,
                         float x,
                         float y,
                         float z)
{
  RutMatrixEntryRotate *entry;

  entry = _rut_matrix_stack_push_operation (stack, RUT_MATRIX_OP_ROTATE);

  entry->angle = angle;
  entry->x = x;
  entry->y = y;
  entry->z = z;
}

void
rut_matrix_stack_rotate_quaternion (RutMatrixStack *stack,
                                     const CoglQuaternion *quaternion)
{
  RutMatrixEntryRotateQuaternion *entry;

  entry = _rut_matrix_stack_push_operation (stack,
                                             RUT_MATRIX_OP_ROTATE_QUATERNION);

  entry->values[0] = quaternion->w;
  entry->values[1] = quaternion->x;
  entry->values[2] = quaternion->y;
  entry->values[3] = quaternion->z;
}

void
rut_matrix_stack_rotate_euler (RutMatrixStack *stack,
                                 const CoglEuler *euler)
{
  RutMatrixEntryRotateEuler *entry;

  entry = _rut_matrix_stack_push_operation (stack,
                                             RUT_MATRIX_OP_ROTATE_EULER);

  entry->heading = euler->heading;
  entry->pitch = euler->pitch;
  entry->roll = euler->roll;
}

void
rut_matrix_stack_scale (RutMatrixStack *stack,
                          float x,
                          float y,
                          float z)
{
  RutMatrixEntryScale *entry;

  entry = _rut_matrix_stack_push_operation (stack, RUT_MATRIX_OP_SCALE);

  entry->x = x;
  entry->y = y;
  entry->z = z;
}

void
rut_matrix_stack_multiply (RutMatrixStack *stack,
                            const CoglMatrix *matrix)
{
  RutMatrixEntryMultiply *entry;

  entry = _rut_matrix_stack_push_operation (stack, RUT_MATRIX_OP_MULTIPLY);

  entry->matrix =
    rut_magazine_chunk_alloc (rut_matrix_stack_matrices_magazine);

  cogl_matrix_init_from_array (entry->matrix, (float *)matrix);
}

void
rut_matrix_stack_set (RutMatrixStack *stack,
                       const CoglMatrix *matrix)
{
  RutMatrixEntryLoad *entry;

  entry =
    _rut_matrix_stack_push_replacement_entry (stack,
                                              RUT_MATRIX_OP_LOAD);

  entry->matrix =
    rut_magazine_chunk_alloc (rut_matrix_stack_matrices_magazine);

  cogl_matrix_init_from_array (entry->matrix, (float *)matrix);
}

void
rut_matrix_stack_frustum (RutMatrixStack *stack,
                          float left,
                          float right,
                          float bottom,
                          float top,
                          float z_near,
                          float z_far)
{
  RutMatrixEntryLoad *entry;

  entry =
    _rut_matrix_stack_push_replacement_entry (stack,
                                               RUT_MATRIX_OP_LOAD);

  entry->matrix =
    rut_magazine_chunk_alloc (rut_matrix_stack_matrices_magazine);

  cogl_matrix_init_identity (entry->matrix);
  cogl_matrix_frustum (entry->matrix,
                       left, right, bottom, top,
                       z_near, z_far);
}

void
rut_matrix_stack_perspective (RutMatrixStack *stack,
                                float fov_y,
                                float aspect,
                                float z_near,
                                float z_far)
{
  RutMatrixEntryLoad *entry;

  entry =
    _rut_matrix_stack_push_replacement_entry (stack,
                                              RUT_MATRIX_OP_LOAD);

  entry->matrix =
    rut_magazine_chunk_alloc (rut_matrix_stack_matrices_magazine);

  cogl_matrix_init_identity (entry->matrix);
  cogl_matrix_perspective (entry->matrix,
                          fov_y, aspect, z_near, z_far);
}

void
rut_matrix_stack_orthographic (RutMatrixStack *stack,
                               float x_1,
                               float y_1,
                               float x_2,
                               float y_2,
                               float near,
                               float far)
{
  RutMatrixEntryLoad *entry;

  entry =
    _rut_matrix_stack_push_replacement_entry (stack,
                                               RUT_MATRIX_OP_LOAD);

  entry->matrix =
    rut_magazine_chunk_alloc (rut_matrix_stack_matrices_magazine);

  cogl_matrix_init_identity (entry->matrix);
  cogl_matrix_orthographic (entry->matrix,
                           x_1, y_1, x_2, y_2, near, far);
}

void
rut_matrix_stack_push (RutMatrixStack *stack)
{
  RutMatrixEntrySave *entry;

  entry = _rut_matrix_stack_push_operation (stack, RUT_MATRIX_OP_SAVE);

  entry->cache_valid = FALSE;
}

RutMatrixEntry *
rut_matrix_entry_ref (RutMatrixEntry *entry)
{
  /* A NULL pointer is considered a valid stack so we should accept
     that as an argument */
  if (entry)
    entry->ref_count++;

  return entry;
}

void
rut_matrix_entry_unref (RutMatrixEntry *entry)
{
  RutMatrixEntry *parent;

  for (; entry && --entry->ref_count <= 0; entry = parent)
    {
      parent = entry->parent;

      switch (entry->op)
        {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
        case RUT_MATRIX_OP_TRANSLATE:
        case RUT_MATRIX_OP_ROTATE:
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
        case RUT_MATRIX_OP_ROTATE_EULER:
        case RUT_MATRIX_OP_SCALE:
          break;
        case RUT_MATRIX_OP_MULTIPLY:
          {
            RutMatrixEntryMultiply *multiply =
              (RutMatrixEntryMultiply *)entry;
            rut_magazine_chunk_free (rut_matrix_stack_matrices_magazine,
                                     multiply->matrix);
            break;
          }
        case RUT_MATRIX_OP_LOAD:
          {
            RutMatrixEntryLoad *load = (RutMatrixEntryLoad *)entry;
            rut_magazine_chunk_free (rut_matrix_stack_matrices_magazine,
                                     load->matrix);
            break;
          }
        case RUT_MATRIX_OP_SAVE:
          {
            RutMatrixEntrySave *save = (RutMatrixEntrySave *)entry;
            if (save->cache_valid)
              rut_magazine_chunk_free (rut_matrix_stack_matrices_magazine,
                                       save->cache);
            break;
          }
        }

      rut_magazine_chunk_free (rut_matrix_stack_magazine, entry);
    }
}

void
rut_matrix_stack_pop (RutMatrixStack *stack)
{
  RutMatrixEntry *old_top;
  RutMatrixEntry *new_top;

  c_return_if_fail (stack != NULL);

  old_top = stack->last_entry;
  c_return_if_fail (old_top != NULL);

  /* To pop we are moving the top of the stack to the old top's parent
   * node. The stack always needs to have a reference to the top entry
   * so we must take a reference to the new top. The stack would have
   * previously had a reference to the old top so we need to decrease
   * the ref count on that. We need to ref the new head first in case
   * this stack was the only thing referencing the old top. In that
   * case the call to rut_matrix_entry_unref will unref the parent.
   */

  /* Find the last save operation and remove it */

  /* XXX: it would be an error to pop to the very beginning of the
   * stack so we don't need to check for NULL pointer dereferencing. */
  for (new_top = old_top;
       new_top->op != RUT_MATRIX_OP_SAVE;
       new_top = new_top->parent)
    ;

  new_top = new_top->parent;
  rut_matrix_entry_ref (new_top);

  rut_matrix_entry_unref (old_top);

  stack->last_entry = new_top;
}

bool
rut_matrix_stack_get_inverse (RutMatrixStack *stack,
                              CoglMatrix *inverse)
{
  CoglMatrix matrix;
  CoglMatrix *internal = rut_matrix_stack_get (stack, &matrix);

  if (internal)
    return cogl_matrix_get_inverse (internal, inverse);
  else
    return cogl_matrix_get_inverse (&matrix, inverse);
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
CoglMatrix *
rut_matrix_entry_get (RutMatrixEntry *entry,
                      CoglMatrix *matrix)
{
  int depth;
  RutMatrixEntry *current;
  RutMatrixEntry **children;
  int i;

  for (depth = 0, current = entry;
       current;
       current = current->parent, depth++)
    {
      switch (current->op)
        {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
          cogl_matrix_init_identity (matrix);
          goto initialized;
        case RUT_MATRIX_OP_LOAD:
          {
            RutMatrixEntryLoad *load = (RutMatrixEntryLoad *)current;
            *matrix = *load->matrix;
            goto initialized;
          }
        case RUT_MATRIX_OP_SAVE:
          {
            RutMatrixEntrySave *save = (RutMatrixEntrySave *)current;
            if (!save->cache_valid)
              {
                RutMagazine *matrices_magazine =
                  rut_matrix_stack_matrices_magazine;
                save->cache = rut_magazine_chunk_alloc (matrices_magazine);
                rut_matrix_entry_get (current->parent, save->cache);
                save->cache_valid = TRUE;
              }
            *matrix = *save->cache;
            goto initialized;
          }
        default:
          continue;
        }
    }

initialized:

  if (depth == 0)
    {
      switch (entry->op)
        {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
        case RUT_MATRIX_OP_TRANSLATE:
        case RUT_MATRIX_OP_ROTATE:
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
        case RUT_MATRIX_OP_ROTATE_EULER:
        case RUT_MATRIX_OP_SCALE:
        case RUT_MATRIX_OP_MULTIPLY:
          return NULL;

        case RUT_MATRIX_OP_LOAD:
          {
            RutMatrixEntryLoad *load = (RutMatrixEntryLoad *)entry;
            return load->matrix;
          }
        case RUT_MATRIX_OP_SAVE:
          {
            RutMatrixEntrySave *save = (RutMatrixEntrySave *)entry;
            return save->cache;
          }
        }
      c_warn_if_reached ();
      return NULL;
    }

#ifdef RUT_ENABLE_DEBUG
  if (!current)
    {
      c_warning ("Inconsistent matrix stack");
      return NULL;
    }

  entry->composite_gets++;
#endif

  children = g_alloca (sizeof (RutMatrixEntry) * depth);

  /* We need walk the list of entries from the init/load/save entry
   * back towards the leaf node but the nodes don't link to their
   * children so we need to re-walk them here to add to a separate
   * array. */
  for (i = depth - 1, current = entry;
       i >= 0 && current;
       i--, current = current->parent)
    {
      children[i] = current;
    }

#ifdef RUT_ENABLE_DEBUG
  if (RUT_DEBUG_ENABLED (RUT_DEBUG_PERFORMANCE) &&
      entry->composite_gets >= 2)
    {
      RUT_NOTE (PERFORMANCE,
                 "Re-composing a matrix stack entry multiple times");
    }
#endif

  for (i = 0; i < depth; i++)
    {
      switch (children[i]->op)
        {
        case RUT_MATRIX_OP_TRANSLATE:
            {
              RutMatrixEntryTranslate *translate =
                (RutMatrixEntryTranslate *)children[i];
              cogl_matrix_translate (matrix,
                                    translate->x,
                                    translate->y,
                                    translate->z);
              continue;
            }
        case RUT_MATRIX_OP_ROTATE:
            {
              RutMatrixEntryRotate *rotate=
                (RutMatrixEntryRotate *)children[i];
              cogl_matrix_rotate (matrix,
                                 rotate->angle,
                                 rotate->x,
                                 rotate->y,
                                 rotate->z);
              continue;
            }
        case RUT_MATRIX_OP_ROTATE_EULER:
            {
              RutMatrixEntryRotateEuler *rotate =
                (RutMatrixEntryRotateEuler *)children[i];
              CoglEuler euler;
              cogl_euler_init (&euler,
                               rotate->heading,
                               rotate->pitch,
                               rotate->roll);
              cogl_matrix_rotate_euler (matrix,
                                       &euler);
              continue;
            }
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
            {
              RutMatrixEntryRotateQuaternion *rotate =
                (RutMatrixEntryRotateQuaternion *)children[i];
              CoglQuaternion quaternion;
              cogl_quaternion_init_from_array (&quaternion, rotate->values);
              cogl_matrix_rotate_quaternion (matrix, &quaternion);
              continue;
            }
        case RUT_MATRIX_OP_SCALE:
            {
              RutMatrixEntryScale *scale =
                (RutMatrixEntryScale *)children[i];
              cogl_matrix_scale (matrix,
                                scale->x,
                                scale->y,
                                scale->z);
              continue;
            }
        case RUT_MATRIX_OP_MULTIPLY:
            {
              RutMatrixEntryMultiply *multiply =
                (RutMatrixEntryMultiply *)children[i];
              cogl_matrix_multiply (matrix, matrix, multiply->matrix);
              continue;
            }

        case RUT_MATRIX_OP_LOAD_IDENTITY:
        case RUT_MATRIX_OP_LOAD:
        case RUT_MATRIX_OP_SAVE:
          c_warn_if_reached ();
          continue;
        }
    }

  return NULL;
}

RutMatrixEntry *
rut_matrix_stack_get_entry (RutMatrixStack *stack)
{
  return stack->last_entry;
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
CoglMatrix *
rut_matrix_stack_get (RutMatrixStack *stack,
                      CoglMatrix *matrix)
{
  return rut_matrix_entry_get (stack->last_entry, matrix);
}

static void
_rut_matrix_stack_free (void *object)
{
  RutMatrixStack *stack = object;

  rut_matrix_entry_unref (stack->last_entry);

  rut_object_free (RutMatrixStack, stack);
}

RutType rut_matrix_stack_type;

static void
_rut_matrix_stack_init_type (void)
{
  rut_type_init (&rut_matrix_stack_type,
                 "RutMatrixStack", _rut_matrix_stack_free);
}

RutMatrixStack *
rut_matrix_stack_new (RutContext *ctx)
{
  RutMatrixStack *stack =
    rut_object_alloc0 (RutMatrixStack,
                       &rut_matrix_stack_type,
                       _rut_matrix_stack_init_type);

  if (G_UNLIKELY (rut_matrix_stack_magazine == NULL))
    {
      rut_matrix_stack_magazine =
        rut_magazine_new (sizeof (RutMatrixEntryFull), 20);
      rut_matrix_stack_matrices_magazine =
        rut_magazine_new (sizeof (CoglMatrix), 20);
    }

  stack->context = ctx;
  stack->last_entry = NULL;

  rut_matrix_entry_ref (&ctx->identity_entry);
  _rut_matrix_stack_push_entry (stack, &ctx->identity_entry);

  return stack;
}

static RutMatrixEntry *
_rut_matrix_entry_skip_saves (RutMatrixEntry *entry)
{
  /* We currently assume that every stack starts with an
   * _OP_LOAD_IDENTITY so we don't need to worry about
   * NULL pointer dereferencing here. */
  while (entry->op == RUT_MATRIX_OP_SAVE)
    entry = entry->parent;

  return entry;
}

bool
rut_matrix_entry_calculate_translation (RutMatrixEntry *entry0,
                                        RutMatrixEntry *entry1,
                                        float *x,
                                        float *y,
                                        float *z)
{
  GSList *head0 = NULL;
  GSList *head1 = NULL;
  RutMatrixEntry *node0;
  RutMatrixEntry *node1;
  int len0 = 0;
  int len1 = 0;
  int count;
  GSList *common_ancestor0;
  GSList *common_ancestor1;

  /* Algorithm:
   *
   * 1) Ignoring _OP_SAVE entries walk the ancestors of each entry to
   *    the root node or any non-translation node, adding a pointer to
   *    each ancestor node to two linked lists.
   *
   * 2) Compare the lists to find the nodes where they start to
   *    differ marking the common_ancestor node for each list.
   *
   * 3) For the list corresponding to entry0, start iterating after
   *    the common ancestor applying the negative of all translations
   *    to x, y and z.
   *
   * 4) For the list corresponding to entry1, start iterating after
   *    the common ancestor applying the positive of all translations
   *    to x, y and z.
   *
   * If we come across any non-translation operations during 3) or 4)
   * then bail out returning FALSE.
   */

  for (node0 = entry0; node0; node0 = node0->parent)
    {
      GSList *link;

      if (node0->op == RUT_MATRIX_OP_SAVE)
        continue;

      link = alloca (sizeof (GSList));
      link->next = head0;
      link->data = node0;
      head0 = link;
      len0++;

      if (node0->op != RUT_MATRIX_OP_TRANSLATE)
        break;
    }
  for (node1 = entry1; node1; node1 = node1->parent)
    {
      GSList *link;

      if (node1->op == RUT_MATRIX_OP_SAVE)
        continue;

      link = alloca (sizeof (GSList));
      link->next = head1;
      link->data = node1;
      head1 = link;
      len1++;

      if (node1->op != RUT_MATRIX_OP_TRANSLATE)
        break;
    }

  if (head0->data != head1->data)
    return FALSE;

  common_ancestor0 = head0;
  common_ancestor1 = head1;
  head0 = head0->next;
  head1 = head1->next;
  count = MIN (len0, len1) - 1;
  while (count--)
    {
      if (head0->data != head1->data)
        break;
      common_ancestor0 = head0;
      common_ancestor1 = head1;
      head0 = head0->next;
      head1 = head1->next;
    }

  *x = 0;
  *y = 0;
  *z = 0;

  for (head0 = common_ancestor0->next; head0; head0 = head0->next)
    {
      RutMatrixEntryTranslate *translate;

      node0 = head0->data;

      if (node0->op != RUT_MATRIX_OP_TRANSLATE)
        return FALSE;

      translate = (RutMatrixEntryTranslate *)node0;

      *x = *x - translate->x;
      *y = *y - translate->y;
      *z = *z - translate->z;
    }
  for (head1 = common_ancestor1->next; head1; head1 = head1->next)
    {
      RutMatrixEntryTranslate *translate;

      node1 = head1->data;

      if (node1->op != RUT_MATRIX_OP_TRANSLATE)
        return FALSE;

      translate = (RutMatrixEntryTranslate *)node1;

      *x = *x + translate->x;
      *y = *y + translate->y;
      *z = *z + translate->z;
    }

  return TRUE;
}

bool
rut_matrix_entry_is_identity (RutMatrixEntry *entry)
{
  return entry ? entry->op == RUT_MATRIX_OP_LOAD_IDENTITY : FALSE;
}

bool
rut_matrix_entry_equal (RutMatrixEntry *entry0,
                         RutMatrixEntry *entry1)
{
  for (;
       entry0 && entry1;
       entry0 = entry0->parent, entry1 = entry1->parent)
    {
      entry0 = _rut_matrix_entry_skip_saves (entry0);
      entry1 = _rut_matrix_entry_skip_saves (entry1);

      if (entry0 == entry1)
        return TRUE;

      if (entry0->op != entry1->op)
        return FALSE;

      switch (entry0->op)
        {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
          return TRUE;
        case RUT_MATRIX_OP_TRANSLATE:
          {
            RutMatrixEntryTranslate *translate0 =
              (RutMatrixEntryTranslate *)entry0;
            RutMatrixEntryTranslate *translate1 =
              (RutMatrixEntryTranslate *)entry1;
            /* We could perhaps use an epsilon to compare here?
             * I expect the false negatives are probaly never going to
             * be a problem and this is a bit cheaper. */
            if (translate0->x != translate1->x ||
                translate0->y != translate1->y ||
                translate0->z != translate1->z)
              return FALSE;
          }
          break;
        case RUT_MATRIX_OP_ROTATE:
          {
            RutMatrixEntryRotate *rotate0 =
              (RutMatrixEntryRotate *)entry0;
            RutMatrixEntryRotate *rotate1 =
              (RutMatrixEntryRotate *)entry1;
            if (rotate0->angle != rotate1->angle ||
                rotate0->x != rotate1->x ||
                rotate0->y != rotate1->y ||
                rotate0->z != rotate1->z)
              return FALSE;
          }
          break;
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
          {
            RutMatrixEntryRotateQuaternion *rotate0 =
              (RutMatrixEntryRotateQuaternion *)entry0;
            RutMatrixEntryRotateQuaternion *rotate1 =
              (RutMatrixEntryRotateQuaternion *)entry1;
            int i;
            for (i = 0; i < 4; i++)
              if (rotate0->values[i] != rotate1->values[i])
                return FALSE;
          }
          break;
        case RUT_MATRIX_OP_ROTATE_EULER:
          {
            RutMatrixEntryRotateEuler *rotate0 =
              (RutMatrixEntryRotateEuler *)entry0;
            RutMatrixEntryRotateEuler *rotate1 =
              (RutMatrixEntryRotateEuler *)entry1;

            if (rotate0->heading != rotate1->heading ||
                rotate0->pitch != rotate1->pitch ||
                rotate0->roll != rotate1->roll)
              return FALSE;
          }
          break;
        case RUT_MATRIX_OP_SCALE:
          {
            RutMatrixEntryScale *scale0 = (RutMatrixEntryScale *)entry0;
            RutMatrixEntryScale *scale1 = (RutMatrixEntryScale *)entry1;
            if (scale0->x != scale1->x ||
                scale0->y != scale1->y ||
                scale0->z != scale1->z)
              return FALSE;
          }
          break;
        case RUT_MATRIX_OP_MULTIPLY:
          {
            RutMatrixEntryMultiply *mult0 = (RutMatrixEntryMultiply *)entry0;
            RutMatrixEntryMultiply *mult1 = (RutMatrixEntryMultiply *)entry1;
            if (!cogl_matrix_equal (mult0->matrix, mult1->matrix))
              return FALSE;
          }
          break;
        case RUT_MATRIX_OP_LOAD:
          {
            RutMatrixEntryLoad *load0 = (RutMatrixEntryLoad *)entry0;
            RutMatrixEntryLoad *load1 = (RutMatrixEntryLoad *)entry1;
            /* There's no need to check any further since an
             * _OP_LOAD makes all the ancestors redundant as far as
             * the final matrix value is concerned. */
            return cogl_matrix_equal (load0->matrix, load1->matrix);
          }
        case RUT_MATRIX_OP_SAVE:
          /* We skip over saves above so we shouldn't see save entries */
          c_warn_if_reached ();
        }
    }

  return FALSE;
}

void
rut_debug_matrix_entry_print (RutMatrixEntry *entry)
{
  int depth;
  RutMatrixEntry *e;
  RutMatrixEntry **children;
  int i;

  for (depth = 0, e = entry; e; e = e->parent)
    depth++;

  children = g_alloca (sizeof (RutMatrixEntry) * depth);

  for (i = depth - 1, e = entry;
       i >= 0 && e;
       i--, e = e->parent)
    {
      children[i] = e;
    }

  c_print ("MatrixEntry %p =\n", entry);

  for (i = 0; i < depth; i++)
    {
      entry = children[i];

      switch (entry->op)
        {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
          c_print ("  LOAD IDENTITY\n");
          continue;
        case RUT_MATRIX_OP_TRANSLATE:
          {
            RutMatrixEntryTranslate *translate =
              (RutMatrixEntryTranslate *)entry;
            c_print ("  TRANSLATE X=%f Y=%f Z=%f\n",
                     translate->x,
                     translate->y,
                     translate->z);
            continue;
          }
        case RUT_MATRIX_OP_ROTATE:
          {
            RutMatrixEntryRotate *rotate =
              (RutMatrixEntryRotate *)entry;
            c_print ("  ROTATE ANGLE=%f X=%f Y=%f Z=%f\n",
                     rotate->angle,
                     rotate->x,
                     rotate->y,
                     rotate->z);
            continue;
          }
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
          {
            RutMatrixEntryRotateQuaternion *rotate =
              (RutMatrixEntryRotateQuaternion *)entry;
            c_print ("  ROTATE QUATERNION w=%f x=%f y=%f z=%f\n",
                     rotate->values[0],
                     rotate->values[1],
                     rotate->values[2],
                     rotate->values[3]);
            continue;
          }
        case RUT_MATRIX_OP_ROTATE_EULER:
          {
            RutMatrixEntryRotateEuler *rotate =
              (RutMatrixEntryRotateEuler *)entry;
            c_print ("  ROTATE EULER heading=%f pitch=%f roll=%f\n",
                     rotate->heading,
                     rotate->pitch,
                     rotate->roll);
            continue;
          }
        case RUT_MATRIX_OP_SCALE:
          {
            RutMatrixEntryScale *scale = (RutMatrixEntryScale *)entry;
            c_print ("  SCALE X=%f Y=%f Z=%f\n",
                     scale->x,
                     scale->y,
                     scale->z);
            continue;
          }
        case RUT_MATRIX_OP_MULTIPLY:
          {
            RutMatrixEntryMultiply *mult = (RutMatrixEntryMultiply *)entry;
            c_print ("  MULT:\n");
            cogl_debug_matrix_print (mult->matrix);
            //_cogl_matrix_prefix_print ("    ", mult->matrix);
            continue;
          }
        case RUT_MATRIX_OP_LOAD:
          {
            RutMatrixEntryLoad *load = (RutMatrixEntryLoad *)entry;
            c_print ("  LOAD:\n");
            cogl_debug_matrix_print (load->matrix);
            //_cogl_matrix_prefix_print ("    ", load->matrix);
            continue;
          }
        case RUT_MATRIX_OP_SAVE:
          c_print ("  SAVE\n");
        }
    }
}

void
_rut_matrix_entry_cache_init (RutMatrixEntryCache *cache)
{
  cache->entry = NULL;
  cache->flushed_identity = FALSE;
  cache->flipped = FALSE;
}

/* NB: This function can report false negatives since it never does a
 * deep comparison of the stack matrices. */
bool
_rut_matrix_entry_cache_maybe_update (RutMatrixEntryCache *cache,
                                      RutMatrixEntry *entry,
                                      bool flip)
{
  bool is_identity;
  bool updated = FALSE;

  if (cache->flipped != flip)
    {
      cache->flipped = flip;
      updated = TRUE;
    }

  is_identity = (entry->op == RUT_MATRIX_OP_LOAD_IDENTITY);
  if (cache->flushed_identity != is_identity)
    {
      cache->flushed_identity = is_identity;
      updated = TRUE;
    }

  if (cache->entry != entry)
    {
      rut_matrix_entry_ref (entry);
      if (cache->entry)
        rut_matrix_entry_unref (cache->entry);
      cache->entry = entry;

      /* We want to make sure here that if the cache->entry and the
       * given @entry are both identity matrices then even though they
       * are different entries we don't want to consider this an
       * update...
       */
      updated |= !is_identity;
    }

  return updated;
}

void
_rut_matrix_entry_cache_destroy (RutMatrixEntryCache *cache)
{
  if (cache->entry)
    rut_matrix_entry_unref (cache->entry);
}
