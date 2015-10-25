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

#include <rut-config.h>

#include <cglib/cglib.h>

#include "rut-shell.h"
#include "rut-object.h"
#include "rut-magazine.h"
#include "rut-matrix-stack.h"

static rut_magazine_t *rut_matrix_stack_magazine;
static rut_magazine_t *rut_matrix_stack_matrices_magazine;

/* XXX: Note: this leaves entry->parent uninitialized! */
static rut_matrix_entry_t *
_rut_matrix_entry_new(c_matrix_op_t operation)
{
    rut_matrix_entry_t *entry =
        rut_magazine_chunk_alloc(rut_matrix_stack_magazine);

    entry->ref_count = 1;
    entry->op = operation;

#ifdef RUT_DEBUG_ENABLED
    entry->composite_gets = 0;
#endif

    return entry;
}

static void *
_rut_matrix_stack_push_entry(rut_matrix_stack_t *stack,
                             rut_matrix_entry_t *entry)
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
_rut_matrix_stack_push_operation(rut_matrix_stack_t *stack,
                                 c_matrix_op_t operation)
{
    rut_matrix_entry_t *entry = _rut_matrix_entry_new(operation);

    _rut_matrix_stack_push_entry(stack, entry);

    return entry;
}

static void *
_rut_matrix_stack_push_replacement_entry(rut_matrix_stack_t *stack,
                                         c_matrix_op_t operation)
{
    rut_matrix_entry_t *old_top = stack->last_entry;
    rut_matrix_entry_t *new_top;

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

    rut_matrix_entry_ref(new_top);
    rut_matrix_entry_unref(old_top);
    stack->last_entry = new_top;

    return _rut_matrix_stack_push_operation(stack, operation);
}

void
_rut_matrix_entry_identity_init(rut_matrix_entry_t *entry)
{
    entry->ref_count = 1;
    entry->op = RUT_MATRIX_OP_LOAD_IDENTITY;
    entry->parent = NULL;
#ifdef RUT_DEBUG_ENABLED
    entry->composite_gets = 0;
#endif
}

void
rut_matrix_stack_load_identity(rut_matrix_stack_t *stack)
{
    _rut_matrix_stack_push_replacement_entry(stack,
                                             RUT_MATRIX_OP_LOAD_IDENTITY);
}

void
rut_matrix_stack_translate(rut_matrix_stack_t *stack, float x, float y, float z)
{
    rut_matrix_entry_translate_t *entry;

    entry = _rut_matrix_stack_push_operation(stack, RUT_MATRIX_OP_TRANSLATE);

    entry->x = x;
    entry->y = y;
    entry->z = z;
}

void
rut_matrix_stack_rotate(
    rut_matrix_stack_t *stack, float angle, float x, float y, float z)
{
    rut_matrix_entry_rotate_t *entry;

    entry = _rut_matrix_stack_push_operation(stack, RUT_MATRIX_OP_ROTATE);

    entry->angle = angle;
    entry->x = x;
    entry->y = y;
    entry->z = z;
}

void
rut_matrix_stack_rotate_quaternion(rut_matrix_stack_t *stack,
                                   const c_quaternion_t *quaternion)
{
    rut_matrix_entry_rotate_quaternion_t *entry;

    entry = _rut_matrix_stack_push_operation(stack,
                                             RUT_MATRIX_OP_ROTATE_QUATERNION);

    entry->values[0] = quaternion->w;
    entry->values[1] = quaternion->x;
    entry->values[2] = quaternion->y;
    entry->values[3] = quaternion->z;
}

void
rut_matrix_stack_rotate_euler(rut_matrix_stack_t *stack,
                              const c_euler_t *euler)
{
    rut_matrix_entry_rotate_euler_t *entry;

    entry = _rut_matrix_stack_push_operation(stack, RUT_MATRIX_OP_ROTATE_EULER);

    entry->heading = euler->heading;
    entry->pitch = euler->pitch;
    entry->roll = euler->roll;
}

