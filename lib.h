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

#ifndef LIB_H
#define LIB_H

#include <defs.h>
#include <arm_defs.h>
#include <string.h>
#include <ctype.h>
#include <vsprintf.h>

void printk(char *fmt, ...);

#define BUG() do {						\
		printk("Panic at %s:%u\n",			\
		       __FILE__, __LINE__);			\
		while (1);                                      \
	} while(0);

#define BUG_ON(condition) do {					\
		if (unlikely(condition)) {			\
			printk("'" S(condition) "' failed\n");	\
			BUG();					\
		}						\
	} while(0)

#endif /* LIB_H */
