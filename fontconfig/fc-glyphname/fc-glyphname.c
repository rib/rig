/*
 * fontconfig/fc-glyphname/fc-glyphname.c
 *
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"

static int
rawindex (const FcGlyphName *gn);

static void
scan (FILE *f, char *filename);

static int
isprime (int i);

static void
find_hash (void);

static FcChar32
FcHashGlyphName (const FcChar8 *name);

static void
insert (FcGlyphName *gn, FcGlyphName **table, FcChar32 h);

static void
dump (FcGlyphName * const *table, const char *name);

static FcGlyphName *
FcAllocGlyphName (FcChar32 ucs, FcChar8 *name)
{
    FcGlyphName	*gn;

    gn = malloc (sizeof (FcGlyphName) + strlen ((char *) name));
    if (!gn)
	return 0;
    gn->ucs = ucs;
    strcpy ((char *) gn->name, (char *) name);
    return gn;
}

static void
fatal (const char *file, int lineno, const char *msg)
{
    if (lineno)
        fprintf (stderr, "%s:%d: %s\n", file, lineno, msg);
    else
	fprintf (stderr, "%s: %s\n", file, msg);

    exit (1);
}

#define MAX_GLYPHFILE	    256
#define MAX_GLYPHNAME	    10240
#define MAX_NAMELEN	    1024

static FcGlyphName *raw[MAX_GLYPHNAME];
static int	    nraw;
static int	    max_name_len;
static FcGlyphName *name_to_ucs[MAX_GLYPHNAME*2];
static FcGlyphName *ucs_to_name[MAX_GLYPHNAME*2];
static unsigned int hash, rehash;
static FILE        *out = NULL;

static int
rawindex (const FcGlyphName *gn)
{
    int	i;

    for (i = 0; i < nraw; i++)
	if (raw[i] == gn)
	    return i;
    return -1;
}

static void
scan (FILE *f, char *filename)
{
    char	    buf[MAX_NAMELEN];
    char	    name[MAX_NAMELEN];
    unsigned long   ucs;
    FcGlyphName	    *gn;
    int		    lineno = 0;
    int		    len;

    while (fgets (buf, sizeof (buf), f))
    {
	lineno++;
	if (sscanf (buf, "%[^;];%lx\n", name, &ucs) != 2)
	    continue;
	gn = FcAllocGlyphName ((FcChar32) ucs, (FcChar8 *) name);
	if (!gn)
	    fatal (filename, lineno, "out of memory");
	len = strlen (name);
	if (len > max_name_len)
	    max_name_len = len;
	raw[nraw++] = gn;
    }
}

static int compare_string (const void *a, const void *b)
{
    const char    *const *as = a, *const *bs = b;
    return strcmp (*as, *bs);
}

static int compare_glyphname (const void *a, const void *b)
{
    const FcGlyphName	*const *ag = a, *const *bg = b;

    return strcmp ((char *) (*ag)->name, (char *) (*bg)->name);
}

static int
isqrt (int a)
{
    int	    l, h, m;

    l = 2;
    h = a/2;
    while ((h-l) > 1)
    {
	m = (h+l) >> 1;
	if (m * m < a)
	    l = m;
	else
	    h = m;
    }
    return h;
}

static int
isprime (int i)
{
    int	l, t;

    if (i < 2)
	return FcFalse;
    if ((i & 1) == 0)
    {
	if (i == 2)
	    return FcTrue;
	return FcFalse;
    }
    l = isqrt (i) + 1;
    for (t = 3; t <= l; t += 2)
	if (i % t == 0)
	    return 0;
    return 1;
}

/*
 * Find a prime pair that leaves at least 25% of the hash table empty
 */

static void
find_hash (void)
{
    int	h;

    h = nraw + nraw / 4;
    if ((h & 1) == 0)
	h++;
    while (!isprime(h-2) || !isprime(h))
	h += 2;
    hash = h;
    rehash = h-2;
}

static FcChar32
FcHashGlyphName (const FcChar8 *name)
{
    FcChar32	h = 0;
    FcChar8	c;

    while ((c = *name++))
    {
	h = ((h << 1) | (h >> 31)) ^ c;
    }
    return h;
}

