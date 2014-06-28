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
} UModuleFlags;
typedef struct _UModule UModule;

UModule *c_module_open(const char *file, UModuleFlags flags);
bool c_module_symbol(UModule *module, const char *symbol_name, void **symbol);
const char *c_module_error(void);
bool c_module_close(UModule *module);
char *c_module_build_path(const char *directory, const char *module_name);

extern char *gmodule_libprefix;
extern char *gmodule_libsuffix;

C_END_DECLS

#endif
