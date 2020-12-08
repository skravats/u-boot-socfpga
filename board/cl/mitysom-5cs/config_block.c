/*
 * config_block.c
 *
 *  Created on: Mar 18, 2010
 *      Author: mikew
 */
#include <common.h>
#include <command.h>
#include <cli.h>
#include <malloc.h>
#include <asm/setup.h>

#include "config_block.h"

static struct I2CFactoryConfig default_factory_config = {
	.ConfigMagicWord = CONFIG_I2C_MAGIC_WORD,
	.ConfigVersion = CONFIG_I2C_VERSION,
	.MACADDR = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.SerialNumber = 0,
	.FpgaType = 0, /* Leave FpgaType in struct to maintain compatibilty across products */
	.ModelNumber = { '\0' }
};

static struct MacAddressBlock default_2nd_mac_config = {
	.MacAddress = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};
static struct RtcCalBlock default_rtc_cal_data = {
	.RtcCalVal = 0x00
};

struct I2CFactoryConfig factory_config_block;
struct GenericDataBlock *generic_data_block = NULL;

typedef enum PrintType { eeInfo, eeDebug, eeWarning, eeError } PrintType;
static void print_console(PrintType type, const char *str, ...)
{
	switch(type)
	{
		case eeInfo:
			printf( "Info - ");
			break;
		case eeWarning:
			printf("Warning - ");
			break;
		case eeError:
			printf("Error - ");
			break;
		case eeDebug:
#ifdef DEBUG
			printf("Debug - ");
#else
			return;
#endif
			break;
		default:
			printf("Unknown - ");
	}

	va_list args;
	va_start(args, str);
	vprintf(str, args);
	printf("\n");
	va_end(args);
}

static struct GenericDataBlock* create_generic_block(u16 type, u16 size, uchar *data)
{
	print_console(eeDebug, "Creating Gen Block|Type: %d, Size: %d", type, size);
	GenericDataBlock *block = malloc(sizeof(GenericDataBlock));
	if(block != NULL)
	{
		block->Header.MagicWord = CONFIG_I2C_MAGIC_WORD;
		block->Header.Type = type;
		block->Header.Size = size;
		block->Data = (uchar*)malloc(size);
		if(block->Data == NULL)
		{
			free(block);
			return NULL;
		}
		memcpy(block->Data, data, size);
		block->Next = NULL;
				
		print_console(eeDebug, "Created Gen Block|Type: %d, Size: %d", block->Header.Type, block->Header.Size);
	}

	return block;
}

static void add_generic_block(struct GenericDataBlock* block)
{
	GenericDataBlock* node;

	/* Set block Next pointer to NULL */
	block->Next = NULL;

	if(generic_data_block == NULL)
	{
		/* No blocks in our list, this will be the first */
		print_console(eeDebug, "Block list empty, adding node(%04X) to head", block->Header.Type);
		generic_data_block = block;
	}
	else
	{
		/* Go search for the end of the list */
		node = generic_data_block;
		while(node->Next != NULL)
		{
			node = node->Next;
		}

		/* End of the list, point to the new block */
		node->Next = block;
		print_console(eeDebug, "Adding node(%04X) to end of block list", block->Header.Type);
	}
}

static GenericDataBlock* get_generic_block(GenericType type)
{
	GenericDataBlock* node = generic_data_block;

	while(node != NULL)
	{
		print_console(eeDebug, "Search Gen Block|Type: %d, Size: %d", node->Header.Type, node->Header.Size);
		if(node->Header.Type == type)
		{
			print_console(eeDebug, "Found generic block(%04X)", node->Header.Type);
			return node;
		}

		node = node->Next;
	}

	print_console(eeInfo, "Didn't find block");
	return NULL;
}