static void
insert (FcGlyphName *gn, FcGlyphName **table, FcChar32 h)
{
    unsigned int	i, r = 0;

    i = (int) (h % hash);
    while (table[i])
    {
	if (!r) r = (h % rehash + 1);
	i += r;
	if (i >= hash)
	    i -= hash;
    }
    table[i] = gn;
}

static void
dump (FcGlyphName * const *table, const char *name)
{
    unsigned int	    i;

    fprintf (out, "static const FcGlyphId %s[%d] = {\n", name, hash);

    for (i = 0; i < hash; i++)
	if (table[i])
	    fprintf (out, "    %d,\n", rawindex(table[i]));
	else
	    fprintf (out, "    -1,\n");

    fprintf (out, "};\n");
}

int
main (int argc FC_UNUSED, char **argv)
{
    char	*files[MAX_GLYPHFILE + 1];
    char	line[1024];
    FILE	*f;
    int		i;
    const char	*type;
    FILE        *template = NULL;

    if (argc < 3)
    {
        fprintf(stderr, "usage: %s template output [glyphfiles...]\n", argv[0]);
        exit(1);
    }

    template = fopen(argv[1], "r");
    if (!template)
    {
        fprintf(stderr, "Failed to open template: %s: %s",
                argv[1], strerror(errno));
        exit(1);
    }

    out = fopen(argv[2], "w");
    if (!out)
    {
        fprintf(stderr, "Failed to open output: %s: %s",
                argv[2], strerror(errno));
        exit(1);
    }

    i = 0;
    while (argv[i+3])
    {
	if (i == MAX_GLYPHFILE)
	    fatal (*argv, 0, "Too many glyphname files");
	files[i] = argv[i+1];
	i++;
    }
    files[i] = 0;
    qsort (files, i, sizeof (char *), compare_string);
    for (i = 0; files[i]; i++)
    {
	f = fopen (files[i], "r");
	if (!f)
	    fatal (files[i], 0, strerror (errno));
	scan (f, files[i]);
	fclose (f);
    }
    qsort (raw, nraw, sizeof (FcGlyphName *), compare_glyphname);

    find_hash ();

    for (i = 0; i < nraw; i++)
    {
	insert (raw[i], name_to_ucs, FcHashGlyphName (raw[i]->name));
	insert (raw[i], ucs_to_name, raw[i]->ucs);
    }

    /*
     * Scan the input until the marker is found
     */

    while (fgets (line, sizeof (line), template))
    {
	if (!strncmp (line, "@@@", 3))
	    break;
	fputs (line, out);
    }

    fprintf (out, "/* %d glyphnames in %d entries, %d%% occupancy */\n\n",
             nraw, hash, nraw * 100 / hash);

    fprintf (out, "#define FC_GLYPHNAME_HASH %u\n", hash);
    fprintf (out, "#define FC_GLYPHNAME_REHASH %u\n", rehash);
    fprintf (out, "#define FC_GLYPHNAME_MAXLEN %d\n\n", max_name_len);
    if (nraw < 128)
	type = "int8_t";
    else if (nraw < 32768)
	type = "int16_t";
    else
	type = "int32_t";

    fprintf (out, "typedef %s FcGlyphId;\n\n", type);

    /*
     * Dump out entries
     */

    fprintf (out, "static const struct { const FcChar32 ucs; const FcChar8 name[%d]; } _fc_glyph_names[%d] = {\n",
             max_name_len + 1, nraw);

    for (i = 0; i < nraw; i++)
	fprintf (out, "    { 0x%lx, \"%s\" },\n",
		(unsigned long) raw[i]->ucs, raw[i]->name);

    fprintf (out, "};\n");

    /*
     * Dump out name_to_ucs table
     */

    dump (name_to_ucs, "_fc_name_to_ucs");

    /*
     * Dump out ucs_to_name table
     */
    dump (ucs_to_name, "_fc_ucs_to_name");

    while (fgets (line, sizeof (line), template))
	fputs (line, out);

    fclose (out);
}
