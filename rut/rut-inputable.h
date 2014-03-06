/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef __RUT_INPUTABLE_H__
#define __RUT_INPUTABLE_H__

#include <cogl/cogl.h>

#include "rut-object.h"

typedef RutInputEventStatus (*RutInputableCallback) (RutObject *inputable,
                                                     RutInputEvent *event);

typedef struct _RutInputableVTable
{
  RutInputableCallback handle_event;
} RutInputableVTable;

static inline RutInputEventStatus
rut_inputable_handle_event (RutObject *inputable,
                            RutInputEvent *event)
{
  RutInputableVTable *vtable =
    rut_object_get_vtable (inputable, RUT_TRAIT_ID_INPUTABLE);

  return vtable->handle_event (inputable, event);
}

#endif /* __RUT_INPUTABLE_H__ */
