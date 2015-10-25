/* Gstream_ter
 * Copyright (C) 2013 Intel Corporation.
 *
 * gstmemsrc.c:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:element-memsrc
 *
 * Read data from a buffer in memory
 *
 */

#include <rut-config.h>

#include <gst/gst.h>
#include "gstmemsrc.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <errno.h>
#include <string.h>

//#include "../../gst/gst-i18n-lib.h"
#define _(X) X

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(gst_mem_src_debug);
#define GST_CAT_DEFAULT gst_mem_src_debug

/* MemSrc signals and args */
enum {
    /* FILL ME */
    LAST_SIGNAL
};

#define DEFAULT_BLOCKSIZE 4 * 1024

enum {
    PROP_0,
    PROP_MEMORY,
    PROP_SIZE
};

static void gst_mem_src_finalize(GObject *object);

static void gst_mem_src_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec);
static void gst_mem_src_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec);

static gboolean gst_mem_src_start(GstBaseSrc *basesrc);
static gboolean gst_mem_src_stop(GstBaseSrc *basesrc);

static gboolean gst_mem_src_is_seekable(GstBaseSrc *src);
static gboolean gst_mem_src_do_seek(GstBaseSrc *bsrc, GstSegment *segment);
static gboolean gst_mem_src_get_size(GstBaseSrc *src, guint64 *size);
static GstFlowReturn gst_mem_src_create(GstPushSrc *psrc, GstBuffer **outbuf);

static void gst_mem_src_uri_handler_init(gpointer g_iface, gpointer iface_data);

#define _do_init                                                               \
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, gst_mem_src_uri_handler_init); \
    GST_DEBUG_CATEGORY_INIT(gst_mem_src_debug, "memsrc", 0, "memsrc element");
#define gst_mem_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(GstMemSrc, gst_mem_src, GST_TYPE_PUSH_SRC, _do_init);

