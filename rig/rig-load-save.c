/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#include <rig-config.h>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "rig.pb-c.h"
#include "rig-engine.h"
#include "rig-pb.h"

typedef struct _buffered_file_t {
    ProtobufCBuffer base;
    FILE *fp;
    bool error;
} buffered_file_t;

static void
append_to_file(ProtobufCBuffer *buffer,
               unsigned len,
               const unsigned char *engine)
{
    buffered_file_t *buffered_file = (buffered_file_t *)buffer;

    if (buffered_file->error)
        return;

    if (fwrite(engine, len, 1, buffered_file->fp) != 1)
        buffered_file->error = true;
}

void
rig_save(rig_engine_t *engine, rig_ui_t *ui, const char *path)
{
    rig_pb_serializer_t *serializer;
    char *dir;
    struct stat sb;
    Rig__UI *pb_ui;
    FILE *fp;

    buffered_file_t buffered_file = { { append_to_file },
                                      NULL, /* file pointer */
                                      false };

    fp = fopen(path, "w");
    if (!fp) {
        c_warning("Failed to open %s for saving", path);
        return;
    }

    buffered_file.fp = fp;

    dir = c_path_get_dirname(path);

    if (stat(dir, &sb) == -1)
        mkdir(dir, 0777);

    c_free(dir);
    dir = NULL;

    serializer = rig_pb_serializer_new(engine);

    pb_ui = rig_pb_serialize_ui(serializer, ui);

    rig__ui__pack_to_buffer(pb_ui, &buffered_file.base);

    rig_pb_serialized_ui_destroy(pb_ui);

    rig_pb_serializer_destroy(serializer);

    fclose(fp);
}

static void
ignore_free(void *allocator_data, void *ptr)
{
    /* NOP */
}

static void *
stack_alloc(void *allocator_data, size_t size)
{
    return rut_memory_stack_alloc(allocator_data, size);
}

rig_ui_t *
rig_load(rig_engine_t *engine, const char *file)
{
    struct stat sb;
    int fd;
    uint8_t *contents;
    size_t len;
    c_error_t *error = NULL;
    bool needs_munmap = false;
    rig_pb_un_serializer_t *unserializer;
    Rig__UI *pb_ui;
    rig_ui_t *ui;

    /* We use a special allocator while unpacking protocol buffers
     * that lets us use the frame_stack. This means much
     * less mucking about with the heap since the frame_stack
     * is a persistent, growable stack which we can just rewind
     * very cheaply before unpacking */
    ProtobufCAllocator protobuf_c_allocator = {
        stack_alloc,        ignore_free, stack_alloc, /* tmp_alloc */
        8192, /* max_alloca */
        engine->frame_stack /* allocator_data */
    };

    fd = open(file, O_CLOEXEC);
    if (fd > 0 && fstat(fd, &sb) == 0 &&
        (contents = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
        len = sb.st_size;
        needs_munmap = true;
    } else if (!c_file_get_contents(file, (char **)&contents, &len, &error)) {
        c_warning("Failed to load ui description: %s", error->message);
        c_error_free(error);
        return NULL;
    }

    unserializer = rig_pb_unserializer_new(engine);

    pb_ui = rig__ui__unpack(&protobuf_c_allocator, len, contents);

    ui = rig_pb_unserialize_ui(unserializer, pb_ui);

    rig__ui__free_unpacked(pb_ui, &protobuf_c_allocator);

    if (needs_munmap)
        munmap(contents, len);
    else
        c_free(contents);

    rig_pb_unserializer_destroy(unserializer);

    return ui;
}
