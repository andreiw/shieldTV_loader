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

.globl asm_start

/*
 * x0 contains the FDT blob PA, which we don't use
 */
asm_start:
        b       asm_real          // branch to kernel start, magic
        .long   0                 // reserved
        .quad   TEXT_BASE         // Image load offset from start of RAM, unused
        .quad   0                 // reserved
        .quad   0                 // reserved
        .quad   0                 // reserved
        .quad   0                 // reserved
        .quad   0                 // reserved
        .byte   0x41              // Magic number, "ARM\x64"
        .byte   0x52
        .byte   0x4d
        .byte   0x64
        .word   0                 // reserved
asm_real:
        ldr     x0, =stack_end
        mov     sp, x0
        mrs     x0, CurrentEL
        lsr     x0, x0, #2
        bl      demo              // Draw to the fb
1:      b       1b

.section .data
.align 4
stack:
        .fill 512, 8, 0
stack_end:

