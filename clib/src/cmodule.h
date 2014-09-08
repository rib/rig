#ifndef __CLIB_UMODULE_H
#define __CLIB_UMODULE_H

#include <clib.h>

#define C_MODULE_IMPORT extern
#ifdef C_OS_WIN32
#define C_MODULE_EXPORT __declspec(dllexport)
#else
#define C_MODULE_EXPORT
#endif

C_BEGIN_DECLS

/*
 * Modules
 */
typedef enum {
    C_MODULE_BIND_LAZY = 0x01,
    C_MODULE_BIND_LOCAL = 0x02,
    C_MODULE_BIND_MASK = 0x03
} u_module_flags_t;
typedef struct _c_module_t c_module_t;

c_module_t *c_module_open(const char *file, u_module_flags_t flags);
bool c_module_symbol(c_module_t *module, const char *symbol_name, void **symbol);
const char *c_module_error(void);
bool c_module_close(c_module_t *module);
char *c_module_build_path(const char *directory, const char *module_name);

C_END_DECLS

#endif
