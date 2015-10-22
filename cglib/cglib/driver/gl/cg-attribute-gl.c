/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011,2012 Intel Corporation.
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
 *
 *
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <string.h>

#include "cg-private.h"
#include "cg-util-gl-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-error-private.h"
#include "cg-device-private.h"
#include "cg-attribute.h"
#include "cg-attribute-private.h"
#include "cg-attribute-gl-private.h"
#include "cg-pipeline-progend-glsl-private.h"
#include "cg-buffer-gl-private.h"

typedef struct _foreach_changed_bit_state_t {
    cg_device_t *dev;
    const CGlibBitmask *new_bits;
    cg_pipeline_t *pipeline;
} foreach_changed_bit_state_t;

static bool
toggle_custom_attribute_enabled_cb(int bit_num, void *user_data)
{
    foreach_changed_bit_state_t *state = user_data;
    bool enabled = _cg_bitmask_get(state->new_bits, bit_num);
    cg_device_t *dev = state->dev;

    if (enabled)
        GE(dev, glEnableVertexAttribArray(bit_num));
    else
        GE(dev, glDisableVertexAttribArray(bit_num));

    return true;
}

static void
foreach_changed_bit_and_save(cg_device_t *dev,
                             CGlibBitmask *current_bits,
                             const CGlibBitmask *new_bits,
                             cg_bitmask_foreach_func_t callback,
                             foreach_changed_bit_state_t *state)
{
    /* Get the list of bits that are different */
    _cg_bitmask_clear_all(&dev->changed_bits_tmp);
    _cg_bitmask_set_bits(&dev->changed_bits_tmp, current_bits);
    _cg_bitmask_xor_bits(&dev->changed_bits_tmp, new_bits);

    /* Iterate over each bit to change */
    state->new_bits = new_bits;
    _cg_bitmask_foreach(&dev->changed_bits_tmp, callback, state);

    /* Store the new values */
    _cg_bitmask_clear_all(current_bits);
    _cg_bitmask_set_bits(current_bits, new_bits);
}

static void
setup_generic_buffered_attribute(cg_device_t *dev,
                                 cg_pipeline_t *pipeline,
                                 cg_attribute_t *attribute,
                                 uint8_t *base)
{
    int name_index = attribute->name_state->name_index;
    int attrib_location =
        _cg_pipeline_progend_glsl_get_attrib_location(dev, pipeline,
                                                      name_index);

    if (attrib_location == -1)
        return;

    GE(dev,
       glVertexAttribPointer(attrib_location,
                             attribute->d.buffered.n_components,
                             attribute->d.buffered.type,
                             attribute->normalized,
                             attribute->d.buffered.stride,
                             base + attribute->d.buffered.offset));

    if (attribute->instance_stride) {
        GE(dev,
           glVertexAttribDivisor(attrib_location, attribute->instance_stride));
    }

    _cg_bitmask_set(
        &dev->enable_custom_attributes_tmp, attrib_location, true);
}

static void
setup_generic_const_attribute(cg_device_t *dev,
                              cg_pipeline_t *pipeline,
                              cg_attribute_t *attribute)
{
    int name_index = attribute->name_state->name_index;
    int attrib_location =
        _cg_pipeline_progend_glsl_get_attrib_location(dev, pipeline,
                                                      name_index);
    int columns;
    int i;

    if (attrib_location == -1)
        return;

    if (attribute->d.constant.boxed.type == CG_BOXED_MATRIX)
        columns = attribute->d.constant.boxed.size;
    else
        columns = 1;

    /* Note: it's ok to access a CG_BOXED_FLOAT as a matrix with only
     * one column... */

    switch (attribute->d.constant.boxed.size) {
    case 1:
        GE(dev,
           glVertexAttrib1fv(attrib_location,
                             attribute->d.constant.boxed.v.matrix));
        break;
    case 2:
        for (i = 0; i < columns; i++)
            GE(dev,
               glVertexAttrib2fv(attrib_location + i,
                                 attribute->d.constant.boxed.v.matrix));
        break;
    case 3:
        for (i = 0; i < columns; i++)
            GE(dev,
               glVertexAttrib3fv(attrib_location + i,
                                 attribute->d.constant.boxed.v.matrix));
        break;
    case 4:
        for (i = 0; i < columns; i++)
            GE(dev,
               glVertexAttrib4fv(attrib_location + i,
                                 attribute->d.constant.boxed.v.matrix));
        break;
    default:
        c_warn_if_reached();
    }
}

