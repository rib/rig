#ifndef _RIG_ASSET_H_
#define _RIG_ASSET_H_

typedef enum _RigAssetType {
  RIG_ASSET_TYPE_TEXTURE,
} RigAssetType;

extern RigType rig_asset_type;
typedef struct _RigAsset RigAsset;
#define RIG_ASSET(X) ((RigAsset *)X)

void _rig_asset_type_init (void);

RigAsset *
rig_asset_new_texture (RigContext *ctx,
                       const char *path);

RigAssetType
rig_asset_get_type (RigAsset *asset);

const char *
rig_asset_get_path (RigAsset *asset);

CoglTexture *
rig_asset_get_texture (RigAsset *asset);

#endif /* _RIG_ASSET_H_ */
