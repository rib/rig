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
#include <gio/gio.h>

#include <rut.h>

#include "rig-types.h"
#include "rig-pb.h"

/* XXX: The definition of an "asset" is getting a big confusing.
 * Initially it used to represent things created in third party
 * programs that you might want to import into Rig. It lets us
 * keep track of the original path, create a thumbnail and track
 * tags for use in the Rig editor.
 *
 * We have been creating components with RigAsset properties
 * though and when we load a UI or send it across to a slave then
 * we are doing redundant work to create models and thumbnails
 * which are only useful to an editor.
 *
 * XXX: Maybe we can introduce the idea of a "Blob" which can
 * track the same kind of data we currently use assets for and
 * perhaps rename RigAsset to RigAsset and clarify that it is
 * only for use in the editor. A Blob can have an optional
 * back-reference to an asset, but when serializing to slaves
 * for example the assets wouldn't be kept.
 */

extern RutType rig_asset_type;

#ifndef RIG_ASSET_TYPEDEF
/* Note: rut-property.h currently avoids including rut-asset.h
 * to avoid a circular header dependency and directly declares
 * a RigAsset typedef */
typedef struct _RigAsset RigAsset;
#define RIG_ASSET_TYPEDEF
#endif

void _rig_asset_type_init (void);

bool
rut_file_info_is_asset (GFileInfo *info, const char *name);

RigAsset *
rig_asset_new_builtin (RutContext *ctx,
                       const char *icon_path);

RigAsset *
rig_asset_new_texture (RutContext *ctx,
                       const char *path,
                       const c_list_t *inferred_tags);

RigAsset *
rig_asset_new_normal_map (RutContext *ctx,
                          const char *path,
                          const c_list_t *inferred_tags);

RigAsset *
rig_asset_new_alpha_mask (RutContext *ctx,
                          const char *path,
                          const c_list_t *inferred_tags);

RigAsset *
rig_asset_new_ply_model (RutContext *ctx,
                         const char *path,
                         const c_list_t *inferred_tags);

RigAsset *
rig_asset_new_font (RutContext *ctx,
                    const char *path,
                    const c_list_t *inferred_tags);

RigAsset *
rig_asset_new_from_file (RigEngine *engine,
                         GFileInfo *info,
                         GFile *asset_file,
                         RutException **e);

RigAsset *
rig_asset_new_from_data (RutContext *ctx,
                         const char *path,
                         RigAssetType type,
                         bool is_video,
                         const uint8_t *data,
                         size_t len);

RigAsset *
rig_asset_new_from_mesh (RutContext *ctx,
                         RutMesh *mesh);

RigAsset *
rig_asset_new_from_pb_asset (RigPBUnSerializer *unserializer,
                             Rig__Asset *pb_asset,
                             RutException **e);

RigAssetType
rig_asset_get_type (RigAsset *asset);

const char *
rig_asset_get_path (RigAsset *asset);

RutContext *
rig_asset_get_context (RigAsset *asset);

CoglTexture *
rig_asset_get_texture (RigAsset *asset);

RutMesh *
rig_asset_get_mesh (RigAsset *asset);

bool
rig_asset_get_is_video (RigAsset *asset);

void
rig_asset_set_inferred_tags (RigAsset *asset,
                             const c_list_t *inferred_tags);

const c_list_t *
rig_asset_get_inferred_tags (RigAsset *asset);

bool
rig_asset_has_tag (RigAsset *asset, const char *tag);

c_list_t *
rut_infer_asset_tags (RutContext *ctx, GFileInfo *info, GFile *asset_file);

void
rig_asset_add_inferred_tag (RigAsset *asset, const char *tag);

bool
rig_asset_needs_thumbnail (RigAsset *asset);

typedef void (*RutThumbnailCallback) (RigAsset *asset, void *user_data);

RutClosure *
rig_asset_thumbnail (RigAsset *asset,
                     RutThumbnailCallback ready_callback,
                     void *user_data,
                     RutClosureDestroyCallback destroy_cb);

void *
rig_asset_get_data (RigAsset *asset);

size_t
rig_asset_get_data_len (RigAsset *asset);

bool
rig_asset_get_mesh_has_tex_coords (RigAsset *asset);

bool
rig_asset_get_mesh_has_normals (RigAsset *asset);

void
rig_asset_reap (RigAsset *asset, RigEngine *engine);

void
rig_asset_get_image_size (RigAsset *asset,
                          int *width,
                          int *height);

#endif /* _RIG_ASSET_H_ */
