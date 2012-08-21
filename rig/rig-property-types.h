
SCALAR_TYPE(float, float, FLOAT)
SCALAR_TYPE(double, double, DOUBLE)
SCALAR_TYPE(integer, int, INTEGER)
SCALAR_TYPE(enum, int, ENUM)
SCALAR_TYPE(uint32, uint32_t, UINT32)
SCALAR_TYPE(boolean, CoglBool, BOOLEAN)

POINTER_TYPE(object, RigObject *, OBJECT)
POINTER_TYPE(pointer, void *, POINTER)

COMPOSITE_TYPE(quaternion, CoglQuaternion, QUATERNION)
COMPOSITE_TYPE(color, RigColor, COLOR)

ARRAY_TYPE(vec3, float, VEC3, 3)
ARRAY_TYPE(vec4, float, VEC4, 4)
