/*
 *  Copyright (C) 2013 Critical Link LLC <www.criticallink.com>
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

#include <common.h>
#include <asm/arch/clock_manager.h>

#include "config_block.h"

#ifndef NUM_EEPROM_RETRIES
#define NUM_EEPROM_RETRIES	2
#endif

/*
 * Print Board information
 */
int checkboard(void)
{
	puts("BOARD: Critical Link MitySOM-5CSx Module\n");
	return 0;
}

/*
 * Read header information from EEPROM into global structure.
 */
static int read_factoryconfig(void)
{

	int retries, rv = 0;

	for (retries = 0; retries <= NUM_EEPROM_RETRIES; ++retries)
	{
		if (retries) {
			printf("Retrying [%d] ...\n", retries);
		}

		/* try and read our configuration block */
		rv = get_factory_config_block();
		if (rv < 0) {
			printf("I2C Error reading factory config block\n");
			continue; /* retry */
		}
		/* No I2C issues, data was either OK or not entered... */
		if (rv > 0)
			printf("Bad I2C configuration found\n");
		break;
	}
	factory_config_block.ModelNumber[31] = '\0';

	return rv;
}

/* Called after all of Altera's SPL setup */
#ifdef CONFIG_SPL_BUILD
void spl_board_init(void)
{
	/* Altera used to call this for us in the SPL, but they removed that */
	cm_print_clock_quick_summary();
}
#endif

int misc_init_r(void)
{
	char tmp[24];
	uint8_t eth_addr[10];
	uint8_t eth1addr[10];

	read_factoryconfig();

	/* Handle setting the first MAC to ethaddr env varible */
	memcpy(eth_addr, factory_config_block.MACADDR, 6);
	if (is_valid_ethaddr(eth_addr)) {
		char* env_ethaddr = env_get("ethaddr");
		sprintf((char *)tmp, "%02x:%02x:%02x:%02x:%02x:%02x", eth_addr[0],
			eth_addr[1], eth_addr[2], eth_addr[3], eth_addr[4], eth_addr[5]);

		if (!env_ethaddr || (0 != strncmp(tmp, env_ethaddr, 18)))
			env_set("ethaddr", (char *)tmp);
	}

	/* Handle setting the second MAC to eth1addr env varible */
	MacAddressBlock *mac2 = get_2nd_mac_address();
	memcpy(eth1addr, mac2->MacAddress, 6);
	if (is_valid_ethaddr(eth1addr)) {
		char* env_ethaddr1 = env_get("eth1addr");
		sprintf((char *)tmp, "%02x:%02x:%02x:%02x:%02x:%02x", eth1addr[0],
			eth1addr[1], eth1addr[2], eth1addr[3], eth1addr[4], eth1addr[5]);

		if (!env_ethaddr1 || (0 != strncmp(tmp, env_ethaddr1, 18)))
			env_set("eth1addr", (char *)tmp);
	}

	/* Handle setting the RTC Cal registers to the uboot env */
	RtcCalBlock *rtc_cal = get_rtc_cal();
	char* env_rtccal = env_get("rtccal");
	sprintf((char *) tmp, "%d", rtc_cal->RtcCalVal);
	if(!env_rtccal || (0 != strcmp(tmp, env_rtccal)))
		env_set("rtccal", (char *)tmp);

	/* Set the clmodelnum variable to what is set in the factory config */
	if(strlen(factory_config_block.ModelNumber) != 0)
		env_set("clmodelnum", factory_config_block.ModelNumber);

	/* If we return an error here, U-Boot will hang, making it more difficult
	 * for the user to correct whatever factoryconfig error occurred */
	return 0;
}
