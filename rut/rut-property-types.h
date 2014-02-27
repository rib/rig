
SCALAR_TYPE(float, float, FLOAT)
SCALAR_TYPE(double, double, DOUBLE)
SCALAR_TYPE(integer, int, INTEGER)
SCALAR_TYPE(enum, int, ENUM)
SCALAR_TYPE(uint32, uint32_t, UINT32)
SCALAR_TYPE(boolean, bool, BOOLEAN)

POINTER_TYPE(object, RutObject *, OBJECT)
POINTER_TYPE(asset, RigAsset *, ASSET)
POINTER_TYPE(pointer, void *, POINTER)

COMPOSITE_TYPE(quaternion, CoglQuaternion, QUATERNION)
COMPOSITE_TYPE(color, CoglColor, COLOR)

ARRAY_TYPE(vec3, float, VEC3, 3)
ARRAY_TYPE(vec4, float, VEC4, 4)