static void
apply_attribute_enable_updates(cg_device_t *dev,
                               cg_pipeline_t *pipeline)
{
    foreach_changed_bit_state_t changed_bits_state;

    changed_bits_state.dev = dev;
    changed_bits_state.pipeline = pipeline;

    changed_bits_state.new_bits = &dev->enable_custom_attributes_tmp;
    foreach_changed_bit_and_save(dev,
                                 &dev->enabled_custom_attributes,
                                 &dev->enable_custom_attributes_tmp,
                                 toggle_custom_attribute_enabled_cb,
                                 &changed_bits_state);
}

void
_cg_gl_flush_attributes_state(cg_framebuffer_t *framebuffer,
                              cg_pipeline_t *pipeline,
                              cg_flush_layer_state_t *layers_state,
                              cg_draw_flags_t flags,
                              cg_attribute_t **attributes,
                              int n_attributes)
{
    cg_device_t *dev = framebuffer->dev;
    int i;
    bool with_color_attrib = false;
    bool unknown_color_alpha = false;
    cg_pipeline_t *copy = NULL;

    /* Iterate the attributes to see if we have a color attribute which
     * may affect our decision to enable blending or not.
     *
     * We need to do this before flushing the pipeline. */
    for (i = 0; i < n_attributes; i++)
        switch (attributes[i]->name_state->name_id) {
        case CG_ATTRIBUTE_NAME_ID_COLOR_ARRAY:
            if (_cg_attribute_get_n_components(attributes[i]) == 4)
                unknown_color_alpha = true;
            with_color_attrib = true;
            break;

        default:
            break;
        }

    if (C_UNLIKELY(layers_state->options.flags)) {
        /* If we haven't already created a derived pipeline... */
        if (!copy) {
            copy = cg_pipeline_copy(pipeline);
            pipeline = copy;
        }
        _cg_pipeline_apply_overrides(pipeline, &layers_state->options);
    }

    _cg_pipeline_flush_gl_state(dev, pipeline, framebuffer,
                                with_color_attrib, unknown_color_alpha);

    _cg_bitmask_clear_all(&dev->enable_custom_attributes_tmp);

    /* Bind the attribute pointers. We need to do this after the
     * pipeline is flushed because when using GLSL that is the only
     * point when we can determine the attribute locations */

    for (i = 0; i < n_attributes; i++) {
        cg_attribute_t *attribute = attributes[i];
        cg_attribute_buffer_t *attribute_buffer;
        cg_buffer_t *buffer;
        uint8_t *base;

        if (attribute->is_buffered) {
            attribute_buffer = cg_attribute_get_buffer(attribute);
            buffer = CG_BUFFER(attribute_buffer);

            /* Note: we don't try and catch errors with binding buffers
             * here since OOM errors at this point indicate that nothing
             * has yet been uploaded to attribute buffer which we
             * consider to be a programmer error.
             */
            base = _cg_buffer_gl_bind(
                buffer, CG_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER, NULL);

            setup_generic_buffered_attribute(dev, pipeline, attribute, base);

            _cg_buffer_gl_unbind(buffer);
        } else
            setup_generic_const_attribute(dev, pipeline, attribute);
    }

    apply_attribute_enable_updates(dev, pipeline);

    if (copy)
        cg_object_unref(copy);
}
