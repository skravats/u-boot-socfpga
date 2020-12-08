/*
 * (C) Copyright 2019
 * Daniel Vincelette, dvincelette@criticallink.com
 *
 * based on rtc/m41t60.c which is ...
 *
 * (C) Copyright 2007
 * Larry Johnson, lrj@acm.org
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

/*
 * Abracon AB1805 serial access real-time clock
 *
 * Application Manual:
 * https://abracon.com/Support/AppsManuals/Precisiontiming/AB18XX-Application-Manual.pdf
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <rtc.h>
#include <i2c.h>

#if CONFIG_IS_ENABLED(DM_RTC)
enum {
	AB1805_CMD_CS,
	AB1805_CMD_CAL_MODE,
};

#define NUM_DATE_TIME_REGS 8
#define AB18XX_REG_HDTHS	0x00	/* 00-99 */
#define AB18XX_REG_SECS		0x01	/* 00-59 */
#define AB18XX_REG_MIN		0x02	/* 00-59 */
#define AB18XX_REG_HOUR		0x03	/* 00-23, or 1-12{am,pm} */
#define AB18XX_REG_MDAY		0x04	/* 01-31 */
#define AB18XX_REG_MONTH	0x05	/* 01-12 */
#define AB18XX_REG_YEAR		0x06	/* 00-99 */
#define AB18XX_REG_WDAY		0x07	/* 00-06 */
#define AB18XX_REG_ALM_HDTHS	0x08	/* 00-99 */
#define AB18XX_REG_CTL2		0x11
#define AB18XX_REG_SQW		0x13
#define AB18XX_REG_XT_CAL	0x14
#define AB18XX_REG_OSC		0x1D
#define AB18XX_REG_CFG_KEY	0x1F

/* Value from application manual */
#define FREQ_ADJ	1.90735

/* Register values for no square wave output */
#define AB18XX_REG_SQW_16HZ_OFF		0x26
#define AB18XX_REG_CTL2_16HZ_OFF	0x3C

/* Register values for 16 Hz square wave output */
#define AB18XX_REG_SQW_16HZ_ON		0xAB
#define AB18XX_REG_CTL2_16HZ_ON		0x24

/* Register value for software reset */
#define ABX8XX_REG_CFG_KEY_RST		0x3C

static int ab1805_rtc_get(struct udevice *dev, struct rtc_time *t)
{
	int rv;
	uchar time_reg[NUM_DATE_TIME_REGS];

	/* Grab all the time registers */
	rv = dm_i2c_read(dev, AB18XX_REG_HDTHS, time_reg, sizeof(time_reg));
	if (rv) {
		printf("%s: I2C read failed: %d\n", __func__, rv);
		return rv;
	}

	t->tm_sec = bcd2bin(time_reg[AB18XX_REG_SECS] & 0x7f);
	t->tm_min = bcd2bin(time_reg[AB18XX_REG_MIN] & 0x7f);
	t->tm_hour = bcd2bin(time_reg[AB18XX_REG_HOUR] & 0x3f); // assumes 24 hr mode
	t->tm_wday = bcd2bin(time_reg[AB18XX_REG_WDAY] & 0x07);
	t->tm_mday = bcd2bin(time_reg[AB18XX_REG_MDAY] & 0x3f);
	t->tm_mon = bcd2bin(time_reg[AB18XX_REG_MONTH] & 0x1f);

	/* assume 20YY not 19YY */
	t->tm_year = bcd2bin(time_reg[AB18XX_REG_YEAR]) + 2000;

	debug("%s: secs=%d, mins=%d, "
		"hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__, t->tm_sec, t->tm_min,
		t->tm_hour, t->tm_mday,
		t->tm_mon, t->tm_year, t->tm_wday);

	/* initial clock setting can be undefined */
	return 0;
}

static int ab1805_rtc_set(struct udevice *dev, const struct rtc_time *t)
{
	int rv;
	uchar buf[NUM_DATE_TIME_REGS];

	buf[AB18XX_REG_SECS] = bin2bcd(t->tm_sec);
	buf[AB18XX_REG_MIN] = bin2bcd(t->tm_min);
	buf[AB18XX_REG_HOUR] = bin2bcd(t->tm_hour);
	buf[AB18XX_REG_WDAY] = bin2bcd(t->tm_wday);
	buf[AB18XX_REG_MDAY] = bin2bcd(t->tm_mday);
	buf[AB18XX_REG_MONTH] = bin2bcd(t->tm_mon);

	/* assume 20YY not 19YY */
	buf[AB18XX_REG_YEAR] = bin2bcd(t->tm_year - 2000);

	rv = dm_i2c_write(dev, AB18XX_REG_HDTHS, buf, sizeof(buf));
	if (rv) {
		printf("%s: I2C write failed: %d\n", __func__, rv);
	}

	return rv;
}

static int ab1805_rtc_reset(struct udevice *dev)
{
	int rv;
	uchar rst = ABX8XX_REG_CFG_KEY_RST;

	rv = dm_i2c_write(dev, AB18XX_REG_CFG_KEY, &rst, sizeof(rst));
	if (rv) {
		printf("%s: I2C write failed: %d\n", __func__, rv);
	}

	return rv;
}

static const struct rtc_ops ab1805_rtc_ops = {
	.get = ab1805_rtc_get,
	.set = ab1805_rtc_set,
	.reset = ab1805_rtc_reset,
};

static const struct udevice_id ab1805_rtc_ids[] = {
	{ .compatible = "abracon,ab1805" },
	{ }
};

U_BOOT_DRIVER(rtc_ab1805) = {
	.name   = "rtc-ab1805",
	.id     = UCLASS_RTC,
	.of_match = ab1805_rtc_ids,
	.ops    = &ab1805_rtc_ops,
};

