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

#include <config.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "rig.pb-c.h"
#include "rig-engine.h"
#include "rig-pb.h"

typedef struct _BufferedFile
{
  ProtobufCBuffer base;
  FILE *fp;
  bool error;
} BufferedFile;

static void
append_to_file (ProtobufCBuffer *buffer,
                unsigned len,
                const unsigned char *engine)
{
  BufferedFile *buffered_file = (BufferedFile *)buffer;

  if (buffered_file->error)
    return;

  if (fwrite (engine, len, 1, buffered_file->fp) != 1)
    buffered_file->error = true;
}

void
rig_save (RigEngine *engine, const char *path)
{
  RigPBSerializer *serializer;
  struct stat sb;
  Rig__UI *ui;
  char *rig_filename;
  FILE *fp;

  BufferedFile buffered_file = {
    { append_to_file },
    NULL, /* file pointer */
    false
  };

  rig_filename = c_strdup (path);

  fp = fopen (rig_filename, "w");
  c_free (rig_filename);

  if (!fp)
    {
      c_warning ("Failed to open %s for saving", rig_filename);
      return;
    }

  buffered_file.fp = fp;

  if (stat (engine->ctx->assets_location, &sb) == -1)
    mkdir (engine->ctx->assets_location, 0777);

  serializer = rig_pb_serializer_new (engine);

  ui = rig_pb_serialize_ui (serializer,
                            false, /* edit mode */
                            engine->edit_mode_ui);

  rig__ui__pack_to_buffer (ui, &buffered_file.base );

  rig_pb_serialized_ui_destroy (ui);

  rig_pb_serializer_destroy (serializer);

  fclose (fp);
}

static void
ignore_free (void *allocator_data, void *ptr)
{
  /* NOP */
}

static void *
stack_alloc (void *allocator_data, size_t size)
{
  return rut_memory_stack_alloc (allocator_data, size);
}

RigUI *
rig_load (RigEngine *engine, const char *file)
{
  struct stat sb;
  int fd;
  uint8_t *contents;
  size_t len;
  GError *error = NULL;
  gboolean needs_munmap = false;
  RigPBUnSerializer *unserializer;
  Rig__UI *pb_ui;
  RigUI *ui;

  /* We use a special allocator while unpacking protocol buffers
   * that lets us use the frame_stack. This means much
   * less mucking about with the heap since the frame_stack
   * is a persistant, growable stack which we can just rewind
   * very cheaply before unpacking */
  ProtobufCAllocator protobuf_c_allocator =
    {
      stack_alloc,
      ignore_free,
      stack_alloc, /* tmp_alloc */
      8192, /* max_alloca */
      engine->frame_stack /* allocator_data */
    };

  /* Simulators shouldn't be trying to load UIs directly */
  c_return_val_if_fail (engine->frontend, NULL);

  if (g_str_has_suffix (file, ".xml"))
    {
      c_warning ("Loading xml UI descriptions in Rig is no longer "
                 "supported, only .rig files can be loaded");
      return NULL;
    }

  fd = open (file, O_CLOEXEC);
  if (fd > 0 &&
      fstat (fd, &sb) == 0 &&
      (contents = mmap (NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)))
    {
      len = sb.st_size;
      needs_munmap = true;
    }
  else if (!g_file_get_contents (file,
                                 (gchar **)&contents,
                                 &len,
                                 &error))
    {
      c_warning ("Failed to load ui description: %s", error->message);
      return NULL;
    }

  unserializer = rig_pb_unserializer_new (engine);

  pb_ui = rig__ui__unpack (&protobuf_c_allocator, len, contents);

  ui = rig_pb_unserialize_ui (unserializer, pb_ui);

  rig__ui__free_unpacked (pb_ui, &protobuf_c_allocator);

  if (needs_munmap)
    munmap (contents, len);
  else
    c_free (contents);

  rig_pb_unserializer_destroy (unserializer);

  return ui;
}
