/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#ifndef _RUT_RENDERER_H_
#define _RUT_RENDERER_H_

#include <rut.h>

#include "rig-entity.h"

/* TODO: Rename this api to use the rig namespace (we currently
 * haven't done that because we already use the rig_renderer_t namespace
 * for our implementation of this interface.) Maybe rename the
 * implementation to RigForwardRenderer.
 */

/*
 * Renderer interface
 *
 * An interface for something to act as the renderer of a scenegraph
 * of entities.
 */
typedef struct _rut_renderer_vtable_t {
    void (*notify_entity_changed)(rig_entity_t *entity);
    void (*free_priv)(rig_entity_t *entity);
} rut_renderer_vtable_t;

void rut_renderer_notify_entity_changed(rut_object_t *renderer,
                                        rig_entity_t *entity);

void rut_renderer_free_priv(rut_object_t *renderer, rig_entity_t *entity);

#endif /* _RUT_RENDERER_H_ */
