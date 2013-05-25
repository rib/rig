/*
 * Rig
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __RIG_LLVM_H__
#define __RIG_LLVM_H__

G_BEGIN_DECLS

typedef struct _RigLLVMModule RigLLVMModule;

/* When we're connected to a slave device we write a native dso for
 * the slave that can be sent over the wire, written and then opened.
 */
RigLLVMModule *
rig_llvm_compile_to_dso (const char *code,
                         char **dso_filename_out,
                         uint8_t **dso_data_out,
                         size_t *dso_len_out);

/* When we run code in the editor we simply rely on the LLVM JIT
 * without needing to write and then open a native dso. */
RigLLVMModule *
rig_llvm_compile_for_jit (const char *code);

G_END_DECLS

#endif /* __RIG_LLVM_H__ */
