/* drivers/staging/taos/tsl277x.c
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version. 
* Copyright (C) 2012 Sony Mobile Communications AB.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
* NOTE: This file has been modified by Sony Ericsson Mobile Communications AB /
* Sony Mobile Communications AB. Modifications are licensed under the License.
*/
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/i2c/tsl2772.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <mach/gpio.h>

//#include <mach/regs-gpio.h>
//#include <mach/gpio.h>
//#include <mach/irqs.h>

enum tsl277x_regs {
	TSL277X_ENABLE,
	TSL277X_ALS_TIME,
	TSL277X_PRX_TIME,
	TSL277X_WAIT_TIME,
	TSL277X_ALS_MINTHRESHLO,
	TSL277X_ALS_MINTHRESHHI,
	TSL277X_ALS_MAXTHRESHLO,
	TSL277X_ALS_MAXTHRESHHI,
	TSL277X_PRX_MINTHRESHLO,
	TSL277X_PRX_MINTHRESHHI,
	TSL277X_PRX_MAXTHRESHLO,
	TSL277X_PRX_MAXTHRESHHI,
	TSL277X_PERSISTENCE,
	TSL277X_CONFIG,
	TSL277X_PRX_PULSE_COUNT,
	TSL277X_CONTROL,

	TSL277X_REVID = 0x11,
	TSL277X_CHIPID,
	TSL277X_STATUS,
	TSL277X_ALS_CHAN0LO,
	TSL277X_ALS_CHAN0HI,
	TSL277X_ALS_CHAN1LO,
	TSL277X_ALS_CHAN1HI,
	TSL277X_PRX_LO,
	TSL277X_PRX_HI,

	TSL277X_REG_PRX_OFFS = 0x1e,
	TSL277X_REG_MAX,
};

enum tsl277x_cmd_reg {
	TSL277X_CMD_REG           = (1 << 7),
	TSL277X_CMD_INCR          = (0x1 << 5),
	TSL277X_CMD_SPL_FN        = (0x3 << 5),
	TSL277X_CMD_PROX_INT_CLR  = (0x5 << 0),
	TSL277X_CMD_ALS_INT_CLR   = (0x6 << 0),
};

enum tsl277x_en_reg {
	TSL277X_EN_PWR_ON   = (1 << 0),
	TSL277X_EN_ALS      = (1 << 1),
	TSL277X_EN_PRX      = (1 << 2),
	TSL277X_EN_WAIT     = (1 << 3),
	TSL277X_EN_ALS_IRQ  = (1 << 4),
	TSL277X_EN_PRX_IRQ  = (1 << 5),
	TSL277X_EN_SAI      = (1 << 6),
};

enum tsl277x_status {
	TSL277X_ST_ALS_VALID  = (1 << 0),
	TSL277X_ST_PRX_VALID  = (1 << 1),
	TSL277X_ST_ALS_IRQ    = (1 << 4),
	TSL277X_ST_PRX_IRQ    = (1 << 5),
	TSL277X_ST_PRX_SAT    = (1 << 6),
};

enum {
	TSL277X_ALS_GAIN_MASK = (3 << 0),
	TSL277X_ALS_AGL_MASK  = (1 << 2),
	TSL277X_ALS_AGL_SHIFT = 2,
	TSL277X_ATIME_PER_100 = 273,
	TSL277X_ATIME_DEFAULT_MS = 50,
	SCALE_SHIFT = 11,
	RATIO_SHIFT = 10,
	MAX_ALS_VALUE = 0xffff,
	MIN_ALS_VALUE = 10,
	GAIN_SWITCH_LEVEL = 100,
	GAIN_AUTO_INIT_VALUE = 16,
};

static u8 const tsl277x_ids[] = {
	0x39,
	0x30,
};

static char const *tsl277x_names[] = {
	"tsl27721 / tsl27725",
	"tsl27723 / tsl2777",
};

static u8 const restorable_regs[] = {
	TSL277X_ALS_TIME,
	TSL277X_PRX_TIME,
	TSL277X_WAIT_TIME,
	TSL277X_PERSISTENCE,
	TSL277X_CONFIG,
	TSL277X_PRX_PULSE_COUNT,
	TSL277X_CONTROL,
	TSL277X_REG_PRX_OFFS,
};

static u8 const als_gains[] = {
	1,
	8,
	16,
	120
};

struct taos_als_info {
	int ch0;
	int ch1;
	u32 cpl;
	u32 saturation;
	int lux;
	int ga;			//20120829
};

struct taos_prox_info {
	int raw;
	int detected;
};

static struct lux_segment segment_default[] = {
	{
		.ratio = (435 << RATIO_SHIFT) / 1000,
		.k0 = (46516 << SCALE_SHIFT) / 1000,
		.k1 = (95381 << SCALE_SHIFT) / 1000,
	},
	{
		.ratio = (551 << RATIO_SHIFT) / 1000,
		.k0 = (23740 << SCALE_SHIFT) / 1000,
		.k1 = (43044 << SCALE_SHIFT) / 1000,
	},
};

struct tsl2772_chip {
	struct mutex lock;
	struct i2c_client *client;
	struct taos_prox_info prx_inf;
	struct taos_als_info als_inf;
	struct taos_parameters params;
	struct tsl2772_i2c_platform_data *pdata;
	u8 shadow[TSL277X_REG_MAX];
	struct input_dev *p_idev;
	struct input_dev *a_idev;
	int in_suspend;
	int wake_irq;
	int irq_pending;
	bool unpowered;
	bool als_enabled;
	bool prx_enabled;
	bool bpre_als_enabled;
	bool bpre_prx_enabled;
	struct lux_segment *segment;
	int segment_num;
	int seg_num_max;
	bool als_gain_auto;
	
	struct	delayed_work work;
	struct	work_struct imwork;
	int 	delay;
	bool	bkflag;
	bool 	blog;
	int 	Panel_ID;
	
	struct wake_lock w_lock;
};

static struct tsl2772_chip *chip;

#define golden_ga 3750

			
static int taos_i2c_read(struct tsl2772_chip *chip, u8 reg, u8 *val)
{
	int ret;
	s32 read;
	struct i2c_client *client = chip->client;

	ret = i2c_smbus_write_byte(client, (TSL277X_CMD_REG | reg));
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to write register %x\n",
				__func__, reg);
		return ret;
	}
	read = i2c_smbus_read_byte(client);
	if (read < 0) {
		dev_err(&client->dev, "%s: failed to read from register %x\n",
				__func__, reg);
		return ret;
	}
	*val = read;
	return 0;
}

static int taos_i2c_blk_read(struct tsl2772_chip *chip,
		u8 reg, u8 *val, int size)
{
	s32 ret;
	struct i2c_client *client = chip->client;

	ret =  i2c_smbus_read_i2c_block_data(client,
			TSL277X_CMD_REG | TSL277X_CMD_INCR | reg, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
				__func__, reg, size);
	return ret;
}

static int taos_i2c_write(struct tsl2772_chip *chip, u8 reg, u8 val)
{
	int ret;
	struct i2c_client *client = chip->client;

	ret = i2c_smbus_write_byte_data(client, TSL277X_CMD_REG | reg, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed to write register %x\n",
				__func__, reg);
	return ret;
}

static int taos_i2c_blk_write(struct tsl2772_chip *chip,
		u8 reg, u8 *val, int size)
{
	s32 ret;
	struct i2c_client *client = chip->client;

	ret =  i2c_smbus_write_i2c_block_data(client,
			TSL277X_CMD_REG | TSL277X_CMD_INCR | reg, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
				__func__, reg, size);
	return ret;
}

static int set_segment_table(struct tsl2772_chip *chip,
		struct lux_segment *segment, int seg_num)
{
	int i;
	struct device *dev = &chip->client->dev;

	chip->seg_num_max = chip->pdata->segment_num ?
			chip->pdata->segment_num : ARRAY_SIZE(segment_default);

	if (!chip->segment) {
		dev_dbg(dev, "%s: allocating segment table\n", __func__);
		chip->segment = kzalloc(sizeof(*chip->segment) *
				chip->seg_num_max, GFP_KERNEL);
		if (!chip->segment) {
			dev_err(dev, "%s: no memory!\n", __func__);
			return -ENOMEM;
		}
	}
	if (seg_num > chip->seg_num_max) {
		dev_warn(dev, "%s: %d segment requested, %d applied\n",
				__func__, seg_num, chip->seg_num_max);
		chip->segment_num = chip->seg_num_max;
	} else {
		chip->segment_num = seg_num;
	}
	memcpy(chip->segment, segment,
			chip->segment_num * sizeof(*chip->segment));
	dev_dbg(dev, "%s: %d segment requested, %d applied\n", __func__,
			seg_num, chip->seg_num_max);
	for (i = 0; i < chip->segment_num; i++)
		dev_dbg(dev, "segment %d: ratio %6u, k0 %6u, k1 %6u\n",
				i, chip->segment[i].ratio,
				chip->segment[i].k0, chip->segment[i].k1);
	return 0;
}

