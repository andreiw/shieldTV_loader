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

#include <lib.h>
#include <lmb.h>

static char *
lmb_type(lmb_type_t type)
{
	switch(type) {
	case LMB_FREE:
		return "FREE";
	case LMB_BOOT:
		return "BOOT";
	case LMB_RUNTIME:
		return "RUNTIME";
	case LMB_MMIO:
		return "MMIO";
	}

	return "INVALID";
}

void
lmb_dump_all(struct lmb *lmb)
{
	unsigned long i;

	for (i=0; i < lmb->memory.cnt ;i++) {
		printk("    memory.reg[0x%lx]   = 0x%016lx-0x%016lx (%.4s, %s)\n", i,
		       lmb->memory.region[i].base,
		       lmb->memory.region[i].base +
		       lmb->memory.region[i].size - 1,
		       (char *) &lmb->memory.region[i].tag,
		       lmb_type(lmb->memory.region[i].type));
	}

	for (i=0; i < lmb->reserved.cnt ;i++) {
		printk("    reserved.reg[0x%lx] = 0x%016lx @ 0x%016lx (%.4s, %s)\n", i,
		       lmb->reserved.region[i].base,
		       lmb->reserved.region[i].base +
		       lmb->reserved.region[i].size - 1,
		       (char *) &lmb->reserved.region[i].tag,
		       lmb_type(lmb->reserved.region[i].type));
	}
}

static long
lmb_addrs_overlap(phys_addr_t base1,
			      size_t size1,
			      phys_addr_t base2,
			      size_t size2)
{
	return ((base1 < (base2+size2)) && (base2 < (base1+size1)));
}

static long
lmb_addrs_adjacent(phys_addr_t base1,
		   size_t size1,
		   phys_addr_t base2,
		   size_t size2)
{
	if (base2 == base1 + size1) {
		return 1;
	} else if (base1 == base2 + size2) {
		return -1;
	}

	return 0;
}

static long
lmb_regions_adjacent(struct lmb_region *rgn,
		     unsigned long r1,
		     unsigned long r2)
{
	phys_addr_t base1 = rgn->region[r1].base;
	size_t size1 = rgn->region[r1].size;
	phys_addr_t base2 = rgn->region[r2].base;
	size_t size2 = rgn->region[r2].size;

	return lmb_addrs_adjacent(base1, size1, base2, size2);
}

static void
lmb_remove_region(struct lmb_region *rgn,
		  unsigned long r)
{
	unsigned long i;

	for (i = r; i < rgn->cnt - 1; i++) {
		rgn->region[i].base = rgn->region[i + 1].base;
		rgn->region[i].size = rgn->region[i + 1].size;
		rgn->region[i].tag = rgn->region[i + 1].tag;
		rgn->region[i].type = rgn->region[i + 1].type;
	}

	rgn->cnt--;
}

/*
 * Assumption: base addr of region 1 < base addr of region 2
 * and tag and type are matching
 */
static void
lmb_coalesce_regions(struct lmb_region *rgn,
		     unsigned long r1,
		     unsigned long r2)
{
	rgn->region[r1].size += rgn->region[r2].size;

	lmb_remove_region(rgn, r2);
}

void
lmb_init(struct lmb *lmb)
{
	/*
	 * Create a dummy zero size LMB which will get coalesced away later.
	 * This simplifies the lmb_add() code below...
	 */
	lmb->memory.region[0].base = 0;
	lmb->memory.region[0].size = 0;
	lmb->memory.region[0].type = LMB_INVALID;
	lmb->memory.region[0].tag = LMB_TAG_NONE;
	lmb->memory.cnt = 1;

	/* Ditto. */
	lmb->reserved.region[0].base = 0;
	lmb->reserved.region[0].size = 0;
	lmb->reserved.region[0].type = LMB_INVALID;
	lmb->reserved.region[0].tag = LMB_TAG_NONE;
	lmb->reserved.cnt = 1;
}