void
rut_matrix_stack_scale(rut_matrix_stack_t *stack, float x, float y, float z)
{
    rut_matrix_entry_scale_t *entry;

    entry = _rut_matrix_stack_push_operation(stack, RUT_MATRIX_OP_SCALE);

    entry->x = x;
    entry->y = y;
    entry->z = z;
}

void
rut_matrix_stack_multiply(rut_matrix_stack_t *stack,
                          const c_matrix_t *matrix)
{
    rut_matrix_entry_multiply_t *entry;

    entry = _rut_matrix_stack_push_operation(stack, RUT_MATRIX_OP_MULTIPLY);

    entry->matrix =
        rut_magazine_chunk_alloc(rut_matrix_stack_matrices_magazine);

    c_matrix_init_from_array(entry->matrix, (float *)matrix);
}

void
rut_matrix_stack_set(rut_matrix_stack_t *stack, const c_matrix_t *matrix)
{
    rut_matrix_entry_load_t *entry;

    entry = _rut_matrix_stack_push_replacement_entry(stack, RUT_MATRIX_OP_LOAD);

    entry->matrix =
        rut_magazine_chunk_alloc(rut_matrix_stack_matrices_magazine);

    c_matrix_init_from_array(entry->matrix, (float *)matrix);
}

void
rut_matrix_stack_frustum(rut_matrix_stack_t *stack,
                         float left,
                         float right,
                         float bottom,
                         float top,
                         float z_near,
                         float z_far)
{
    rut_matrix_entry_load_t *entry;

    entry = _rut_matrix_stack_push_replacement_entry(stack, RUT_MATRIX_OP_LOAD);

    entry->matrix =
        rut_magazine_chunk_alloc(rut_matrix_stack_matrices_magazine);

    c_matrix_init_identity(entry->matrix);
    c_matrix_frustum(entry->matrix, left, right, bottom, top, z_near, z_far);
}

void
rut_matrix_stack_perspective(rut_matrix_stack_t *stack,
                             float fov_y,
                             float aspect,
                             float z_near,
                             float z_far)
{
    rut_matrix_entry_load_t *entry;

    entry = _rut_matrix_stack_push_replacement_entry(stack, RUT_MATRIX_OP_LOAD);

    entry->matrix =
        rut_magazine_chunk_alloc(rut_matrix_stack_matrices_magazine);

    c_matrix_init_identity(entry->matrix);
    c_matrix_perspective(entry->matrix, fov_y, aspect, z_near, z_far);
}

void
rut_matrix_stack_orthographic(rut_matrix_stack_t *stack,
                              float x_1,
                              float y_1,
                              float x_2,
                              float y_2,
                              float near,
                              float far)
{
    rut_matrix_entry_load_t *entry;

    entry = _rut_matrix_stack_push_replacement_entry(stack, RUT_MATRIX_OP_LOAD);

    entry->matrix =
        rut_magazine_chunk_alloc(rut_matrix_stack_matrices_magazine);

    c_matrix_init_identity(entry->matrix);
    c_matrix_orthographic(entry->matrix, x_1, y_1, x_2, y_2, near, far);
}

void
rut_matrix_stack_push(rut_matrix_stack_t *stack)
{
    rut_matrix_entry_save_t *entry;

    entry = _rut_matrix_stack_push_operation(stack, RUT_MATRIX_OP_SAVE);

    entry->cache_valid = false;
}

rut_matrix_entry_t *
rut_matrix_entry_ref(rut_matrix_entry_t *entry)
{
    /* A NULL pointer is considered a valid stack so we should accept
       that as an argument */
    if (entry)
        entry->ref_count++;

    return entry;
}