static void taos_calc_cpl(struct tsl2772_chip *chip)
{
	u32 cpl;
	u32 sat;
	u8 atime = chip->shadow[TSL277X_ALS_TIME];
	u8 agl = (chip->shadow[TSL277X_CONFIG] & TSL277X_ALS_AGL_MASK)
			>> TSL277X_ALS_AGL_SHIFT;
	u32 time_scale = (256 - atime ) * 2730 / 600;   //2730 =2.73*1000  ( CPL 放大 1000倍 ）
	

	cpl = time_scale * chip->params.als_gain;
	if (agl)
		cpl = cpl * 16 / 1000;
	sat = min_t(u32, MAX_ALS_VALUE, (u32)(256 - atime) << 10);
	sat = sat * 8 / 10;
	dev_dbg(&chip->client->dev,
			"%s: cpl = %u [time_scale %u, gain %u, agl %u], "
			"saturation %u\n", __func__, cpl, time_scale,
			chip->params.als_gain, agl, sat);
	chip->als_inf.cpl = cpl;
	chip->als_inf.saturation = sat;
}

static int set_als_gain(struct tsl2772_chip *chip, int gain)
{
	int rc;
	u8 ctrl_reg  = chip->shadow[TSL277X_CONTROL] & ~TSL277X_ALS_GAIN_MASK;

	switch (gain) {
	case 1:
		ctrl_reg |= AGAIN_1;
		break;
	case 8:
		ctrl_reg |= AGAIN_8;
		break;
	case 16:
		ctrl_reg |= AGAIN_16;
		break;
	case 120:
		ctrl_reg |= AGAIN_120;
		break;
	default:
		dev_err(&chip->client->dev, "%s: wrong als gain %d\n",
				__func__, gain);
		return -EINVAL;
	}
	rc = taos_i2c_write(chip, TSL277X_CONTROL, ctrl_reg);
	if (!rc) {
		chip->shadow[TSL277X_CONTROL] = ctrl_reg;
		chip->params.als_gain = gain;
		dev_dbg(&chip->client->dev, "%s: new gain %d\n",
				__func__, gain);
	}
	return rc;
}

static int taos_get_lux(struct tsl2772_chip *chip)
{
	unsigned i;
	int ret = 0;
	struct device *dev = &chip->client->dev;
	struct lux_segment *s = chip->segment;
	u32 c0 = chip->als_inf.ch0;
	u32 c1 = chip->als_inf.ch1;
	u32 sat = chip->als_inf.saturation;
	u32 ratio;
	u64 lux_0, lux_1;
	u32 cpl = chip->als_inf.cpl;
	u32 lux, k0 = 0, k1 = 0;

	lux_0 = 0;
	lux_1 = 0;
	if (!chip->als_gain_auto) {
		if (c0 <= MIN_ALS_VALUE) {
			dev_dbg(dev, "%s: darkness\n", __func__);
			lux = 0;
			goto exit;
		} else if (c0 >= sat) {
			dev_dbg(dev, "%s: saturation, keep lux\n", __func__);
			lux = chip->als_inf.lux;
			goto exit;
		}
	} else {
		u8 gain = chip->params.als_gain;
		int rc = -EIO;

		if (gain == 16 && c0 >= sat) {
			rc = set_als_gain(chip, 1);
		} else if (gain == 16 && c0 < GAIN_SWITCH_LEVEL) {
			rc = set_als_gain(chip, 120);
		} else if ((gain == 120 && c0 >= sat) ||
				(gain == 1 && c0 < GAIN_SWITCH_LEVEL)) {
			rc = set_als_gain(chip, 16);
		}
		if (!rc) {
			dev_dbg(dev, "%s: gain adjusted, skip\n", __func__);
			taos_calc_cpl(chip);
			ret = -EAGAIN;
			lux = chip->als_inf.lux;
			goto exit;
		}

		if (c0 <= MIN_ALS_VALUE) {
			dev_dbg(dev, "%s: darkness\n", __func__);
			lux = 0;
			goto exit;
		} else if (c0 >= sat) {
			dev_dbg(dev, "%s: saturation, keep lux\n", __func__);
			lux = chip->als_inf.lux;
			goto exit;
		}
	}

	ratio = (c1 << RATIO_SHIFT) / c0;
	for (i = 0; i < chip->segment_num; i++, s++) {
		if (ratio <= s->ratio) {
			dev_dbg(&chip->client->dev, "%s: ratio %u segment %u "
					"[r %u, k0 %u, k1 %u]\n", __func__,
					ratio, i, s->ratio, s->k0, s->k1);
			k0 = s->k0;
			k1 = s->k1;
			break;
		}
	}
	if (i >= chip->segment_num) {
		dev_dbg(&chip->client->dev, "%s: ratio %u - darkness\n",
				__func__, ratio);
		lux = 0;
		goto exit;
	}

	lux_0 = (u64)(( ( (c0 * 100 ) - (c1 * 187 ) ) * 10 ) / cpl); //( CPL 已經放大 1000 ） 
	lux_1 = (u64)(( ( (c0 *  63 ) - (c1 * 100 ) ) * 10 ) / cpl); 
	lux = max(lux_0, lux_1);
	lux = max(lux , (u32)0);
	/*if( chip->als_inf.ga <= 0 ) 						//20120829	cal
		chip->als_inf.ga = 1 ;
	*/

	//lux = (lux * chip->als_inf.ga) / 4096;						//20120829	cal

/*	lux_0 = k0 * (s64)c0;
	lux_1 = k1 * (s64)c1;
	if (lux_1 >= lux_0) {
		dev_dbg(&chip->client->dev, "%s: negative result - darkness\n",
				__func__);
		lux = 0;
		goto exit;
	}
	lux_0 -= lux_1;
	while (lux_0 & ((u64)0xffffffff << 32)) {				// lux0 & ( 0xFFFF FFFF << 32 )
		dev_dbg(&chip->client->dev, "%s: overwlow lux64 = 0x%16llx",
				__func__, lux_0);
		lux_0 >>= 1;
		cpl >>= 1;
	}

	if (!cpl) {
		dev_dbg(&chip->client->dev, "%s: zero cpl - darkness\n",
				__func__);
		lux = 0;
		goto exit;
	}
*/

/*
	lux = lux_0;								//lux0 = 11283680H 
	lux = lux / cpl;
*/
							//LUX = 17883 or 1120031  
exit:
	dev_dbg(&chip->client->dev, "%s: lux %u (%u x %u - %u x %u) / %u\n",
		__func__, lux, k0, c0, k1, c1, cpl);
	chip->als_inf.lux = lux;
	return ret;
}

static int pltf_power_on(struct tsl2772_chip *chip)
{
	int rc = 0;
	if (chip->pdata->platform_power) {
		rc = chip->pdata->platform_power(&chip->client->dev,
			POWER_ON);
		msleep(10);
	}
	chip->unpowered = rc != 0;
	return rc;
}

static int pltf_power_off(struct tsl2772_chip *chip)
{
	int rc = 0;
	if (chip->pdata->platform_power) {
		rc = chip->pdata->platform_power(&chip->client->dev,
			POWER_OFF);
		chip->unpowered = rc == 0;
	} else {
		chip->unpowered = false;
	}
	return rc;
}

static int taos_irq_clr(struct tsl2772_chip *chip, u8 bits)
{
	int ret = i2c_smbus_write_byte(chip->client, TSL277X_CMD_REG |
			TSL277X_CMD_SPL_FN | bits);
	if (ret < 0)
		dev_err(&chip->client->dev, "%s: failed, bits %x\n",
				__func__, bits);
	return ret;
}

static void taos_get_als(struct tsl2772_chip *chip)
{
	u32 ch0, ch1;
	u8 *buf = &chip->shadow[TSL277X_ALS_CHAN0LO];

	ch0 = le16_to_cpup((const __le16 *)&buf[0]);
	ch1 = le16_to_cpup((const __le16 *)&buf[2]);
	chip->als_inf.ch0 = ch0;
	chip->als_inf.ch1 = ch1;
	dev_dbg(&chip->client->dev, "%s: ch0 %u, ch1 %u\n", __func__, ch0, ch1);
}

static void taos_get_prox(struct tsl2772_chip *chip)
{
	u8 *buf = &chip->shadow[TSL277X_PRX_LO];
	bool d = chip->prx_inf.detected;
	int i;
	int j=2;
	int temp = 0;
	int ret = 0;
	
	for (i = 0 ; i < j; i++)
	{
		ret = taos_i2c_blk_read(chip, TSL277X_PRX_LO,
					&chip->shadow[TSL277X_PRX_LO],2);
		temp += (buf[1] << 8) | buf[0];
	}
	chip->prx_inf.raw = temp / j;

	chip->prx_inf.detected =
			(d && (chip->prx_inf.raw > chip->params.prox_th_min)) ||
			(!d && (chip->prx_inf.raw > chip->params.prox_th_max));
}

