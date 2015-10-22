/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
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
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-matrix-stack.h"
#include "cg-framebuffer-private.h"
#include "cg-object-private.h"
#include "cg-offscreen.h"
#include "cg-magazine-private.h"

static void _cg_matrix_stack_free(cg_matrix_stack_t *stack);

CG_OBJECT_DEFINE(MatrixStack, matrix_stack);

static cg_magazine_t *cg_matrix_stack_magazine;
static cg_magazine_t *cg_matrix_stack_matrices_magazine;

/* XXX: Note: this leaves entry->parent uninitialized! */
static cg_matrix_entry_t *
_cg_matrix_entry_new(cg_matrix_op_t operation)
{
    cg_matrix_entry_t *entry =
        _cg_magazine_chunk_alloc(cg_matrix_stack_magazine);

    entry->ref_count = 1;
    entry->op = operation;

#ifdef CG_DEBUG_ENABLED
    entry->composite_gets = 0;
#endif

    return entry;
}

static void *
_cg_matrix_stack_push_entry(cg_matrix_stack_t *stack,
                            cg_matrix_entry_t *entry)
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
_cg_matrix_stack_push_operation(cg_matrix_stack_t *stack,
                                cg_matrix_op_t operation)
{
    cg_matrix_entry_t *entry = _cg_matrix_entry_new(operation);

    _cg_matrix_stack_push_entry(stack, entry);

    return entry;
}

static void *
_cg_matrix_stack_push_replacement_entry(cg_matrix_stack_t *stack,
                                        cg_matrix_op_t operation)
{
    cg_matrix_entry_t *old_top = stack->last_entry;
    cg_matrix_entry_t *new_top;

    /* This would only be called for operations that completely replace
     * the matrix. In that case we don't need to keep a reference to
     * anything up to the last save entry. This optimisation could be
     * important for applications that aren't using the stack but
     * instead just perform their own matrix manipulations and load a
     * new stack every frame. If this optimisation isn't done then the
     * stack would just grow endlessly. See the comments
     * cg_matrix_stack_pop for a description of how popping works. */
    for (new_top = old_top; new_top->op != CG_MATRIX_OP_SAVE && new_top->parent;
         new_top = new_top->parent)
        ;

    cg_matrix_entry_ref(new_top);
    cg_matrix_entry_unref(old_top);
    stack->last_entry = new_top;

    return _cg_matrix_stack_push_operation(stack, operation);
}

void
_cg_matrix_entry_identity_init(cg_matrix_entry_t *entry)
{
    entry->ref_count = 1;
    entry->op = CG_MATRIX_OP_LOAD_IDENTITY;
    entry->parent = NULL;
#ifdef CG_DEBUG_ENABLED
    entry->composite_gets = 0;
#endif
}

void
cg_matrix_stack_load_identity(cg_matrix_stack_t *stack)
{
    _cg_matrix_stack_push_replacement_entry(stack, CG_MATRIX_OP_LOAD_IDENTITY);
}

void
cg_matrix_stack_translate(cg_matrix_stack_t *stack, float x, float y, float z)
{
    cg_matrix_entry_translate_t *entry;

    entry = _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_TRANSLATE);

    entry->x = x;
    entry->y = y;
    entry->z = z;
}

void
cg_matrix_stack_rotate(
    cg_matrix_stack_t *stack, float angle, float x, float y, float z)
{
    cg_matrix_entry_rotate_t *entry;

    entry = _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_ROTATE);

    entry->angle = angle;
    entry->x = x;
    entry->y = y;
    entry->z = z;
}

void
cg_matrix_stack_rotate_quaternion(cg_matrix_stack_t *stack,
                                  const c_quaternion_t *quaternion)
{
    cg_matrix_entry_rotate_quaternion_t *entry;

    entry =
        _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_ROTATE_QUATERNION);

    entry->values[0] = quaternion->w;
    entry->values[1] = quaternion->x;
    entry->values[2] = quaternion->y;
    entry->values[3] = quaternion->z;
}

