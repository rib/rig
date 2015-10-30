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
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <fnmatch.h>
#include <stdbool.h>

#include <clib.h>

#include "glob.h"

typedef struct _glob {
    char *glob;
    bool needs_magic; /* E.g. .ogg, .avi */
    int weight;
    char *mime_type;
} glob_t;

/* searched first */
static c_hash_table_t *literals = NULL;

/* A hash table of the simple *.abc patterns or *.abc.efg
 * (so long as everything after the * is just a literal)
 */
static c_hash_table_t *simple_globs = NULL;

/* patterns involving multiple *s or the characters [? */
static int max_fancy_globs_weight = -1;
static c_llist_t *fancy_globs = NULL;

static glob_t *
lookup_glob_in_list(c_llist_t *list, char *glob)
{
    c_llist_t *l;

    for (l = list; l; l = l->next) {
        glob_t *entry = l->data;
        if (strcmp(entry->glob, glob) == 0)
            return entry;
    }
    return NULL;
}

static bool
glob_read_file(const char *path)
{
    FILE *fp;
    size_t line_buf_len;
    char *line;
    bool added_glob = false;

    if ((fp = fopen(path, "r")) == NULL)
        return NULL;

    line_buf_len = 512;
    line = malloc(line_buf_len);
    if (!line)
        c_error("Could not allocate getline buffer");

    while (getline(&line, &line_buf_len, fp) > 0) {
        char *weight_str;
        int weight;
        char *mime;
        char *glob;
        int glob_len;
        bool literal = false;
        bool simple = false;
        bool needs_magic = false;
        glob_t *existing = NULL;
        glob_t *entry;
        char *tokenizer;

        if (line[0] == '#' || !c_ascii_isdigit(line[0]))
            continue;

        weight_str = strtok_r(line, ":", &tokenizer);
        mime = strtok_r(NULL, ":", &tokenizer);
        glob = strtok_r(NULL, ":", &tokenizer);

        if (!weight_str || !mime || !glob)
            continue;

        weight = c_ascii_strtoull(weight_str, NULL, 10);

        glob_len = strlen(glob);
        if (glob[glob_len - 1] == '\n')
            glob[glob_len - 1] = '\0';

        if (glob[0] == '*' && glob[1] == '.' &&
            strstr(glob + 2, "*") == NULL &&
            strstr(glob + 2, "[") == NULL &&
            strstr(glob + 2, "?") == NULL)
        {
            simple = true;
        } else if (strstr(glob, "*") == NULL &&
                   strstr(glob, "[") == NULL &&
                   strstr(glob, "?") == NULL)
        {
            literal = true;
        }

        if (literal)
            existing = c_hash_table_lookup(literals, glob);
        else if (simple)
            existing = c_hash_table_lookup(simple_globs, glob + 2);
        else
            existing = lookup_glob_in_list(fancy_globs, glob);

        /* stuff like .ogg or .avi might have numerous mime types
         * for the same pattern so we have to try and use magic
         * to correctly determine the mime type */
        if (existing) {
            if (strcmp(existing->mime_type, mime) == 0)
                continue; /* duplicate */

            existing->needs_magic = true;
            needs_magic = true;
        }

        entry = c_new0(glob_t, 1);
        entry->glob = c_strdup(glob);
        entry->weight = weight;
        entry->mime_type = c_strdup(mime);
        entry->needs_magic = needs_magic;

        if (literal)
            c_hash_table_insert(literals, entry->glob, entry);
        else if (simple)
            c_hash_table_insert(simple_globs, entry->glob + 2, entry);
        else {
            fancy_globs = c_llist_prepend(fancy_globs, entry);
            if (weight > max_fancy_globs_weight)
                max_fancy_globs_weight = weight;
        }

        added_glob = true;
    }

    fclose(fp);
    free(line);

    return added_glob;
}

static void
glob_list_free(c_llist_t *list)
{
    c_llist_t *l;

    for (l = list; l; l = l->next) {
        glob_t *entry = l->data;

        c_free(entry->glob);
        c_free(entry->mime_type);
        c_free(entry);
    }

    c_llist_free(list);
}

bool
glob_add_file(const char *globpath)
{
    if (!literals) {
        literals = c_hash_table_new_full(c_str_hash,
                                         c_str_equal,
                                         c_free, /* key destroy */
                                         c_free); /* value destroy */

        simple_globs = c_hash_table_new_full(c_str_hash,
                                             c_str_equal,
                                             c_free, /* key destroy */
                                             c_free); /* value destroy */
    }

    return glob_read_file(globpath);
}

void
glob_cleanup(void)
{
    if (literals) {
        c_hash_table_destroy(literals);
        literals = NULL;

        c_hash_table_destroy(simple_globs);
        simple_globs = NULL;
    }

    glob_list_free(fancy_globs);
    fancy_globs = NULL;
}

static int
compare_weights_cb(const void *a, const void *b)
{
    const glob_t * const *entry0v = a;
    const glob_t *entry0 = entry0v[0];
    const glob_t * const *entry1v = b;
    const glob_t *entry1 = entry1v[0];

    /* Note: we want to sort in descending order */
    return entry1->weight - entry0->weight;
}

/* Things seem pretty ugly here as we're  */
const char *
lookup_mime_type_case_sensitive(const char *filename,
                                bool *needs_magic)
{
#define MAX_CONFLICTS 20
    glob_t *entries[MAX_CONFLICTS];
    int n_entries = 0;
    glob_t *entry;
    const char *extension;
    c_llist_t *l;

    /* Note: we're currently ignoring weights if we match a literal,
     * hopefully that's ok? */
    entry = c_hash_table_lookup(literals, filename);
    if (entry)
        return entry->mime_type;

    for (extension = strstr(filename, ".");
         extension;
         extension = strstr(extension + 1, "."))
    {
        if (extension[1]) {
            entry = c_hash_table_lookup(simple_globs, extension + 1);
            if (entry) {
                entries[n_entries++] = entry;
                break; /* ignore shorter patterns */
            }
        }
    }

    /* only check the fancy patterns if we know we might
     * trump an entry we've already found... */
    if (n_entries && entries[0]->weight <= max_fancy_globs_weight) {
        for (l = fancy_globs; n_entries < MAX_CONFLICTS && l; l = l->next) {
            entry = l->data;

            if (fnmatch(entry->glob, filename, FNM_NOESCAPE))
                entries[n_entries++] = entry;
        }
    }

    if (!n_entries)
        return NULL;

    if (n_entries > 1)
        qsort(entries, n_entries, sizeof(void *), compare_weights_cb);

    entry = entries[0];
    *needs_magic = entry->needs_magic;

    return entry->mime_type;
}

const char *
lookup_mime_type_case_insensitive(const char *filename,
                                  bool *needs_magic)
{
    int len = strlen(filename);
    char *copy = alloca(len + 1);

    memcpy(copy, filename, len + 1);

    /* XXX: we're assuming that the filename encoding is ascii compatible. */
    c_ascii_strdown(copy, len);

    return lookup_mime_type_case_sensitive(filename, needs_magic);
}

const char *
glob_lookup_mime_type(const char *filename,
                      bool *needs_magic)
{
    int len = strlen(filename);
    char *copy = alloca(len + 1);
    char *base;
    const char *mime_type;

    memcpy(copy, filename, len + 1);
    base = basename(filename);

    mime_type = lookup_mime_type_case_sensitive(base, needs_magic);

    if (!mime_type)
        mime_type = lookup_mime_type_case_insensitive(base, needs_magic);

    return mime_type;
}