static int taos_read_all(struct tsl2772_chip *chip)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	dev_dbg(&client->dev, "%s\n", __func__);
	ret = taos_i2c_blk_read(chip, TSL277X_STATUS,
			&chip->shadow[TSL277X_STATUS],
			TSL277X_PRX_HI - TSL277X_STATUS + 1);
	return (ret < 0) ? ret : 0;
}

static int update_prox_thresh(struct tsl2772_chip *chip, bool on_enable)
{
	s32 ret;
	u8 *buf = &chip->shadow[TSL277X_PRX_MINTHRESHLO];
	u16 from, to;

	if (on_enable) {
		/* zero gate to force irq */
		from = to = 0;
	} else {
		if (chip->prx_inf.detected) {
			from = chip->params.prox_th_min;
			to = 0xffff;
		} else {
			from = 0;
			to = chip->params.prox_th_max;
		}
	}
	dev_dbg(&chip->client->dev, "%s: %u - %u\n", __func__, from, to);
	*buf++ = from & 0xff;
	*buf++ = from >> 8;
	*buf++ = to & 0xff;
	*buf++ = to >> 8;
	ret = taos_i2c_blk_write(chip, TSL277X_PRX_MINTHRESHLO,
			&chip->shadow[TSL277X_PRX_MINTHRESHLO],
			TSL277X_PRX_MAXTHRESHHI - TSL277X_PRX_MINTHRESHLO + 1);
	return (ret < 0) ? ret : 0;
}

static int update_als_thres(struct tsl2772_chip *chip, bool on_enable)
{
	s32 ret;
	u8 *buf = &chip->shadow[TSL277X_ALS_MINTHRESHLO];
	u16 gate = chip->params.als_gate;
	u16 from, to, cur;

	cur = chip->als_inf.ch0;
	if (on_enable) {
		/* zero gate far away form current position to force an irq */
		from = to = cur > 0xffff / 2 ? 0 : 0xffff;
	} else {
		gate = cur * gate / 100;
		if (!gate)
			gate = 1;
		if (cur > gate)
			from = cur - gate;
		else
			from = 0;
		if (cur < (0xffff - gate))
			to = cur + gate;
		else
			to = 0xffff;
	}
	dev_dbg(&chip->client->dev, "%s: [%u - %u]\n", __func__, from, to);
	*buf++ = from & 0xff;
	*buf++ = from >> 8;
	*buf++ = to & 0xff;
	*buf++ = to >> 8;
	ret = taos_i2c_blk_write(chip, TSL277X_ALS_MINTHRESHLO,
			&chip->shadow[TSL277X_ALS_MINTHRESHLO],
			TSL277X_ALS_MAXTHRESHHI - TSL277X_ALS_MINTHRESHLO + 1);
	return (ret < 0) ? ret : 0;
}

static void report_prox(struct tsl2772_chip *chip)
{
	if (chip->p_idev) {
		input_report_abs(chip->p_idev, ABS_DISTANCE,
				chip->prx_inf.detected ? 0 : 5);
		if (chip->blog)
			printk("\n [andy_test] %s ABS_DISTANCE = %d(%d)\n", __FUNCTION__, chip->prx_inf.raw, chip->prx_inf.detected);
		input_sync(chip->p_idev);
	}
}

static void report_als(struct tsl2772_chip *chip)
{
	if (chip->a_idev) {
		int rc = taos_get_lux(chip);
		if (!rc) {
			int lux = chip->als_inf.lux * chip->als_inf.ga / 4096;
			input_report_abs(chip->a_idev, ABS_MISC, lux);
			input_sync(chip->a_idev);
			update_als_thres(chip, 0);
			
			if (chip->blog)
				printk("\n [andy_test] %s ABS_MISC = %d\n", __FUNCTION__, lux);
		} else {
			update_als_thres(chip, 1);
		}
	}
}

static int taos_check_and_report(struct tsl2772_chip *chip)
{
	u8 status = chip->shadow[TSL277X_ENABLE];
	int ret = taos_read_all(chip); 	//READ STATUS~PROX_HI
	if (ret)			//讀值正常 ret = 0 則不進入
		goto exit_clr;

	status = chip->shadow[TSL277X_STATUS];
	dev_dbg(&chip->client->dev, "%s: status 0x%02x\n", __func__, status);

	//if ((status & (TSL277X_ST_PRX_VALID | TSL277X_ST_PRX_IRQ)) ==
	//		(TSL277X_ST_PRX_VALID | TSL277X_ST_PRX_IRQ)) {
	if(chip->prx_enabled){
		
		taos_get_prox(chip);
		report_prox(chip);
		update_prox_thresh(chip, 0);
	}

	//if ((status & (TSL277X_ST_ALS_VALID | TSL277X_ST_ALS_IRQ)) ==
	//		(TSL277X_ST_ALS_VALID | TSL277X_ST_ALS_IRQ)) {
	if(chip->als_enabled){
		taos_get_als(chip);
		taos_get_lux(chip);
		report_als(chip);
	}
exit_clr:
	taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR | TSL277X_CMD_ALS_INT_CLR);
	return ret;
}

static irqreturn_t taos_irq(int irq, void *handle)
{
	struct tsl2772_chip *chip = handle;
	struct device *dev = &chip->client->dev;

	printk("\n [andy_test] %s,pending=%d\n", __FUNCTION__,chip->irq_pending);

	if(chip->irq_pending)
		return IRQ_HANDLED;

	//disable_irq_nosync(chip->client->irq);
	//wake_lock_timeout(&chip->w_lock, 0.5*HZ);

	mutex_lock(&chip->lock);
	if (chip->in_suspend) {
		dev_dbg(dev, "%s: in suspend\n", __func__);
		printk("\n [andy_test] %s in suspend, detect=%d\n", __FUNCTION__,chip->prx_inf.detected);
		wake_lock_timeout(&chip->w_lock, 2*HZ);
		chip->irq_pending = 1;
		//disable_irq_nosync(chip->client->irq);
		goto bypass;
	}else{
		schedule_work(&chip->imwork);//HY,test
	}
	dev_dbg(dev, "%s\n", __func__);
	//(void)taos_check_and_report(chip);
bypass:
	mutex_unlock(&chip->lock);
	//enable_irq(chip->client->irq);
	return IRQ_HANDLED;
}

static void set_pltf_settings(struct tsl2772_chip *chip)
{
	struct taos_raw_settings const *s = chip->pdata->raw_settings;
	u8 *sh = chip->shadow;
	struct device *dev = &chip->client->dev;
	int cci_board_type = board_type_with_hw_id();

	if (chip->blog)
		printk("\n [andy_test] %s (board_type=%d) DVT3_1_HW_ID =%d\n", __FUNCTION__, cci_board_type, DVT3_1_BOARD_HW_ID );

	if (s) {
		dev_dbg(dev, "%s: form pltf data\n", __func__);
		sh[TSL277X_ALS_TIME] = s->als_time;
		sh[TSL277X_PRX_TIME] = s->prx_time;
		sh[TSL277X_WAIT_TIME] = s->wait_time;
		sh[TSL277X_PERSISTENCE] = s->persist;
		sh[TSL277X_CONFIG] = s->cfg_reg;
		sh[TSL277X_PRX_PULSE_COUNT] = s->prox_pulse_cnt;
		sh[TSL277X_CONTROL] = s->ctrl_reg;
		sh[TSL277X_REG_PRX_OFFS] = s->prox_offs;
	} else {
		dev_dbg(dev, "%s: use defaults\n", __func__);
		sh[TSL277X_ALS_TIME] = 238; /* ~50 ms */
		sh[TSL277X_PRX_TIME] = 255;
		sh[TSL277X_WAIT_TIME] = 0;
		sh[TSL277X_PERSISTENCE] = PRX_PERSIST(1) | ALS_PERSIST(3);
		sh[TSL277X_CONFIG] = 0;
		
		if(cci_board_type >= DVT3_1_BOARD_HW_ID ){
			sh[TSL277X_PRX_PULSE_COUNT] = 50;//new
		}else{
			sh[TSL277X_PRX_PULSE_COUNT] = 70;//ori
		}

		sh[TSL277X_CONTROL] = AGAIN_8 | PGAIN_1 |
				PDIOD_CH0 | PDRIVE_120MA;
		/*sh[TSL277X_CONTROL] = AGAIN_8 | PGAIN_4 |
				PDIOD_CH0 | PDRIVE_120MA;*/
		sh[TSL277X_REG_PRX_OFFS] = 0;

		if (chip->blog)
			printk("\n [andy_test] %s (CONTROL=%d,PULSE_COUNT=%d)\n", __FUNCTION__, sh[TSL277X_CONTROL],sh[TSL277X_PRX_PULSE_COUNT] );
	}
	chip->params.als_gate = chip->pdata->parameters.als_gate;
	chip->params.prox_th_max = chip->pdata->parameters.prox_th_max;
	chip->params.prox_th_min = chip->pdata->parameters.prox_th_min;
	chip->params.als_gain = chip->pdata->parameters.als_gain;
	if (chip->pdata->parameters.als_gain) {
		chip->params.als_gain = chip->pdata->parameters.als_gain;
	} else {
		chip->als_gain_auto = true;
		chip->params.als_gain = GAIN_AUTO_INIT_VALUE;
		dev_dbg(&chip->client->dev, "%s: auto als gain.\n", __func__);
	}
	(void)set_als_gain(chip, chip->params.als_gain);
	taos_calc_cpl(chip);
}

