/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation
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

#ifndef _RIG_TEXT_RENDERER_H_
#define _RIG_TEXT_RENDERER_H_

#include "rig-renderer.h"

#include "components/rig-text.h"

typedef struct _rig_text_renderer_state_t rig_text_renderer_state_t;

rig_text_renderer_state_t *rig_text_renderer_state_new(rig_frontend_t *frontend);

void rig_text_renderer_state_destroy(rig_text_renderer_state_t *text_state);

void rig_text_renderer_draw(rig_paint_context_t *paint_ctx,
                            rig_text_renderer_state_t *text_state,
                            rig_text_t *text);

#endif /* _RIG_TEXT_RENDERER_H_ */
