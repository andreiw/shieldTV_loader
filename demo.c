/*
 * Copyright (C) 2014-2018 Andrei Warkentin <andrey.warkentin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <lib.h>
#include <libfdt.h>
#include <video_fb.h>
#include <usbd.h>
#include <lmb.h>

extern void fb_launch();

struct lmb lmb;

static uint64_t
memparse(const char *ptr, char **retptr)
{
	/* local pointer to end of parsed string */
	char *endptr;

	uint64_t ret = simple_strtoull(ptr, &endptr, 0);

	switch (*endptr) {
	case 'G':
	case 'g':
		ret <<= 10;
	case 'M':
	case 'm':
		ret <<= 10;
	case 'K':
	case 'k':
		ret <<= 10;
	endptr++;
	default:
		break;
	}

	if (retptr) {
		*retptr = endptr;
	}

	return ret;
}

static int
parse_memloc(const char *arg,
	     uint64_t *psize,
	     uint64_t *ploc)
{
	char *at;
	unsigned long long size;

	size = memparse(arg, &at);
	if (*at != '@') {
		/*
		 * Treat a single location as a page at that address.
		 */
		*ploc = size;
		*psize = PAGE_SIZE;
		return 0;
	}

	*ploc = memparse(at + 1, NULL);
	*psize = size;
	return 0;
}

static void
arch_dump(void)
{
	uint64_t el;
	uint64_t sctlr;

	ReadSysReg(el, CurrentEL);
	el = SPSR_2_EL(el);
	printk("EL%u ", el);

	if (el == 2) {
		ReadSysReg(sctlr, sctlr_el2);
	} else {
		ReadSysReg(sctlr, sctlr_el1);
	}

	if ((sctlr & SCTLR_M) != 0) {
		printk("MMU ");
	}
	if ((sctlr & SCTLR_A) != 0) {
		printk("A n");
	}
	if ((sctlr & SCTLR_C) != 0) {
		printk("D ");
	}
	if ((sctlr & SCTLR_SA) != 0) {
		printk("SA ");
	}
	if ((sctlr & SCTLR_I) == 0) {
		printk("I ");
	}

	printk("\n");
}

static void
dump_cmdline(const char *s)
{
	const char *n = s;

	if (s == NULL) {
		printk("No cmdline\n");
		return;
	}

	printk("Cmdline: ");
	/*
	 * Dump is word by word because printk buffer has a maximum size.
	 */
	while ((n = strchr (s, ' ')) != NULL) {
		printk("%.*s", n - s + 1, s);
		s = n + 1;
	}

	printk("%s\n", s);
}

static bool_t
parse_range(const char *cmdline,
	    const char *range_name,
	    phys_addr_t *base,
	    size_t *size)
{
	const char *param;
	phys_addr_t b;
	size_t s;

	if (cmdline == NULL) {
		return false;
	}

	param = strstr(cmdline, range_name);
	if (param == NULL) {
		return false;
	}

	if (parse_memloc(param + strlen(range_name), &s, &b) != 0) {
		return false;
	}

	if (base != NULL) {
		*base = b;
	}

	if (size != NULL) {
		*size = s;
	}

	return true;
}

static bool_t
parse_and_reserve_range(const char *cmdline,
			const char *range_name,
			lmb_type_t type,
			lmb_tag_t tag,
			phys_addr_t *base,
			size_t *size)
{
	phys_addr_t b;
	size_t s;

	if (parse_range(cmdline, range_name, &b, &s)) {
		printk("0x%lx - 0x%lx\n", b, s);
		lmb_reserve(&lmb, b, s, type, tag);

		if (base != NULL) {
			*base = b;
		}

		if (size != NULL) {
			*size = s;
		}

		return true;
	}

	return false;
}