static int flush_regs(struct tsl2772_chip *chip)
{
	unsigned i;
	int rc;
	u8 reg;

	dev_dbg(&chip->client->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(restorable_regs); i++) {
		reg = restorable_regs[i];
		rc = taos_i2c_write(chip, reg, chip->shadow[reg]);
		if (rc) {
			dev_err(&chip->client->dev, "%s: err on reg 0x%02x\n",
					__func__, reg);
			break;
		}
	}
	return rc;
}

int get_defult_ga(void)
{
	int ret = golden_ga;
	//int cci_board_type = board_type_with_hw_id();
	
	return ret;
}

#if 0
static int TSL2772_load_kvalue(struct tsl2772_chip *chip)
{
	struct file *fp = NULL;
	char buf[16] = {0};
	int ret = 0;
	//printk("\n [andy_test] %s entering\n", __FUNCTION__);
	
	fp = filp_open("/persist/lux_ga", O_RDONLY, 0);	
	{
		int iga = 3750;	
		if(IS_ERR(fp))
		{
			//return -1;
		}
		else//if (NULL != fp)
		{
			if(fp->f_op->read == NULL)
			{
				//return -1;
			}
			else
			{
				mm_segment_t old_fs=get_fs();
				set_fs(get_ds());
		  		fp->f_op->read(fp, buf, 16, &fp->f_pos);
				iga = simple_strtol(buf, NULL, 10);
				memset(buf, 0, 16);
				set_fs(old_fs);
				filp_close(fp, NULL);
			}
			fp = NULL;
		}
		chip->als_inf.ga = (0 == iga)?3750:iga;
		printk("\n [andy_test] ga = %d \n", chip->als_inf.ga);
		//TSL2771_calibration_wait_ga(iga);
	}
	
	fp = filp_open("/persist/prox_pilth", O_RDONLY, 0);
	{
		int ihigh = 0;
		if(IS_ERR(fp))
		{
	  		//return -1;
		}
		else //if (NULL != fp)	
		{
			if(NULL == fp->f_op->read)
			{
				//return -1;
			}
			else
			{
				mm_segment_t old_fs=get_fs();
				set_fs(get_ds());
		  		fp->f_op->read(fp, buf, 16, &fp->f_pos);
				ihigh = simple_strtol(buf, NULL, 10);
				memset(buf, 0, 16);
				set_fs(old_fs);
				filp_close(fp, NULL);
				fp = NULL;
			}
		}
		if (0 != ihigh)
			chip->params.prox_th_max = ihigh;
	}
	
	fp = filp_open("/persist/prox_piltl", O_RDONLY, 0);
	{
		int ilow = 0;
		if(IS_ERR(fp))
		{
	  		//return -1;
		}
		else //if (NULL != fp)
		{
			if(fp->f_op->read== NULL)
			{
				printk("read file error \n");
				//return -1;
			}
			else
			{
				mm_segment_t old_fs=get_fs();
				set_fs(get_ds());
		  		fp->f_op->read(fp, buf, 16, &fp->f_pos);
				ilow = simple_strtol(buf, NULL, 10);
				memset(buf, 0, 16);
				set_fs(old_fs);
				filp_close(fp, NULL);
				fp = NULL;
			}
		}
		if (0 !=ilow)
			chip->params.prox_th_min = ilow;
	}
	if (chip->blog)
		printk("\n [andy_test] %s entering (%d)(%d)(%d)\n", __FUNCTION__,chip->als_inf.ga,chip->params.prox_th_max,chip->params.prox_th_min);
	return ret;
}
#endif
static int update_enable_reg(struct tsl2772_chip *chip)
{
	int rc;
	dev_dbg(&chip->client->dev, "%s: %02x\n", __func__,
			chip->shadow[TSL277X_ENABLE]);
	cancel_delayed_work_sync(&chip->work);
	
	rc = taos_i2c_write(chip, TSL277X_ENABLE,
			chip->shadow[TSL277X_ENABLE]);
			
	if(!rc && 
		(chip->shadow[TSL277X_ENABLE] & (TSL277X_EN_PRX | TSL277X_EN_ALS)))
	{
		if(!(chip->bkflag))
		{
			extern int Get_Panel_ID(void);
			chip->Panel_ID = Get_Panel_ID();
			chip->bkflag = true;
			
			switch(chip->Panel_ID)
			{
			case 0:
			case 1:
			case 2:
			case 3:
				break;
			default:
					//chip->params.prox_th_max;
					//chip->params.prox_th_min;
				break;
			}
		}
		schedule_delayed_work(&chip->work, msecs_to_jiffies(chip->delay));
	}
	
	return rc;
}

static int taos_prox_enable(struct tsl2772_chip *chip, int on)
{
	int rc;

	if (chip->blog)
		printk("\n [andy_test] %s (%d)\n", __FUNCTION__, on);
	dev_dbg(&chip->client->dev, "%s: on = %d\n", __func__, on);
	
	if(on != chip->prx_enabled)
	{
		if (on) {
			update_prox_thresh(chip, 1);
			chip->shadow[TSL277X_ENABLE] |=
					(TSL277X_EN_PWR_ON | TSL277X_EN_PRX |
					TSL277X_EN_PRX_IRQ);
			enable_irq(chip->client->irq);
			msleep(3);
		} else {
			chip->shadow[TSL277X_ENABLE] &=
					~(TSL277X_EN_PRX_IRQ | TSL277X_EN_PRX);
			if (!(chip->shadow[TSL277X_ENABLE] & TSL277X_EN_ALS))
				chip->shadow[TSL277X_ENABLE] &= ~TSL277X_EN_PWR_ON;
			if( 1 == chip->wake_irq )
			{
				irq_set_irq_wake(chip->client->irq, 0);
				chip->wake_irq = 0;
				printk("\n [andy_test] %s ,irq_set_irq_wake\n", __FUNCTION__);
			}
			//disable_irq_nosync(chip->client->irq);
			disable_irq(chip->client->irq);
		}
	}
	taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR);
	rc = update_enable_reg(chip);
	if (rc)
		return rc;
	if (!rc)
		chip->prx_enabled = on;
	return rc;
}

static int taos_als_enable(struct tsl2772_chip *chip, int on)
{
	int rc;

	if (chip->blog)
		printk("\n [andy_test] %s (%d)\n", __FUNCTION__, on);
	if(on != chip->als_enabled)
	{
		if (on) {
			update_als_thres(chip, 1);
			chip->shadow[TSL277X_ENABLE] |=
					(TSL277X_EN_PWR_ON | TSL277X_EN_ALS);
			msleep(3);
		} else {
			chip->shadow[TSL277X_ENABLE] &=
					~(TSL277X_EN_ALS_IRQ | TSL277X_EN_ALS);
			if (!(chip->shadow[TSL277X_ENABLE] & TSL277X_EN_PRX))
				chip->shadow[TSL277X_ENABLE] &= ~TSL277X_EN_PWR_ON;
		}
	}
	taos_irq_clr(chip, TSL277X_CMD_ALS_INT_CLR);
	rc = update_enable_reg(chip);
	if (rc)
			return rc;
	if (!rc)
		chip->als_enabled = on;
	return rc;
}

static int taos_device_prx_cal_avg_10(struct tsl2772_chip *chip) //20120829
{
// add retry mechanism
#if 0
	int ret = 0;
	int p_avg = 0 ;
	int	i = 10;
	u8 *buf = &chip->shadow[TSL277X_PRX_LO];
	
	for( ; i>0 ; i--)			// PROX  當無物體接近時 得繞射值 10次平均 
	{
		ret = taos_i2c_blk_read(chip, TSL277X_PRX_LO,
					&chip->shadow[TSL277X_PRX_LO],2);  

		chip->prx_inf.raw = (buf[1] << 8) | buf[0];
		if(ret < 0)
			return ret;
			
		msleep(3);	 		   // DELAY 3ms 
		p_avg += chip->prx_inf.raw;											   //PDATA 累加 
	} 
	p_avg /= 10;
	
	return 	p_avg;														   // 	
#else
	int ret = 0;
	int p_avg = 0 ;
	u8 *buf = &chip->shadow[TSL277X_PRX_LO];
	int nRetry = 100;  // max times we try to calibrate
	int iRetry = 0;  // total times we try to calibrate
	int nCalibrate = 10;  // total calibration times
	int iCalibrate = 0;  // total successful calibration times

	printk("\n [andy_test] %s starts\n", __FUNCTION__);
	
	for ( ; iRetry <  nRetry && iCalibrate < nCalibrate; iRetry++)			// PROX  當無物體接近時 得繞射值 10次平均 
	{
		ret = taos_i2c_blk_read(chip, TSL277X_PRX_LO,
					&chip->shadow[TSL277X_PRX_LO],2);  

		if(ret < 0) {
			printk("\n [andy_test] %s taos_i2c_blk_read() failed. ret=%d\n", __FUNCTION__, ret);
			msleep(3);
			continue;  // fail to cailbrate!
			//return ret;
		}
		
		chip->prx_inf.raw = (buf[1] << 8) | buf[0];
		if (chip->prx_inf.raw <= 0)
			printk("\n [andy_test] %s chip->prx_inf.raw=%d\n", __FUNCTION__, chip->prx_inf.raw);

			
		msleep(3);	 		   // DELAY 3ms 
		p_avg += chip->prx_inf.raw;											   //PDATA 累加 

		iCalibrate++;  // successfully calibrate!
	} 
	
	if (iRetry == nRetry) {
		p_avg = 900;  // error!! Set the value of golden sample to it
		printk("\n [andy_test] %s retry %d times!\n", __FUNCTION__, iRetry);
	}
	else
		p_avg /= nCalibrate;  // normal case

	printk("\n [andy_test] %s p_avg=%d\n", __FUNCTION__, p_avg);
	return 	p_avg;	
#endif
}

