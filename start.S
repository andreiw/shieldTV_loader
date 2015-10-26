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
 * x0 contains the FDT blob PA
 */
.section ".text"
.globl image_end
.globl image_start
asm_start:
        b       asm_real          // branch to kernel start, magic
        .long   0                 // reserved
        .quad   image_start
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
        adr     x1, image_start
        adr     x2, rela_start
        adr     x3, rela_end
reloc_loop:
        cmp     x2, x3
        bhs     reloc_done

        //
        // struct rela_entry_t {
        //     uint64_t offset;  // relative offset
        //     uint64_t info;    // 0x403 == R_AARCH64_RELATIVE
        //     uint64_t addend;  // to adjust by actual loaded address
        // } rela_entry_t;
        //
        ldp     x4, x5, [x2], #24
        cmp     x5, #0x403       // R_AARCH64_RELATIVE
        bne     reloc_loop
        ldr     x5, [x2, #-8]
        add     x5, x5, x1      // relies on link base being 0, so
                                  // no need to subtract link base
        str     x5, [x4, x1]
        b       reloc_loop
reloc_done:
        ldr     x1, =stack_end
        mov     sp, x1
        mrs     x1, CurrentEL
        lsr     x1, x1, #2
        bl      demo              // x0 = fdt, x1 = EL
1:      b       1b

.section .data
.align 4
stack:
        .fill 4096, 8, 0
stack_end:

