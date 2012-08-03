/*
 * Rig
 *
 * A tiny toolkit
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include "rig-ease.h"

const RigUIEnum
rig_ease_mode_enum =
  {
    "EaseMode",
    "Ease mode",
    {
      { RIG_EASE_MODE_LINEAR, "Linear", "Linear" },
      { RIG_EASE_MODE_IN_QUAD, "in-quad", "In Quad" },
      { RIG_EASE_MODE_OUT_QUAD, "out-quad", "Out Quad" },
      { RIG_EASE_MODE_IN_OUT_QUAD, "in-out-quad", "In Out Quad" },
      { RIG_EASE_MODE_IN_CUBIC, "in-cubic", "In Cubic" },
      { RIG_EASE_MODE_OUT_CUBIC, "out-cubic", "Out Cubic" },
      { RIG_EASE_MODE_IN_OUT_CUBIC, "in-out-cubic", "In Out Cubic" },
      { RIG_EASE_MODE_IN_QUART, "in-quart", "In Quart" },
      { RIG_EASE_MODE_OUT_QUART, "out-quart", "Out Quart" },
      { RIG_EASE_MODE_IN_OUT_QUART, "in-out-quart", "In Out Quart" },
      { RIG_EASE_MODE_IN_QUINT, "in-quint", "In Quint" },
      { RIG_EASE_MODE_OUT_QUINT, "out-quint", "Out Quint" },
      { RIG_EASE_MODE_IN_OUT_QUINT, "in-out-quint", "In Out Quint" },
      { RIG_EASE_MODE_IN_SINE, "in-sine", "In Sine" },
      { RIG_EASE_MODE_OUT_SINE, "out-sine", "Out Sine" },
      { RIG_EASE_MODE_IN_OUT_SINE, "in-out-sine", "In Out Sine" },
      { RIG_EASE_MODE_IN_EXPO, "in-expo", "In Expo" },
      { RIG_EASE_MODE_OUT_EXPO, "out-expo", "Out Expo" },
      { RIG_EASE_MODE_IN_OUT_EXPO, "in-out-expo", "In Out Expo" },
      { RIG_EASE_MODE_IN_CIRC, "in-circ", "In Circ" },
      { RIG_EASE_MODE_OUT_CIRC, "out-circ", "Out Circ" },
      { RIG_EASE_MODE_IN_OUT_CIRC, "in-out-circ", "In Out Circ" },
      { RIG_EASE_MODE_IN_ELASTIC, "in-elastic", "In Elastic" },
      { RIG_EASE_MODE_OUT_ELASTIC, "out-elastic", "Out Elastic" },
      { RIG_EASE_MODE_IN_OUT_ELASTIC, "in-out-elastic", "In Out Elastic" },
      { RIG_EASE_MODE_IN_BACK, "in-back", "In Back" },
      { RIG_EASE_MODE_OUT_BACK, "out-back", "Out Back" },
      { RIG_EASE_MODE_IN_OUT_BACK, "in-out-back", "In Out Back" },
      { RIG_EASE_MODE_IN_BOUNCE, "in-bounce", "In Bounce" },
      { RIG_EASE_MODE_OUT_BOUNCE, "out-bounce", "Out Bounce" },
      { RIG_EASE_MODE_IN_OUT_BOUNCE, "in-out-bounce", "In Out Bounce" },
      { 0, NULL, NULL }
    }
  };
