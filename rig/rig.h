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

#ifndef _RIG_H_
#define _RIG_H_

#include "rig-split-view.h"
#include "rig-node.h"
#include "rig-text-renderer.h"
#include "rig-pb-debug.h"
#include "rig-image-source.h"
#include "rig-emscripten-lib.h"
#include "rig-defines.h"
#include "rig-engine-op.h"
#include "rig-asset.h"
#include "rig-pb.h"
#include "rut-renderer.h"
#include "rig-code-module.h"
#include "rig-logs.h"
#include "rig-slave-address.h"
#include "rig-path.h"
#include "rig-slave.h"
#include "rig-binding.h"
#include "usc_impl.h"
#include "rig-controller.h"
#include "rig-engine.h"
#include "rig-view.h"
#include "rig-renderer.h"
#include "rig-frontend.h"
#include "rig-downsampler.h"
#include "rig-rpc-network.h"
#include "rig-text-engine-private.h"
#include "rig-entity.h"
#include "rig-camera-view.h"
#include "rig-ui.h"
#include "rig-types.h"
#include "rig-code.h"
#include "rig-dof-effect.h"
#include "rig-load-save.h"
#include "rig-text-engine-funcs.h"
#include "rig-text-pipeline-cache.h"
#include "rig-text-engine.h"

#ifdef RIG_EDITOR_ENABLED
#include "rig-editor.h"
#include "rig-rotation-tool.h"
#include "rig-selection-tool.h"
#include "rig-undo-journal.h"
#include "rig-inspector.h"
#include "rig-prop-inspector.h"
#include "rig-asset-inspector.h"
#include "rig-binding-view.h"
#include "rig-controller-view.h"
#include "rig-simulator.h"
#include "rig-application.h"
#include "rig-slave-master.h"
#include "rig.pb.h"
#endif

#ifdef USE_AVAHI
#include "rig-avahi.h"
#endif

#ifdef USE_NCURSES
#include "rig-curses-debug.h"
#endif

#include "rig.pb-c.h"

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"
#include "protobuf-c-rpc/rig-protobuf-c-stream.h"

#include "components/rig-material.h"

#ifdef USE_UV
#include "components/rig-native-module.h"
#endif

#include "components/rig-button-input.h"
#include "components/rig-pointalism-grid.h"
#include "components/rig-nine-slice.h"
#include "components/rig-shape.h"
#include "components/rig-camera.h"
#include "components/rig-light.h"
#include "components/rig-text.h"
#include "components/rig-hair.h"
#include "components/rig-diamond.h"
#include "components/rig-model.h"

#endif
