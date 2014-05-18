/*
 * gmodule.c: dl* functions, glib style
 *
 * Author:
 *   Gonzalo Paniagua Javier (gonzalo@novell.com)
 *   Jonathan Chambers (joncham@gmail.com)
 *   Robert Jordan (robertj@gmx.net)
 *
 * (C) 2006 Novell, Inc.
 * (C) 2006 Jonathan Chambers
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <config.h>

#include <clib.h>
#include <gmodule.h>
#include <windows.h>
#include <psapi.h>

#define LIBSUFFIX ".dll"
#define LIBPREFIX ""

struct _UModule {
	HMODULE handle;
	int main_module;
};

UModule *
c_module_open (const char *file, UModuleFlags flags)
{
	UModule *module;
	module = c_malloc (sizeof (UModule));
	if (module == NULL)
		return NULL;

	if (file != NULL) {
		cunichar2 *file16;
		file16 = u8to16(file); 
		module->main_module = FALSE;
		module->handle = LoadLibraryW (file16);
		c_free(file16);
		if (!module->handle) {
			c_free (module);
			return NULL;
		}
			
	} else {
		module->main_module = TRUE;
		module->handle = UetModuleHandle (NULL);
	}

	return module;
}

static void *
w32_find_symbol (const char *symbol_name)
{
	HMODULE *modules;
	DWORD buffer_size = sizeof (HMODULE) * 1024;
	DWORD needed, i;

	modules = (HMODULE *) c_malloc (buffer_size);

	if (modules == NULL)
		return NULL;

	if (!EnumProcessModules (UetCurrentProcess (), modules,
				 buffer_size, &needed)) {
		c_free (modules);
		return NULL;
	}

	/* check whether the supplied buffer was too small, realloc, retry */
	if (needed > buffer_size) {
		c_free (modules);

		buffer_size = needed;
		modules = (HMODULE *) c_malloc (buffer_size);

		if (modules == NULL)
			return NULL;

		if (!EnumProcessModules (UetCurrentProcess (), modules,
					 buffer_size, &needed)) {
			c_free (modules);
			return NULL;
		}
	}

	for (i = 0; i < needed / sizeof (HANDLE); i++) {
		void * proc = (void *)(intptr_t)UetProcAddress (modules [i], symbol_name);
		if (proc != NULL) {
			c_free (modules);
			return proc;
		}
	}

	c_free (modules);
	return NULL;
}

cboolean
c_module_symbol (UModule *module, const char *symbol_name, void * *symbol)
{
	if (module == NULL || symbol_name == NULL || symbol == NULL)
		return FALSE;

	if (module->main_module) {
		*symbol = (void *)(intptr_t)UetProcAddress (module->handle, symbol_name);
		if (*symbol != NULL)
			return TRUE;

		*symbol = w32_find_symbol (symbol_name);
		return *symbol != NULL;
	} else {
		*symbol = (void *)(intptr_t)UetProcAddress (module->handle, symbol_name);
		return *symbol != NULL;
	}
}

const char *
c_module_error (void)
{
	char* ret = NULL;
	TCHAR* buf = NULL;
	DWORD code = UetLastError ();

	/* FIXME: buf must not be NULL! */
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 
		code, MAKELANGID(LANC_NEUTRAL, SUBLANC_DEFAULT), buf, 0, NULL);

	ret = u16to8 (buf);
	LocalFree(buf);

	return ret;
}

cboolean
c_module_close (UModule *module)
{
	HMODULE handle;
	int main_module;

	if (module == NULL || module->handle == NULL)
		return FALSE;

	handle = module->handle;
	main_module = module->main_module;
	module->handle = NULL;
	c_free (module);
	return (main_module ? 1 : (0 == FreeLibrary (handle)));
}

char *
c_module_build_path (const char *directory, const char *module_name)
{
	char *lib_prefix = "";
	
	if (module_name == NULL)
		return NULL;

	if (strncmp (module_name, "lib", 3) != 0)
		lib_prefix = LIBPREFIX;
	
	if (directory && *directory){ 
		
		return c_strdup_printf ("%s/%s%s" LIBSUFFIX, directory, lib_prefix, module_name);
	}
	return c_strdup_printf ("%s%s" LIBSUFFIX, lib_prefix, module_name); 
}