static long
lmb_add_region(struct lmb_region *rgn,
	       phys_addr_t base,
	       size_t size,
	       lmb_type_t type,
	       lmb_tag_t tag)
{
	unsigned long coalesced = 0;
	long adjacent, i;

	if ((rgn->cnt == 1) && (rgn->region[0].size == 0)) {
		rgn->region[0].base = base;
		rgn->region[0].size = size;
		rgn->region[0].tag = tag;
		rgn->region[0].type = type;
		return 0;
	}

	/* First try and coalesce this LMB with another. */
	for (i=0; i < rgn->cnt; i++) {
		phys_addr_t rgnbase = rgn->region[i].base;
		size_t rgnsize = rgn->region[i].size;

		if (lmb_addrs_overlap(base, size, rgnbase, rgnsize)) {

			/* No overlapping with existing ones. */
			return -1;
		}

		adjacent = lmb_addrs_adjacent(base,size,rgnbase,rgnsize);
		if ( adjacent > 0 ) {
			if (tag == rgn->region[i].tag &&
			    type == rgn->region[i].type) {
				rgn->region[i].base -= size;
				rgn->region[i].size += size;
				coalesced++;
			}
			break;
		}
		else if ( adjacent < 0 ) {
			if (tag == rgn->region[i].tag &&
			    type == rgn->region[i].type) {
				rgn->region[i].size += size;
				coalesced++;
			}
			break;
		}
	}

	if ((i < rgn->cnt-1) &&
	    lmb_regions_adjacent(rgn, i, i+1) &&
	    rgn->region[i].tag == rgn->region[i + 1].tag &&
	    rgn->region[i].type == rgn->region[i + 1].type) {
		lmb_coalesce_regions(rgn, i, i+1);
		coalesced++;
	}

	if (coalesced) {
		return coalesced;
	}

	if (rgn->cnt >= MAX_LMB_REGIONS) {
		return -1;
	}

	/* Couldn't coalesce the LMB, so add it to the sorted table. */
	for (i = rgn->cnt-1; i >= 0; i--) {
		if (base < rgn->region[i].base) {
			rgn->region[i+1].base = rgn->region[i].base;
			rgn->region[i+1].size = rgn->region[i].size;
			rgn->region[i+1].type = rgn->region[i].type;
			rgn->region[i+1].tag = rgn->region[i].tag;
		} else {
			rgn->region[i+1].base = base;
			rgn->region[i+1].size = size;
			rgn->region[i+1].tag = tag;
			rgn->region[i+1].type = type;
			break;
		}
	}

	if (base < rgn->region[0].base) {
		rgn->region[0].base = base;
		rgn->region[0].size = size;
		rgn->region[0].tag = tag;
		rgn->region[0].type = type;
	}

	rgn->cnt++;

	return 0;
}

long
lmb_add(struct lmb *lmb,
	phys_addr_t base,
	size_t size,
	lmb_tag_t tag)
{
	struct lmb_region *_rgn = &(lmb->memory);

	return lmb_add_region(_rgn, base, size, LMB_FREE, tag);
}

long
lmb_add_mmio(struct lmb *lmb,
	     phys_addr_t base,
	     size_t size,
	     lmb_tag_t tag)
{
	long ret;
	struct lmb_region *_rgn = &(lmb->memory);

	ret = lmb_add_region(_rgn, base, size, LMB_MMIO, tag);
	if (ret == 0) {
		lmb_add_region(&(lmb->reserved), base, size, LMB_MMIO, tag);
	}

	return ret;
}

static phys_addr_t
lmb_align_down(phys_addr_t addr,
	       size_t size)
{
	return addr & ~(size - 1);
}

static phys_addr_t
lmb_align_up(phys_addr_t addr,
	     size_t size)
{
	return (addr + size - 1) & ~(size - 1);
}

long
lmb_free(struct lmb *lmb,
	 phys_addr_t base,
	 size_t size,
	 size_t align)
{
	struct lmb_region *rgn = &(lmb->reserved);
	phys_addr_t rgnbegin;
	phys_addr_t rgnend;
	phys_addr_t end;
	size = lmb_align_up(size, align);
	end  = base + size;
	int i;

	rgnbegin = rgnend = 0; /* supress gcc warnings */

	/* Find the region where (base, size) belongs to */
	for (i=0; i < rgn->cnt; i++) {
		rgnbegin = rgn->region[i].base;
		rgnend = rgnbegin + rgn->region[i].size;

		if ((rgnbegin <= base) && (end <= rgnend))
			break;
	}

	/* Didn't find the region */
	if (i == rgn->cnt) {
		return -1;
	}

	if (rgn->region[i].type == LMB_MMIO) {
		return -1;
	}

	/* Check to see if we are removing entire region */
	if ((rgnbegin == base) && (rgnend == end)) {
		lmb_remove_region(rgn, i);
		return 0;
	}

	/* Check to see if region is matching at the front */
	if (rgnbegin == base) {
		rgn->region[i].base = end;
		rgn->region[i].size -= size;
		return 0;
	}

	/* Check to see if the region is matching at the end */
	if (rgnend == end) {
		rgn->region[i].size -= size;
		return 0;
	}

	/*
	 * We need to split the entry -  adjust the current one to the
	 * beginging of the hole and add the region after hole.
	 */
	rgn->region[i].size = base - rgn->region[i].base;
	return lmb_add_region(rgn, end, rgnend - end,
			      rgn->region[i].type,
			      rgn->region[i].tag);
}