MacAddressBlock* get_2nd_mac_address(void)
{
	GenericDataBlock *block = get_generic_block(ee2ndMAC);
	MacAddressBlock *mac = NULL; 
	if(block == NULL)
	{
		print_console(eeDebug, "No 2nd MAC address in list, loading defaults");
		/* No 2nd mac address in list, so make one with defaults */
		block = create_generic_block(ee2ndMAC, 
				sizeof(MacAddressBlock), (uchar*)&default_2nd_mac_config);
		if(block != NULL)
		{
			print_console(eeDebug, "Adding default 2nd mac to list");
			add_generic_block(block);
			mac = (MacAddressBlock*)block->Data;
		}
	}
	else
	{
		print_console(eeDebug, "Found 2nd MAC, returning");
		mac = (MacAddressBlock*)block->Data;
	}
	return mac;
}

RtcCalBlock* get_rtc_cal(void)
{
	GenericDataBlock *block = get_generic_block(eeRtcCal);
	RtcCalBlock *rtc = NULL;
	if(block == NULL)
	{
		print_console(eeDebug, "No RTC Cal data in list, loading defaults");
		/* No RTC cal in list, so make one with defaults */
		block = create_generic_block(eeRtcCal,
				sizeof(RtcCalBlock), (uchar*)&default_rtc_cal_data);
		if(block != NULL)
		{
			print_console(eeDebug, "Adding default RTC Cal data to list");
			add_generic_block(block);
			rtc = (RtcCalBlock*)block->Data;
		}
	}
	else
	{
		print_console(eeDebug, "Found RTC Cal Data, returning");
		rtc = (RtcCalBlock*)block->Data;
	}
	return rtc;
}


/**
 * get_factory_config_version
 * return the version of the factory config block or 0xFFFFFFFF if invalid
 */
static u32 get_factory_config_version(void)
{
	int i = 0;
	u32 rv = 0xFFFFFFFF;
	u32 data[2] = {0,0};
	i = eeprom_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0x00, (uchar*)data, sizeof(data));
	if (0 != i)
	{
		print_console(eeError, "Failure reading Factory Configuration Version Info");
	}
	if(CONFIG_I2C_MAGIC_WORD == data[0])
	{
		rv = data[1];
	}
	return rv;
}

/**
 * get_factory_config_size
 * \return the size of the factory config block or -1 on error
 */
static int get_factory_config_size(void)
{
	int rv = -1;
	u32 version = get_factory_config_version();
	switch(version)
	{
		case CONFIG_I2C_VERSION_1_1:
		case CONFIG_I2C_VERSION_1_2:
			rv = sizeof(struct I2CFactoryConfig);
			break;
		case CONFIG_I2C_VERSION_1_3:
			rv = sizeof(struct I2CFactoryConfigV3);
			break;
		default:
			break;
	}
	return rv;
}

static u32 calc_checksum(uchar* data, u16 size)
{
	u32 sum = 0;
	int i;

	for (i = 0; i < size; i++)
	{
		sum += *data++;
	}

	return sum;
}

/* Only used via do_factoryconfig, so don't build for SPL */
#ifndef CONFIG_SPL_BUILD
static int put_generic_data_block(GenericDataBlock* node, int addr)
{
	/* Write out header */
	int i = 0;
	int offset = 0;
	int ret = 0;

	uchar *tmp = (uchar*)&node->Header;
	for(i = 0; i < sizeof(GenericDataBlockHeader); i++)
	{
		ret |= eeprom_write(CONFIG_SYS_I2C_EEPROM_ADDR, addr+(offset++), tmp++, 1);
	}
	/* Write out Data */
	tmp = (uchar*)node->Data;
	for(i = 0; i < node->Header.Size; i++)
	{
		ret |= eeprom_write(CONFIG_SYS_I2C_EEPROM_ADDR, addr+(offset++), tmp++, 1);
	}
	/* Calc/writeout checksum */
	u32 sumHeader = calc_checksum((uchar*)&node->Header, sizeof(GenericDataBlockHeader));
	u32 sumData = calc_checksum((uchar*)node->Data, node->Header.Size);
	u32 sumGen = sumHeader + sumData;
	tmp = (uchar*)&sumGen;
	for(i = 0; i < sizeof(u32); i++)
	{
		ret |= eeprom_write(CONFIG_SYS_I2C_EEPROM_ADDR, addr+(offset++), tmp++, 1);
	}

	if(ret != 0)
		offset = 0;

	return offset;
}
#endif // CONFIG_SPL_BUILD