static ssize_t taos_device_prx_offset_cal(struct device *dev,			
				   struct device_attribute *attr, char *buf)	//20120829
{
	int p_avg = 0 ;	
	int ret = 0; 
	int prox_offset = 0;							  							//prox_offset = 寫入 OFFSET 的 DATA
	int prox_cross_talk = 400;						  							// 設定 欲定的繞射值(maybe change)
	int cci_board_type = board_type_with_hw_id();

		if(cci_board_type >= DVT3_1_BOARD_HW_ID )
		prox_cross_talk = 500;
		else if(cci_board_type == DVT3_BOARD_HW_ID)
		prox_cross_talk =  300;
	else
		prox_cross_talk = 400;
	
	mutex_lock(&chip->lock);	
	p_avg = taos_device_prx_cal_avg_10(chip);	//呼叫上方的 " taos_device_prx_cal_avg_10 " 副程式
	if(p_avg < 0 )					//如果 i2c fail 則跳開 
		goto exit_prox_offset_cal;

	if(p_avg > prox_cross_talk)
	{
		while(p_avg > prox_cross_talk)			//如果P-DATA >  prox_cross_talk
		{	 
			ret =0;
			prox_offset += 1;					// 這裡 OFFSET +1 可將 	P-DATA 往下降
			ret = taos_i2c_write(chip, TSL277X_REG_PRX_OFFS, prox_offset);
			msleep(3);	 		   // DELAY 3ms 
			p_avg = taos_device_prx_cal_avg_10(chip);	//呼叫最上方的prox_avg副程式
			if(p_avg < 0 )					//如果 i2c fail 則跳開 
				goto exit_prox_offset_cal;

			if(prox_offset == 127)	   //0x7FH // 如果減到極限就跳出     ( OFFSET 的第7個 BIT 是選擇 OFFSET 是加還是減
			{
				goto boundary;
			}
		}
	}
	else
	{
		prox_offset = 128; // = 0X80H		//以下為 OFFSET 以加的方式做,同上方減的FOR 迴圈 
		while(chip->prx_inf.raw < prox_cross_talk)
		{	 
			ret = 0;
			prox_offset += 1;
			ret = taos_i2c_write(chip, TSL277X_REG_PRX_OFFS, prox_offset);
			msleep(3);	 		 					// DELAY 3ms 
			p_avg = taos_device_prx_cal_avg_10(chip);	//呼叫最上方的prox_avg副程式
			if(p_avg < 0 )					//如果 i2c fail 則跳開 
				goto exit_prox_offset_cal;
		
			if(prox_offset == 255)
			{
				goto boundary;
			}
		}
	}

boundary:
	ret = taos_i2c_write(chip, TSL277X_REG_PRX_OFFS, prox_offset);			//TSL2771_calibration_prox_wait_offset(prox_offset);
	
exit_prox_offset_cal:
	mutex_unlock(&chip->lock);
	if(ret < 0)
		return 	ret;
	
	return snprintf(buf, PAGE_SIZE, "%d\n", prox_offset);
}

static ssize_t taos_device_prx_pilth_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	unsigned long value;

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	chip->params.prox_th_max = value;

	if (chip->params.prox_th_max > 1022)
		chip->params.prox_th_max = 1022;  // set max to 1022

	return size;
}

static ssize_t taos_device_prx_pilth_cal(struct device *dev,	
	struct device_attribute *attr, char *buf)		//20120829
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	int p_avg = 0 ;
	int cci_board_type = board_type_with_hw_id();
	
    mutex_lock(&chip->lock);

	p_avg = taos_device_prx_cal_avg_10(chip);	//呼叫最上方的prox_avg副程式
	if(p_avg < 0 )					//如果 i2c fail 則跳開 
		goto exit_prx_pilth_cal;
														   // 得到 10 次平均 
	//chip->shadow[TSL277X_PRX_MAXTHRESHLO] = p_avg + 300;//400 ;	// 將 繞射值固定加 400 & 200 設為 上下限值 
	switch(chip->Panel_ID)
	{
	case 0:
	case 1:
	case 2:
	case 3:
// all kinds of panel ID shall utilize the value of ADC
//		break;
	default:
		if(cci_board_type >= DVT3_1_BOARD_HW_ID )
			chip->params.prox_th_max = p_avg + 500;  // new threshold high
		else if(cci_board_type == DVT3_BOARD_HW_ID)
			chip->params.prox_th_max = p_avg + 300;  // new threshold high
		else
			chip->params.prox_th_max = p_avg + 400;  // new threshold high
		break;
	}								
	//ret = taos_i2c_blk_write(chip, TSL277X_PRX_MAXTHRESHLO, &chip->shadow[TSL277X_PRX_MAXTHRESHLO], 2);
	
exit_prx_pilth_cal:

	// reserve 1023 for 0-cm condition
	if (chip->params.prox_th_max > 1022)
		chip->params.prox_th_max = 1022;  // set max to 1022

	mutex_unlock(&chip->lock);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->params.prox_th_max/*chip->shadow[TSL277X_PRX_MAXTHRESHLO]*/);
}

static ssize_t taos_device_prx_piltl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	unsigned long value;

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	chip->params.prox_th_min = value;

	if (chip->params.prox_th_min > 1022)
		chip->params.prox_th_min = 1022;  // set max to 1022
			
	return size;
}

static ssize_t taos_device_prx_piltl_cal(struct device *dev,
	struct device_attribute *attr, char *buf)		//20120829
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	int p_avg = 0 ;
	
	mutex_lock(&chip->lock);
		
	p_avg = taos_device_prx_cal_avg_10(chip);	//呼叫最上方的prox_avg副程式
	if(p_avg < 0 )					//如果 i2c fail 則跳開 
		goto exit_prx_piltl_cal;
															   // 得到 10 次平均 
	//chip->shadow[TSL277X_PRX_MINTHRESHLO] = p_avg + 80;//200 ;	// 將 繞射值固定加 400 & 200 設為 上下限值 
	switch(chip->Panel_ID)
	{
	case 0:
	case 1:
	case 2:
	case 3:
// all kinds of panel ID shall utilize the value of ADC
//		break;
	default:
		chip->params.prox_th_min = p_avg + 90;
		break;
	}
	//ret = taos_i2c_blk_write(chip, TSL277X_PRX_MINTHRESHLO, &chip->shadow[TSL277X_PRX_MINTHRESHLO], 2);
	
	if (chip->params.prox_th_min > 1022)
		chip->params.prox_th_min = 1022;  // set max to 1022
			
exit_prx_piltl_cal:
	mutex_unlock(&chip->lock);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->params.prox_th_min/*chip->shadow[TSL277X_PRX_MINTHRESHLO]*/);
}

static ssize_t taos_device_prx_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	unsigned long value;
	int cci_board_type = board_type_with_hw_id();

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	chip->params.prox_th_min = value + 90;

	if (chip->params.prox_th_min > 1022)
		chip->params.prox_th_min = 1022;  // set max to 1022

	if(cci_board_type >= DVT3_1_BOARD_HW_ID )
		chip->params.prox_th_max = value + 500;  // new threshold high
	else if(cci_board_type == DVT3_BOARD_HW_ID)
		chip->params.prox_th_max = value + 300;  // new threshold high
	else
		chip->params.prox_th_max = value + 400;  // new threshold high

		if (chip->params.prox_th_max > 1022)
			chip->params.prox_th_max = 1022;  // set max to 1022
	
	return size;
}


