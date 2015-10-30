/* Copyright (C) 2015 Robert Bragg
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

#include "xdgmime-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <err.h>

#include <clib.h>

#define MIME_MAGIC_STRING "MIME-Magic\0\n"

typedef struct magic_section_blob {
    c_list_t link;

    int indent;
    int start_offset;
    int value_length;
    uint8_t *value;
    uint8_t *mask;
    int range_length;
    int word_size;
} magic_section_blob_t;

typedef struct _magic_section {
    c_list_t link;

    char *mime_type;
    int priority;
    int read_size;

    c_list_t blobs;
} magic_section_t;

static bool initialized = false;
static c_list_t magic_sections;

static bool
match_section_blob(magic_section_t *section,
                   magic_section_blob_t *blob,
                   c_array_t *scratch_array)
{
    char *scratch = scratch_array->data;
    int range_len = blob->range_length;
    int word_size = blob->word_size ? blob->word_size : 1;
    int value_len = blob->value_length;
    magic_section_blob_t *next_blob;
    int i, j;

    switch(word_size) {
    case 1:
        if (blob->mask) {
            for (i = 0; i < range_len; i++) {
                uint8_t *file_data = (uint8_t *)(scratch + blob->start_offset + i);
                uint8_t *value_data = blob->value;
                uint8_t *mask_data = blob->mask;

                for (j = 0; j < value_len; j++)
                    if ((value_data[j] & mask_data[j]) != (file_data[j] & mask_data[j]))
                        return false;
            }
        } else
            for (i = 0; i < range_len; i++)
                if (memcmp(scratch + blob->start_offset + i,
                           blob->value, value_len) != 0)
                    return false;
        break;

    case 2:
        if (blob->mask) {
            for (i = 0; i < range_len; i++) {
                uint16_t *file_data = (uint16_t *)(scratch + blob->start_offset + i);
                uint16_t *value_data = (uint16_t *)blob->value;
                uint16_t *mask_data = (uint16_t *)blob->mask;
                int len = value_len / 2;

                for (j = 0; j < len; j++) {
                    uint16_t mask = ntohs(mask_data[j]);
                    uint16_t value = ntohs(value_data[j]);

                    if ((value & mask) != (file_data[j] & mask))
                        return false;
                }
            }
        } else {
            for (i = 0; i < range_len; i++) {
                uint16_t *file_data = (uint16_t *)(scratch + blob->start_offset + i);
                uint16_t *value_data = (uint16_t *)blob->value;
                int len = value_len / 2;

                for (j = 0; j < len; j++)
                    if (ntohs(value_data[j]) != file_data[j])
                        return false;
            }
        }
        break;

    case 4:
        if (blob->mask) {
            for (i = 0; i < range_len; i++) {
                uint32_t *file_data = (uint32_t *)(scratch + blob->start_offset + i);
                uint32_t *value_data = (uint32_t *)blob->value;
                uint32_t *mask_data = (uint32_t *)blob->mask;
                int len = value_len / 4;

                for (j = 0; j < len; j++) {
                    uint32_t mask = ntohl(mask_data[j]);
                    uint32_t value = ntohl(value_data[j]);

                    if ((value & mask) != (file_data[j] & mask))
                        return false;
                }
            }
        } else {
            for (i = 0; i < range_len; i++) {
                uint32_t *file_data = (uint32_t *)(scratch + blob->start_offset + i);
                uint32_t *value_data = (uint32_t *)blob->value;
                int len = value_len / 4;

                for (j = 0; j < len; j++)
                    if (ntohl(value_data[j]) != file_data[j])
                        return false;
            }
        }
        break;
    }

    /* If we're at the end of the list, nothing more to check */
    if (&blob->link.next == &section->blobs.next)
        return true;

    /* If the blob isn't followed by any indented blobs then nothing
     * more to check */
    next_blob = c_container_of(blob->link.next,
                               magic_section_blob_t,
                               link);
    if (next_blob->indent <= blob->indent)
        return true;

    /* NB: the blob may be followed by multiple sub-trees of indented
     * blobs. We only want to check those blobs with an indent ==
     * (indent + 1), ignoring blobs with an indent greater than that
     * and stopping once we reach any blob with indent <= the current
     * blob.
     */
    for (c_list_set_iterator(blob->link.next, next_blob, link);
         &next_blob->link != &section->blobs && next_blob->indent > blob->indent;
         c_list_set_iterator(next_blob->link.next, next_blob, link))
    {
        if (next_blob->indent != (blob->indent + 1))
            continue;

        if (match_section_blob(section, next_blob, scratch_array))
            return true;
    }

    return false;
}

