#ifndef _RIG_CLOSURE_LIST_H_
#define _RIG_CLOSURE_LIST_H_

#include "rig-list.h"

/*
 * This implements a list of callbacks that can be used like signals
 * in GObject, but that don't have any marshalling overhead. The idea
 * is that any object that wants to provide a callback point will
 * provide a function to add a callback for that particular point. The
 * function can take a function pointer with the correct signature.
 * Internally the function will just call rig_closure_list_add. The
 * function should directly return a RigClosure pointer. The caller
 * can use this to disconnect the callback later without the object
 * having to provide a separate disconnect function.
 */

typedef void (* RigClosureDestroyCallback) (void *user_data);

typedef struct
{
  RigList list_node;

  void *function;
  void *user_data;
  RigClosureDestroyCallback destroy_cb;
} RigClosure;

/**
 * rig_closure_disconnect:
 * @closure: A closure connected to a Rig closure list
 *
 * Removes the given closure from the callback list it is connected to
 * and destroys it. If the closure was created with a destroy function
 * then it will be invoked. */
void
rig_closure_disconnect (RigClosure *closure);

void
rig_closure_list_disconnect_all (RigList *list);

RigClosure *
rig_closure_list_add (RigList *list,
                      void *function,
                      void *user_data,
                      RigClosureDestroyCallback destroy_cb);

/**
 * rig_closure_list_invoke:
 * @list: A pointer to a RigList containing RigClosures
 * @cb_type: The name of a typedef for the closure callback function signature
 * @...: The the arguments to pass to the callback
 *
 * A convenience macro to invoke a closure list.
 *
 * Note that the arguments will be evaluated multiple times so it is
 * not safe to pass expressions that have side-effects.
 *
 * Note also that this function ignores the return value from the
 * callbacks. If you want to handle the return value you should
 * manually iterate the list and invoke the callbacks yourself.
 */
#define rig_closure_list_invoke(list, cb_type, ...)             \
  G_STMT_START {                                                \
    RigClosure *_c, *_tmp;                                      \
                                                                \
    rig_list_for_each_safe (_c, _tmp, (list), list_node)        \
      {                                                         \
        cb_type _cb = _c->function;                             \
        _cb (__VA_ARGS__, _c->user_data);                       \
      }                                                         \
  } G_STMT_END

#endif /* _RIG_CLOSURE_LIST_H_ */