void
cg_matrix_stack_rotate_euler(cg_matrix_stack_t *stack,
                             const c_euler_t *euler)
{
    cg_matrix_entry_rotate_euler_t *entry;

    entry = _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_ROTATE_EULER);

    entry->heading = euler->heading;
    entry->pitch = euler->pitch;
    entry->roll = euler->roll;
}

void
cg_matrix_stack_scale(cg_matrix_stack_t *stack, float x, float y, float z)
{
    cg_matrix_entry_scale_t *entry;

    entry = _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_SCALE);

    entry->x = x;
    entry->y = y;
    entry->z = z;
}

void
cg_matrix_stack_multiply(cg_matrix_stack_t *stack,
                         const c_matrix_t *matrix)
{
    cg_matrix_entry_multiply_t *entry;

    entry = _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_MULTIPLY);

    entry->matrix = _cg_magazine_chunk_alloc(cg_matrix_stack_matrices_magazine);

    c_matrix_init_from_array(entry->matrix, (float *)matrix);
}

void
cg_matrix_stack_set(cg_matrix_stack_t *stack, const c_matrix_t *matrix)
{
    cg_matrix_entry_load_t *entry;

    entry = _cg_matrix_stack_push_replacement_entry(stack, CG_MATRIX_OP_LOAD);

    entry->matrix = _cg_magazine_chunk_alloc(cg_matrix_stack_matrices_magazine);

    c_matrix_init_from_array(entry->matrix, (float *)matrix);
}

void
cg_matrix_stack_frustum(cg_matrix_stack_t *stack,
                        float left,
                        float right,
                        float bottom,
                        float top,
                        float z_near,
                        float z_far)
{
    cg_matrix_entry_load_t *entry;

    entry = _cg_matrix_stack_push_replacement_entry(stack, CG_MATRIX_OP_LOAD);

    entry->matrix = _cg_magazine_chunk_alloc(cg_matrix_stack_matrices_magazine);

    c_matrix_init_identity(entry->matrix);
    c_matrix_frustum(entry->matrix, left, right, bottom, top, z_near, z_far);
}

void
cg_matrix_stack_perspective(cg_matrix_stack_t *stack,
                            float fov_y,
                            float aspect,
                            float z_near,
                            float z_far)
{
    cg_matrix_entry_load_t *entry;

    entry = _cg_matrix_stack_push_replacement_entry(stack, CG_MATRIX_OP_LOAD);

    entry->matrix = _cg_magazine_chunk_alloc(cg_matrix_stack_matrices_magazine);

    c_matrix_init_identity(entry->matrix);
    c_matrix_perspective(entry->matrix, fov_y, aspect, z_near, z_far);
}

void
cg_matrix_stack_orthographic(cg_matrix_stack_t *stack,
                             float x_1,
                             float y_1,
                             float x_2,
                             float y_2,
                             float near,
                             float far)
{
    cg_matrix_entry_load_t *entry;

    entry = _cg_matrix_stack_push_replacement_entry(stack, CG_MATRIX_OP_LOAD);

    entry->matrix = _cg_magazine_chunk_alloc(cg_matrix_stack_matrices_magazine);

    c_matrix_init_identity(entry->matrix);
    c_matrix_orthographic(entry->matrix, x_1, y_1, x_2, y_2, near, far);
}

void
cg_matrix_stack_push(cg_matrix_stack_t *stack)
{
    cg_matrix_entry_save_t *entry;

    entry = _cg_matrix_stack_push_operation(stack, CG_MATRIX_OP_SAVE);

    entry->cache_valid = false;
}

cg_matrix_entry_t *
cg_matrix_entry_ref(cg_matrix_entry_t *entry)
{
    /* A NULL pointer is considered a valid stack so we should accept
       that as an argument */
    if (entry)
        entry->ref_count++;

    return entry;
}