static bool
match_section(FILE *fp, magic_section_t *section, c_array_t *scratch_array)
{
    magic_section_blob_t *blob;
    int n_bytes;

    c_array_set_size(scratch_array, section->read_size);
    n_bytes = fread(scratch_array->data, 1, section->read_size, fp);
    if (n_bytes < 0)
        return false;

    memset(scratch_array->data + n_bytes, 0, section->read_size - n_bytes);

    c_list_for_each(blob, &section->blobs, link)
        if (match_section_blob(section, blob, scratch_array))
            return true;

    return false;
}

static void
free_blob(magic_section_blob_t *blob)
{
    c_free(blob->value);
    c_free(blob->mask);
    c_free(blob);
}

static void
free_section(magic_section_t *section)
{
    magic_section_blob_t *blob, *tmp;

    c_free(section->mime_type);

    c_list_for_each_safe(blob, tmp, &section->blobs, link) {
        c_list_remove(&blob->link);
        free_blob(blob);
    }

    c_free(section);
}

static bool
read_into_scratch(FILE *fp, c_array_t *scratch_array, const char *delim)
{
    c_array_set_size(scratch_array, 0);

    for (;;) {
        int c = fgetc(fp);
        if (c == EOF)
            return false;

        c_array_append_val(scratch_array, c);

        for (int i = 0; delim[i]; i++)
            if (c == delim[i])
                return true;
    }

    c_warn_if_reached();
    return false;
}

static bool
read_number(FILE *fp, int *number)
{
    int len = 0;
    char buf[16];
    char *p = buf;

    for (len = 0; len < (sizeof(buf) - 1); len++) {
        int c = fgetc(fp);
        if (c == EOF)
            return false;

        if (c_ascii_isdigit(c))
            *(p++) = c;
        else {
            ungetc(c, fp);
            if (len) {
                *(p++) = '\0';
                *number = c_ascii_strtoull(buf, NULL, 10);
                return true;
            } else
                return false;
        }
    }

    c_warn_if_reached();
    return false;
}