/* Algorithm from application manual: 5.9.1 XT Oscillator Digital Calibration */
static int ab1805_calibration_set(struct udevice *dev, int cal_val)
{
	int rv;
	double padj = (double)cal_val / 10.0;
	int adj;
	if (padj < 0)
		adj = (int)((padj/FREQ_ADJ)-0.5);
	else
		adj = (int)((padj/FREQ_ADJ)+0.5);

	debug("%s: adj = %d\n", __func__, adj);

	int xtcal = 0;
	int cmdx = 0;
	int offsetx = 0;
	if (adj < -320) {
		printf("%s: XT frequency too high to calibrate\n", __func__);
		return CMD_RET_FAILURE;
	} else if (adj < -256) {
		xtcal = 3;
		cmdx = 1;
		offsetx = (adj + 192)/2;
	} else if (adj < -192) {
		xtcal = 3;
		cmdx = 0;
		offsetx = adj + 192;
	} else if (adj < -128) {
		xtcal = 2;
		cmdx = 0;
		offsetx = adj + 128;
	} else if (adj < -64) {
		xtcal = 1;
		cmdx = 0;
		offsetx = adj + 64;
	} else if (adj < 64) {
		xtcal = 0;
		cmdx = 0;
		offsetx = adj;
	} else if (adj < 128) {
		xtcal = 0;
		cmdx = 1;
		offsetx = adj/2;
	} else {
		printf("%s: XT frequency too low to calibrate\n", __func__);
		return CMD_RET_FAILURE;
	}

	debug("%s: xtcal = %x cmdx = %x offsetx = %x\n", __func__, xtcal, cmdx, offsetx);
	uchar cal_xt_reg = (cmdx << 7) & 0x80;
	cal_xt_reg |= offsetx & 0x7F;
	debug("%s: cal_xt_reg = %x\n", __func__, cal_xt_reg);
	rv = dm_i2c_write(dev, AB18XX_REG_XT_CAL, &cal_xt_reg, sizeof(cal_xt_reg));
	if (rv) {
		printf("%s: I2C write failed: %d\n", __func__, rv);
		return CMD_RET_FAILURE;
	}

	uchar osc_reg;
	rv = dm_i2c_read(dev, AB18XX_REG_OSC, &osc_reg, sizeof(osc_reg));
	if (rv) {
		printf("%s: I2C read failed: %d\n", __func__, rv);
		return CMD_RET_FAILURE;
	}

	debug("%s: osc_reg rd = %x\n", __func__, osc_reg);
	osc_reg &= 0x3F;
	osc_reg |= (xtcal << 6) & 0xC0;
	debug("%s: osc_reg wr = %x\n", __func__, osc_reg);
	rv = dm_i2c_write(dev, AB18XX_REG_OSC, &osc_reg, sizeof(osc_reg));
	if (rv) {
		printf("%s: I2C write failed: %d\n", __func__, rv);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int ab1805_calibration_mode(struct udevice *dev, int mode)
{
	int rv;
	uchar sqw_val, ctl2_val;
	if (mode == 0) {
		sqw_val = AB18XX_REG_SQW_16HZ_OFF;
		ctl2_val = AB18XX_REG_CTL2_16HZ_OFF;
	} else if (mode == 1) {
		sqw_val = AB18XX_REG_SQW_16HZ_ON;
		ctl2_val = AB18XX_REG_CTL2_16HZ_ON;
	} else {
		return CMD_RET_USAGE;
	}

	rv = dm_i2c_write(dev, AB18XX_REG_SQW, &sqw_val, sizeof(sqw_val));
	if (rv) {
		printf("%s: I2C write failed: %d\n", __func__, rv);
		return CMD_RET_FAILURE;
	}

	rv = dm_i2c_write(dev, AB18XX_REG_CTL2, &ctl2_val, sizeof(ctl2_val));
	if (rv) {
		printf("%s: I2C write failed: %d\n", __func__, rv);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

cmd_tbl_t cmd_ab1805[] = {
	U_BOOT_CMD_MKENT(cs, 3, 0, (void *)AB1805_CMD_CS, "", ""),
	U_BOOT_CMD_MKENT(cal_mode, 3, 0, (void *)AB1805_CMD_CAL_MODE, "", ""),
};

static int do_ab1805(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int rv;
	int ret = CMD_RET_USAGE;
	int arg2 = 0;
	cmd_tbl_t *c;
	struct udevice *dev;

	c = find_cmd_tbl(argv[1], cmd_ab1805, ARRAY_SIZE(cmd_ab1805));

	/* All commands require 'maxargs' arguments */
	if (!c || argc != (c->maxargs)) {
		return CMD_RET_USAGE;
	}

	rv = uclass_get_device(UCLASS_RTC, 0, &dev);
	if (rv) {
		printf("Cannot find RTC: err=%d\n", rv);
		return CMD_RET_FAILURE;
	}

	/* arg2 used as calibration_value or cal_mode */
	arg2 = simple_strtol(argv[2], NULL, 10);

	switch ((int)c->cmd) {
	case AB1805_CMD_CS:
		ret = ab1805_calibration_set(dev, arg2);
		break;
	case AB1805_CMD_CAL_MODE:
		ret = ab1805_calibration_mode(dev, arg2);
		break;
	}

	return ret;
}

U_BOOT_CMD(
	ab1805,	3,	0, do_ab1805,
	"ab1805 rtc control",
	"cs calibration_value - write the calibration value to the rtc\n"
	"ab1805 cal_mode 0|1 - put the rtc into calibration mode\n"
);
#endif /* CONFIG_IS_ENABLED(DM_RTC) */
