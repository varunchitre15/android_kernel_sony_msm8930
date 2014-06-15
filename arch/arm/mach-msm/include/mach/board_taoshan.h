/* arch/arm/mach-msm/include/mach/board_taoshan.h
 *
 * Copyright (C) 2014 CyanogenMod
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_BOARD_TAOSHAN_H
#define __ASM_ARCH_MSM_BOARD_TAOSHAN_H

#ifdef CONFIG_ANDROID_PERSISTENT_RAM
#define TAOSHAN_PERSISTENT_RAM_SIZE	(SZ_1M)
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define TAOSHAN_RAM_CONSOLE_SIZE	(124*SZ_1K * 2)
#endif

void __init taoshan_reserve(void);

#ifdef CONFIG_ANDROID_PERSISTENT_RAM
void __init taoshan_add_persistent_ram(void);
#else
static inline void __init taoshan_add_persistent_ram(void)
{
	/* empty */
}
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
void __init taoshan_add_ramconsole_devices(void);
#else
static inline void __init taoshan_add_ramconsole_devices(void)
{
	/* empty */
}
#endif

#endif // __ASM_ARCH_MSM_BOARD_TAOSHAN_H