long
lmb_reserve(struct lmb *lmb,
	    phys_addr_t base,
	    size_t size,
	    lmb_type_t type,
	    lmb_tag_t tag)
{
	struct lmb_region *_rgn = &(lmb->reserved);

	if (type != LMB_BOOT &&
	    type != LMB_RUNTIME) {
		return -1;
	}

	if (!lmb_is_known(lmb, base, size)) {
		return -1;
	}

	return lmb_add_region(_rgn, base, size, type, tag);
}

long
lmb_overlaps_region(struct lmb_region *rgn,
		    phys_addr_t base,
		    size_t size)
{
	unsigned long i;

	for (i=0; i < rgn->cnt; i++) {
		phys_addr_t rgnbase = rgn->region[i].base;
		size_t rgnsize = rgn->region[i].size;
		if ( lmb_addrs_overlap(base,size,rgnbase,rgnsize) ) {
			break;
		}
	}

	return (i < rgn->cnt) ? i : -1;
}

phys_addr_t
lmb_alloc(struct lmb *lmb,
	  size_t size,
	  size_t align,
	  lmb_type_t type,
	  lmb_tag_t tag)
{
	return lmb_alloc_base(lmb, size, align, LMB_ALLOC_ANYWHERE, type, tag);
}

phys_addr_t
lmb_alloc_base(struct lmb *lmb,
	       size_t size,
	       size_t align,
	       phys_addr_t max_addr,
	       lmb_type_t type,
	       lmb_tag_t tag)
{
	phys_addr_t alloc;

	if (type == LMB_MMIO) {
		return 0;
	}

	alloc = __lmb_alloc_base(lmb, size, align, max_addr, type, tag);
	return alloc;
}

phys_addr_t
__lmb_alloc_base(struct lmb *lmb,
		 size_t size,
		 size_t align,
		 phys_addr_t max_addr,
		 lmb_type_t type,
		 lmb_tag_t tag)
{
	long i, j;
	phys_addr_t base = 0;
	phys_addr_t res_base;
	size = lmb_align_up(size, align);

	for (i = lmb->memory.cnt-1; i >= 0; i--) {
		phys_addr_t lmbbase = lmb->memory.region[i].base;
		size_t lmbsize = lmb->memory.region[i].size;

		if (lmbsize < size)
			continue;
		if (max_addr == LMB_ALLOC_ANYWHERE)
			base = lmb_align_down(lmbbase + lmbsize - size, align);
		else if (lmbbase < max_addr) {
			base = min(lmbbase + lmbsize, max_addr);
			base = lmb_align_down(base - size, align);
		} else {
			continue;
		}

		while (base && lmbbase <= base) {
			j = lmb_overlaps_region(&lmb->reserved, base,
                                                size);
			if (j < 0) {

				/* This area isn't reserved, take it */
				if (lmb_add_region(&lmb->reserved, base,
						   size,
						   type,
						   tag) < 0) {
					return 0;
				}

				return base;
			}

			res_base = lmb->reserved.region[j].base;
			if (res_base < size) {
				break;
			}

			base = lmb_align_down(res_base - size, align);
		}
	}
	return 0;
}

int
lmb_is_reserved(struct lmb *lmb,
		phys_addr_t addr)
{
	int i;

	for (i = 0; i < lmb->reserved.cnt; i++) {
		phys_addr_t upper = lmb->reserved.region[i].base +
			lmb->reserved.region[i].size - 1;
		if ((addr >= lmb->reserved.region[i].base) && (addr <= upper))
			return 1;
	}
	return 0;
}

int
lmb_is_known(struct lmb *lmb,
	     phys_addr_t addr,
	     size_t size)
{
	int i;

	for (i = 0; i < lmb->memory.cnt; i++) {
		phys_addr_t upper = lmb->memory.region[i].base +
			lmb->memory.region[i].size;
		if ((addr >= lmb->memory.region[i].base) && (addr <= upper - 1) &&
		    ((addr + size) >= lmb->memory.region[i].base) &&
		    ((addr + size) <= upper)) {
			return 1;
		}
	}

	return 0;
}