void
cg_matrix_entry_unref(cg_matrix_entry_t *entry)
{
    cg_matrix_entry_t *parent;

    for (; entry && --entry->ref_count <= 0; entry = parent) {
        parent = entry->parent;

        switch (entry->op) {
        case CG_MATRIX_OP_LOAD_IDENTITY:
        case CG_MATRIX_OP_TRANSLATE:
        case CG_MATRIX_OP_ROTATE:
        case CG_MATRIX_OP_ROTATE_QUATERNION:
        case CG_MATRIX_OP_ROTATE_EULER:
        case CG_MATRIX_OP_SCALE:
            break;
        case CG_MATRIX_OP_MULTIPLY: {
            cg_matrix_entry_multiply_t *multiply =
                (cg_matrix_entry_multiply_t *)entry;
            _cg_magazine_chunk_free(cg_matrix_stack_matrices_magazine,
                                    multiply->matrix);
            break;
        }
        case CG_MATRIX_OP_LOAD: {
            cg_matrix_entry_load_t *load = (cg_matrix_entry_load_t *)entry;
            _cg_magazine_chunk_free(cg_matrix_stack_matrices_magazine,
                                    load->matrix);
            break;
        }
        case CG_MATRIX_OP_SAVE: {
            cg_matrix_entry_save_t *save = (cg_matrix_entry_save_t *)entry;
            if (save->cache_valid)
                _cg_magazine_chunk_free(cg_matrix_stack_matrices_magazine,
                                        save->cache);
            break;
        }
        }

        _cg_magazine_chunk_free(cg_matrix_stack_magazine, entry);
    }
}

void
cg_matrix_stack_pop(cg_matrix_stack_t *stack)
{
    cg_matrix_entry_t *old_top;
    cg_matrix_entry_t *new_top;

    c_return_if_fail(stack != NULL);

    old_top = stack->last_entry;
    c_return_if_fail(old_top != NULL);

    /* To pop we are moving the top of the stack to the old top's parent
     * node. The stack always needs to have a reference to the top entry
     * so we must take a reference to the new top. The stack would have
     * previously had a reference to the old top so we need to decrease
     * the ref count on that. We need to ref the new head first in case
     * this stack was the only thing referencing the old top. In that
     * case the call to cg_matrix_entry_unref will unref the parent.
     */

    /* Find the last save operation and remove it */

    /* XXX: it would be an error to pop to the very beginning of the
     * stack so we don't need to check for NULL pointer dereferencing. */
    for (new_top = old_top; new_top->op != CG_MATRIX_OP_SAVE;
         new_top = new_top->parent)
        ;

    new_top = new_top->parent;
    cg_matrix_entry_ref(new_top);

    cg_matrix_entry_unref(old_top);

    stack->last_entry = new_top;
}

