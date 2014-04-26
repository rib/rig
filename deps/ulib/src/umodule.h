#ifndef __ULIB_UMODULE_H
#define __ULIB_UMODULE_H

#include <ulib.h>

#define U_MODULE_IMPORT extern
#ifdef U_OS_WIN32
#define U_MODULE_EXPORT __declspec(dllexport)
#else
#define U_MODULE_EXPORT
#endif

U_BEGIN_DECLS

/*
 * Modules
 */
typedef enum {
	U_MODULE_BIND_LAZY = 0x01,
	U_MODULE_BIND_LOCAL = 0x02,
	U_MODULE_BIND_MASK = 0x03
} UModuleFlags;
typedef struct _UModule UModule;

UModule *u_module_open (const char *file, UModuleFlags flags);
uboolean u_module_symbol (UModule *module, const char *symbol_name,
			  void * *symbol);
const char *u_module_error (void);
uboolean u_module_close (UModule *module);
char *  u_module_build_path (const char *directory, const char *module_name);

extern char *gmodule_libprefix;
extern char *gmodule_libsuffix;

U_END_DECLS

#endif
