/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016-2017 Intel Corporation
 */

#ifndef _MISC_H_
#define _MISC_H_

#include <asm/sections.h>

void dwmac_deassert_reset(const unsigned int of_reset_id, const u32 phymode);

struct bsel {
	const char	*mode;
	const char	*name;
};

extern struct bsel bsel_str[];

#ifdef CONFIG_FPGA
void socfpga_fpga_add(void *fpga_desc);
#else
inline void socfpga_fpga_add(void *fpga_desc) {}
#endif

#ifdef CONFIG_TARGET_SOCFPGA_GEN5
void socfpga_sdram_remap_zero(void);
void socfpga_sdram_apply_static_cfg(void);
static inline bool socfpga_is_booting_from_fpga(void)
{
	if ((__image_copy_start >= (char *)SOCFPGA_FPGA_SLAVES_ADDRESS) &&
	    (__image_copy_start < (char *)SOCFPGA_STM_ADDRESS))
		return true;
	return false;
}
#endif

#ifdef CONFIG_TARGET_SOCFPGA_ARRIA10
void socfpga_init_security_policies(void);
void socfpga_sdram_remap_zero(void);
void set_regular_boot(unsigned int status);
bool is_regular_boot_valid(void);
int qspi_flash_software_reset(void);
#endif

#if defined(CONFIG_TARGET_SOCFPGA_STRATIX10) || \
	defined(CONFIG_TARGET_SOCFPGA_AGILEX)
int socfpga_get_fpga_config(void);
#endif

void do_bridge_reset(int enable, unsigned int mask);
void socfpga_pl310_clear(void);

#endif /* _MISC_H_ */