static const char *
fdt_get_cmdline(void *fdt)
{
	int node;

	if (fdt_check_header(fdt) < 0) {
		return NULL;
	}

	node = fdt_path_offset(fdt, "/chosen");
	if (node < 0) {
		return NULL;
	}

	return fdt_getprop(fdt, node, "bootargs", NULL);
}

void
demo(void *fdt, uint32_t el)
{
	int node;
	int resv_count;
	phys_addr_t base;
	uint64_t size;
	phys_addr_t fb_base;
	uint64_t fb_size;
	extern void *image_start;
	extern void *image_end;
	const char *cmdline = NULL;

	cmdline = fdt_get_cmdline(fdt);
	if (cmdline == NULL ||
	    !parse_range(cmdline, "tegra_fbmem=", &fb_base,
			 &fb_size)) {
		/*
		 * The 1.3 firmware default.
		 *
		 * 1.4 and 2.1 use 0x92ca8000.
		 */
		fb_base = 0x92ca6000;
		fb_size = CONFIG_VIDEO_VISIBLE_COLS *
			CONFIG_VIDEO_VISIBLE_ROWS *
			CONFIG_VIDEO_PIXEL_SIZE;
	}

	video_init(VP(fb_base));
	arch_dump();

	lmb_init(&lmb);

	dump_cmdline(cmdline);

	for (node = fdt_node_offset_by_dtype(fdt, -1, "memory");
	     node != -1;
	     node = fdt_node_offset_by_dtype(fdt, node, "memory")) {
		int prop_len;
		const uint32_t *prop = fdt_getprop(fdt, node, "reg", &prop_len);

		BUG_ON_EX(prop == NULL, "node %d has no reg property", node);
		while (prop_len != 0) {
			base = fdt32_to_cpu(*(prop + 1)) |
				((uint64_t) fdt32_to_cpu(*prop)) << 32;
			prop += 2;
			size = fdt32_to_cpu(*(prop + 1)) |
				((uint64_t) fdt32_to_cpu(*prop)) << 32;
			prop += 2;
			lmb_add(&lmb, base, size, LMB_TAG("RAMR"));
			prop_len -= 16;
		}
	}

	/*
	 * Not seen on the shield with my firmware version (2.1),
	 * but just in case.
	 */
	parse_and_reserve_range(cmdline, "tsec=", LMB_RUNTIME,
				LMB_TAG("TSEC"), NULL, NULL);

	/*
	 * Not seen on the shield with my firmware version (2.1), but
	 * just in case.
	 */
	parse_and_reserve_range(cmdline, "vpr=", LMB_RUNTIME,
				LMB_TAG("VPRR"), NULL, NULL);

	/*
	 * On my shield, above reported available memory ranges, but
	 * just in case.
	 */
	parse_and_reserve_range(cmdline, "lp0_vec=", LMB_RUNTIME,
				LMB_TAG("LP0V"), NULL, NULL);

	/*
	 * On my shield, above reported available memory ranges, but
	 * just in case.
	 */
	parse_and_reserve_range(cmdline, "nvdumper_reserved=", LMB_RUNTIME,
				LMB_TAG("NVDR"), NULL, NULL);

	lmb_reserve(&lmb, fb_base, fb_size,
		    LMB_RUNTIME, LMB_TAG("FBMM"));

	lmb_reserve(&lmb, (phys_addr_t) fdt, fdt_totalsize(fdt), LMB_BOOT,
		    LMB_TAG("FDTB"));

	lmb_reserve(&lmb, (phys_addr_t) &image_start,
		    UN(&image_end) - UN(&image_start), LMB_BOOT,
		    LMB_TAG("LDRS"));

	resv_count = fdt_num_mem_rsv(fdt);
	for (node = 0; node < resv_count; node++) {
		phys_addr_t base;
		size_t size;

		fdt_get_mem_rsv(fdt, node, &base, &size);
		lmb_reserve(&lmb, base, size, LMB_BOOT, LMB_TAG("RESV"));
	}

	fb_launch();
	BUG();
}