void
rut_matrix_entry_unref(rut_matrix_entry_t *entry)
{
    rut_matrix_entry_t *parent;

    for (; entry && --entry->ref_count <= 0; entry = parent) {
        parent = entry->parent;

        switch (entry->op) {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
        case RUT_MATRIX_OP_TRANSLATE:
        case RUT_MATRIX_OP_ROTATE:
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
        case RUT_MATRIX_OP_ROTATE_EULER:
        case RUT_MATRIX_OP_SCALE:
            break;
        case RUT_MATRIX_OP_MULTIPLY: {
            rut_matrix_entry_multiply_t *multiply =
                (rut_matrix_entry_multiply_t *)entry;
            rut_magazine_chunk_free(rut_matrix_stack_matrices_magazine,
                                    multiply->matrix);
            break;
        }
        case RUT_MATRIX_OP_LOAD: {
            rut_matrix_entry_load_t *load = (rut_matrix_entry_load_t *)entry;
            rut_magazine_chunk_free(rut_matrix_stack_matrices_magazine,
                                    load->matrix);
            break;
        }
        case RUT_MATRIX_OP_SAVE: {
            rut_matrix_entry_save_t *save = (rut_matrix_entry_save_t *)entry;
            if (save->cache_valid)
                rut_magazine_chunk_free(rut_matrix_stack_matrices_magazine,
                                        save->cache);
            break;
        }
        }

        rut_magazine_chunk_free(rut_matrix_stack_magazine, entry);
    }
}

void
rut_matrix_stack_pop(rut_matrix_stack_t *stack)
{
    rut_matrix_entry_t *old_top;
    rut_matrix_entry_t *new_top;

    c_return_if_fail(stack != NULL);

    old_top = stack->last_entry;
    c_return_if_fail(old_top != NULL);

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
    for (new_top = old_top; new_top->op != RUT_MATRIX_OP_SAVE;
         new_top = new_top->parent)
        ;

    new_top = new_top->parent;
    rut_matrix_entry_ref(new_top);

    rut_matrix_entry_unref(old_top);

    stack->last_entry = new_top;
}

