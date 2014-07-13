/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __RIG_LLVM_H__
#define __RIG_LLVM_H__

C_BEGIN_DECLS

typedef struct _rig_llvm_module_t rig_llvm_module_t;

/* When we're connected to a slave device we write a native dso for
 * the slave that can be sent over the wire, written and then opened.
 */
rig_llvm_module_t *rig_llvm_compile_to_dso(const char *code,
                                           char **dso_filename_out,
                                           uint8_t **dso_data_out,
                                           size_t *dso_len_out);

/* When we run code in the editor we simply rely on the LLVM JIT
 * without needing to write and then open a native dso. */
rig_llvm_module_t *rig_llvm_compile_for_jit(const char *code);

C_END_DECLS

#endif /* __RIG_LLVM_H__ */
