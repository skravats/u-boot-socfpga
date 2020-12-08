/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __CONFIG_CL_MITYSOM_5CS_H__
#define __CONFIG_CL_MITYSOM_5CS_H__

#include <asm/arch/base_addr_ac5.h>
#include <asm/mach-types.h>

/* Define machine type for Cyclone 5 */
#define CONFIG_MACH_TYPE	MACH_TYPE_SOCFPGA_CYCLONE5

/* Memory configuration
 *
 * For Cyclone V this is only used to set CONFIG_SYS_MEMTEST_END, which in
 * turn is only used as the upper test bound by the mtest command--the "most
 * useless" option for memory testing, according to doc/README.memory-test.
 * Just use 32 MiB like our 2013 branch did.
 */
#define PHYS_SDRAM_1_SIZE		0x02000000

/* Environment for SDMMC boot */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0 /* device 0 */
#define CONFIG_ENV_OFFSET		512 /* just after the MBR */
#endif

/* Environment for QSPI boot */
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_OFFSET		0x00040000 /* 256 KiB, after the SPL */
#define CONFIG_ENV_SECT_SIZE		(64 * 1024)
#endif

/* I2C clock in MHz. Defaults to 166 MHz if we don't set it here */
#ifndef __ASSEMBLY__
extern unsigned int cm_get_l4_sp_clk_hz(void);
#define IC_CLK	(cm_get_l4_sp_clk_hz() / 1000000)
#endif

/* If this isn't set and the environment variables bootm_size and
 * bootm_mapsize aren't set either, Linux won't boot on the 2 GB SOM. Use
 * the value from our 2013 branch.
 *
 * Initial Memory map size for Linux, minus 4k alignment for DFT blob
 */
#define CONFIG_SYS_BOOTMAPSZ	(64 * 1024 * 1024)

/* Booting Linux */
#define CONFIG_LOADADDR		0x01000000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR

/* QSPI boot */
#define FDT_SIZE		__stringify(0x00010000)
#define KERNEL_SIZE		__stringify(0x005d0000)
#define QSPI_FDT_ADDR		__stringify(0x00220000)
#define QSPI_KERNEL_ADDR	__stringify(0x00230000)

#define SOCFPGA_BOOT_SETTINGS \
	"fdt_size=" FDT_SIZE "\0" \
	"kernel_size=" KERNEL_SIZE "\0" \
	"qspi_fdt_addr=" QSPI_FDT_ADDR "\0" \
	"qspi_kernel_addr=" QSPI_KERNEL_ADDR "\0" \
	"qspiboot=setenv bootargs earlycon " \
		"root=/dev/mtdblock1 rw rootfstype=jffs2; " \
		"bootz ${kernel_addr_r} - ${fdt_addr_r}\0" \
	"qspiload=sf probe; " \
		"sf read ${kernel_addr_r} ${qspi_kernel_addr} ${kernel_size}; " \
		"sf read ${fdt_addr_r} ${qspi_fdt_addr} ${fdt_size}\0"

/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

/*
 * Defined in socfpga_common.h, but we don't use a redundant environment. When
 * this is enabled, U-Boot expects an extra byte between the environment CRC
 * and the environment data, which breaks the CRC check
 */
#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
#undef CONFIG_SYS_REDUNDAND_ENVIRONMENT
#endif

#endif	/* __CONFIG_CL_MITYSOM_5CS_H__ */
