#ifndef _RUT_DROP_DOWN_H_
#define _RUT_DROP_DOWN_H_

#include <cogl/cogl.h>

#include "rut.h"

extern RutType rut_drop_down_type;

typedef struct _RutDropDown RutDropDown;

#define RUT_DROP_DOWN(x) ((RutDropDown *) x)

typedef struct
{
  const char *name;
  int value;
} RutDropDownValue;

RutDropDown *
rut_drop_down_new (RutContext *ctx);

void
rut_drop_down_set_value (RutDropDown *slider,
                         int value);

int
rut_drop_down_get_value (RutDropDown *slider);

void
rut_drop_down_set_values (RutDropDown *drop,
                          ...) G_GNUC_NULL_TERMINATED;

void
rut_drop_down_set_values_array (RutDropDown *drop,
                                const RutDropDownValue *values,
                                int n_values);

#endif /* _RUT_DROP_DOWN_H_ */
