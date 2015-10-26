/*
 * Copyright (C) 2014 Andrei Warkentin <andrey.warkentin@gmail.com>
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

#include <defs.h>
#include <libfdt.h>
#include <vsprintf.h>
#include <video_fb.h>

/*
 * The 1.3 firmware default.
 *
 * 1.4 and 2.1 use 0x92ca8000, but
 * we try to detect it below.
 */
void *fb_base = (void *) 0x92ca6000;
#define FB_COLS 1920
#define FB_ROWS 1080

void printk(char *fmt, ...)
{
	va_list list;
	char buf[512];
	va_start(list, fmt);
	vsnprintf(buf, sizeof(buf), fmt, list);
	video_puts(buf);
	va_end(list);
}

void draw_pixel(int x, int y, uint32_t color)
{
	uint32_t *fb = (uint32_t *) fb_base;
	fb[FB_COLS * y + x] = color;
}

void bres(int x1, int y1, int x2, int y2, uint32_t color)
{
	int dx, dy, i, e;
	int incx, incy, inc1, inc2;
	int x,y;

	dx = x2 - x1;
	dy = y2 - y1;

	if(dx < 0) dx = -dx;
	if(dy < 0) dy = -dy;
	incx = 1;
	if(x2 < x1) incx = -1;
	incy = 1;
	if(y2 < y1) incy = -1;
	x=x1;
	y=y1;

	if(dx > dy)
	{
		draw_pixel(x, y, color);
		e = 2*dy - dx;
		inc1 = 2*( dy -dx);
		inc2 = 2*dy;
		for(i = 0; i < dx; i++)
		{
			if(e >= 0)
			{
				y += incy;
				e += inc1;
			}
			else e += inc2;
			x += incx;
			draw_pixel(x, y, color);
		}
	}
	else
	{
		draw_pixel(x, y, color);
		e = 2*dx - dy;
		inc1 = 2*( dx - dy);
		inc2 = 2*dx;
		for(i = 0; i < dy; i++)
		{
			if(e >= 0)
			{
				x += incx;
				e += inc1;
			}
			else e += inc2;
			y += incy;
			draw_pixel(x, y, color);
		}
	}
}

static uint64_t
memparse(const char *ptr, char **retptr)
{
	char *endptr;   /* local pointer to end of parsed string */

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

void demo(void *fdt, uint32_t el)
{
	int nodeoffset;
	char *fbparam;
	uint64_t base;
	uint64_t size;
	uint32_t color;
	const char *cmdline;
	extern void *image_start;
	extern void *image_end;

	/*
	 * Parse /chosen/bootargs for the real base
	 * of tegra framebuffer.
	 */
	nodeoffset = fdt_path_offset(fdt,
				     "/chosen");
	if (nodeoffset < 0) {
		goto cont;
	}

	cmdline = fdt_getprop(fdt, nodeoffset,
			      "bootargs", NULL);
	if (cmdline == NULL) {
		goto cont;
	}
	fbparam = strstr(cmdline, "tegra_fbmem=");
	if (parse_memloc(fbparam + 12, &size, &base) == 0) {
		fb_base = (void *) base;
	}

cont:
	video_init(fb_base);
	printk("We are at EL%u\n", el);
	printk("We are 0x%lx bytes at %p\n",
	       (uint64_t) &image_end -
	       (uint64_t) &image_start,
	       &image_start);

	/*
	 * Draw some lines diagonal lines in the bottom half of screen.
	 *
	 * Green: we're at EL2.
	 * White: we're at EL1.
	 */
	color = el == 2 ? 0xff00ff00 : 0xffffffff;
	bres(0, FB_ROWS / 2, FB_COLS - 1, FB_ROWS - 1, color);
	bres(FB_COLS - 1, FB_ROWS / 2, 0, FB_ROWS - 1, color);
	bres(1, FB_ROWS / 2, FB_COLS - 1, FB_ROWS - 2, color);
	bres(FB_COLS - 2, FB_ROWS / 2, 0, FB_ROWS - 2, color);
}
