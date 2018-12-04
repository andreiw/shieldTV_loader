/*
 * Tegra defs.
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

#ifndef TEGRA_H
#define TEGRA_H

#define TEGRA_EHCI_BASE 0x7d000000

#define TEGRA_PMC_BASE         0x7000e400
#define TEGRA_PMC_CONFIG       (TEGRA_PMC_BASE + 0)
#define TEGRA_PMC_CONFIG_RESET (0x10)
#define TEGRA_PMC_SCRATCH0     (TEGRA_PMC_BASE + 0x50)

typedef enum {
	REBOOT_NORMAL = 0,
	REBOOT_RCM = (1 << 1),
	REBOOT_BOOTLOADER = (1 << 30),
	REBOOT_RECOVERY = (1 << 31)
} reboot_type;

static inline void
tegra_reboot(reboot_type type)
{
	OUT32(type, TEGRA_PMC_SCRATCH0);
	OUT32(IN32(TEGRA_PMC_CONFIG) | TEGRA_PMC_CONFIG_RESET, TEGRA_PMC_CONFIG);
}

#endif /* TEGRA_H */