/* Only used via do_factoryconfig, so don't build for SPL */
#ifndef CONFIG_SPL_BUILD
static int put_factory_config_block(void)
{
	int i, ret;
	uchar* tmp;
	unsigned int addr = 0x00;
	GenericDataBlock* block = NULL;

	/* Set the magic word/version */
	factory_config_block.ConfigMagicWord = CONFIG_I2C_MAGIC_WORD;
	factory_config_block.ConfigVersion = CONFIG_I2C_VERSION;

	/* Write out factory config */
	tmp = (uchar*)&factory_config_block;
	ret = 0;
	for (i = 0; i < sizeof(factory_config_block); i++)
	{
		ret |= eeprom_write(CONFIG_SYS_I2C_EEPROM_ADDR, addr++, tmp++, 1);
	}

	/* Calculate and writeout the factory config block check sum */
	u16 fcSum = calc_checksum((uchar*)&factory_config_block, sizeof(struct I2CFactoryConfig));
	tmp = (uchar*)&fcSum;
	for (i = 0; i < sizeof(u16); i++)
	{
		ret |= eeprom_write(CONFIG_SYS_I2C_EEPROM_ADDR, addr++, tmp++, 1);
	}

	/* Add the 2nd MAC address */
	/* call get_2nd_mac_address() to make sure its generic block exists first */
	if (get_2nd_mac_address() == NULL)
		return 1;
	block = get_generic_block(ee2ndMAC);
	addr += put_generic_data_block(block, addr);

	/* Add the RTC Cal block */
	/* call get_rtc_cal() to make sure its generic block exists first */
	if (get_rtc_cal() == NULL)
		return 1;
	block = get_generic_block(eeRtcCal);
	addr += put_generic_data_block(block, addr);

	/* Blank out the next 4 bytes in EEPROM, so there will be no false magic word readings */
	for(i = 0; i < sizeof(u32); i++)
	{
		ret |= eeprom_write(CONFIG_SYS_I2C_EEPROM_ADDR, addr++, 0, 1);
	}

	if (ret) {
		print_console(eeError, "Error Writing Factory Configuration Block");
	}
	else {
		print_console(eeInfo, "Factory Configuration Block Saved"); 
	}
	return ret;
}
#endif // CONFIG_SPL_BUILD

static int is_known_generic_type(int type)
{
	switch(type)
	{
		case ee2ndMAC:
		case eeRtcCal:
			return 1;
	}

	return 0;
}

