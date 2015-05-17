#include "rig-c.h"

void
load(RModule *module)
{
    r_debug(module, "load\n");
}

void
update(RModule *module)
{
    r_debug(module, "update\n");
}

void
input(RModule *module, RInputEvent *event)
{
    switch(r_input_event_get_type(event)) {
    case R_INPUT_EVENT_TYPE_MOTION:
        r_debug(module, "editor: motion event");
        break;
    case R_INPUT_EVENT_TYPE_KEY:
        r_debug(module, "editor: key event");
        break;
    case R_INPUT_EVENT_TYPE_TEXT:
        r_debug(module, "editor: text event");
        break;
    }
}
