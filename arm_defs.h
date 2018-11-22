/*
 * Various useful macros.
 *
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

#ifndef ARM_DEFS_H
#define ARM_DEFS_H

#define ReadSysReg(var, reg) asm volatile("mrs %0, " #reg : "=r" (var))
#define WriteSysReg(reg, val) asm volatile("msr " #reg ", %0"  : : "r" (val))
#define ATS12E1R(what) asm volatile("at s12e1r, %0" : :"r" (what))

#define ISB() asm volatile("isb");
#define DSB_LD() asm volatile("dsb ld");
#define DSB_ST() asm volatile("dsb st");
#define DSB_ISH() asm volatile("dsb ish");

#define SPSR_2_EL(spsr) (X((spsr), 2, 3))
#define SPSR_2_BITNESS(spsr) (X((spsr), 4, 4) ? 32 : 64)
#define SPSR_2_SPSEL(spsr) (X((spsr), 0, 0) ? 1 : 0)

#define HPFAR_2_GPA(hpfar, far) (I(X((hpfar), 4, 39), 12, 47) | \
                                 I((far), 11, 0))
#define ESR_EC_HVC64   0x16
#define ESR_EC_SMC64   0x17
#define ESR_EC_MSR     0x18
#define ESR_EC_IABT_LO 0x20
#define ESR_EC_DABT_LO 0x24
#define ESR_EC_BRK     0x3C
#define ESR_2_IL(x)  (X((x), 25, 25))
#define ESR_2_EC(x)  (X((x), 26, 31))
#define ESR_2_ISS(x) (X((x), 0, 24))

#define ISS_SYS_WRITE(x) ((X((x), 0, 0)) == 0)
#define ISS_SYS_CRm(x)   (X((x), 4, 1))
#define ISS_SYS_Rt(x)    (X((x), 9, 5))
#define ISS_SYS_CRn(x)   (X((x), 13, 10))
#define ISS_SYS_Op1(x)   (X((x), 16, 14))
#define ISS_SYS_Op2(x)   (X((x), 19, 17))
#define ISS_SYS_Op0(x)   (X((x), 21, 20))
#define ISS_SYS_MSR(x)   (X(ISS_SYS_Op0((x)), 1, 1) == 1)
#define ISS_SYS_O0(x)    X(ISS_SYS_Op0((x)), 0, 0)

/*
 * Cook an ISS MSR access field into a common form,
 * where we saturate the access and register fields into
 * to known values.
 */
#define ISS_SYS_2_MSRDEF(x) ((x) | I(1, 0, 0) | I(0x1F, 9, 5))
#define MSRDEF(O0, Op1, CRn, CRm, Op2) \
  ISS_SYS_2_MSRDEF(I(CRm, 4, 1) | I(CRn, 13, 10) | \
                   I(Op1, 16, 14) | I(Op2, 19, 17) | \
                   I(1, 21, 21) | I(O0, 20, 20))

#define MSRDEF_OSDTRRX_EL1     MSRDEF(0, 0, 0, 0, 2)
#define MSRDEF_MDCCINT_EL1     MSRDEF(0, 0, 0, 2, 0)
#define MSRDEF_MDSCR_EL1       MSRDEF(0, 0, 0, 2, 2)
#define MSRDEF_OSDTRTX_EL1     MSRDEF(0, 0, 0, 3, 2)
#define MSRDEF_OSECCR_EL1      MSRDEF(0, 0, 0, 6, 2)
#define MSRDEF_DBGBVR_EL1(x)   MSRDEF(0, 0, 0, (x), 4)
#define MSRDEF_DBGBCR_EL1(x)   MSRDEF(0, 0, 0, (x), 5)
#define MSRDEF_DBGWVR_EL1(x)   MSRDEF(0, 0, 0, (x), 6)
#define MSRDEF_DBGWCR_EL1(x)   MSRDEF(0, 0, 0, (x), 7)
#define MSRDEF_MDRAR_EL1       MSRDEF(0, 0, 1, 0, 0)
#define MSRDEF_OSLAR_EL1       MSRDEF(0, 0, 1, 0, 4)
#define MSRDEF_OSLSR_EL1       MSRDEF(0, 0, 1, 1, 4)
#define MSRDEF_OSDLR_EL1       MSRDEF(0, 0, 1, 3, 4)
#define MSRDEF_DBGPRCR_EL1     MSRDEF(0, 0, 1, 4, 4)
#define MSRDEF_DBGCLAIMSET_EL1 MSRDEF(0, 0, 7, 8, 6)
#define MSRDEF_DBGCLAIMCLR_EL1 MSRDEF(0, 0, 7, 9, 6)
#define MSRDEF_DBGAUTHSTAT_EL1 MSRDEF(0, 0, 7, 14, 6)
#define MSRDEF_MDCCSR_EL0      MSRDEF(0, 3, 0, 1, 0)
#define MSRDEF_DBGDTR_EL0      MSRDEF(0, 3, 0, 4, 0)
#define MSRDEF_DBGDTRF_EL0     MSRDEF(0, 3, 0, 5, 0)

