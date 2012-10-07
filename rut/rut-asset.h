#ifndef _RUT_ASSET_H_
#define _RUT_ASSET_H_

typedef enum _RutAssetType {
  RUT_ASSET_TYPE_TEXTURE,
  RUT_ASSET_TYPE_NORMAL_MAP,
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

RutAssetType
rut_asset_get_type (RutAsset *asset);

const char *
rut_asset_get_path (RutAsset *asset);

CoglTexture *
rut_asset_get_texture (RutAsset *asset);

void
rut_asset_set_directory_tags (RutAsset *asset,
                              GList *directory_tags);

CoglBool
rut_asset_has_tag (RutAsset *asset, const char *tag);

#endif /* _RUT_ASSET_H_ */
