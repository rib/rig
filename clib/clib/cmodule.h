#ifndef __CLIB_UMODULE_H
#define __CLIB_UMODULE_H

#include <clib.h>

#define C_MODULE_IMPORT extern
#ifdef WIN32
#define C_MODULE_EXPORT __declspec(dllexport)
#else
#define C_MODULE_EXPORT
#endif

C_BEGIN_DECLS

typedef struct _c_module_t c_module_t;

c_module_t *c_module_open(const char *file);
bool c_module_symbol(c_module_t *module, const char *symbol_name, void **symbol);
void c_module_close(c_module_t *module);
char *c_module_build_path(const char *directory, const char *module_name);

C_END_DECLS

#endif