static void
gst_mem_src_class_init(GstMemSrcClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSrcClass *gstbasesrc_class;
    GstPushSrcClass *gstpushsrc_class;

    gobject_class = G_OBJECT_CLASS(klass);
    gstelement_class = GST_ELEMENT_CLASS(klass);
    gstbasesrc_class = GST_BASE_SRC_CLASS(klass);
    gstpushsrc_class = GST_PUSH_SRC_CLASS(klass);

    gobject_class->set_property = gst_mem_src_set_property;
    gobject_class->get_property = gst_mem_src_get_property;

    g_object_class_install_property(
        gobject_class,
        PROP_MEMORY,
        g_param_spec_pointer("memory",
                             "Memory Address",
                             "Address of memory to read",
                             G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));
    g_object_class_install_property(
        gobject_class,
        PROP_MEMORY,
        g_param_spec_uint64("size",
                            "Memory Size",
                            "Length of memory region",
                            0,
                            G_MAXUINT64,
                            0,
                            G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

    gobject_class->finalize = gst_mem_src_finalize;

    gst_element_class_set_static_metadata(
        gstelement_class,
        "Memory Source",
        "Source/Memory",
        "Read from arbitrary point in memory",
        "Robert Bragg <robert@sixbynine.org>");
    gst_element_class_add_pad_template(
        gstelement_class, gst_static_pad_template_get(&srctemplate));

    gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_mem_src_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_mem_src_stop);
    gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR(gst_mem_src_is_seekable);
    gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR(gst_mem_src_get_size);
    gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR(gst_mem_src_do_seek);

    gstpushsrc_class->create = GST_DEBUG_FUNCPTR(gst_mem_src_create);
}

static void
gst_mem_src_init(GstMemSrc *src)
{
    src->memory = NULL;

    gst_base_src_set_blocksize(GST_BASE_SRC(src), DEFAULT_BLOCKSIZE);
}

static void
gst_mem_src_finalize(GObject *object)
{
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gboolean
gst_mem_src_set_memory(GstMemSrc *src, void *memory)
{
    GstState state;

    /* the element must be stopped in order to do this */
    GST_OBJECT_LOCK(src);
    state = GST_STATE(src);
    if (state != GST_STATE_READY && state != GST_STATE_NULL)
        goto wrong_state;
    GST_OBJECT_UNLOCK(src);

    if (src->memory == memory)
        return true;

    src->memory = memory;
    GST_INFO("memory : %p", src->memory);

    g_object_notify(G_OBJECT(src), "memory");

    return true;

/* ERROR */
wrong_state: {
        g_warning("Changing the `memory' property on memsrc while not in "
                  "the 'null' or 'ready' state is not supported");
        GST_OBJECT_UNLOCK(src);
        return false;
    }
}

static gboolean
gst_mem_src_set_size(GstMemSrc *src, guint64 size)
{
    GstState state;

    /* the element must be stopped in order to do this */
    GST_OBJECT_LOCK(src);
    state = GST_STATE(src);
    if (state != GST_STATE_READY && state != GST_STATE_NULL)
        goto wrong_state;
    GST_OBJECT_UNLOCK(src);

    if (src->size == size)
        return true;

    src->size = size;
    GST_INFO("size : %" G_GUINT64_FORMAT, src->size);

    g_object_notify(G_OBJECT(src), "size");

    return true;

/* ERROR */
wrong_state: {
        g_warning("Changing the `size' property on memsrc while not in "
                  "the 'null' or 'ready' state is not supported");
        GST_OBJECT_UNLOCK(src);
        return false;
    }
}

static gboolean
gst_mem_src_get_size(GstBaseSrc *basesrc, guint64 *size)
{
    GstMemSrc *src;

    src = GST_MEM_SRC(basesrc);

    if (!src->size)
        return false;

    *size = src->size;

    return true;
}

static void
gst_mem_src_set_property(GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
    GstMemSrc *src;

    g_return_if_fail(GST_IS_MEM_SRC(object));

    src = GST_MEM_SRC(object);

    switch (prop_id) {
    case PROP_MEMORY:
        gst_mem_src_set_memory(src, g_value_get_pointer(value));
        break;
    case PROP_SIZE:
        gst_mem_src_set_size(src, g_value_get_uint64(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_mem_src_get_property(GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
    GstMemSrc *src;

    g_return_if_fail(GST_IS_MEM_SRC(object));

    src = GST_MEM_SRC(object);

    switch (prop_id) {
    case PROP_MEMORY:
        g_value_set_pointer(value, src->memory);
        break;
    case PROP_SIZE:
        g_value_set_uint64(value, src->size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GstFlowReturn
gst_mem_src_create(GstPushSrc *psrc, GstBuffer **outbuf)
{
    GstMemSrc *src;
    guint blocksize;

    src = GST_MEM_SRC_CAST(psrc);

    if (src->offset >= src->size) {
        GST_DEBUG("EOS");
        return GST_FLOW_EOS;
    }

    blocksize = GST_BASE_SRC(src)->blocksize;
    if (src->offset + blocksize > src->size)
        blocksize = src->size - src->offset;

    *outbuf = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
                                          src->memory,
                                          src->size,
                                          src->offset,
                                          blocksize,
                                          NULL, /* user data */
                                          NULL); /* destroy notify */

    src->offset += blocksize;

    return GST_FLOW_OK;
}

static gboolean
gst_mem_src_is_seekable(GstBaseSrc *basesrc)
{
    return true;
}

static gboolean
gst_mem_src_do_seek(GstBaseSrc *bsrc, GstSegment *segment)
{
    gint64 offset;
    GstMemSrc *src = GST_MEM_SRC(bsrc);

    offset = segment->start;

    /* No need to seek to the current position */
    if (offset == src->offset)
        return true;

    segment->position = segment->start;
    segment->time = segment->start;

    src->offset = offset;

    return true;
}

/* open the file, necessary to go to READY state */
static gboolean
gst_mem_src_start(GstBaseSrc *basesrc)
{
    GstMemSrc *src = GST_MEM_SRC(basesrc);

    if (src->memory == NULL || src->size == 0)
        goto no_memory;

    src->offset = 0;

    gst_base_src_set_dynamic_size(basesrc, true);

    return true;

/* ERROR */
no_memory:
    GST_ELEMENT_ERROR(src,
                      RESOURCE,
                      NOT_FOUND,
                      (_("No memory pointer with a given size has been "
                         "specified for reading.")),
                      (NULL));
    return false;
}

static gboolean
gst_mem_src_stop(GstBaseSrc *basesrc)
{
    return true;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_mem_src_uri_get_type(GType type)
{
    return GST_URI_SRC;
}

static const gchar *const *
gst_mem_src_uri_get_protocols(GType type)
{
    static const gchar *protocols[] = { "mem", NULL };

    return protocols;
}

static gchar *
gst_mem_src_uri_get_uri(GstURIHandler *handler)
{
    GstMemSrc *src = GST_MEM_SRC(handler);

    /* FIXME: make thread-safe */
    return g_strdup(src->uri);
}

static gboolean
gst_mem_src_uri_set_uri(GstURIHandler *handler, const gchar *uri, GError **err)
{
    GstMemSrc *src = GST_MEM_SRC(handler);
    gchar *protocol;
    void *memory;
    unsigned long size;

    GST_INFO_OBJECT(src, "checking uri %s", uri);

    protocol = gst_uri_get_protocol(uri);
    if (strcmp(protocol, "mem") != 0) {
        g_free(protocol);
        return false;
    }
    g_free(protocol);

    if (strcmp(uri, "mem://") == 0) {
        /* Special case for "mem://" as this is used by some applications
         * to setup a GstPlayBin with a suitable source element which
         * can be configured during the "source-setup" signal.
         */
        gst_mem_src_set_memory(src, NULL);
        gst_mem_src_set_size(src, 0);
        return true;
    }

    if (sscanf(uri, "mem://%p:%lu", &memory, &size) != 2)
        return false;

    gst_mem_src_set_memory(src, memory);
    gst_mem_src_set_size(src, size);

    return true;
}

static void
gst_mem_src_uri_handler_init(gpointer g_iface, gpointer iface_data)
{
    GstURIHandlerInterface *iface = (GstURIHandlerInterface *)g_iface;

    iface->get_type = gst_mem_src_uri_get_type;
    iface->get_protocols = gst_mem_src_uri_get_protocols;
    iface->get_uri = gst_mem_src_uri_get_uri;
    iface->set_uri = gst_mem_src_uri_set_uri;
}