typedef enum ParseStatus 
{
	eeSuccess, 
	eeNoMagicWord, 
	eeUnknownConfig, 
	eeI2CError, 
	eeBadChecksum, 
	eeMallocError, 
	eeBadGenBlock
} ParseStatus;
static ParseStatus parse_factory_config(void)
{
	int status;
	u16 calcSum, readSum;
	uchar buffer[sizeof(struct I2CFactoryConfigV3)];
	int configSize;

	/* Grab the base config size */
	configSize = get_factory_config_size();
	if(configSize == -1)
	{
		print_console(eeDebug, "Couldn't figure out config size");
		return eeUnknownConfig;
	}

	/* Read data from eeprom */
	status = eeprom_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0x00, buffer, configSize);
	if(status != 0)
		return eeI2CError;

	/* Check Magic Word */
	struct I2CFactoryConfig *config = (struct I2CFactoryConfig*)buffer;
	if(config->ConfigMagicWord != CONFIG_I2C_MAGIC_WORD)
	{
		print_console(eeDebug, "No magic word found for factoryconfig");
		return eeNoMagicWord;
	}

	/* Read base config checksum */
	status = eeprom_read(CONFIG_SYS_I2C_EEPROM_ADDR, configSize,
			(uchar*)&readSum, sizeof(u16));
	if(status != 0)
		return eeI2CError;

	/* Calculate the checksum of the config we just read */
	calcSum = calc_checksum(buffer, configSize);

	/* Check if the calculated checksum vs stored */
	if(calcSum != readSum)
	{
		print_console(eeDebug, "factoryconfig checksum didn't match, Expected: %04X Actual: %04X", readSum, calcSum);
		return eeBadChecksum;
	}

	/* Set the factory config block */
	memcpy(&factory_config_block, buffer, sizeof(struct I2CFactoryConfig));

	/* If this is a config 1.3 block, convert the 2nd MAC address to a generic block */
	if(factory_config_block.ConfigVersion == CONFIG_I2C_VERSION_1_3)
	{
		print_console(eeDebug, "Version 1.3 detected, converting to 1.2");
		struct I2CFactoryConfigV3 *config3 = (struct I2CFactoryConfigV3*)buffer;
		/* Set the mac 2 address block */
		MacAddressBlock *mac = get_2nd_mac_address();
		memcpy(mac->MacAddress, config3->MACADDR2, sizeof(MacAddressBlock));
	}
	
	/* Go through all generic blocks and add them to the list */
	int flashIndex = configSize + sizeof(u16);
	while((CONFIG_SYS_EEPROM_SIZE - flashIndex) > sizeof(GenericDataBlockHeader))
	{
		/* Allocate a generic block */
		GenericDataBlock *block = malloc(sizeof(GenericDataBlock));
		if(block == NULL)
			return eeMallocError;

		/* Read in the generic header */
		status = eeprom_read(CONFIG_SYS_I2C_EEPROM_ADDR, flashIndex,
				(uchar*)&block->Header, sizeof(GenericDataBlockHeader));
		flashIndex += sizeof(GenericDataBlockHeader);
		if(status != 0)
		{
			free(block);
			return eeI2CError;
		}
		 
		/* Check if this a generic block by checking for the magic word */	
		if(block->Header.MagicWord != CONFIG_I2C_MAGIC_WORD)
		{
			print_console(eeDebug, "No magic word for block, must be done");
			free(block);
			break;
		}
		else
			print_console(eeDebug, "Found magic word for block(%04X), parsing", block->Header.Type);

		/* Quick sanity check to see if data size is reasonable */
		if(block->Header.Size > CONFIG_SYS_EEPROM_SIZE)
		{
			print_console(eeInfo, "Generic block size(%04X) too large", block->Header.Size);
			free(block);
			return eeBadGenBlock;
		}

		/* malloc room for the data */	
		block->Data = malloc(block->Header.Size);
		if(block->Data == NULL)
		{
			free(block);
			return eeMallocError;
		}

		/* Read in the data */
		status = eeprom_read(CONFIG_SYS_I2C_EEPROM_ADDR, flashIndex,
				(uchar*)block->Data, block->Header.Size);
		flashIndex += block->Header.Size;
		if(status != 0)
		{
			free(block);
			return eeI2CError;
		}

		/* Calculate the checksum */
		u32 sumHeader = calc_checksum((uchar*)&block->Header, sizeof(GenericDataBlockHeader));
		u32 sumData = calc_checksum((uchar*)block->Data, block->Header.Size);
		u32 sumGen = sumHeader + sumData;
		u32 readSumGen;

		/* Grab generic block checksum */
		status = eeprom_read(CONFIG_SYS_I2C_EEPROM_ADDR, flashIndex,
				(uchar*)&readSumGen, sizeof(u32));
		flashIndex += sizeof(u32);
		if(status != 0)
		{
			free(block->Data);
			free(block);
			return eeI2CError;
		}

		/* Check if checksum matches stored value */
		if(readSumGen != sumGen)
		{
			print_console(eeDebug, "Block's checksum didn't match, Expected: %08X Actual: %08X", readSumGen, sumGen);
			free(block->Data);
			free(block);
			return eeBadGenBlock;
		}	

		/* Check to make sure this is a known type */ 
		if(is_known_generic_type(block->Header.Type) == 0)
		{
			print_console(eeInfo, "Unknown generic block(%04X), skipping", block->Header.Type);
			free(block->Data);
			free(block);
			/* This type is unknown, skip it */
			continue;
		}

		/* Add this block to our list */
		print_console(eeDebug, "Found generic block(%04X), adding to list", block->Header.Type);
		add_generic_block(block);
	}

	return eeSuccess;
}

