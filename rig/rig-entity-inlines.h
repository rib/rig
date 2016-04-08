/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#pragma once

#include <rut.h>

#include "rig-engine.h"

static inline rig_property_context_t *
rig_entity_get_property_context(rig_entity_t *entity)
{
    return entity->engine->property_ctx;
}

static inline rig_engine_t *
rig_entity_get_engine(rig_entity_t *entity)
{
    return entity->engine;
}

static inline rut_shell_t *
rig_entity_get_shell(rig_entity_t *entity)
{
    return entity->engine->shell;
}

static inline rig_property_context_t *
rig_component_props_get_property_context(rut_componentable_props_t *component)
{
    if (component->parented)
        return component->entity->engine->property_ctx;
    else
        return component->engine->property_ctx;
}

static inline rut_shell_t *
rig_component_props_get_shell(rut_componentable_props_t *component)
{
    if (component->parented)
        return component->entity->engine->shell;
    else
        return component->engine->shell;
}

static inline rig_engine_t *
rig_component_props_get_engine(rut_componentable_props_t *component)
{
    if (component->parented)
        return component->entity->engine;
    else
        return component->engine;
}