bool
cg_matrix_stack_get_inverse(cg_matrix_stack_t *stack, c_matrix_t *inverse)
{
    c_matrix_t matrix;
    c_matrix_t *internal = cg_matrix_stack_get(stack, &matrix);

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
cg_matrix_entry_get(cg_matrix_entry_t *entry, c_matrix_t *matrix)
{
    int depth;
    cg_matrix_entry_t *current;
    cg_matrix_entry_t **children;
    int i;

    for (depth = 0, current = entry; current;
         current = current->parent, depth++) {
        switch (current->op) {
        case CG_MATRIX_OP_LOAD_IDENTITY:
            c_matrix_init_identity(matrix);
            goto initialized;
        case CG_MATRIX_OP_LOAD: {
            cg_matrix_entry_load_t *load = (cg_matrix_entry_load_t *)current;
            *matrix = *load->matrix;
            goto initialized;
        }
        case CG_MATRIX_OP_SAVE: {
            cg_matrix_entry_save_t *save = (cg_matrix_entry_save_t *)current;
            if (!save->cache_valid) {
                cg_magazine_t *matrices_magazine =
                    cg_matrix_stack_matrices_magazine;
                save->cache = _cg_magazine_chunk_alloc(matrices_magazine);
                cg_matrix_entry_get(current->parent, save->cache);
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
        case CG_MATRIX_OP_LOAD_IDENTITY:
        case CG_MATRIX_OP_TRANSLATE:
        case CG_MATRIX_OP_ROTATE:
        case CG_MATRIX_OP_ROTATE_QUATERNION:
        case CG_MATRIX_OP_ROTATE_EULER:
        case CG_MATRIX_OP_SCALE:
        case CG_MATRIX_OP_MULTIPLY:
            return NULL;

        case CG_MATRIX_OP_LOAD: {
            cg_matrix_entry_load_t *load = (cg_matrix_entry_load_t *)entry;
            return load->matrix;
        }
        case CG_MATRIX_OP_SAVE: {
            cg_matrix_entry_save_t *save = (cg_matrix_entry_save_t *)entry;
            return save->cache;
        }
        }
        c_warn_if_reached();
        return NULL;
    }

#ifdef CG_ENABLE_DEBUG
    if (!current) {
        c_warning("Inconsistent matrix stack");
        return NULL;
    }

    entry->composite_gets++;
#endif

    children = c_alloca(sizeof(cg_matrix_entry_t) * depth);

    /* We need walk the list of entries from the init/load/save entry
     * back towards the leaf node but the nodes don't link to their
     * children so we need to re-walk them here to add to a separate
     * array. */
    for (i = depth - 1, current = entry; i >= 0 && current;
         i--, current = current->parent) {
        children[i] = current;
    }

#ifdef CG_ENABLE_DEBUG
    if (CG_DEBUG_ENABLED(CG_DEBUG_PERFORMANCE) && entry->composite_gets >= 2) {
        CG_NOTE(PERFORMANCE,
                "Re-composing a matrix stack entry multiple times");
    }
#endif

    for (i = 0; i < depth; i++) {
        switch (children[i]->op) {
        case CG_MATRIX_OP_TRANSLATE: {
            cg_matrix_entry_translate_t *translate =
                (cg_matrix_entry_translate_t *)children[i];
            c_matrix_translate(
                matrix, translate->x, translate->y, translate->z);
            continue;
        }
        case CG_MATRIX_OP_ROTATE: {
            cg_matrix_entry_rotate_t *rotate =
                (cg_matrix_entry_rotate_t *)children[i];
            c_matrix_rotate(
                matrix, rotate->angle, rotate->x, rotate->y, rotate->z);
            continue;
        }
        case CG_MATRIX_OP_ROTATE_EULER: {
            cg_matrix_entry_rotate_euler_t *rotate =
                (cg_matrix_entry_rotate_euler_t *)children[i];
            c_euler_t euler;
            c_euler_init(&euler, rotate->heading, rotate->pitch, rotate->roll);
            c_matrix_rotate_euler(matrix, &euler);
            continue;
        }
        case CG_MATRIX_OP_ROTATE_QUATERNION: {
            cg_matrix_entry_rotate_quaternion_t *rotate =
                (cg_matrix_entry_rotate_quaternion_t *)children[i];
            c_quaternion_t quaternion;
            c_quaternion_init_from_array(&quaternion, rotate->values);
            c_matrix_rotate_quaternion(matrix, &quaternion);
            continue;
        }
        case CG_MATRIX_OP_SCALE: {
            cg_matrix_entry_scale_t *scale =
                (cg_matrix_entry_scale_t *)children[i];
            c_matrix_scale(matrix, scale->x, scale->y, scale->z);
            continue;
        }
        case CG_MATRIX_OP_MULTIPLY: {
            cg_matrix_entry_multiply_t *multiply =
                (cg_matrix_entry_multiply_t *)children[i];
            c_matrix_multiply(matrix, matrix, multiply->matrix);
            continue;
        }

        case CG_MATRIX_OP_LOAD_IDENTITY:
        case CG_MATRIX_OP_LOAD:
        case CG_MATRIX_OP_SAVE:
            c_warn_if_reached();
            continue;
        }
    }

    return NULL;
}

cg_matrix_entry_t *
cg_matrix_stack_get_entry(cg_matrix_stack_t *stack)
{
    return stack->last_entry;
}

/* In addition to writing the stack matrix into the give @matrix
 * argument this function *may* sometimes also return a pointer
 * to a matrix too so if we are querying the inverse matrix we
 * should query from the return matrix so that the result can
 * be cached within the stack. */
c_matrix_t *
cg_matrix_stack_get(cg_matrix_stack_t *stack, c_matrix_t *matrix)
{
    return cg_matrix_entry_get(stack->last_entry, matrix);
}

static void
_cg_matrix_stack_free(cg_matrix_stack_t *stack)
{
    cg_matrix_entry_unref(stack->last_entry);
    c_slice_free(cg_matrix_stack_t, stack);
}

cg_matrix_stack_t *
cg_matrix_stack_new(cg_device_t *dev)
{
    cg_matrix_stack_t *stack = c_slice_new(cg_matrix_stack_t);

    if (C_UNLIKELY(cg_matrix_stack_magazine == NULL)) {
        cg_matrix_stack_magazine =
            _cg_magazine_new(sizeof(cg_matrix_entry_full_t), 20);
        cg_matrix_stack_matrices_magazine =
            _cg_magazine_new(sizeof(c_matrix_t), 20);
    }

    stack->dev = dev;
    stack->last_entry = NULL;

    cg_matrix_entry_ref(&dev->identity_entry);
    _cg_matrix_stack_push_entry(stack, &dev->identity_entry);

    return _cg_matrix_stack_object_new(stack);
}

static cg_matrix_entry_t *
_cg_matrix_entry_skip_saves(cg_matrix_entry_t *entry)
{
    /* We currently assume that every stack starts with an
     * _OP_LOAD_IDENTITY so we don't need to worry about
     * NULL pointer dereferencing here. */
    while (entry->op == CG_MATRIX_OP_SAVE)
        entry = entry->parent;

    return entry;
}

bool
cg_matrix_entry_calculate_translation(cg_matrix_entry_t *entry0,
                                      cg_matrix_entry_t *entry1,
                                      float *x,
                                      float *y,
                                      float *z)
{
    c_sllist_t *head0 = NULL;
    c_sllist_t *head1 = NULL;
    cg_matrix_entry_t *node0;
    cg_matrix_entry_t *node1;
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

        if (node0->op == CG_MATRIX_OP_SAVE)
            continue;

        link = alloca(sizeof(c_sllist_t));
        link->next = head0;
        link->data = node0;
        head0 = link;
        len0++;

        if (node0->op != CG_MATRIX_OP_TRANSLATE)
            break;
    }
    for (node1 = entry1; node1; node1 = node1->parent) {
        c_sllist_t *link;

        if (node1->op == CG_MATRIX_OP_SAVE)
            continue;

        link = alloca(sizeof(c_sllist_t));
        link->next = head1;
        link->data = node1;
        head1 = link;
        len1++;

        if (node1->op != CG_MATRIX_OP_TRANSLATE)
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
        cg_matrix_entry_translate_t *translate;

        node0 = head0->data;

        if (node0->op != CG_MATRIX_OP_TRANSLATE)
            return false;

        translate = (cg_matrix_entry_translate_t *)node0;

        *x = *x - translate->x;
        *y = *y - translate->y;
        *z = *z - translate->z;
    }
    for (head1 = common_ancestor1->next; head1; head1 = head1->next) {
        cg_matrix_entry_translate_t *translate;

        node1 = head1->data;

        if (node1->op != CG_MATRIX_OP_TRANSLATE)
            return false;

        translate = (cg_matrix_entry_translate_t *)node1;

        *x = *x + translate->x;
        *y = *y + translate->y;
        *z = *z + translate->z;
    }

    return true;
}

bool
cg_matrix_entry_is_identity(cg_matrix_entry_t *entry)
{
    return entry ? entry->op == CG_MATRIX_OP_LOAD_IDENTITY : false;
}

bool
cg_matrix_entry_equal(cg_matrix_entry_t *entry0, cg_matrix_entry_t *entry1)
{
    for (; entry0 && entry1; entry0 = entry0->parent, entry1 = entry1->parent) {
        entry0 = _cg_matrix_entry_skip_saves(entry0);
        entry1 = _cg_matrix_entry_skip_saves(entry1);

        if (entry0 == entry1)
            return true;

        if (entry0->op != entry1->op)
            return false;

        switch (entry0->op) {
        case CG_MATRIX_OP_LOAD_IDENTITY:
            return true;
        case CG_MATRIX_OP_TRANSLATE: {
            cg_matrix_entry_translate_t *translate0 =
                (cg_matrix_entry_translate_t *)entry0;
            cg_matrix_entry_translate_t *translate1 =
                (cg_matrix_entry_translate_t *)entry1;
            /* We could perhaps use an epsilon to compare here?
             * I expect the false negatives are probaly never going to
             * be a problem and this is a bit cheaper. */
            if (translate0->x != translate1->x ||
                translate0->y != translate1->y ||
                translate0->z != translate1->z)
                return false;
        } break;
        case CG_MATRIX_OP_ROTATE: {
            cg_matrix_entry_rotate_t *rotate0 =
                (cg_matrix_entry_rotate_t *)entry0;
            cg_matrix_entry_rotate_t *rotate1 =
                (cg_matrix_entry_rotate_t *)entry1;
            if (rotate0->angle != rotate1->angle || rotate0->x != rotate1->x ||
                rotate0->y != rotate1->y || rotate0->z != rotate1->z)
                return false;
        } break;
        case CG_MATRIX_OP_ROTATE_QUATERNION: {
            cg_matrix_entry_rotate_quaternion_t *rotate0 =
                (cg_matrix_entry_rotate_quaternion_t *)entry0;
            cg_matrix_entry_rotate_quaternion_t *rotate1 =
                (cg_matrix_entry_rotate_quaternion_t *)entry1;
            int i;
            for (i = 0; i < 4; i++)
                if (rotate0->values[i] != rotate1->values[i])
                    return false;
        } break;
        case CG_MATRIX_OP_ROTATE_EULER: {
            cg_matrix_entry_rotate_euler_t *rotate0 =
                (cg_matrix_entry_rotate_euler_t *)entry0;
            cg_matrix_entry_rotate_euler_t *rotate1 =
                (cg_matrix_entry_rotate_euler_t *)entry1;

            if (rotate0->heading != rotate1->heading ||
                rotate0->pitch != rotate1->pitch ||
                rotate0->roll != rotate1->roll)
                return false;
        } break;
        case CG_MATRIX_OP_SCALE: {
            cg_matrix_entry_scale_t *scale0 = (cg_matrix_entry_scale_t *)entry0;
            cg_matrix_entry_scale_t *scale1 = (cg_matrix_entry_scale_t *)entry1;
            if (scale0->x != scale1->x || scale0->y != scale1->y ||
                scale0->z != scale1->z)
                return false;
        } break;
        case CG_MATRIX_OP_MULTIPLY: {
            cg_matrix_entry_multiply_t *mult0 =
                (cg_matrix_entry_multiply_t *)entry0;
            cg_matrix_entry_multiply_t *mult1 =
                (cg_matrix_entry_multiply_t *)entry1;
            if (!c_matrix_equal(mult0->matrix, mult1->matrix))
                return false;
        } break;
        case CG_MATRIX_OP_LOAD: {
            cg_matrix_entry_load_t *load0 = (cg_matrix_entry_load_t *)entry0;
            cg_matrix_entry_load_t *load1 = (cg_matrix_entry_load_t *)entry1;
            /* There's no need to check any further since an
             * _OP_LOAD makes all the ancestors redundant as far as
             * the final matrix value is concerned. */
            return c_matrix_equal(load0->matrix, load1->matrix);
        }
        case CG_MATRIX_OP_SAVE:
            /* We skip over saves above so we shouldn't see save entries */
            c_warn_if_reached();
        }
    }

    return false;
}

void
cg_debug_matrix_entry_print(cg_matrix_entry_t *entry)
{
    int depth;
    cg_matrix_entry_t *e;
    cg_matrix_entry_t **children;
    int i;

    for (depth = 0, e = entry; e; e = e->parent)
        depth++;

    children = c_alloca(sizeof(cg_matrix_entry_t) * depth);

    for (i = depth - 1, e = entry; i >= 0 && e; i--, e = e->parent) {
        children[i] = e;
    }

    c_print("MatrixEntry %p =\n", entry);

    for (i = 0; i < depth; i++) {
        entry = children[i];

        switch (entry->op) {
        case CG_MATRIX_OP_LOAD_IDENTITY:
            c_print("  LOAD IDENTITY\n");
            continue;
        case CG_MATRIX_OP_TRANSLATE: {
            cg_matrix_entry_translate_t *translate =
                (cg_matrix_entry_translate_t *)entry;
            c_print("  TRANSLATE X=%f Y=%f Z=%f\n",
                    translate->x,
                    translate->y,
                    translate->z);
            continue;
        }
        case CG_MATRIX_OP_ROTATE: {
            cg_matrix_entry_rotate_t *rotate =
                (cg_matrix_entry_rotate_t *)entry;
            c_print("  ROTATE ANGLE=%f X=%f Y=%f Z=%f\n",
                    rotate->angle,
                    rotate->x,
                    rotate->y,
                    rotate->z);
            continue;
        }
        case CG_MATRIX_OP_ROTATE_QUATERNION: {
            cg_matrix_entry_rotate_quaternion_t *rotate =
                (cg_matrix_entry_rotate_quaternion_t *)entry;
            c_print("  ROTATE QUATERNION w=%f x=%f y=%f z=%f\n",
                    rotate->values[0],
                    rotate->values[1],
                    rotate->values[2],
                    rotate->values[3]);
            continue;
        }
        case CG_MATRIX_OP_ROTATE_EULER: {
            cg_matrix_entry_rotate_euler_t *rotate =
                (cg_matrix_entry_rotate_euler_t *)entry;
            c_print("  ROTATE EULER heading=%f pitch=%f roll=%f\n",
                    rotate->heading,
                    rotate->pitch,
                    rotate->roll);
            continue;
        }
        case CG_MATRIX_OP_SCALE: {
            cg_matrix_entry_scale_t *scale = (cg_matrix_entry_scale_t *)entry;
            c_print("  SCALE X=%f Y=%f Z=%f\n", scale->x, scale->y, scale->z);
            continue;
        }
        case CG_MATRIX_OP_MULTIPLY: {
            cg_matrix_entry_multiply_t *mult =
                (cg_matrix_entry_multiply_t *)entry;
            c_print("  MULT:\n");
            c_matrix_prefix_print("    ", mult->matrix);
            continue;
        }
        case CG_MATRIX_OP_LOAD: {
            cg_matrix_entry_load_t *load = (cg_matrix_entry_load_t *)entry;
            c_print("  LOAD:\n");
            c_matrix_prefix_print("    ", load->matrix);
            continue;
        }
        case CG_MATRIX_OP_SAVE:
            c_print("  SAVE\n");
        }
    }
}

void
_cg_matrix_entry_cache_init(cg_matrix_entry_cache_t *cache)
{
    cache->entry = NULL;
    cache->flushed_identity = false;
}

/* NB: This function can report false negatives since it never does a
 * deep comparison of the stack matrices. */
bool
_cg_matrix_entry_cache_maybe_update(cg_matrix_entry_cache_t *cache,
                                    cg_matrix_entry_t *entry)
{
    bool is_identity;
    bool updated = false;

    is_identity = (entry->op == CG_MATRIX_OP_LOAD_IDENTITY);
    if (cache->flushed_identity != is_identity) {
        cache->flushed_identity = is_identity;
        updated = true;
    }

    if (cache->entry != entry) {
        cg_matrix_entry_ref(entry);
        if (cache->entry)
            cg_matrix_entry_unref(cache->entry);
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
_cg_matrix_entry_cache_destroy(cg_matrix_entry_cache_t *cache)
{
    if (cache->entry)
        cg_matrix_entry_unref(cache->entry);
}
