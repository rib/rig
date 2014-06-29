/*
 * Copyright (c) 2008-2011, Dave Benson.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the following
 * disclaimer.

 * Redistributions in binary form must reproduce
 * the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * Neither the name
 * of "protobuf-c" nor the names of its contributors
 * may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PROTOBUF_C_DATA_BUFFER_H_
#define __PROTOBUF_C_DATA_BUFFER_H_

#include <protobuf-c/protobuf-c.h>
#include <stdarg.h>

typedef struct _protobuf_c_data_buffer_t protobuf_c_data_buffer_t;
typedef struct _protobuf_c_data_buffer_fragment_t
    protobuf_c_data_buffer_fragment_t;

struct _protobuf_c_data_buffer_fragment_t {
    protobuf_c_data_buffer_fragment_t *next;
    unsigned buf_start; /* offset in buf of valid data */
    unsigned buf_length; /* length of valid data in buf */
};

struct _protobuf_c_data_buffer_t {
    size_t size;

    protobuf_c_data_buffer_fragment_t *first_frag;
    protobuf_c_data_buffer_fragment_t *last_frag;
    ProtobufCAllocator *allocator;
};

void rig_protobuf_c_data_buffer_init(protobuf_c_data_buffer_t *buffer,
                                     ProtobufCAllocator *allocator);
void rig_protobuf_c_data_buffer_clear(protobuf_c_data_buffer_t *buffer);
void rig_protobuf_c_data_buffer_reset(protobuf_c_data_buffer_t *buffer);

size_t rig_protobuf_c_data_buffer_read(protobuf_c_data_buffer_t *buffer,
                                       void *data,
                                       size_t max_length);
size_t rig_protobuf_c_data_buffer_peek(const protobuf_c_data_buffer_t *buffer,
                                       void *data,
                                       size_t max_length);
size_t rig_protobuf_c_data_buffer_discard(protobuf_c_data_buffer_t *buffer,
                                          size_t max_discard);
char *rig_protobuf_c_data_buffer_read_line(protobuf_c_data_buffer_t *buffer);

char *
rig_protobuf_c_data_buffer_parse_string0(protobuf_c_data_buffer_t *buffer);
/* Returns first char of buffer, or -1. */
int
rig_protobuf_c_data_buffer_peek_char(const protobuf_c_data_buffer_t *buffer);
int rig_protobuf_c_data_buffer_read_char(protobuf_c_data_buffer_t *buffer);

int rig_protobuf_c_data_buffer_index_of(protobuf_c_data_buffer_t *buffer,
                                        char char_to_find);
/*
 * Appending to the buffer.
 */
void rig_protobuf_c_data_buffer_append(protobuf_c_data_buffer_t *buffer,
                                       const void *data,
                                       size_t length);
void rig_protobuf_c_data_buffer_append_string(protobuf_c_data_buffer_t *buffer,
                                              const char *string);
void rig_protobuf_c_data_buffer_append_char(protobuf_c_data_buffer_t *buffer,
                                            char character);
void rig_protobuf_c_data_buffer_append_repeated_char(
    protobuf_c_data_buffer_t *buffer, char character, size_t count);
#define rig_protobuf_c_data_buffer_append_zeros(buffer, count)                 \
    rig_protobuf_c_data_buffer_append_repeated_char((buffer), 0, (count))

/* XXX: rig_protobuf_c_data_buffer_append_repeated_data() is UNIMPLEMENTED */
void rig_protobuf_c_data_buffer_append_repeated_data(
    protobuf_c_data_buffer_t *buffer,
    const void *data_to_repeat,
    size_t data_length,
    size_t count);

void rig_protobuf_c_data_buffer_append_string0(protobuf_c_data_buffer_t *buffer,
                                               const char *string);

/* Take all the contents from src and append
 * them to dst, leaving src empty.
 */
size_t rig_protobuf_c_data_buffer_drain(protobuf_c_data_buffer_t *dst,
                                        protobuf_c_data_buffer_t *src);

/* Like `drain', but only transfers some of the data. */
size_t rig_protobuf_c_data_buffer_transfer(protobuf_c_data_buffer_t *dst,
                                           protobuf_c_data_buffer_t *src,
                                           size_t max_transfer);

/* file-descriptor mucking */
int rig_protobuf_c_data_buffer_writev(protobuf_c_data_buffer_t *read_from,
                                      int fd);
int rig_protobuf_c_data_buffer_writev_len(protobuf_c_data_buffer_t *read_from,
                                          int fd,
                                          size_t max_bytes);
int rig_protobuf_c_data_buffer_read_in_fd(protobuf_c_data_buffer_t *write_to,
                                          int read_from);

/* This deallocates memory used by the buffer-- you are responsible
 * for the allocation and deallocation of the protobuf_c_data_buffer_t itself.
 */
void rig_protobuf_c_data_buffer_destruct(protobuf_c_data_buffer_t *to_destroy);

/* Free all unused buffer fragments. */
void rig_protobuf_c_data_buffer_cleanup_recycling_bin();

#endif