static void set_default_config(void)
{
	print_console(eeInfo, "Setting configuration to defaults");

	/* Set default config */
	memcpy(&factory_config_block, &default_factory_config, sizeof(struct I2CFactoryConfig));

	/* Set default 2nd mac */
	get_2nd_mac_address();

	/* Set default RTC Cal data */
	get_rtc_cal();
}

#define ERROR_I2C (-1)
#define ERROR_BAD_CONFIG (1)
/**
 *  return value: -1 i2c access error, 1 bad factory configuration, 0 = OK
 */
int get_factory_config_block(void)
{
	ParseStatus parseStatus;
	int status = 0;


	/* Parse in the config */
	parseStatus = parse_factory_config();

	switch(parseStatus)
	{
		case eeUnknownConfig:
		case eeNoMagicWord:
		case eeBadChecksum:
			print_console(eeError, "Factory Configuration Invalid");
			print_console(eeInfo, "You must set the factory configuration to make permanent");
			status = ERROR_BAD_CONFIG;
			set_default_config();
			break;
		case eeI2CError:
			print_console(eeError, "I2C error will reading Factory Configuration");
			status = ERROR_I2C;
			set_default_config();
			break;
		case eeMallocError:
			print_console(eeError, "malloc Failed");
			status = ERROR_BAD_CONFIG;
			set_default_config();
			break;
		case eeBadGenBlock:
			print_console(eeWarning, "Generic Configuration Block Invalid");
			status = ERROR_BAD_CONFIG;
			break;
		case eeSuccess:
			status = 0;
			break;
	}


	return status;
}

/* This is a trivial atoi implementation since we don't have one available */
int atoi(char *string)
{
	int length;
	int retval = 0;
	int i;
	int sign = 1;

	length = strlen(string);
	for (i = 0; i < length; i++) {
		if (0 == i && string[0] == '-') {
			sign = -1;
			continue;
		}
		if (string[i] > '9' || string[i] < '0') {
			break;
		}
		retval *= 10;
		retval += string[i] - '0';
	}
	retval *= sign;
	return retval;
}

void get_board_serial(struct tag_serialnr *sn)
{
	sn->low = factory_config_block.SerialNumber;
	sn->high = 0;
}

/* Only used via do_factoryconfig, so don't build for SPL */
#ifndef CONFIG_SPL_BUILD
/**
 * Set the factory config block mac address from an ascii string
 * of the form xx:xx:xx:xx:xx:xx
 */
static void set_mac_from_string(u8* mac,char *buffer)
{
	char *p = buffer;
	int i=0;
	for (i = 0; i < 6; i++) {
		int j = 0;
		while(p[j] && (p[j] != ':')) j++;
		if (1) {
			unsigned int t;
			char temp = p[j];
			p[j] = 0;
			t = simple_strtoul(p, NULL, 16);
			p[j] = temp;
			mac[i] = t&0xFF;
		}
		p = &p[j];
		if (*p) p++;
	}
}
#endif // CONFIG_SPL_BUILD

