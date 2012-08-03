#ifndef _RIG_EASE_H_
#define _RIG_EASE_H_

#include "rig-types.h"
#include "rig-type.h"

typedef enum _RigEaseMode
{
  RIG_EASE_MODE_LINEAR,

  /* quadratic */
  RIG_EASE_MODE_IN_QUAD,
  RIG_EASE_MODE_OUT_QUAD,
  RIG_EASE_MODE_IN_OUT_QUAD,

  /* cubic */
  RIG_EASE_MODE_IN_CUBIC,
  RIG_EASE_MODE_OUT_CUBIC,
  RIG_EASE_MODE_IN_OUT_CUBIC,

  /* quartic */
  RIG_EASE_MODE_IN_QUART,
  RIG_EASE_MODE_OUT_QUART,
  RIG_EASE_MODE_IN_OUT_QUART,

  /* quintic */
  RIG_EASE_MODE_IN_QUINT,
  RIG_EASE_MODE_OUT_QUINT,
  RIG_EASE_MODE_IN_OUT_QUINT,

  /* sinusoidal */
  RIG_EASE_MODE_IN_SINE,
  RIG_EASE_MODE_OUT_SINE,
  RIG_EASE_MODE_IN_OUT_SINE,

  /* exponential */
  RIG_EASE_MODE_IN_EXPO,
  RIG_EASE_MODE_OUT_EXPO,
  RIG_EASE_MODE_IN_OUT_EXPO,

  /* circular */
  RIG_EASE_MODE_IN_CIRC,
  RIG_EASE_MODE_OUT_CIRC,
  RIG_EASE_MODE_IN_OUT_CIRC,

  /* elastic */
  RIG_EASE_MODE_IN_ELASTIC,
  RIG_EASE_MODE_OUT_ELASTIC,
  RIG_EASE_MODE_IN_OUT_ELASTIC,

  /* overshooting cubic */
  RIG_EASE_MODE_IN_BACK,
  RIG_EASE_MODE_OUT_BACK,
  RIG_EASE_MODE_IN_OUT_BACK,

  /* exponentially decaying parabolic */
  RIG_EASE_MODE_IN_BOUNCE,
  RIG_EASE_MODE_OUT_BOUNCE,
  RIG_EASE_MODE_IN_OUT_BOUNCE,
} RigEaseMode;

extern const RigUIEnum rig_ease_mode_enum;

#endif /* _RIG_EASE_H_ */