bool
rut_matrix_stack_get_inverse(rut_matrix_stack_t *stack,
                             c_matrix_t *inverse)
{
    c_matrix_t matrix;
    c_matrix_t *internal = rut_matrix_stack_get(stack, &matrix);

    if (internal)
        return c_matrix_get_inverse(internal, inverse);
    else
        return c_matrix_get_inverse(&matrix, inverse);
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
c_matrix_t *
rut_matrix_entry_get(rut_matrix_entry_t *entry,
                     c_matrix_t *matrix)
{
    int depth;
    rut_matrix_entry_t *current;
    rut_matrix_entry_t **children;
    int i;

    for (depth = 0, current = entry; current;
         current = current->parent, depth++) {
        switch (current->op) {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
            c_matrix_init_identity(matrix);
            goto initialized;
        case RUT_MATRIX_OP_LOAD: {
            rut_matrix_entry_load_t *load = (rut_matrix_entry_load_t *)current;
            *matrix = *load->matrix;
            goto initialized;
        }
        case RUT_MATRIX_OP_SAVE: {
            rut_matrix_entry_save_t *save = (rut_matrix_entry_save_t *)current;
            if (!save->cache_valid) {
                rut_magazine_t *matrices_magazine =
                    rut_matrix_stack_matrices_magazine;
                save->cache = rut_magazine_chunk_alloc(matrices_magazine);
                rut_matrix_entry_get(current->parent, save->cache);
                save->cache_valid = true;
            }
            *matrix = *save->cache;
            goto initialized;
        }
        default:
            continue;
        }
    }

initialized:

    if (depth == 0) {
        switch (entry->op) {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
        case RUT_MATRIX_OP_TRANSLATE:
        case RUT_MATRIX_OP_ROTATE:
        case RUT_MATRIX_OP_ROTATE_QUATERNION:
        case RUT_MATRIX_OP_ROTATE_EULER:
        case RUT_MATRIX_OP_SCALE:
        case RUT_MATRIX_OP_MULTIPLY:
            return NULL;

        case RUT_MATRIX_OP_LOAD: {
            rut_matrix_entry_load_t *load = (rut_matrix_entry_load_t *)entry;
            return load->matrix;
        }
        case RUT_MATRIX_OP_SAVE: {
            rut_matrix_entry_save_t *save = (rut_matrix_entry_save_t *)entry;
            return save->cache;
        }
        }
        c_warn_if_reached();
        return NULL;
    }

#ifdef RUT_ENABLE_DEBUG
    if (!current) {
        c_warning("Inconsistent matrix stack");
        return NULL;
    }

    entry->composite_gets++;
#endif

    children = c_alloca(sizeof(rut_matrix_entry_t) * depth);

    /* We need walk the list of entries from the init/load/save entry
     * back towards the leaf node but the nodes don't link to their
     * children so we need to re-walk them here to add to a separate
     * array. */
    for (i = depth - 1, current = entry; i >= 0 && current;
         i--, current = current->parent) {
        children[i] = current;
    }

#ifdef RUT_ENABLE_DEBUG
    if (RUT_DEBUG_ENABLED(RUT_DEBUG_PERFORMANCE) &&
        entry->composite_gets >= 2) {
        RUT_NOTE(PERFORMANCE,
                 "Re-composing a matrix stack entry multiple times");
    }
#endif

    for (i = 0; i < depth; i++) {
        switch (children[i]->op) {
        case RUT_MATRIX_OP_TRANSLATE: {
            rut_matrix_entry_translate_t *translate =
                (rut_matrix_entry_translate_t *)children[i];
            c_matrix_translate(
                matrix, translate->x, translate->y, translate->z);
            continue;
        }
        case RUT_MATRIX_OP_ROTATE: {
            rut_matrix_entry_rotate_t *rotate =
                (rut_matrix_entry_rotate_t *)children[i];
            c_matrix_rotate(
                matrix, rotate->angle, rotate->x, rotate->y, rotate->z);
            continue;
        }
        case RUT_MATRIX_OP_ROTATE_EULER: {
            rut_matrix_entry_rotate_euler_t *rotate =
                (rut_matrix_entry_rotate_euler_t *)children[i];
            c_euler_t euler;
            c_euler_init(&euler, rotate->heading, rotate->pitch, rotate->roll);
            c_matrix_rotate_euler(matrix, &euler);
            continue;
        }
        case RUT_MATRIX_OP_ROTATE_QUATERNION: {
            rut_matrix_entry_rotate_quaternion_t *rotate =
                (rut_matrix_entry_rotate_quaternion_t *)children[i];
            c_quaternion_t quaternion;
            c_quaternion_init_from_array(&quaternion, rotate->values);
            c_matrix_rotate_quaternion(matrix, &quaternion);
            continue;
        }
        case RUT_MATRIX_OP_SCALE: {
            rut_matrix_entry_scale_t *scale =
                (rut_matrix_entry_scale_t *)children[i];
            c_matrix_scale(matrix, scale->x, scale->y, scale->z);
            continue;
        }
        case RUT_MATRIX_OP_MULTIPLY: {
            rut_matrix_entry_multiply_t *multiply =
                (rut_matrix_entry_multiply_t *)children[i];
            c_matrix_multiply(matrix, matrix, multiply->matrix);
            continue;
        }

        case RUT_MATRIX_OP_LOAD_IDENTITY:
        case RUT_MATRIX_OP_LOAD:
        case RUT_MATRIX_OP_SAVE:
            c_warn_if_reached();
            continue;
        }
    }

    return NULL;
}

rut_matrix_entry_t *
rut_matrix_stack_get_entry(rut_matrix_stack_t *stack)
{
    return stack->last_entry;
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
c_matrix_t *
rut_matrix_stack_get(rut_matrix_stack_t *stack,
                     c_matrix_t *matrix)
{
    return rut_matrix_entry_get(stack->last_entry, matrix);
}

static void
_rut_matrix_stack_free(void *object)
{
    rut_matrix_stack_t *stack = object;

    rut_matrix_entry_unref(stack->last_entry);

    rut_object_free(rut_matrix_stack_t, stack);
}

rut_type_t rut_matrix_stack_type;

static void
_rut_matrix_stack_init_type(void)
{
    rut_type_init(
        &rut_matrix_stack_type, "rut_matrix_stack_t", _rut_matrix_stack_free);
}

rut_matrix_stack_t *
rut_matrix_stack_new(rut_shell_t *shell)
{
    rut_matrix_stack_t *stack = rut_object_alloc0(rut_matrix_stack_t,
                                                  &rut_matrix_stack_type,
                                                  _rut_matrix_stack_init_type);

    if (C_UNLIKELY(rut_matrix_stack_magazine == NULL)) {
        rut_matrix_stack_magazine =
            rut_magazine_new(sizeof(rut_matrix_entry_full_t), 20);
        rut_matrix_stack_matrices_magazine =
            rut_magazine_new(sizeof(c_matrix_t), 20);
    }

    stack->shell = shell;
    stack->last_entry = NULL;

    rut_matrix_entry_ref(&shell->identity_entry);
    _rut_matrix_stack_push_entry(stack, &shell->identity_entry);

    return stack;
}

static rut_matrix_entry_t *
_rut_matrix_entry_skip_saves(rut_matrix_entry_t *entry)
{
    /* We currently assume that every stack starts with an
     * _OP_LOAD_IDENTITY so we don't need to worry about
     * NULL pointer dereferencing here. */
    while (entry->op == RUT_MATRIX_OP_SAVE)
        entry = entry->parent;

    return entry;
}

bool
rut_matrix_entry_calculate_translation(rut_matrix_entry_t *entry0,
                                       rut_matrix_entry_t *entry1,
                                       float *x,
                                       float *y,
                                       float *z)
{
    c_sllist_t *head0 = NULL;
    c_sllist_t *head1 = NULL;
    rut_matrix_entry_t *node0;
    rut_matrix_entry_t *node1;
    int len0 = 0;
    int len1 = 0;
    int count;
    c_sllist_t *common_ancestor0;
    c_sllist_t *common_ancestor1;

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
     * then bail out returning false.
     */

    for (node0 = entry0; node0; node0 = node0->parent) {
        c_sllist_t *link;

        if (node0->op == RUT_MATRIX_OP_SAVE)
            continue;

        link = alloca(sizeof(c_sllist_t));
        link->next = head0;
        link->data = node0;
        head0 = link;
        len0++;

        if (node0->op != RUT_MATRIX_OP_TRANSLATE)
            break;
    }
    for (node1 = entry1; node1; node1 = node1->parent) {
        c_sllist_t *link;

        if (node1->op == RUT_MATRIX_OP_SAVE)
            continue;

        link = alloca(sizeof(c_sllist_t));
        link->next = head1;
        link->data = node1;
        head1 = link;
        len1++;

        if (node1->op != RUT_MATRIX_OP_TRANSLATE)
            break;
    }

    if (head0->data != head1->data)
        return false;

    common_ancestor0 = head0;
    common_ancestor1 = head1;
    head0 = head0->next;
    head1 = head1->next;
    count = MIN(len0, len1) - 1;
    while (count--) {
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

    for (head0 = common_ancestor0->next; head0; head0 = head0->next) {
        rut_matrix_entry_translate_t *translate;

        node0 = head0->data;

        if (node0->op != RUT_MATRIX_OP_TRANSLATE)
            return false;

        translate = (rut_matrix_entry_translate_t *)node0;

        *x = *x - translate->x;
        *y = *y - translate->y;
        *z = *z - translate->z;
    }
    for (head1 = common_ancestor1->next; head1; head1 = head1->next) {
        rut_matrix_entry_translate_t *translate;

        node1 = head1->data;

        if (node1->op != RUT_MATRIX_OP_TRANSLATE)
            return false;

        translate = (rut_matrix_entry_translate_t *)node1;

        *x = *x + translate->x;
        *y = *y + translate->y;
        *z = *z + translate->z;
    }

    return true;
}

bool
rut_matrix_entry_is_identity(rut_matrix_entry_t *entry)
{
    return entry ? entry->op == RUT_MATRIX_OP_LOAD_IDENTITY : false;
}

bool
rut_matrix_entry_equal(rut_matrix_entry_t *entry0,
                       rut_matrix_entry_t *entry1)
{
    for (; entry0 && entry1; entry0 = entry0->parent, entry1 = entry1->parent) {
        entry0 = _rut_matrix_entry_skip_saves(entry0);
        entry1 = _rut_matrix_entry_skip_saves(entry1);

        if (entry0 == entry1)
            return true;

        if (entry0->op != entry1->op)
            return false;

        switch (entry0->op) {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
            return true;
        case RUT_MATRIX_OP_TRANSLATE: {
            rut_matrix_entry_translate_t *translate0 =
                (rut_matrix_entry_translate_t *)entry0;
            rut_matrix_entry_translate_t *translate1 =
                (rut_matrix_entry_translate_t *)entry1;
            /* We could perhaps use an epsilon to compare here?
             * I expect the false negatives are probaly never going to
             * be a problem and this is a bit cheaper. */
            if (translate0->x != translate1->x ||
                translate0->y != translate1->y ||
                translate0->z != translate1->z)
                return false;
        } break;
        case RUT_MATRIX_OP_ROTATE: {
            rut_matrix_entry_rotate_t *rotate0 =
                (rut_matrix_entry_rotate_t *)entry0;
            rut_matrix_entry_rotate_t *rotate1 =
                (rut_matrix_entry_rotate_t *)entry1;
            if (rotate0->angle != rotate1->angle || rotate0->x != rotate1->x ||
                rotate0->y != rotate1->y || rotate0->z != rotate1->z)
                return false;
        } break;
        case RUT_MATRIX_OP_ROTATE_QUATERNION: {
            rut_matrix_entry_rotate_quaternion_t *rotate0 =
                (rut_matrix_entry_rotate_quaternion_t *)entry0;
            rut_matrix_entry_rotate_quaternion_t *rotate1 =
                (rut_matrix_entry_rotate_quaternion_t *)entry1;
            int i;
            for (i = 0; i < 4; i++)
                if (rotate0->values[i] != rotate1->values[i])
                    return false;
        } break;
        case RUT_MATRIX_OP_ROTATE_EULER: {
            rut_matrix_entry_rotate_euler_t *rotate0 =
                (rut_matrix_entry_rotate_euler_t *)entry0;
            rut_matrix_entry_rotate_euler_t *rotate1 =
                (rut_matrix_entry_rotate_euler_t *)entry1;

            if (rotate0->heading != rotate1->heading ||
                rotate0->pitch != rotate1->pitch ||
                rotate0->roll != rotate1->roll)
                return false;
        } break;
        case RUT_MATRIX_OP_SCALE: {
            rut_matrix_entry_scale_t *scale0 =
                (rut_matrix_entry_scale_t *)entry0;
            rut_matrix_entry_scale_t *scale1 =
                (rut_matrix_entry_scale_t *)entry1;
            if (scale0->x != scale1->x || scale0->y != scale1->y ||
                scale0->z != scale1->z)
                return false;
        } break;
        case RUT_MATRIX_OP_MULTIPLY: {
            rut_matrix_entry_multiply_t *mult0 =
                (rut_matrix_entry_multiply_t *)entry0;
            rut_matrix_entry_multiply_t *mult1 =
                (rut_matrix_entry_multiply_t *)entry1;
            if (!c_matrix_equal(mult0->matrix, mult1->matrix))
                return false;
        } break;
        case RUT_MATRIX_OP_LOAD: {
            rut_matrix_entry_load_t *load0 = (rut_matrix_entry_load_t *)entry0;
            rut_matrix_entry_load_t *load1 = (rut_matrix_entry_load_t *)entry1;
            /* There's no need to check any further since an
             * _OP_LOAD makes all the ancestors redundant as far as
             * the final matrix value is concerned. */
            return c_matrix_equal(load0->matrix, load1->matrix);
        }
        case RUT_MATRIX_OP_SAVE:
            /* We skip over saves above so we shouldn't see save entries */
            c_warn_if_reached();
        }
    }

    return false;
}

void
rut_debug_matrix_entry_print(rut_matrix_entry_t *entry)
{
    int depth;
    rut_matrix_entry_t *e;
    rut_matrix_entry_t **children;
    int i;

    for (depth = 0, e = entry; e; e = e->parent)
        depth++;

    children = c_alloca(sizeof(rut_matrix_entry_t) * depth);

    for (i = depth - 1, e = entry; i >= 0 && e; i--, e = e->parent) {
        children[i] = e;
    }

    c_debug("MatrixEntry %p =\n", entry);

    for (i = 0; i < depth; i++) {
        entry = children[i];

        switch (entry->op) {
        case RUT_MATRIX_OP_LOAD_IDENTITY:
            c_debug("  LOAD IDENTITY\n");
            continue;
        case RUT_MATRIX_OP_TRANSLATE: {
            rut_matrix_entry_translate_t *translate =
                (rut_matrix_entry_translate_t *)entry;
            c_debug("  TRANSLATE X=%f Y=%f Z=%f\n",
                    translate->x,
                    translate->y,
                    translate->z);
            continue;
        }
        case RUT_MATRIX_OP_ROTATE: {
            rut_matrix_entry_rotate_t *rotate =
                (rut_matrix_entry_rotate_t *)entry;
            c_debug("  ROTATE ANGLE=%f X=%f Y=%f Z=%f\n",
                    rotate->angle,
                    rotate->x,
                    rotate->y,
                    rotate->z);
            continue;
        }
        case RUT_MATRIX_OP_ROTATE_QUATERNION: {
            rut_matrix_entry_rotate_quaternion_t *rotate =
                (rut_matrix_entry_rotate_quaternion_t *)entry;
            c_debug("  ROTATE QUATERNION w=%f x=%f y=%f z=%f\n",
                    rotate->values[0],
                    rotate->values[1],
                    rotate->values[2],
                    rotate->values[3]);
            continue;
        }
        case RUT_MATRIX_OP_ROTATE_EULER: {
            rut_matrix_entry_rotate_euler_t *rotate =
                (rut_matrix_entry_rotate_euler_t *)entry;
            c_debug("  ROTATE EULER heading=%f pitch=%f roll=%f\n",
                    rotate->heading,
                    rotate->pitch,
                    rotate->roll);
            continue;
        }
        case RUT_MATRIX_OP_SCALE: {
            rut_matrix_entry_scale_t *scale = (rut_matrix_entry_scale_t *)entry;
            c_debug("  SCALE X=%f Y=%f Z=%f\n", scale->x, scale->y, scale->z);
            continue;
        }
        case RUT_MATRIX_OP_MULTIPLY: {
            rut_matrix_entry_multiply_t *mult =
                (rut_matrix_entry_multiply_t *)entry;
            c_debug("  MULT:\n");
            c_matrix_prefix_print ("    ", mult->matrix);
            continue;
        }
        case RUT_MATRIX_OP_LOAD: {
            rut_matrix_entry_load_t *load = (rut_matrix_entry_load_t *)entry;
            c_debug("  LOAD:\n");
            c_matrix_prefix_print ("    ", load->matrix);
            continue;
        }
        case RUT_MATRIX_OP_SAVE:
            c_debug("  SAVE\n");
        }
    }
}

void
_rut_matrix_entry_cache_init(rut_matrix_entry_cache_t *cache)
{
    cache->entry = NULL;
    cache->flushed_identity = false;
}

/* NB: This function can report false negatives since it never does a
 * deep comparison of the stack matrices. */
bool
_rut_matrix_entry_cache_maybe_update(rut_matrix_entry_cache_t *cache,
                                     rut_matrix_entry_t *entry)
{
    bool is_identity;
    bool updated = false;

    is_identity = (entry->op == RUT_MATRIX_OP_LOAD_IDENTITY);
    if (cache->flushed_identity != is_identity) {
        cache->flushed_identity = is_identity;
        updated = true;
    }

    if (cache->entry != entry) {
        rut_matrix_entry_ref(entry);
        if (cache->entry)
            rut_matrix_entry_unref(cache->entry);
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
_rut_matrix_entry_cache_destroy(rut_matrix_entry_cache_t *cache)
{
    if (cache->entry)
        rut_matrix_entry_unref(cache->entry);
}
