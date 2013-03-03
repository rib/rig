/*
 * Rig
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "rig.pb-c.h"
#include "rig-engine.h"
#include "rig-load-xml.h"
#include "rig-pb.h"

typedef struct _BufferedFile
{
  ProtobufCBuffer base;
  FILE *fp;
  CoglBool error;
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
    buffered_file->error = TRUE;
}

void
rig_save (RigEngine *engine, const char *path)
{
  struct stat sb;
  Rig__UI *ui;
  char *rig_filename = g_strconcat (path, ".rig", NULL);
  FILE *fp = fopen (rig_filename, "w");

  BufferedFile buffered_file = {
    { append_to_file },
    fp,
    FALSE
  };

  g_free (rig_filename);

  if (stat (engine->ctx->assets_location, &sb) == -1)
    mkdir (engine->ctx->assets_location, 0777);

  ui = rig_pb_serialize_ui (engine, NULL, NULL);

  rig__ui__pack_to_buffer (ui, &buffered_file.base );

  fclose (fp);
}

static void
ignore_free (void *allocator_data, void *ptr)
{
  /* NOP */
}

void
rig_load (RigEngine *engine, const char *file)
{
  char *ext;
  struct stat sb;
  int fd;
  uint8_t *contents;
  size_t len;
  GError *error = NULL;
  gboolean needs_munmap = FALSE;
  Rig__UI *ui;

  /* We use a special allocator while unpacking protocol buffers
   * that lets us use the serialization_stack. This means much
   * less mucking about with the heap since the serialization_stack
   * is a persistant, growable stack which we can just rewind
   * very cheaply before unpacking */
  ProtobufCAllocator protobuf_c_allocator =
    {
      rut_memory_stack_alloc,
      ignore_free,
      rut_memory_stack_alloc, /* tmp_alloc */
      8192, /* max_alloca */
      engine->serialization_stack /* allocator_data */
    };


  /* XXX: For compatability with Rig 2 we support loading XML ui
   * descriptions, though the plan is to only maintain this for one
   * release, just to give time to convert existing files. */
  ext = g_strrstr (file, ".xml");
  if (ext && ext[4] == '\0')
    {
      rig_load_xml (engine, file);
      return;
    }

  fd = open (file, O_CLOEXEC);
  if (fd > 0 &&
      fstat (fd, &sb) == 0 &&
      (contents = mmap (NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)))
    {
      len = sb.st_size;
      needs_munmap = TRUE;
    }
  else if (!g_file_get_contents (file,
                                 (gchar **)&contents,
                                 &len,
                                 &error))
    {
      g_warning ("Failed to load ui description: %s", error->message);
      return;
    }

  ui = rig__ui__unpack (&protobuf_c_allocator, len, contents);

  rig_pb_unserialize_ui (engine, ui);

  rig__ui__free_unpacked (ui, &protobuf_c_allocator);

  if (needs_munmap)
    munmap (contents, len);
  else
    g_free (contents);
}