static ssize_t taos_device_prx_reset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
		
	if(!chip ){
		printk("\n [andy_test] %s null point 1\n", __FUNCTION__);
		return -ENXIO;
	}

	if(!chip->pdata){
		printk("\n [andy_test] %s null point 2\n", __FUNCTION__);
		return -ENXIO;
	}

	printk("\n [andy_test] %s max: %d,min: %d \n golden[%d,%d]\n", __FUNCTION__,chip->params.prox_th_max,
			chip->params.prox_th_min,chip->pdata->parameters.prox_th_max,chip->pdata->parameters.prox_th_min);

	return snprintf(buf, PAGE_SIZE, "High:%4d ,Low:%4d \ngolden[%4d,%4d]\n", chip->params.prox_th_max,
			chip->params.prox_th_min,chip->pdata->parameters.prox_th_max,chip->pdata->parameters.prox_th_min);
}

static ssize_t taos_device_prx_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	unsigned long value;
	//int cci_board_type = board_type_with_hw_id();
	
	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	if(!chip ){
		printk("\n [andy_test] %s null point 1\n", __FUNCTION__);
		return -ENXIO;
	}

	if(!chip->pdata){
		printk("\n [andy_test] %s null point 2\n", __FUNCTION__);
		return -ENXIO;
	}

	if(value == 1){
		printk("\n [andy_test] %s max: %d,min: %d B\n", __FUNCTION__,chip->params.prox_th_max,chip->params.prox_th_min);
		mutex_lock(&chip->lock);
		chip->params.prox_th_max = chip->pdata->parameters.prox_th_max;
		chip->params.prox_th_min = chip->pdata->parameters.prox_th_min;
		mutex_unlock(&chip->lock);
		printk("\n [andy_test] %s max: %d,min: %d E\n", __FUNCTION__,chip->params.prox_th_max,chip->params.prox_th_min);
	}else{

		printk("\n [andy_test] %s NO RESET max: %d,min: %d\n", __FUNCTION__,chip->params.prox_th_max,chip->params.prox_th_min);
	}


		
	return size;
}

static ssize_t taos_device_prx_cal(struct device *dev,
	struct device_attribute *attr, char *buf)		//20120829
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	int p_avg = 0 ;
	int cci_board_type = board_type_with_hw_id();
	
	mutex_lock(&chip->lock);
		
	p_avg = taos_device_prx_cal_avg_10(chip);	//呼叫最上方的prox_avg副程式
	if(p_avg < 0 )					//如果 i2c fail 則跳開 
		goto exit_prx_piltl_cal;
															   // 得到 10 次平均 
	//chip->shadow[TSL277X_PRX_MINTHRESHLO] = p_avg + 80;//200 ;	// 將 繞射值固定加 400 & 200 設為 上下限值 
	switch(chip->Panel_ID)
	{
	case 0:
	case 1:
	case 2:
	case 3:
// all kinds of panel ID shall utilize the value of ADC
//		break;
	default:
		chip->params.prox_th_min = p_avg + 90;
		
		if (chip->params.prox_th_min > 1022)
			chip->params.prox_th_min = 1022;  // set max to 1022
		
		if(cci_board_type >= DVT3_1_BOARD_HW_ID )
			chip->params.prox_th_max = p_avg + 500;  // new threshold high
		else if(cci_board_type == DVT3_BOARD_HW_ID)
			chip->params.prox_th_max = p_avg + 300;  // new threshold high
		else
			chip->params.prox_th_max = p_avg + 400;  // new threshold high
		
		if (chip->params.prox_th_max > 1022)
			chip->params.prox_th_max = 1022;  // set max to 1022
		break;
	}
	//ret = taos_i2c_blk_write(chip, TSL277X_PRX_MINTHRESHLO, &chip->shadow[TSL277X_PRX_MINTHRESHLO], 2);
	
exit_prx_piltl_cal:
	mutex_unlock(&chip->lock);
	return snprintf(buf, PAGE_SIZE, "ADC:%4d TH_HIGH:%4d TH_LOW:%4d\n",p_avg,chip->params.prox_th_max, chip->params.prox_th_min);
}

static ssize_t taos_device_als_cal_ga_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	unsigned long value;

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	if(0 == value)
		chip->als_inf.ga = get_defult_ga();
	else
	chip->als_inf.ga = value;

	return size;
}

static ssize_t taos_device_als_cal_ga(struct device *dev,	
				   struct device_attribute *attr, char *buf)	//20120829
{
	int rc=0;	
	int ret = 0;
 	int calibration_lux_default = 500;	   // 校正用標準光源 = 500 LUX
	u32 ch0, ch1;
	u8 *sbuf = &chip->shadow[TSL277X_ALS_CHAN0LO];
	int lux = 0;

	mutex_lock(&chip->lock);													     

	ret = taos_i2c_blk_read(chip, TSL277X_ALS_CHAN0LO,
				&chip->shadow[TSL277X_ALS_CHAN0LO],4);  //	讀出  CLEAR & IR DATA	
				    
	ch0 = le16_to_cpup((const __le16 *)&sbuf[0]); 
	ch1 = le16_to_cpup((const __le16 *)&sbuf[2]);
	chip->als_inf.ch0 = ch0;
	chip->als_inf.ch1 = ch1;

	{
		int i;
		int value = 0;
		
		for (i= 0; i<5; i++)
		{
			rc = taos_get_lux(chip);		//若 taos_get_lux 正常,rc = 0
			value += chip->als_inf.lux;	
			msleep(3);
		}
	
		if (!rc) {
			lux = value / 5;//chip->als_inf.lux;
			update_als_thres(chip, 0);	//修改對應的 TH_HI & TH_LOW
		} else {
			update_als_thres(chip, 1);
			//chip->als_inf.ga = 3750;
			printk("\n [andy_test] %s read fail \n", __func__);
			goto exit_als_cal_ga;	
		}
	}

	/* Store used gain for calculations */		
	if(ret < 0)
	{
		goto exit_als_cal_ga;
	}
	else
	{
		chip->als_inf.ga = ( calibration_lux_default * 4096) / lux;	// 計算 LUX 須乘 GA = ? 才能得到 500 LUX
	}
											
exit_als_cal_ga:
	mutex_unlock(&chip->lock);
	if(ret < 0)
		return 	ret;
	else
	printk("\n [andy_test] ga = %d \n", chip->als_inf.ga);
	return snprintf(buf, PAGE_SIZE,"%d\n", chip->als_inf.ga);
}

static ssize_t taos_device_als_ch0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.ch0);
}

static ssize_t taos_device_als_ch1(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.ch1);
}

static ssize_t taos_device_als_cpl(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.cpl);
}

static ssize_t taos_device_als_lux(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.lux);
}

static ssize_t taos_lux_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	struct lux_segment *s = chip->segment;
	int i, k;

	for (i = k = 0; i < chip->segment_num; i++)
		k += snprintf(buf + k, PAGE_SIZE - k, "%d:%u,%u,%u\n", i,
				(s[i].ratio * 1000) >> RATIO_SHIFT,
				(s[i].k0 * 1000) >> SCALE_SHIFT,
				(s[i].k1 * 1000) >> SCALE_SHIFT);
	return k;
}

static ssize_t taos_lux_table_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/*struct tsl2772_chip *chip = dev_get_drvdata(dev);
	int i;
	u32 ratio, k0, k1;

	if (4 != sscanf(buf, "%10d:%10u,%10u,%10u", &i, &ratio, &k0, &k1))
		return -EINVAL;
	if (i >= chip->segment_num)
		return -EINVAL;
	mutex_lock(&chip->lock);
	chip->segment[i].ratio = (ratio << RATIO_SHIFT) / 1000;
	chip->segment[i].k0 = (k0 << SCALE_SHIFT) / 1000;
	chip->segment[i].k1 = (k1 << SCALE_SHIFT) / 1000;
	mutex_unlock(&chip->lock);*/
	return size;
}

static ssize_t TSL2772_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4,"%d\n", chip->delay);
}

static ssize_t TSL2772_delay_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	unsigned long value;

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;
		
	chip->delay = value/1000000;
	return 0;
}

static ssize_t TSL2772_log_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4,"%d\n", chip->blog);
}

static ssize_t TSL2772_log_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;
	
	if(0 == value)	
		chip->blog = false;
	else
		chip->blog = true;
	return len;
}
						   
static ssize_t tsl2772_als_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enabled);
}

static ssize_t tsl2772_als_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value){
		taos_als_enable(chip,1);
		taos_check_and_report(chip);//report immediately

		if (chip->blog)
		printk("\n [andy_test] %s end\n", __FUNCTION__);
	}else
		taos_als_enable(chip,0);

	return size;
}

static ssize_t tsl2772_prox_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);

		if (chip->blog)
			printk("\n [andy_test] %s ,prx_enabled=%d\n", __FUNCTION__,(int)chip->prx_enabled);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prx_enabled);
}

static ssize_t tsl2772_prox_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value){
		taos_prox_enable(chip,1);
		taos_check_and_report(chip);//report immediately

		if (chip->blog)
		printk("\n [andy_test] %s end\n", __FUNCTION__);
	}else
		taos_prox_enable(chip,0);

	return size;
}

static ssize_t taos_als_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d (%s)\n", chip->params.als_gain,
			chip->als_gain_auto ? "auto" : "manual");
}

