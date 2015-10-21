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

#ifndef _RUT_H_
#define _RUT_H_

#ifndef RIG_SIMULATOR_ONLY
#include <cglib/cglib.h>
#endif

#include <clib.h>

#include "rut-type.h"
#include "rut-exception.h"
#include "rut-object.h"
#include "rut-property.h"
#include "rut-interfaces.h"
#include "rut-introspectable.h"
#include "rut-graphable.h"
#include "rut-camera.h"
#include "rut-matrix-stack.h"
#include "rut-meshable.h"
#include "rut-inputable.h"
#include "rut-shell.h"
#include "rut-shell.h"
#include "rut-os.h"
#include "rut-bitmask.h"
#include "rut-memory-stack.h"
#include "rut-magazine.h"
#include "rut-graph.h"
#include "rut-arcball.h"
#include "rut-util.h"
#include "rut-geometry.h"
#include "rut-paintable.h"
#include "rut-color.h"
#include "rut-gaussian-blurrer.h"
#include "rut-mesh.h"
#include "rut-mesh-ply.h"
#include "rut-mimable.h"
#include "rut-poll.h"
#include "rut-texture-cache.h"
#include "rut-settings.h"
#include "rut-closure.h"
#include "rut-gradient.h"
#include "rut-input-region.h"


#ifdef RUT_EDITOR_ENABLED
#include "rut-pickable.h"
#include "rut-stack.h"
#include "rut-transform.h"
#include "rut-rectangle.h"
#include "rut-ui-viewport.h"
#include "rut-nine-slice.h"
#include "rut-image.h"
#include "rut-box-layout.h"
#include "rut-bin.h"
#include "rut-fixed.h"
#include "rut-composite-sizable.h"
#include "rut-shim.h"

#include "rut-text-buffer.h"
#include "rut-text.h"
#include "rut-number-slider.h"
#include "rut-vec3-slider.h"
#include "rut-drop-down.h"
#include "rut-entry.h"
#include "rut-toggle.h"
#include "rut-color-picker.h"
#include "rut-color-button.h"
#include "rut-icon.h"
#include "rut-flow-layout.h"
#include "rut-fold.h"
#include "rut-button.h"
#include "rut-slider.h"
#include "rut-scale.h"
#include "rut-icon-button.h"
#include "rut-drag-bin.h"
#include "rut-icon-toggle.h"
#include "rut-icon-toggle-set.h"
#include "rut-rotation-inspector.h"
#include "rut-adb.h"
#endif

#endif /* _RUT_H_ */
