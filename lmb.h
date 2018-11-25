/*
 * Logical memory blocks.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 * Copyright (C) 2018 Andrei Warkentin <andrey.warkentin@gmail.com>
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

#ifndef LMB_H
#define LMB_H

/* Each lmb property has a tag associated with it. */
#define _LMB_TAG(a, b, c, d) \
	((uint32_t) d << 24 | (uint32_t) c << 16 | (uint32_t) b << 8 | a)

#define LMB_TAG_NONE _LMB_TAG('N','O','N','E')

#define LMB_TAG(string)							\
	(!string ? LMB_TAG_NONE :					\
	 sizeof(string) > 4 ? _LMB_TAG(string[0], string[1], string[2], string[3]) : \
	 sizeof(string) > 3 ? _LMB_TAG(string[0], string[1], string[2], 0) : \
	 sizeof(string) > 2 ? _LMB_TAG(string[0], string[1], 0, 0) :	\
	 sizeof(string) > 1 ? _LMB_TAG(string[0], 0, 0, 0) :		\
	 LMB_TAG_NONE)

typedef uint32_t lmb_tag_t;
typedef uint32_t lmb_type_t;

#define MAX_LMB_REGIONS 32

struct lmb_property {
	phys_addr_t base;
	size_t size;
	lmb_tag_t   tag;
	lmb_type_t  type;

	/* Shouldn't ever be set to this. */
#define LMB_INVALID (0)

	/* Memory range is free. */
#define LMB_FREE    (1)

	/* Memory can be freed after kernel boot. */
#define LMB_BOOT    (2)

	/* Memory must be reserved and unused after kernel boot. */
#define LMB_RUNTIME (3)

	/*
	 * I/O memory must be reserved and unused after kernel boot.
	 *
	 * Regions of this type can only be manipulated using
	 * lmb_add_mmio - not allocated or freed.
	 */
#define LMB_MMIO    (4)
};

struct lmb_region {
	unsigned long cnt;
	struct lmb_property region[MAX_LMB_REGIONS+1];
};

struct lmb {
	struct lmb_region memory;
	struct lmb_region reserved;
};

extern struct lmb lmb;

void lmb_init(struct lmb *lmb);
long lmb_add(struct lmb *lmb,
	     phys_addr_t base,
	     size_t size,
	     lmb_tag_t tag);
long lmb_add_mmio(struct lmb *lmb,
		  phys_addr_t base,
		  size_t size,
		  lmb_tag_t tag);
long lmb_reserve(struct lmb *lmb,
		 phys_addr_t base,
		 size_t size,
		 lmb_type_t type,
		 lmb_tag_t tag);
phys_addr_t lmb_alloc(struct lmb *lmb,
		      size_t size,
		      size_t align,
		      lmb_type_t type,
		      lmb_tag_t tag);

#define LMB_ALLOC_ANYWHERE 0
#define LMB_ALLOC_32BIT    0xffffffff
phys_addr_t lmb_alloc_base(struct lmb *lmb,
			   size_t size,
			   size_t align,
			   phys_addr_t max_addr,
			   lmb_type_t type,
			   lmb_tag_t tag);
phys_addr_t __lmb_alloc_base(struct lmb *lmb,
			     size_t size,
			     size_t align,
			     phys_addr_t max_addr,
			     lmb_type_t type,
			     lmb_tag_t tag);
int lmb_is_reserved(struct lmb *lmb, phys_addr_t addr);
int lmb_is_known(struct lmb *lmb,
		 phys_addr_t addr,
		 size_t size);
long lmb_free(struct lmb *lmb,
	      phys_addr_t base,
	      size_t size,
	      size_t align);

void lmb_dump_all(struct lmb *lmb);

long lmb_overlaps_region(struct lmb_region *rgn,
			 phys_addr_t base,
			 size_t size);

static inline size_t
lmb_size_bytes(struct lmb_region *type, unsigned long region_nr)
{
	return type->region[region_nr].size;
}

#endif /* LMB_H */
