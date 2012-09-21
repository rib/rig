#include "rut-type.h"
#include "rut-object.h"

void
rut_object_init (RutObjectProps *object, RutType *type)
{
  object->type = type;
}


