/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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


#ifndef _RUT_ENTRY_H_
#define _RUT_ENTRY_H_

#include "rut-object.h"
#include "rut-icon.h"

extern RutType rut_entry_type;
typedef struct _RutEntry RutEntry;

RutEntry *
rut_entry_new (RutContext *context);

void
rut_entry_set_size (RutObject *entry,
                    float width,
                    float height);

void
rut_entry_set_width (RutObject *entry,
                     float width);

void
rut_entry_set_height (RutObject *entry,
                      float height);

void
rut_entry_get_size (RutObject *entry,
                    float *width,
                    float *height);

RutText *
rut_entry_get_text (RutEntry *entry);

void
rut_entry_set_icon (RutEntry *entry,
                    RutIcon *icon);

#endif /* _RUT_ENTRY_H_ */
