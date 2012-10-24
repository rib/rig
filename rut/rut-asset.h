#ifndef _RUT_ASSET_H_
#define _RUT_ASSET_H_

#include <gio/gio.h>

typedef enum _RutAssetType {
  RUT_ASSET_TYPE_TEXTURE,
  RUT_ASSET_TYPE_NORMAL_MAP,
  RUT_ASSET_TYPE_ALPHA_MASK,
} RutAssetType;

extern RutType rut_asset_type;
typedef struct _RutAsset RutAsset;
#define RUT_ASSET(X) ((RutAsset *)X)

void _rut_asset_type_init (void);

RutAsset *
rut_asset_new_texture (RutContext *ctx,
                       const char *path);

RutAsset *
rut_asset_new_normal_map (RutContext *ctx,
                          const char *path);

RutAsset *
rut_asset_new_alpha_mask (RutContext *ctx,
                          const char *path);

RutAssetType
rut_asset_get_type (RutAsset *asset);

const char *
rut_asset_get_path (RutAsset *asset);

CoglTexture *
rut_asset_get_texture (RutAsset *asset);

void
rut_asset_set_inferred_tags (RutAsset *asset,
                             const GList *inferred_tags);

const GList *
rut_asset_get_inferred_tags (RutAsset *asset);

CoglBool
rut_asset_has_tag (RutAsset *asset, const char *tag);

GList *
rut_infer_asset_tags (RutContext *ctx, GFileInfo *info, GFile *asset_file);

#endif /* _RUT_ASSET_H_ */
