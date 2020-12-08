#ifndef CONFIG_BLOCK_H_
#define CONFIG_BLOCK_H_

#define CONFIG_I2C_MAGIC_WORD	0x012C0138
#define CONFIG_I2C_VERSION_1_1	0x00010001 /* Never released */
#define CONFIG_I2C_VERSION_1_2	0x00010002 /* Base config */
#define CONFIG_I2C_VERSION_1_3	0x00010003 /* Base config with MACADDR2 */
#define CONFIG_I2C_VERSION	CONFIG_I2C_VERSION_1_2

struct I2CFactoryConfig {
	u32               ConfigMagicWord;  /** CONFIG_I2C_MAGIC_WORD */
	u32               ConfigVersion;    /** CONFIG_I2C_VERSION */
	u8                MACADDR[6];       /** MAC Address assigned to part */
	/* Two bytes padding, written to EEPROM */
	u32               FpgaType;         /** Not Used */
	u32               Spare;            /** Not Used */
	u32               SerialNumber;     /** serial number assigned to part */
	char              ModelNumber[32];  /** board model number, human readable text, NULL terminated */
};

/* NOTE: This struct was added in order to handle a 2nd MAC address.
 * Unfortunately it is not backwards compatible with older versions of
 * uboot, which can only parse V2 structs. This struct will be kept for
 * parsing any EEPROMs with V3 format but the new V2 with generic
 * data blocks for storing the 2nd MAC address will be written if
 * factoryconfig save is used
 */
struct I2CFactoryConfigV3 {
	u32               ConfigMagicWord;  /** CONFIG_I2C_MAGIC_WORD */
	u32               ConfigVersion;    /** CONFIG_I2C_VERSION */
	u8                MACADDR[6];       /** MAC Address assigned to part */
	/* Two bytes padding, written to EEPROM */
	u32               FpgaType;         /** Not Used */
	u32               Spare;            /** Not Used */
	u32               SerialNumber;     /** serial number assigned to part */
	char              ModelNumber[32];  /** board model number, human readable text, NULL terminated */
	u8                MACADDR2[6];      /** 2nd MAC Address assigned to part */
	/* Two bytes padding, written to EEPROM */
};

typedef struct GenericDataBlockHeader {
	u32 MagicWord;
	u16 Type;
	u16 Size;
} GenericDataBlockHeader;

struct GenericDataBlock;
typedef struct GenericDataBlock {
	struct GenericDataBlockHeader Header;
	uchar *Data;
	struct GenericDataBlock *Next;
}GenericDataBlock;

typedef enum GenericType {
	ee2ndMAC = 0x0000,
	eeRtcCal = 0x0001,
} GenericType;

typedef struct MacAddressBlock {
	u8 MacAddress[6];
	/* Two bytes padding, NOT written to EEPROM because we write length Header.Size */
}MacAddressBlock;

typedef struct RtcCalBlock {
	s32 RtcCalVal; /* PPM * 10 to allow for signal decimal percision */
}RtcCalBlock;

extern struct I2CFactoryConfig  factory_config_block;
extern struct GenericDataBlock *generic_data_block;
extern int get_factory_config_block(void);
extern MacAddressBlock* get_2nd_mac_address(void);
extern RtcCalBlock* get_rtc_cal(void);

#endif
