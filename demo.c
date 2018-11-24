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

/*
 * The 1.3 firmware default.
 *
 * 1.4 and 2.1 use 0x92ca8000, but
 * we try to detect it below.
 */
void *fb_base = VP(0x92ca6000);
#define FB_COLS 1920
#define FB_ROWS 1080

extern void fb_launch();

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
		return -1;
	}

	*ploc = memparse(at + 1, NULL);
	*psize = size;
	return 0;
}

void arch_dump(void)
{
	uint64_t el;
	uint64_t sctlr;
	uint64_t tcr;
	uint64_t ttbr0;
	uint64_t vbar;

	ReadSysReg(el, CurrentEL);
	el = SPSR_2_EL(el);
	printk("We are at EL%u\n", el);

	if (el == 2) {
		ReadSysReg(sctlr, sctlr_el2);
		ReadSysReg(tcr, tcr_el2);
		ReadSysReg(ttbr0, ttbr0_el2);
		ReadSysReg(vbar, vbar_el2);
	} else {
		ReadSysReg(sctlr, sctlr_el1);
		ReadSysReg(tcr, tcr_el1);
		ReadSysReg(ttbr0, ttbr0_el1);
		ReadSysReg(vbar, vbar_el1);
	}

	printk("SCTLR 0x%lx VBAR  0x%lx\n",
	       sctlr, vbar);
	printk("TCR   0x%lx TTBR0 0x%lx\n",
	       tcr, ttbr0);

	if ((sctlr & SCTLR_M) == 0) {
		printk("MMU off\n");
	}
	if ((sctlr & SCTLR_A) != 0) {
		printk("Alignment check set\n");
	}
	if ((sctlr & SCTLR_C) == 0) {
		printk("Data cache off\n");
	}
	if ((sctlr & SCTLR_SA) != 0) {
		printk("SP alignment check set\n");
	}
	if ((sctlr & SCTLR_I) == 0) {
		printk("Instruction cache off\n");
	}
}

void demo(void *fdt, uint32_t el)
{
	int nodeoffset;
	char *fbparam;
	uint64_t base;
	uint64_t size;
	const char *cmdline;
	extern void *image_start;
	extern void *image_end;

	/*
	 * Parse /chosen/bootargs for the real base
	 * of tegra framebuffer.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");
	if (nodeoffset < 0) {
		goto cont;
	}

	cmdline = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if (cmdline == NULL) {
		goto cont;
	}
	fbparam = strstr(cmdline, "tegra_fbmem=");
	if (parse_memloc(fbparam + 12, &size, &base) == 0) {
		fb_base = VP(base);
	}

cont:
	video_init(fb_base);
	printk("We are 0x%lx bytes at %p\n",
	       (uint64_t) &image_end -
	       (uint64_t) &image_start,
	       &image_start);
	printk("FB is at %p\n", fb_base);

	arch_dump();
	fb_launch();
	BUG();
}
