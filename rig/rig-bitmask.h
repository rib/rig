/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_BITMASK_H
#define __COGL_BITMASK_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * RigBitmask implements a growable array of bits. A RigBitmask can
 * be allocated on the stack but it must be initialised with
 * _rig_bitmask_init() before use and then destroyed with
 * _rig_bitmask_destroy(). A RigBitmask will try to avoid allocating
 * any memory unless more than the number of bits in a long - 1 bits
 * are needed.
 *
 * Internally a RigBitmask is a pointer. If the least significant bit
 * of the pointer is 1 then the rest of the bits are directly used as
 * part of the bitmask, otherwise it is a pointer to a GArray of
 * unsigned ints. This relies on the fact the g_malloc will return a
 * pointer aligned to at least two bytes (so that the least
 * significant bit of the address is always 0). It also assumes that
 * the size of a pointer is always greater than or equal to the size
 * of a long (although there is a compile time assert to verify this).
 *
 * If the maximum possible bit number in the set is known at compile
 * time, it may make more sense to use the macros in cogl-flags.h
 * instead of this type.
 */

typedef struct _RigBitmaskImaginaryType *RigBitmask;

/* These are internal helper macros */
#define _rig_bitmask_to_number(bitmask) \
  ((unsigned long) (*bitmask))
#define _rig_bitmask_to_bits(bitmask) \
  (_rig_bitmask_to_number (bitmask) >> 1UL)
/* The least significant bit is set to mark that no array has been
   allocated yet */
#define _rig_bitmask_from_bits(bits) \
  ((void *) ((((unsigned long) (bits)) << 1UL) | 1UL))

/* Internal helper macro to determine whether this bitmask has a
   GArray allocated or whether the pointer is just used directly */
#define _rig_bitmask_has_array(bitmask) \
  (!(_rig_bitmask_to_number (bitmask) & 1UL))

/* Number of bits we can use before needing to allocate an array */
#define COGL_BITMASK_MAX_DIRECT_BITS (sizeof (unsigned long) * 8 - 1)

/*
 * _rig_bitmask_init:
 * @bitmask: A pointer to a bitmask
 *
 * Initialises the cogl bitmask. This must be called before any other
 * bitmask functions are called. Initially all of the values are
 * zero
 */
#define _rig_bitmask_init(bitmask) \
  G_STMT_START { *(bitmask) = _rig_bitmask_from_bits (0); } G_STMT_END

gboolean
_rig_bitmask_get_from_array (const RigBitmask *bitmask,
                              unsigned int bit_num);

void
_rig_bitmask_set_in_array (RigBitmask *bitmask,
                            unsigned int bit_num,
                            gboolean value);

void
_rig_bitmask_set_range_in_array (RigBitmask *bitmask,
                                  unsigned int n_bits,
                                  gboolean value);

void
_rig_bitmask_clear_all_in_array (RigBitmask *bitmask);

void
_rig_bitmask_set_flags_array (const RigBitmask *bitmask,
                              unsigned long *flags);

int
_rig_bitmask_popcount_in_array (const RigBitmask *bitmask);

int
_rig_bitmask_popcount_upto_in_array (const RigBitmask *bitmask,
                                     int upto);

/*
 * cogl_bitmask_set_bits:
 * @dst: The bitmask to modify
 * @src: The bitmask to copy bits from
 *
 * This makes sure that all of the bits that are set in @src are also
 * set in @dst. Any unset bits in @src are left alone in @dst.
 */
void
_rig_bitmask_set_bits (RigBitmask *dst,
                       const RigBitmask *src);

/*
 * cogl_bitmask_xor_bits:
 * @dst: The bitmask to modify
 * @src: The bitmask to copy bits from
 *
 * For every bit that is set in src, the corresponding bit in dst is
 * inverted.
 */
void
_rig_bitmask_xor_bits (RigBitmask *dst,
                       const RigBitmask *src);

/* The foreach function can return FALSE to stop iteration */
typedef gboolean (* RigBitmaskForeachFunc) (int bit_num, void *user_data);

/*
 * cogl_bitmask_foreach:
 * @bitmask: A pointer to a bitmask
 * @func: A callback function
 * @user_data: A pointer to pass to the callback
 *
 * This calls @func for each bit that is set in @bitmask.
 */
void
_rig_bitmask_foreach (const RigBitmask *bitmask,
                      RigBitmaskForeachFunc func,
                      void *user_data);

/*
 * _rig_bitmask_equal:
 * @a: The first #RigBitmask to compare
 * @b: The second #RigBitmask to compare
 *
 * Returns %TRUE if the bitmask @a is equal to bitmask @b else returns
 * %FALSE.
 */
gboolean
_rig_bitmask_equal (const RigBitmask *a,
                    const RigBitmask *b);

/*
 * _rig_bitmask_get:
 * @bitmask: A pointer to a bitmask
 * @bit_num: A bit number
 *
 * Return value: whether bit number @bit_num is set in @bitmask
 */
