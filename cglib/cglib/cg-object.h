/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010 Intel Corporation.
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
 *
 */

#ifndef __CG_OBJECT_H
#define __CG_OBJECT_H

#include <cglib/cg-types.h>
#ifdef CG_HAS_GTYPE_SUPPORT
#include <glib.h>
#endif

CG_BEGIN_DECLS

typedef struct _cg_object_t cg_object_t;

#define CG_OBJECT(X) ((cg_object_t *)X)

/**
 * cg_object_ref: (skip)
 * @object: a #cg_object_t
 *
 * Increases the reference count of @object by 1
 *
 * Returns: the @object, with its reference count increased
 */
void *cg_object_ref(void *object);

/**
 * cg_object_unref: (skip)
 * @object: a #cg_object_t
 *
 * Drecreases the reference count of @object by 1; if the reference
 * count reaches 0, the resources allocated by @object will be freed
 */
void cg_object_unref(void *object);

/**
 * cg_user_data_key_t:
 * @unused: ignored.
 *
 * A #cg_user_data_key_t is used to declare a key for attaching data to a
 * #cg_object_t using cg_object_set_user_data. The typedef only exists as a
 * formality to make code self documenting since only the unique address of a
 * #cg_user_data_key_t is used.
 *
 * Typically you would declare a static #cg_user_data_key_t and set private data
 * on an object something like this:
 *
 * |[
 * static cg_user_data_key_t path_private_key;
 *
 * static void
 * destroy_path_private_cb (void *data)
 * {
 *   c_free (data);
 * }
 *
 * static void
 * my_path_set_data (CGlibPath *path, void *data)
 * {
 *   cg_object_set_user_data (CG_OBJECT (path),
 *                              &private_key,
 *                              data,
 *                              destroy_path_private_cb);
 * }
 * ]|
 *
 */
typedef struct {
    int unused;
} cg_user_data_key_t;

/**
 * cg_user_data_destroy_callback_t:
 * @user_data: The data whos association with a #cg_object_t has been
 *             destoyed.
 *
 * When associating private data with a #cg_object_t a callback can be
 * given which will be called either if the object is destroyed or if
 * cg_object_set_user_data() is called with NULL user_data for the
 * same key.
 *
 */
#ifdef CG_HAS_GTYPE_SUPPORT
typedef GDestroyNotify cg_user_data_destroy_callback_t;
#else
typedef void (*cg_user_data_destroy_callback_t)(void *user_data);
#endif

/**
 * cg_debug_object_type_info_t:
 * @name: A human readable name for the type.
 * @instance_count: The number of objects of this type that are
 *   currently in use
 *
 * This struct is used to pass information to the callback when
 * cg_debug_object_foreach_type() is called.
 *
 * Stability: unstable
 */
typedef struct {
    const char *name;
    unsigned int instance_count;
} cg_debug_object_type_info_t;

/**
 * CGlibDebugObjectForeachTypeCallback:
 * @info: A pointer to a struct containing information about the type.
 *
 * A callback function to use for cg_debug_object_foreach_type().
 *
 * Stability: unstable
 */
typedef void (*CGlibDebugObjectForeachTypeCallback)(
    const cg_debug_object_type_info_t *info, void *user_data);

/**
 * cg_object_set_user_data: (skip)
 * @object: The object to associate private data with
 * @key: The address of a #cg_user_data_key_t which provides a unique value
 *   with which to index the private data.
 * @user_data: The data to associate with the given object,
 *   or %NULL to remove a previous association.
 * @destroy: A #cg_user_data_destroy_callback_t to call if the object is
 *   destroyed or if the association is removed by later setting
 *   %NULL data for the same key.
 *
 * Associates some private @user_data with a given #cg_object_t. To
 * later remove the association call cg_object_set_user_data() with
 * the same @key but NULL for the @user_data.
 *
 */
void cg_object_set_user_data(cg_object_t *object,
                             cg_user_data_key_t *key,
                             void *user_data,
                             cg_user_data_destroy_callback_t destroy);

/**
 * cg_object_get_user_data: (skip)
 * @object: The object with associated private data to query
 * @key: The address of a #cg_user_data_key_t which provides a unique value
 *       with which to index the private data.
 *
 * Finds the user data previously associated with @object using
 * the given @key. If no user data has been associated with @object
 * for the given @key this function returns NULL.
 *
 * Returns: (transfer none): The user data previously associated
 *   with @object using the given @key; or %NULL if no associated
 *   data is found.
 *
 */
void *cg_object_get_user_data(cg_object_t *object, cg_user_data_key_t *key);

/**
 * cg_debug_object_foreach_type:
 * @func: (scope call): A callback function for each type
 * @user_data: (closure): A pointer to pass to @func
 *
 * Invokes @func once for each type of object that CGlib uses and
 * passes a count of the number of objects for that type. This is
 * intended to be used solely for debugging purposes to track down
 * issues with objects leaking.
 *
 * Stability: unstable
 */
void cg_debug_object_foreach_type(CGlibDebugObjectForeachTypeCallback func,
                                  void *user_data);

/**
 * cg_debug_object_print_instances:
 *
 * Prints a list of all the object types that CGlib uses along with the
 * number of objects of that type that are currently in use. This is
 * intended to be used solely for debugging purposes to track down
 * issues with objects leaking.
 *
 * Stability: unstable
 */
void cg_debug_object_print_instances(void);

CG_END_DECLS

#endif /* __CG_OBJECT_H */