#define MDCR_TDE  BIT(8)
#define HCR_VM    BIT(0)
#define HCR_AMO   BIT(5)
#define HCR_VSE   BIT(8)
#define HCR_TSC   BIT(19)
#define HCR_RW_64 BIT(31)
#define SPSR_D    BIT(9)
#define SPSR_EL1  0x4
#define SPSR_ELx  0x1

#define SCTLR_M   BIT(0)
#define SCTLR_A   BIT(1)
#define SCTLR_C   BIT(2)
#define SCTLR_SA  BIT(3)
#define SCTLR_I   BIT(12)
#define SCTLR_EL1_RES1 (BIT(29) | BIT(28) | BIT(23) |   \
                        BIT(22) | BIT(20) | BIT(11))
/*
 * Device is GRE (to allow EL1 to do whatever).
 * Memory is Inner-WB Outer-WB.
 */
#define PTE_S2_ATTR_MEM (I(3, 4, 5) | I(3, 2, 3))
#define PTE_S2_ATTR_DEV (I(0, 4, 5) | I(3, 2, 3))
#define PTE_RW          I(0x1, 6, 7)
#define PTE_S2_RW       I(0x3, 6, 7)
#define PTE_S2_RO       I(0x1, 6, 7)
#define PTE_SH_INNER    I(0x3, 8, 9)
#define PTE_AF          I(0x1, 10, 10)

#define PTE_TYPE_TAB      0x3
#define PTE_TYPE_BLOCK    0x1
#define PTE_TYPE_BLOCK_L3 0x3
#define PTE_2_TYPE(x)     ((x) & 0x3)
/*
 * Assumes 4K granularity.
 */
#define PTE_2_TAB(x)      ((VOID *)M((x), 47, 12))
#define VA_2_PL1_IX(x)    X((x), 30, 38)
#define VA_2_PL2_IX(x)    X((x), 21, 29)
#define VA_2_PL3_IX(x)    X((x), 20, 12)

#define PAR_IS_BAD(par)   (((par) & 1) == 1)
#define PAR_2_ADDR(par)   M((par), 47, 12)

#define TLBI_S12() do {					\
		ISB();					\
		asm volatile("tlbi vmalls12e1");	\
		DSB_ISH();				\
		ISB();					\
	} while (0)

#define PSCI_CPU_ON_64               0xC4000003
#define PSCI_CPU_OFF                 0x84000002
#define PSCI_RETURN_STATUS_SUCCESS   0
#define PSCI_RETURN_STATUS_DENIED    -3
#define PSCI_RETURN_INTERNAL_FAILURE -6

static inline uint8_t
IN8(const volatile void *addr)
{
	uint8_t val;
	asm volatile("ldrb %w0, [%1]" : "=r" (val) : "r" (addr));
	DSB_LD();
	return val;
}

static inline uint16_t
_IN16(const volatile void *addr)
{
	uint16_t val;
	asm volatile("ldrh %w0, [%1]" : "=r" (val) : "r" (addr));
	DSB_LD();
	return val;
}
#define IN16(x) _IN16(VP(x))

static inline uint32_t
_IN32(const volatile void *addr)
{
	uint32_t val;
	asm volatile("ldr %w0, [%1]" : "=r" (val) : "r" (addr));
	DSB_LD();
	return val;
}
#define IN32(x) _IN32(VP(x))

static inline uint32_t
_IN64(const volatile void *addr)
{
	uint64_t val;
	asm volatile("ldr %0, [%1]" : "=r" (val) : "r" (addr));
	DSB_LD();
	return val;
}
#define IN64(x) _IN64(VP(x))

static inline void
_OUT8(uint8_t val, volatile void *addr)
{
	DSB_ST();
	asm volatile("strb %w0, [%1]" : : "r" (val), "r" (addr));
}
#define OUT8(val, addr) _OUT8((uint8_t) (val), VP(addr))

static inline void
_OUT16(uint16_t val, volatile void *addr)
{
	DSB_ST();
	asm volatile("strh %w0, [%1]" : : "r" (val), "r" (addr));
}
#define OUT16(val, addr) _OUT16((uint16_t) (val), VP(addr))

static inline void
_OUT32(uint32_t val, volatile void *addr)
{
	DSB_ST();
	asm volatile("str %w0, [%1]" : : "r" (val), "r" (addr));
}
#define OUT32(val, addr) _OUT32((uint32_t) (val), VP(addr))

static inline void
_OUT64(uint64_t val, volatile void *addr)
{
	DSB_ST();
	asm volatile("str %0, [%1]" : : "r" (val), "r" (addr));
}
#define OUT64(val, addr) _OUT8((uint64_t) (val), VP(addr))

#endif /* ARM_DEFS_H */
