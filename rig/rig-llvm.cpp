/* Copyright (c) 2003-2011 University of Illinois at Urbana-Champaign.
 *
 * All rights reserved.
 *
 * Developed by:
 *
 *   LLVM Team
 *
 *    University of Illinois at Urbana-Champaign
 *
 *    http://llvm.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimers.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimers in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the names of the LLVM Team, University of Illinois at
 *       Urbana-Champaign, nor the names of its contributors may be used to
 *       endorse or promote products derived from this Software without specific
 *       prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
 * THE SOFTWARE.
 */

#include <config.h>

#include <dlfcn.h>
#include <unistd.h>

#include <clib.h>

#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendDiagnostic.h>
#include <clang/Frontend/TextDiagnosticBuffer.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/ChainedDiagnosticConsumer.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/FrontendTool/Utils.h>

#if 0
#include <mcld/Environment.h>
#include <mcld/Module.h>
#include <mcld/InputTree.h>
#include <mcld/IRBuilder.h>
#include <mcld/Linker.h>
#include <mcld/LinkerConfig.h>
#include <mcld/LinkerScript.h>

#include <mcld/Support/Path.h>
#endif

#include <llvm/ADT/OwningPtr.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/TargetSelect.h>

#include "rut/rut-object.h"
#include "rut/rut-interfaces.h"
#include "rut/rut-context.h"

#include "rig-llvm.h"

using namespace clang;
using namespace clang::driver;

typedef int (*binding_sym_t)(rut_property_t *property, void *user_data);

static llvm::Module *
compile_code(const char *code, char **tmp_object_file)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // std::string code = code;

    llvm::MemoryBuffer *buf = llvm::MemoryBuffer::getMemBuffer(code);
    FrontendInputFile file(llvm::StringRef("rig-codegen.c"), clang::IK_C);

    OwningPtr<clang::CompilerInvocation> CI(new clang::CompilerInvocation);
    CI->getFrontendOpts().Inputs.push_back(file);

    char *clang_resourcedir = c_build_filename(
        RIG_LLVM_PREFIX, "lib", "clang", RIG_LLVM_VERSION, NULL);

    CI->getHeaderSearchOpts().ResourceDir = clang_resourcedir;

    /* XXX: for some reason these don't seem to have any affect... */
    CI->getHeaderSearchOpts().UseBuiltinIncludes = false;
    CI->getHeaderSearchOpts().UseStandardSystemIncludes = false;

    char *clang_system_includedir =
        c_build_filename(clang_resourcedir, "include", NULL);

    CI->getHeaderSearchOpts().AddPath(
        clang_system_includedir, frontend::System, false, true);

    c_free(clang_system_includedir);

    c_free(clang_resourcedir);

    CI->getHeaderSearchOpts().Verbose = true;

    char *includedir = rut_find_data_file("codegen_includes");
    CI->getHeaderSearchOpts().AddPath(
        includedir, frontend::System, false, true);
    c_free(includedir);

    CI->getPreprocessorOpts().Includes.push_back("stdint.h");
    CI->getPreprocessorOpts().Includes.push_back("stdbool.h");
    CI->getPreprocessorOpts().Includes.push_back("stddef.h");
    CI->getPreprocessorOpts().Includes.push_back("rig-codegen.h");
    CI->getPreprocessorOpts().Includes.push_back("rut-property-bare.h");

    /* TODO: The first time we build some code at runtime we should
     * do a special pass where we set the frontend options
     * ProgramAction = frontend::GeneratePCH so we can generate a pre
     * compiled header to make subsequent compilations faster.
     */

    CI->getPreprocessorOpts().addRemappedFile("rig-codegen.c", buf);

    std::string tmp_filename = "/tmp/rigXXXXXX.o";
    int fd = mkstemps((char *)tmp_filename.c_str(), 2);

    CI->getFrontendOpts().OutputFile = tmp_filename;

    // CI->setLangDefaults(clang::IK_C);

    CI->getCodeGenOpts().setDebugInfo(CodeGenOptions::FullDebugInfo);

    CI->getTargetOpts().Triple = llvm::sys::getProcessTriple();

    clang::CompilerInstance Clang;
    Clang.setInvocation(CI.take());

    Clang.createDiagnostics();

    clang::TextDiagnosticBuffer *diagnostics_buff =
        new clang::TextDiagnosticBuffer;
    clang::ChainedDiagnosticConsumer *chained_diagnostics =
        new clang::ChainedDiagnosticConsumer(
            diagnostics_buff, Clang.getDiagnostics().takeClient());

    Clang.getDiagnostics().setClient(chained_diagnostics);

    if (!Clang.hasDiagnostics())
        return NULL;

    /* XXX: We can't currently avoid writting to an intermediate .o file
     * here even though we are going to pass the data straight through
     * to mclinker.
     *
     * The problem is that it's not currently possible to override
     * CodeGenAction::CreateASTConsumer and create your own raw_ostream
     * for the assembly printer, since the BackendConsumer class is
     * private to that file.
     *
     * Note: we can set the path for the output file via the
     * FrontendOptions.
     */
    OwningPtr<clang::CodeGenAction> Act(new clang::EmitObjAction);
    if (!Clang.ExecuteAction(*Act)) {
        c_print("Failed to execute action\n");
    }

    TextDiagnosticBuffer::const_iterator diag_iterator;

    for (diag_iterator = diagnostics_buff->warn_begin();
         diag_iterator != diagnostics_buff->warn_end();
         ++diag_iterator)
        c_print("Buffer Diangostics: warning: %s\n",
                (*diag_iterator).second.c_str());

    for (diag_iterator = diagnostics_buff->err_begin();
         diag_iterator != diagnostics_buff->err_end();
         ++diag_iterator)
        c_print("Buffer Diangostics: error: %s\n",
                (*diag_iterator).second.c_str());

    /* XXX: I think this now means we need to explicitly delete mod
     * since it's no longer wrapped by an OwningPtr... */
    llvm::Module *mod = Act->takeModule();
#if 0
    mod->dump();
#endif

    close(fd);

    /* XXX: we can't shut down llvm all the while that we might need
     * it again */
    // llvm::llvm_shutdown();

    *tmp_object_file = c_strdup(tmp_filename.c_str());

    return mod;
}

static char *
llvm_link(char *tmp_object_file, uint8_t **dso_data_out, size_t *dso_len_out)
{
    *dso_data_out = NULL;
    return NULL;

#if 0
    void *handle;
    binding_sym_t binding_sym;
    GError *error;

    /*
     * Deal with linking...
     */

    mcld::Initialize();

    mcld::LinkerConfig config;

    std::string triple = llvm::sys::getProcessTriple();
    llvm::Triple TargetTriple(triple);
    config.targets().setTriple(TargetTriple);

    mcld::LinkerScript script;
    mcld::Module module("test", script);
    mcld::IRBuilder builder(module, config);

    config.setCodeGenType(mcld::LinkerConfig::DynObj);

    mcld::Linker linker;
    linker.emulate(script, config);

    mcld::sys::fs::Path path(tmp_object_file);

    builder.ReadInput("rig", path);

    std::string tmp_filename = "/tmp/rigXXXXXX.so";
    int fd = mkstemps((char *)tmp_filename.c_str(), 3);

    if (linker.link(module, builder))
        linker.emit(fd);

    close(fd);

    mcld::Finalize();

    error = NULL;
    if (!c_file_get_contents(tmp_filename.c_str(),
                             (char **)dso_data_out,
                             (size_t *)dso_len_out,
                             &error)) {
        c_warning("Failed to read DSO");
        return NULL;
    }

    return c_strdup(tmp_filename.c_str());
#endif
}

struct _rig_llvm_module_t {
    rut_object_base_t _base;

    llvm::Module *mod;
};

static void
_rig_llvm_module_free(void *object)
{
    rig_llvm_module_t *module = (rig_llvm_module_t *)object;

    delete module->mod;

    rut_object_free(rig_llvm_module_t, object);
}

rut_type_t rig_llvm_module_type;

static void
_rig_llvm_module_init_type(void)
{
    rut_type_init(
        &rig_llvm_module_type, "rig_llvm_module_t", _rig_llvm_module_free);
}

static rig_llvm_module_t *
_rig_llvm_module_new(llvm::Module *mod)
{
    rig_llvm_module_t *module = (rig_llvm_module_t *)rut_object_alloc0(
        rig_llvm_module_t, &rig_llvm_module_type, _rig_llvm_module_init_type);

    module->mod = mod;

    return module;
}

extern "C" {

/* When we're connected to a slave device we write a native dso for
 * the slave that can be sent over the wire, written and then opened.
 */
rig_llvm_module_t *
rig_llvm_compile_to_dso(const char *code,
                        char **dso_filename_out,
                        uint8_t **dso_data_out,
                        size_t *dso_len_out)
{
    rig_llvm_module_t *ret = NULL;
    char *tmp_object_file = NULL;

    llvm::Module *mod = compile_code(code, &tmp_object_file);

    if (mod) {
        ret = _rig_llvm_module_new(mod);

        *dso_filename_out = llvm_link(tmp_object_file, dso_data_out, dso_len_out);
        if (!*dso_filename_out) {
            /* Note: we shouldn't just avoid calling
             * _rig_llvm_module_new in this case otherwise we won't
             * destroy the llvm::Module. */
            rut_object_unref(ret);
            ret = NULL;
        }

        c_free(tmp_object_file);
    }

    return ret;
}

/* When we run code in the editor we simply rely on the LLVM JIT
 * without needing to write and then open a native dso. */
rig_llvm_module_t *
rig_llvm_compile_for_jit(const char *code)
{
    char *tmp_object_file = NULL;

    /* TODO: avoid generating a redundant .o file in this case! */
    llvm::Module *mod = compile_code(code, &tmp_object_file);

    if (mod)
        return _rig_llvm_module_new(mod);
    else
        return NULL;
}

} /* extern "C" */
