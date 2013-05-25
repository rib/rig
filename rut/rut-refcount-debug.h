/*
 * Rut
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

#ifndef _RUT_REFCOUNT_DEBUG_H_
#define _RUT_REFCOUNT_DEBUG_H_

#include <glib.h>

G_BEGIN_DECLS

#ifdef RUT_ENABLE_REFCOUNT_DEBUG

void
_rut_refcount_debug_object_created (void *object);

void
_rut_refcount_debug_ref (void *object);

void
_rut_refcount_debug_claim (void *object, void *owner);

void
_rut_refcount_debug_unref (void *object);

void
_rut_refcount_debug_release (void *object, void *owner);

#else /* RUT_ENABLE_REFCOUNT_DEBUG */

/* If recounting isn't enabled then we'll just no-op the debugging
 * code so that it won't be a performance burden */

#define _rut_refcount_debug_object_created(o) G_STMT_START { } G_STMT_END

#define _rut_refcount_debug_ref(o) G_STMT_START { } G_STMT_END
#define _rut_refcount_debug_claim(o, owner) G_STMT_START { } G_STMT_END

#define _rut_refcount_debug_unref(o) G_STMT_START { } G_STMT_END
#define _rut_refcount_debug_release(o, owner) G_STMT_START { } G_STMT_END

#endif /* RUT_ENABLE_REFCOUNT_DEBUG */

G_END_DECLS

#endif /* _RUT_REFCOUNT_DEBUG_H_ */

