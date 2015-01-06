/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015  Intel Corporation
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

extern "C" {
#include "rig-js.h"
}

#include "jsapi.h"

using namespace JS;

struct _rig_js_runtime {
    rut_object_base_t _base;

    JSRuntime *runtime;
};

static void
_rig_js_runtime_free(void *object)
{
    rig_js_runtime_t *js_runtime = static_cast<rig_js_runtime_t *>(object);
    rut_object_free(rig_js_runtime_t, js_runtime);
}

rut_type_t rig_js_runtime_type;

static void
_rig_js_runtime_init_type(void)
{
    rut_type_init(&rig_js_runtime_type, "rig_js_runtime_t", _rig_js_runtime_free);
}

static void
js_error_callback(JSContext *cx, const char *message, JSErrorReport *report)
{
     c_warning("%s:%u:%s\n",
               report->filename ? report->filename : "[no filename]",
               (unsigned int) report->lineno,
               message);
}

extern "C" {

rig_js_runtime_t *
rig_js_runtime_new(rig_simulator_t *simulator)
{
    rig_js_runtime_t *runtime = rut_object_alloc0(
        rig_js_runtime_t, &rig_js_runtime_type, _rig_js_runtime_init_type);

#if 0
    JS_Init();

    js->runtime = JS_NewRuntime(8 * 1024 * 1024);
    js->ctx = JS_NewContext(js->runtime, 8192);

    JS_SetErrorReporter(js->runtime, js_error_callback);
#endif

    return runtime;
}

} /* extern C */