static ssize_t taos_als_gain_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long gain;
	int rc;
	struct tsl2772_chip *chip = dev_get_drvdata(dev);

	rc = strict_strtoul(buf, 10, &gain);
	if (rc)
		return -EINVAL;
	if (gain != 0 && gain != 1 && gain != 8 && gain != 16 && gain != 120)
		return -EINVAL;
	mutex_lock(&chip->lock);
	if (gain) {
		chip->als_gain_auto = false;
		rc = set_als_gain(chip, gain);
		if (!rc)
			taos_calc_cpl(chip);
	} else {
		chip->als_gain_auto = true;
	}
	mutex_unlock(&chip->lock);
	return rc ? rc : size;
}

static ssize_t taos_als_gate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d (in %%)\n", chip->params.als_gate);
}

static ssize_t taos_als_gate_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long gate;
	int rc;
	struct tsl2772_chip *chip = dev_get_drvdata(dev);

	rc = strict_strtoul(buf, 10, &gate);
	if (rc || gate > 100)
		return -EINVAL;
	mutex_lock(&chip->lock);
	chip->params.als_gate = gate;
	mutex_unlock(&chip->lock);
	return size;
}

static ssize_t taos_device_prx_raw(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prx_inf.raw);
}

static ssize_t taos_device_prx_detected(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prx_inf.detected);
}

static ssize_t TSL2772_K_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	int golden_value_als_ga = get_defult_ga();
	
	
	return snprintf(buf, PAGE_SIZE, "\nga = %4d, high = %4d, low = %4d \n[golden: ga = %4d, high = %4d, low = %4d]\n", chip->als_inf.ga,
		chip->params.prox_th_max,chip->params.prox_th_min,golden_value_als_ga,chip->pdata->parameters.prox_th_max,chip->pdata->parameters.prox_th_min);
}

static struct device_attribute prox_attrs[] = {
	__ATTR(prx_raw, 0444, taos_device_prx_raw, NULL),
	__ATTR(prx_detect, 0444, taos_device_prx_detected, NULL),
	__ATTR(prox_offset, 0444, taos_device_prx_offset_cal, NULL), 	//加入下方3行//20120829
	__ATTR(prox_pilth, 0644, taos_device_prx_pilth_cal, taos_device_prx_pilth_store),	//20120829
	__ATTR(prox_piltl, 0644, taos_device_prx_piltl_cal, taos_device_prx_piltl_store),	//20120829
	__ATTR(prox_cal, 0644, taos_device_prx_cal, taos_device_prx_store),	//20121225
	__ATTR(prox_reset, 0644, taos_device_prx_reset, taos_device_prx_reset_store),	//20120223
};

static struct device_attribute als_attrs[] = {
	__ATTR(als_ch0, 0444, taos_device_als_ch0, NULL),
	__ATTR(als_ch1, 0444, taos_device_als_ch1, NULL),
	__ATTR(als_cpl, 0444, taos_device_als_cpl, NULL),
	__ATTR(lux_ga, 0644, taos_device_als_cal_ga, taos_device_als_cal_ga_store),		//20120829
	__ATTR(als_lux, 0444, taos_device_als_lux, NULL),
	__ATTR(als_gain, 0644, taos_als_gain_show, taos_als_gain_store),
	__ATTR(als_gate, 0644, taos_als_gate_show, taos_als_gate_store),
	__ATTR(lux_table, 0644, taos_lux_table_show, taos_lux_table_store),
	__ATTR(als_power_state, 0644, tsl2772_als_enable_show, tsl2772_als_enable),
	__ATTR(prox_power_state, 0644, tsl2772_prox_enable_show, tsl2772_prox_enable),
	__ATTR(delay, 0644, TSL2772_delay_show, TSL2772_delay_store),
	__ATTR(trace_on, 0644, TSL2772_log_show, TSL2772_log_store),
	__ATTR(cal_value, 0444, TSL2772_K_show, NULL),
};

static int add_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;
	for (i = 0; i < size; i++)
		if (device_create_file(dev, a + i))
			goto undo;
	return 0;
undo:
	for (; i >= 0 ; i--)
		device_remove_file(dev, a + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void remove_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;
	for (i = 0; i < size; i++)
		device_remove_file(dev, a + i);
}

static int taos_get_id(struct tsl2772_chip *chip, u8 *id, u8 *rev)
{
	int rc = taos_i2c_read(chip, TSL277X_REVID, rev);
	if (rc)
		return rc;
	return taos_i2c_read(chip, TSL277X_CHIPID, id);
}

static int power_on(struct tsl2772_chip *chip)
{
	int rc;
	rc = pltf_power_on(chip);
	if (rc)
		return rc;
	dev_dbg(&chip->client->dev, "%s: chip was off, restoring regs\n",
			__func__);
	return flush_regs(chip);
}

static int prox_idev_open(struct input_dev *idev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(&idev->dev);
	int rc = 0;
	//bool als = chip->a_idev && chip->a_idev->users;
	dev_dbg(&idev->dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	if (chip->unpowered) {
		rc = power_on(chip);
		if (rc)
			goto chip_on_err;
	}
	//rc = taos_prox_enable(chip, 1);
	//if (rc && !als)
	//	pltf_power_off(chip);
chip_on_err:
	mutex_unlock(&chip->lock);
	return rc;
}

static void prox_idev_close(struct input_dev *idev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(&idev->dev);

	dev_dbg(&idev->dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	taos_prox_enable(chip, 0);
	if (!chip->a_idev || !chip->a_idev->users)
		pltf_power_off(chip);
	mutex_unlock(&chip->lock);
}

static int als_idev_open(struct input_dev *idev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(&idev->dev);
	int rc = 0;
	//bool prox = chip->p_idev && chip->p_idev->users;

	dev_dbg(&idev->dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	if (chip->unpowered) {
		rc = power_on(chip);
		if (rc)
			goto chip_on_err;
	}
	//rc = taos_als_enable(chip, 1);
	//if (rc && !prox)
	//	pltf_power_off(chip);
chip_on_err:
	mutex_unlock(&chip->lock);
	return rc;
}

static void als_idev_close(struct input_dev *idev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(&idev->dev);
	dev_dbg(&idev->dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	taos_als_enable(chip, 0);
	if (!chip->p_idev || !chip->p_idev->users)
		pltf_power_off(chip);
	mutex_unlock(&chip->lock);
}

static void tsl_work_func(struct work_struct *work)
{
	if (chip->blog)
		printk("\n [andy_test] %s(delay work)\n", __FUNCTION__);
	
	taos_check_and_report(chip);
	schedule_delayed_work(&chip->work, msecs_to_jiffies(chip->delay));
}

static void tsl_work_func_nodelay(struct work_struct *work)
{
	if (chip->blog)
		printk("\n [andy_test] %s(schedule imwork)\n", __FUNCTION__);
	taos_check_and_report(chip);
}


static int __devinit taos_probe(struct i2c_client *client,
	const struct i2c_device_id *idp)
{
	int i, ret;
	u8 id, rev;
	struct device *dev = &client->dev;
//	static struct tsl2772_chip *chip;
	struct tsl2772_i2c_platform_data *pdata = dev->platform_data;
	bool powered = 0;

	dev_info(dev, "%s: client->irq = %d\n", __func__, client->irq);
	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c smbus byte data unsupported\n", __func__);
		ret = -EOPNOTSUPP;
		goto init_failed;
	}
	if (!pdata) {
		dev_err(dev, "%s: platform data required\n", __func__);
		ret = -EINVAL;
		goto init_failed;
	}
	if (!(pdata->prox_name || pdata->als_name) || client->irq < 0) {
		dev_err(dev, "%s: no reason to run.\n", __func__);
		ret = -EINVAL;
		goto init_failed;
	}
	if (pdata->platform_init) {
		ret = pdata->platform_init(dev);
		if (ret)
			goto init_failed;
	}
	if (pdata->platform_power) {
		ret = pdata->platform_power(dev, POWER_ON);
		if (ret) {
			dev_err(dev, "%s: pltf power on failed\n", __func__);
			goto pon_failed;
		}
		powered = true;
		msleep(10);
	}
	chip = kzalloc(sizeof(struct tsl2772_chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto malloc_failed;
	}
	chip->blog = false;
	chip->client = client;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);

	chip->seg_num_max = chip->pdata->segment_num ?
			chip->pdata->segment_num : ARRAY_SIZE(segment_default);
	if (chip->pdata->segment)
		ret = set_segment_table(chip, chip->pdata->segment,
			chip->pdata->segment_num);
	else
		ret =  set_segment_table(chip, segment_default,
			ARRAY_SIZE(segment_default));
	if (ret)
		goto set_segment_failed;

	ret = taos_get_id(chip, &id, &rev);
	if (ret)
		goto id_failed;
	for (i = 0; i < ARRAY_SIZE(tsl277x_ids); i++) {
		if (id == tsl277x_ids[i])
			break;
	}
	if (i < ARRAY_SIZE(tsl277x_names)) {
		dev_info(dev, "%s: '%s rev. %d' detected\n", __func__,
			tsl277x_names[i], rev);
	} else {
		dev_err(dev, "%s: not supported chip id\n", __func__);
		ret = -EOPNOTSUPP;
		goto id_failed;
	}
	mutex_init(&chip->lock);
	set_pltf_settings(chip);
	ret = flush_regs(chip);
	if (ret)
		goto flush_regs_failed;
	if (pdata->platform_power) {
		pdata->platform_power(dev, POWER_OFF);
		powered = false;
		chip->unpowered = true;
	}

	if (!pdata->prox_name)
		goto bypass_prox_idev;
	chip->p_idev = input_allocate_device();
	if (!chip->p_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
				__func__, pdata->prox_name);
		ret = -ENODEV;
		goto input_p_alloc_failed;
	}
	chip->p_idev->name = pdata->prox_name;
	chip->p_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->p_idev->evbit);
	set_bit(ABS_DISTANCE, chip->p_idev->absbit);
	input_set_abs_params(chip->p_idev, ABS_DISTANCE, 0, 5, 0, 0);
	chip->p_idev->open = prox_idev_open;
	chip->p_idev->close = prox_idev_close;
	dev_set_drvdata(&chip->p_idev->dev, chip);
	ret = input_register_device(chip->p_idev);
	if (ret) {
		input_free_device(chip->p_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
				__func__, pdata->prox_name);
		goto input_p_alloc_failed;
	}
	ret = add_sysfs_interfaces(&chip->p_idev->dev,
			prox_attrs, ARRAY_SIZE(prox_attrs));
	if (ret)
		goto input_p_sysfs_failed;
bypass_prox_idev:
	if (!pdata->als_name)
		goto bypass_als_idev;
	chip->a_idev = input_allocate_device();
	if (!chip->a_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
				__func__, pdata->als_name);
		ret = -ENODEV;
		goto input_a_alloc_failed;
	}
	chip->a_idev->name = pdata->als_name;
	chip->a_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->a_idev->evbit);
	set_bit(ABS_MISC, chip->a_idev->absbit);
	input_set_abs_params(chip->a_idev, ABS_MISC, 0, 65535, 0, 0);
	chip->a_idev->open = als_idev_open;
	chip->a_idev->close = als_idev_close;
	dev_set_drvdata(&chip->a_idev->dev, chip);
	ret = input_register_device(chip->a_idev);
	if (ret) {
		input_free_device(chip->a_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
				__func__, pdata->prox_name);
		goto input_a_alloc_failed;
	}
	ret = add_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
	if (ret)
		goto input_a_sysfs_failed;

	wake_lock_init(&chip->w_lock, WAKE_LOCK_SUSPEND, "Psensor_weak_lock");

