#ifndef _RIG_H_
#define _RIG_H_

#include <cogl/cogl.h>

#include <glib.h>

#ifndef __ANDROID__
#define USE_SDL
//#define USE_GLIB
#endif

#include "rig-global.h"
#include "rig-type.h"
#include "rig-object.h"
#include "rig-property.h"
#include "rig-interfaces.h"
#include "rig-context.h"
#include "rig-shell.h"
#include "rig-bitmask.h"
#include "rig-stack.h"
#include "rig-timeline.h"
#include "rig-display-list.h"
#include "rig-arcball.h"
#include "rig-util.h"
#include "rig-text-buffer.h"
#include "rig-text.h"
#include "rig-tool.h"
#include "rig-geometry.h"

/* entity/components system */
#include "rig-entity.h"
#include "rig-components.h"

#endif /* _RIG_H_ */