static bool
add_magic_sections(const char *path)
{
    FILE *fp;
    char magic_buf[sizeof(MIME_MAGIC_STRING) + 1];
    static c_array_t *scratch_array;
    c_llist_t *sections = NULL;
    uint16_t value_length;
    char *tokenizer;
    char c;

    if ((fp = fopen(path, "r")) == NULL)
        return NULL;

    if (fgets(magic_buf, sizeof(MIME_MAGIC_STRING) + 1, fp) == NULL)
        goto exit;

    if (memcmp(magic_buf, MIME_MAGIC_STRING, sizeof(MIME_MAGIC_STRING)) != 0) {
        c_warning("ignorning invalid mime type magic file (%s)", path);
        goto exit;
    }

    scratch_array = c_array_sized_new(false, /* zero terminated */
                                      false, /* clear */
                                      1, /* element size */
                                      512);

    /* Read sections... */
    for (;;) {
        char *line;
        char *priority_str;
        int priority;
        const char *mime_type;
        magic_section_t *section;

        if (!read_into_scratch(fp, scratch_array, "\n")) {
            c_warning("Failed to read section header; ignoring rest of file (%s)",
                      path);
            goto exit;
        }

        line = scratch_array->data;

        priority_str = strtok_r(line, "[:", &tokenizer);
        priority = strtoul(priority_str, NULL, 10);

        mime_type = strtok_r(NULL, "]", &tokenizer);

        section = c_new0(magic_section_t, 1);
        section->priority = priority;
        section->mime_type = c_strdup(mime_type);
        mime_type = section->mime_type;
        section->read_size = 0;
        c_list_init(&section->blobs);

        c_list_insert(magic_sections.prev, &section->link);

        /* Read section lines... */
        for (;;) {
            int indent = 0;
            int start_offset;
            magic_section_blob_t *blob;

            if (!read_into_scratch(fp, scratch_array, "=")) {
                c_warning("Failed to read mime type magic section (%s) line; "
                          "ignoring rest of file (%s)",
                          mime_type, path);
                goto exit;
            }

            line = scratch_array->data;

            if (line[0] != '>')
                indent = c_ascii_strtoull(line, &line, 10);

            if (line[0] != '>') {
                c_warning("Missing expected '>' parsing mime type magic section (%s) line; "
                          "ignoring reset of file (%s)", mime_type, path);
                goto exit;
            }

            line++;
            start_offset = c_ascii_strtoull(line, &line, 10);
            if (line[0] != '=') {
                c_warning("Missing expected '=' parsing mime type magic section (%s) line; "
                          "ignoring rest of file (%s)", mime_type, path);
                goto exit;
            }

            if (fread(&value_length, 2, 1, fp) < 0) {
                c_warning("Failed to read mime type magic value length in section (%s); "
                          "ignoring rest of file (%s)", mime_type, path);
                goto exit;
            }
            value_length = ntohs(value_length);

            blob = c_new0(magic_section_blob_t, 1);
            blob->indent = indent;
            blob->start_offset = start_offset;
            blob->range_length = 1;
            blob->word_size = 1;

            blob->value_length = value_length;
            blob->value = c_malloc(value_length);
            if (fread(blob->value, value_length, 1, fp) < 0) {
                c_warning("Failed to read mime type magic value in section (%s); "
                          "ignoring rest of file (%s)", mime_type, path);
                free_blob(blob);
                goto exit;
            }

            c = fgetc(fp);
            if (c == '\n' || c == EOF || c != '&')
                goto end_of_blob;

            blob->mask = c_malloc(value_length);
            if (fread(blob->mask, value_length, 1, fp) < 0) {
                c_warning("Failed to read mime type magic value mask in section (%s); "
                          "ignoring rest of file (%s)", mime_type, path);
                free_blob(blob);
                goto exit;
            }

            c = fgetc(fp);
            if (c == '\n' || c == EOF || c != '~')
                goto end_of_blob;

            if (!read_number(fp, &blob->word_size)) {
                c_warning("Failed to read mime type magic value word size in section (%s); "
                          "ignoring reset of file (%s)", mime_type, path);
                free_blob(blob);
                goto exit;
            }

            c = fgetc(fp);
            if (c == '\n' || c == EOF || c != '+')
                goto end_of_blob;

            if (!read_number(fp, &blob->range_length)) {
                c_warning("Failed to read mime type magic value range length in section (%s); "
                          "ignoring reset of file (%s)", mime_type, path);
                free_blob(blob);
                goto exit;
            }

end_of_blob:
            /* Skip over any unknown extension fields */
            while (c != '\n' && c != EOF)
                c = fgetc(fp);

            if ((blob->start_offset + blob->value_length + blob->range_length) >
                section->read_size)
            {
                section->read_size =
                    blob->start_offset + blob->value_length + blob->range_length;
            }

            c_list_insert(section->blobs.prev, &blob->link);

            if (c == EOF)
                goto exit;

            c = fgetc(fp);
            ungetc(c, fp);
            if (c == '[')
                break;
        }
    }

exit:
    c_array_free(scratch_array, true);
    fclose(fp);
    return sections;
}

const char *
magic_lookup_mime_type(int fd)
{
    FILE *fp = fdopen(fd, "r");
    magic_section_t *section;
    c_array_t *scratch_array;

    if (!initialized) {
        c_list_init(&magic_sections);
        initialized = true;
    }

    if (!fp) {
        c_warning("Failed to open buffered FILE * for detecting mime type");
        return NULL;
    }

    scratch_array = c_array_sized_new(false, /* zero terminated */
                                      false, /* clear */
                                      1, /* element size */
                                      512);

    c_list_for_each(section, &magic_sections, link) {
        if (match_section(fp, section, scratch_array)) {
            fclose(fp);
            return section->mime_type;
        }
    }

    c_array_free(scratch_array, true);

    fclose(fp);
    return NULL;
}

bool
magic_add_file(const char *filename)
{
    if (!initialized) {
        c_list_init(&magic_sections);
        initialized = true;
    }

    return add_magic_sections(filename);
}

static void
free_magic_sections(void)
{
    magic_section_t *section, *tmp;

    c_list_for_each_safe(section, tmp, &magic_sections, link) {
        c_list_remove(&section->link);
        free_section(section);
    }
}

void
magic_cleanup(void)
{
    if (initialized)
        free_magic_sections();
}