static inline gboolean
_rig_bitmask_get (const RigBitmask *bitmask, unsigned int bit_num)
{
  if (_rig_bitmask_has_array (bitmask))
    return _rig_bitmask_get_from_array (bitmask, bit_num);
  else if (bit_num >= COGL_BITMASK_MAX_DIRECT_BITS)
    return FALSE;
  else
    return !!(_rig_bitmask_to_bits (bitmask) & (1UL << bit_num));
}

/*
 * _rig_bitmask_set:
 * @bitmask: A pointer to a bitmask
 * @bit_num: A bit number
 * @value: The new value
 *
 * Sets or resets a bit number @bit_num in @bitmask according to @value.
 */
static inline void
_rig_bitmask_set (RigBitmask *bitmask, unsigned int bit_num, gboolean value)
{
  if (_rig_bitmask_has_array (bitmask) ||
      bit_num >= COGL_BITMASK_MAX_DIRECT_BITS)
    _rig_bitmask_set_in_array (bitmask, bit_num, value);
  else if (value)
    *bitmask = _rig_bitmask_from_bits (_rig_bitmask_to_bits (bitmask) |
                                        (1UL << bit_num));
  else
    *bitmask = _rig_bitmask_from_bits (_rig_bitmask_to_bits (bitmask) &
                                        ~(1UL << bit_num));
}

/*
 * _rig_bitmask_set_range:
 * @bitmask: A pointer to a bitmask
 * @n_bits: The number of bits to set
 * @value: The value to set
 *
 * Sets the first @n_bits in @bitmask to @value.
 */
static inline void
_rig_bitmask_set_range (RigBitmask *bitmask,
                        unsigned int n_bits,
                        gboolean value)
{
  if (_rig_bitmask_has_array (bitmask) ||
      n_bits > COGL_BITMASK_MAX_DIRECT_BITS)
    _rig_bitmask_set_range_in_array (bitmask, n_bits, value);
  else if (value)
    *bitmask = _rig_bitmask_from_bits (_rig_bitmask_to_bits (bitmask) |
                                       ~(~0UL << n_bits));
  else
    *bitmask = _rig_bitmask_from_bits (_rig_bitmask_to_bits (bitmask) &
                                       (~0UL << n_bits));
}

/*
 * _rig_bitmask_destroy:
 * @bitmask: A pointer to a bitmask
 *
 * Destroys any resources allocated by the bitmask
 */
static inline void
_rig_bitmask_destroy (RigBitmask *bitmask)
{
  if (_rig_bitmask_has_array (bitmask))
    g_array_free ((GArray *) *bitmask, TRUE);
}

/*
 * _rig_bitmask_clear_all:
 * @bitmask: A pointer to a bitmask
 *
 * Clears all the bits in a bitmask without destroying any resources.
 */
static inline void
_rig_bitmask_clear_all (RigBitmask *bitmask)
{
  if (_rig_bitmask_has_array (bitmask))
    _rig_bitmask_clear_all_in_array (bitmask);
  else
    *bitmask = _rig_bitmask_from_bits (0);
}

/*
 * _rig_bitmask_set_flags:
 * @bitmask: A pointer to a bitmask
 * @flags: An array of flags
 *
 * Bitwise or's the bits from @bitmask into the flags array (see
 * cogl-flags) pointed to by @flags.
 */
static inline void
_rig_bitmask_set_flags (const RigBitmask *bitmask,
                        unsigned long *flags)
{
  if (_rig_bitmask_has_array (bitmask))
    _rig_bitmask_set_flags_array (bitmask, flags);
  else
    flags[0] |= _rig_bitmask_to_bits (bitmask);
}

/*
 * _rig_bitmask_popcount:
 * @bitmask: A pointer to a bitmask
 *
 * Counts the number of bits that are set in the bitmask.
 *
 * Return value: the number of bits set in @bitmask.
 */
static inline int
_rig_bitmask_popcount (const RigBitmask *bitmask)
{
  return (_rig_bitmask_has_array (bitmask) ?
          _rig_bitmask_popcount_in_array (bitmask) :
          __builtin_popcountl (_rig_bitmask_to_bits (bitmask)));
}

/*
 * _rig_bitmask_popcount:
 * @Bitmask: A pointer to a bitmask
 * @upto: The maximum bit index to consider
 *
 * Counts the number of bits that are set and have an index which is
 * less than @upto.
 *
 * Return value: the number of bits set in @bitmask that are less than @upto.
 */
static inline int
_rig_bitmask_popcount_upto (const RigBitmask *bitmask,
                            int upto)
{
  if (_rig_bitmask_has_array (bitmask))
    return _rig_bitmask_popcount_upto_in_array (bitmask, upto);
  else if (upto >= COGL_BITMASK_MAX_DIRECT_BITS)
    return __builtin_popcountl (_rig_bitmask_to_bits (bitmask));
  else
    return __builtin_popcountl (_rig_bitmask_to_bits (bitmask) &
                                ((1UL << upto) - 1));
}

G_END_DECLS

#endif /* __COGL_BITMASK_H */
