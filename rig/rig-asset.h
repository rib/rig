/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RIG_ASSET_H_
#define _RIG_ASSET_H_

#include <stdbool.h>

#include <rut.h>

#include "rig-types.h"
#include "rig-pb.h"

/* XXX: The definition of an "asset" is getting a big confusing.
 * Initially it used to represent things created in third party
 * programs that you might want to import into Rig. It lets us
 * keep track of the original path, create a thumbnail and track
 * tags for use in the Rig editor.
 *
 * We have been creating components with rig_asset_t properties
 * though and when we load a UI or send it across to a slave then
 * we are doing redundant work to create models and thumbnails
 * which are only useful to an editor.
 *
 * XXX: Maybe we can introduce the idea of a "Blob" which can
 * track the same kind of data we currently use assets for and
 * perhaps rename rig_asset_t to rig_asset_t and clarify that it is
 * only for use in the editor. A Blob can have an optional
 * back-reference to an asset, but when serializing to slaves
 * for example the assets wouldn't be kept.
 *
 * ...we can likely avoid having a generic 'blob' type and just have
 * types and property support for images + models + fonts etc and all
 * of these types can support a 'derivable' trait that lets the editor
 * know they where originally created from some particular asset and
 * so it can e.g. potentially update the object if the asset changes.
 * When we package things up for distribution though we wont keep the
 * asset references/state around.
 */

extern rut_type_t rig_asset_type;

#ifndef RIG_ASSET_TYPEDEF
/* Note: rig-property.h currently avoids including rut-asset.h
 * to avoid a circular header dependency and directly declares
 * a rig_asset_t typedef */
typedef struct _rig_asset_t rig_asset_t;
#define RIG_ASSET_TYPEDEF
#endif

void _rig_asset_type_init(void);

rig_asset_t *rig_asset_new_from_pb_asset(rig_pb_un_serializer_t *unserializer,
                                         Rig__Asset *pb_asset,
                                         rut_exception_t **e);

rig_asset_type_t rig_asset_get_type(rig_asset_t *asset);

const char *rig_asset_get_path(rig_asset_t *asset);

const char *rig_asset_get_mime_type(rig_asset_t *asset);

rut_shell_t *rig_asset_get_shell(rig_asset_t *asset);

rut_mesh_t *rig_asset_get_mesh(rig_asset_t *asset);

void *rig_asset_get_data(rig_asset_t *asset);

size_t rig_asset_get_data_len(rig_asset_t *asset);

bool rig_asset_get_mesh_has_tex_coords(rig_asset_t *asset);

bool rig_asset_get_mesh_has_normals(rig_asset_t *asset);

void rig_asset_reap(rig_asset_t *asset, rig_engine_t *engine);

void rig_asset_get_image_size(rig_asset_t *asset, int *width, int *height);


#ifdef RIG_EDITOR_ENABLED
bool rig_file_is_asset(const char *filename, const char *mime_type);
rig_asset_t *rig_asset_new_builtin(rig_engine_t *engine, const char *icon_path);
rig_asset_t *rig_asset_new_from_file(rig_engine_t *engine,
                                     const char *filename,
                                     const char *mime_type,
                                     rut_exception_t **e);

void rig_asset_set_inferred_tags(rig_asset_t *asset,
                                 const c_llist_t *inferred_tags);

const c_llist_t *rig_asset_get_inferred_tags(rig_asset_t *asset);

bool rig_asset_has_tag(rig_asset_t *asset, const char *tag);

void rig_asset_add_inferred_tag(rig_asset_t *asset, const char *tag);

bool rig_asset_needs_thumbnail(rig_asset_t *asset);

typedef void (*rut_thumbnail_callback_t)(rig_asset_t *asset, void *user_data);

rut_closure_t *rig_asset_thumbnail(rig_asset_t *asset,
                                   rut_thumbnail_callback_t ready_callback,
                                   void *user_data,
                                   rut_closure_destroy_callback_t destroy_cb);

cg_texture_t *rig_asset_get_thumbnail(rig_asset_t *asset);

#endif /* RIG_EDITOR_ENABLED */

#endif /* _RIG_ASSET_H_ */