bypass_als_idev:
	ret = request_threaded_irq(client->irq, NULL, taos_irq,
		      IRQF_TRIGGER_FALLING,
		      dev_name(dev), chip);
	if (ret) {
		dev_info(dev, "Failed to request irq %d\n", client->irq);
		goto irq_register_fail;
	}
	
	chip->als_inf.ga = get_defult_ga();
	chip->delay = 200;
	INIT_DELAYED_WORK(&chip->work, tsl_work_func);
	INIT_WORK(&chip->imwork, tsl_work_func_nodelay);
	chip->bkflag = false;
	chip->Panel_ID = -1;

	dev_info(dev, "Probe ok.\n");
	return 0;

irq_register_fail:
	if (chip->a_idev) {
		remove_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
input_a_sysfs_failed:
		input_unregister_device(chip->a_idev);
	}
input_a_alloc_failed:
	if (chip->p_idev) {
		remove_sysfs_interfaces(&chip->p_idev->dev,
			prox_attrs, ARRAY_SIZE(prox_attrs));
input_p_sysfs_failed:
		input_unregister_device(chip->p_idev);
	}
input_p_alloc_failed:
flush_regs_failed:
id_failed:
	kfree(chip->segment);
set_segment_failed:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
malloc_failed:
	if (powered && pdata->platform_power)
		pdata->platform_power(dev, POWER_OFF);
pon_failed:
	if (pdata->platform_teardown)
		pdata->platform_teardown(dev);
init_failed:
	dev_err(dev, "Probe failed.\n");
	return ret;
}

static int taos_suspend(struct device *dev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	struct tsl2772_i2c_platform_data *pdata = dev->platform_data;

	if (chip->blog)
		printk("\n [andy_test] %s \n", __FUNCTION__);
	dev_dbg(dev, "%s\n", __func__);
	
	cancel_delayed_work_sync(&chip->work);
	chip->bpre_als_enabled = chip->als_enabled;
	chip->bpre_prx_enabled = chip->prx_enabled;
	mutex_lock(&chip->lock);
	chip->in_suspend = 1;
	if (chip->p_idev && chip->p_idev->users) {
		if (pdata->proximity_can_wake) {
			dev_dbg(dev, "set wake on proximity\n");
	
			if(chip->prx_enabled)
				chip->wake_irq = 1;
		} else {
			dev_dbg(dev, "proximity off\n");
			taos_prox_enable(chip, 0);
		}
	}
	if (chip->a_idev && chip->a_idev->users) {
		if (pdata->als_can_wake) {
			dev_dbg(dev, "set wake on als\n");
			chip->wake_irq = 1;
		} else {
			dev_dbg(dev, "als off\n");
			taos_als_enable(chip, 0);
		}
	}
	if (chip->wake_irq) {
		//enable_irq(chip->client->irq);
		irq_set_irq_wake(chip->client->irq, 1);
		chip->prx_inf.detected = 1;
		update_prox_thresh(chip, false);
		taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR);
	} else if (!chip->unpowered) {
		dev_dbg(dev, "powering off\n");
		pltf_power_off(chip);
	}
	mutex_unlock(&chip->lock);

	return 0;
}

static int taos_resume(struct device *dev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	bool als_on, prx_on;
	int rc = 0;

	if (chip->blog)
		printk("\n [andy_test] %s \n", __FUNCTION__);
	chip->als_enabled = chip->bpre_als_enabled ;
	chip->prx_enabled = chip->bpre_prx_enabled;
		
	mutex_lock(&chip->lock);
	prx_on = chip->p_idev && chip->p_idev->users;
	als_on = chip->a_idev && chip->a_idev->users;
	chip->in_suspend = 0;
	dev_dbg(dev, "%s: powerd %d, als: needed %d  enabled %d,"
			" prox: needed %d  enabled %d\n", __func__,
			!chip->unpowered, als_on, chip->als_enabled,
			prx_on, chip->prx_enabled);
	if (chip->wake_irq) {
		int rt = irq_set_irq_wake(chip->client->irq, 0);
		chip->wake_irq = 0;
		//(void)taos_check_and_report(chip);
		rt = taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR);
		//disable_irq_nosync(chip->client->irq);
		//wake_lock_timeout(&chip->w_lock, 2*HZ);
	}
	if (chip->unpowered && (prx_on || als_on)) {
		dev_dbg(dev, "powering on\n");
		rc = power_on(chip);
		if (rc)
			goto err_power;
	}
	if (prx_on && chip->prx_enabled)
		(void)taos_prox_enable(chip, 1);
	if (als_on && chip->als_enabled)
		(void)taos_als_enable(chip, 1);
	if (chip->irq_pending) {
		dev_dbg(dev, "%s: pending interrupt\n", __func__);
		chip->irq_pending = 0;
		//(void)taos_check_and_report(chip);
		//enable_irq(chip->client->irq);
	}
err_power:
	mutex_unlock(&chip->lock);

	return 0;
}

static int __devexit taos_remove(struct i2c_client *client)
{
	struct tsl2772_chip *chip = i2c_get_clientdata(client);
	free_irq(client->irq, chip);
	if (chip->a_idev) {
		remove_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
		input_unregister_device(chip->a_idev);
	}
	if (chip->p_idev) {
		remove_sysfs_interfaces(&chip->p_idev->dev,
			prox_attrs, ARRAY_SIZE(prox_attrs));
		input_unregister_device(chip->p_idev);
	}
	if (chip->pdata->platform_teardown)
		chip->pdata->platform_teardown(&client->dev);
	i2c_set_clientdata(client, NULL);
	kfree(chip->segment);
	kfree(chip);
	return 0;
}

static struct i2c_device_id taos_idtable[] = {
	{ "tsl2772", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, taos_idtable);

static const struct dev_pm_ops taos_pm_ops = {
	.suspend = taos_suspend,
	.resume  = taos_resume,
};

static struct i2c_driver taos_driver = {
	.driver = {
		.name = "tsl2772",
		.pm = &taos_pm_ops,
	},
	.id_table = taos_idtable,
	.probe = taos_probe,
	.remove = __devexit_p(taos_remove),
};

static int __init taos_init(void)
{
	return i2c_add_driver(&taos_driver);
}

static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_driver);
}

module_init(taos_init);
module_exit(taos_exit);

MODULE_AUTHOR("Aleksej Makarov <aleksej.makarov@sonyericsson.com>");
MODULE_DESCRIPTION("TAOS tsl2772 ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");
