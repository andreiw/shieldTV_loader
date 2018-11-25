/*
 * Various useful macros.
 *
 * Copyright (C) 2015 Andrei Warkentin <andrey.warkentin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef DEFS_H
#define DEFS_H

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>

typedef uint64_t size_t;
typedef uint64_t ptrdiff_t;
typedef _Bool bool_t;
typedef uint64_t phys_addr_t;

#define min(x,y) ({				\
			typeof(x) _x = (x);	\
			typeof(y) _y = (y);	\
			(void) (&_x == &_y);	\
			_x < _y ? _x : _y; })

#define max(x,y) ({				\
			typeof(x) _x = (x);	\
			typeof(y) _y = (y);	\
			(void) (&_x == &_y);	\
			_x > _y ? _x : _y; })

#define NULL ((void *) 0)
#define BITS_PER_LONG 64
#define PAGE_SIZE     4096

#define BIT(x)    (1ull << (x))

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define USHRT_MAX       ((uint16_t)(~0U))
#define SHRT_MAX        ((int16_t)(USHRT_MAX>>1))
#define SHRT_MIN        ((int16_t)(-SHRT_MAX - 1))
#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN         (-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN        (-LONG_MAX - 1)
#define ULONG_MAX       (~0UL)

#define likely(x)     (__builtin_constant_p(x) ? !!(x) : __builtin_expect(!!(x), 1))
#define unlikely(x)   (__builtin_constant_p(x) ? !!(x) : __builtin_expect(!!(x), 0))

#define MB(x) (1UL * x * 1024 * 1024)
#define TB(x) (1UL * x * 1024 * 1024 * 1024 * 1024)

#define __packed                __attribute__((packed))
#define __align(x)              __attribute__((__aligned__(x)))
#define __unused                __attribute__((unused))
#define __used                  __attribute__((used))
#define __section(x)            __attribute__((__section__(x)))
#define __noreturn              __attribute__((noreturn))
#define __attrconst             __attribute__((const))
#define __warn_unused_result    __attribute__((warn_unused_result))
#define __alwaysinline          __attribute__((always_inline))

#define do_div(n,base) ({\
  uint32_t __base = (base);\
  uint32_t __rem;\
  __rem = ((uint64_t)(n)) % __base;\
  (n) = ((uint64_t)(n)) / __base;\
  __rem;\
    })

static inline uint16_t
swab16(uint16_t x)
{
	return  ((x & (uint16_t)0x00ffU) << 8) |
		((x & (uint16_t)0xff00U) >> 8);
}

static inline uint32_t
swab32(uint32_t x)
{
	return  ((x & (uint32_t)0x000000ffUL) << 24) |
		((x & (uint32_t)0x0000ff00UL) <<  8) |
		((x & (uint32_t)0x00ff0000UL) >>  8) |
		((x & (uint32_t)0xff000000UL) >> 24);
}

static inline uint64_t
swab64(uint64_t x)
{
	return  (uint64_t)((x & (uint64_t)0x00000000000000ffULL) << 56) |
		(uint64_t)((x & (uint64_t)0x000000000000ff00ULL) << 40) |
		(uint64_t)((x & (uint64_t)0x0000000000ff0000ULL) << 24) |
		(uint64_t)((x & (uint64_t)0x00000000ff000000ULL) <<  8) |
		(uint64_t)((x & (uint64_t)0x000000ff00000000ULL) >>  8) |
		(uint64_t)((x & (uint64_t)0x0000ff0000000000ULL) >> 24) |
		(uint64_t)((x & (uint64_t)0x00ff000000000000ULL) >> 40) |
		(uint64_t)((x & (uint64_t)0xff00000000000000ULL) >> 56);
}

#define cpu_to_be64(x) swab64(x)
#define cpu_to_be32(x) swab32(x)
#define cpu_to_be16(x) swab16(x)
#define cpu_to_le64(x) ((uint64_t) x)
#define cpu_to_le32(x) ((uint32_t) x)
#define cpu_to_le16(x) ((uint16_t) x)
#define be64_to_cpu(x) swab64(x)
#define be32_to_cpu(x) swab32(x)
#define be16_to_cpu(x) swab16(x)
#define le64_to_cpu(x) ((uint64_t) x)
#define le32_to_cpu(x) ((uint32_t) x)
#define le16_to_cpu(x) ((uint16_t) x)

#define _IX_BITS(sm, bg) ((bg) - (sm) + 1)
#define _IX_MASK(sm, bg) ((1ul << _IX_BITS((sm), (bg))) - 1)
#define _INTERNAL_X(val, sm, bg) ((val) >> (sm)) & _IX_MASK((sm), (bg))
#define X(val, ix1, ix2) (((ix1) < (ix2)) ? _INTERNAL_X((val), (ix1), (ix2)) :   \
                          _INTERNAL_X((val), (ix2), (ix1)))

#define _INTERNAL_I(val, sm, bg)  (((val) & _IX_MASK((sm), (bg))) << (sm))
#define I(val, ix1, ix2) (((ix1) < (ix2)) ? _INTERNAL_I((val), (ix1), (ix2)) :   \
                          _INTERNAL_I((val), (ix2), (ix1)))
#define _INTERNAL_M(val, sm, bg)  ((val) & (_IX_MASK((sm), (bg)) << (sm)))
#define M(val, ix1, ix2) (((ix1) < (ix2)) ? _INTERNAL_M((val), (ix1), (ix2)) :   \
                          _INTERNAL_M((val), (ix2), (ix1)))

#define A_UP(Value, Alignment)  (((Value) + (Alignment) - 1) & (~((Alignment) - 1)))
#define PA_UP(p, align) ((typeof(p)) A_UP(((uintptr_t) (p)), align))
#define A_DOWN(Value, Alignment) ((Value) & (~((Alignment) - 1)))

#define IS_ALIGNED(Value, Alignment) (((UINTN) (Value) & (Alignment - 1)) == 0)

#define VP(x) ((void *)(x))
#define U8P(x) ((uint8_t *)(x))
#define U16P(x) ((uint16_t *)(x))
#define U32P(x) ((uint32_t *)(x))
#define U64P(x) ((uint64_t *)(x))
#define UN(x) ((uintptr_t)(x))

#define ELES(x) (sizeof((x)) / sizeof((x)[0]))

#define _INTERNAL_S(x) #x
#define S(x) _INTERNAL_S(x)

#define C_ASSERT(e) { typedef char __C_ASSERT__[(e)?0:-1]; __C_ASSERT__ c; (void)c; }

#endif /* DEFS_H */