/* Only build u-boot commands for u-boot proper, not the SPL */
#ifndef CONFIG_SPL_BUILD
static int do_factoryconfig(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = CMD_RET_FAILURE;
	char *buffer = (char*)malloc(80 * sizeof(char));
	char *strcopy = NULL;

	if (argc == 1) {
		/* List configuration info */
		puts ("Factory Configuration:\n");
		printf("Config Version : %d.%d\n",
				factory_config_block.ConfigVersion>>16,
				factory_config_block.ConfigVersion&0xFFFF);
		printf("MAC Address    : %02X:%02X:%02X:%02X:%02X:%02X\n",
				factory_config_block.MACADDR[0],
				factory_config_block.MACADDR[1],
				factory_config_block.MACADDR[2],
				factory_config_block.MACADDR[3],
				factory_config_block.MACADDR[4],
				factory_config_block.MACADDR[5]);
		MacAddressBlock *mac2 = get_2nd_mac_address();
		printf("MAC Address2   : %02X:%02X:%02X:%02X:%02X:%02X\n",
				mac2->MacAddress[0],
				mac2->MacAddress[1],
				mac2->MacAddress[2],
				mac2->MacAddress[3],
				mac2->MacAddress[4],
				mac2->MacAddress[5]);

		printf("Serial Number  : %d\n",
				factory_config_block.SerialNumber);
		printf("Model Number   : %s\n",
				factory_config_block.ModelNumber);
		RtcCalBlock *rtc_cal = get_rtc_cal();
		printf("RTC Cal Value  : %d\n",
				rtc_cal->RtcCalVal);
	} else {
		unsigned int i;
		if (0 == strncmp(argv[1],"set",3)) {
			sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
					factory_config_block.MACADDR[0],
					factory_config_block.MACADDR[1],
					factory_config_block.MACADDR[2],
					factory_config_block.MACADDR[3],
					factory_config_block.MACADDR[4],
					factory_config_block.MACADDR[5]);
			cli_readline_into_buffer ("MAC Address(1): ", buffer, 0);
			if((strlen(buffer) == 1) && ('.' == buffer[0]))
				goto done;
			set_mac_from_string(factory_config_block.MACADDR,buffer);

			MacAddressBlock *mac2 = get_2nd_mac_address();
			sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
					mac2->MacAddress[0],
					mac2->MacAddress[1],
					mac2->MacAddress[2],
					mac2->MacAddress[3],
					mac2->MacAddress[4],
					mac2->MacAddress[5]);
			cli_readline_into_buffer ("MAC Address(2): ", buffer, 0);
			if((strlen(buffer) == 1) && ('.' == buffer[0]))
				goto done;                        
			set_mac_from_string(mac2->MacAddress,buffer);

			sprintf(buffer, "%d", factory_config_block.SerialNumber);
			cli_readline_into_buffer ("Serial Number : ", buffer, 0);
			if((strlen(buffer) == 1) && ('.' == buffer[0]))
				goto done;
			i = atoi(buffer);
			if (i > 0) factory_config_block.SerialNumber = i;

			sprintf(buffer, "%s", factory_config_block.ModelNumber);
			cli_readline_into_buffer ("Model Number  : ", buffer, 0);
			if((strlen(buffer) == 1) && ('.' == buffer[0]))
				goto done;
			buffer[31] = '\0';
			strncpy(factory_config_block.ModelNumber, buffer, 32);
			/* make sure it is null terminated */
			factory_config_block.ModelNumber[31] = '\0';

			RtcCalBlock *rtc_cal = get_rtc_cal();
			sprintf(buffer, "%d", rtc_cal->RtcCalVal);
			cli_readline_into_buffer ("RTC Cal Value  : ", buffer, 0);
			if((strlen(buffer) == 1) && ('.' == buffer[0]))
				goto done;
			i = atoi(buffer);
			rtc_cal->RtcCalVal = i;

		} else if (0 == strncmp(argv[1],"save",4)) {
			put_factory_config_block();
			print_console(eeInfo, "Configuration Saved");
		}
		else {
			print_console(eeError, "Unknown Option");
			goto done;
		}
	}
	ret = CMD_RET_SUCCESS;
done:
	free(buffer);
	if(strcopy)
		free(strcopy);
	return ret;
}

U_BOOT_CMD(factoryconfig,	CONFIG_SYS_MAXARGS,	0,	do_factoryconfig,
		"mitysom-5csx factory config block operations",
		"    - print current configuration\n"
		"factoryconfig set\n"
		"         - set new configuration (interactive)\n"
		"factoryconfig save\n"
		"         - write new configuration to I2C FLASH\n"
		);
#endif // CONFIG_SPL_BUILD
