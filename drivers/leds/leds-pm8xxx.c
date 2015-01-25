/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/ctype.h>

#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pwm.h>
#include <linux/leds-pm8xxx.h>

#define SSBI_REG_ADDR_DRV_KEYPAD	0x48
#define PM8XXX_DRV_KEYPAD_BL_MASK	0xf0
#define PM8XXX_DRV_KEYPAD_BL_SHIFT	0x04

#define SSBI_REG_ADDR_FLASH_DRV0        0x49
#define PM8XXX_DRV_FLASH_MASK           0xf0
#define PM8XXX_DRV_FLASH_SHIFT          0x04

#define SSBI_REG_ADDR_FLASH_DRV1        0xFB

#define SSBI_REG_ADDR_LED_CTRL_BASE	0x131
#define SSBI_REG_ADDR_LED_CTRL(n)	(SSBI_REG_ADDR_LED_CTRL_BASE + (n))
#define PM8XXX_DRV_LED_CTRL_MASK	0xf8
#define PM8XXX_DRV_LED_CTRL_SHIFT	0x03

#define SSBI_REG_ADDR_WLED_CTRL_BASE	0x25A
#define SSBI_REG_ADDR_WLED_CTRL(n)	(SSBI_REG_ADDR_WLED_CTRL_BASE + (n) - 1)

/* wled control registers */
#define WLED_MOD_CTRL_REG		SSBI_REG_ADDR_WLED_CTRL(1)
#define WLED_MAX_CURR_CFG_REG(n)	SSBI_REG_ADDR_WLED_CTRL(n + 2)
#define WLED_BRIGHTNESS_CNTL_REG1(n)	SSBI_REG_ADDR_WLED_CTRL((2 * n) + 5)
#define WLED_BRIGHTNESS_CNTL_REG2(n)	SSBI_REG_ADDR_WLED_CTRL((2 * n) + 6)
#define WLED_SYNC_REG			SSBI_REG_ADDR_WLED_CTRL(11)
#define WLED_OVP_CFG_REG		SSBI_REG_ADDR_WLED_CTRL(13)
#define WLED_BOOST_CFG_REG		SSBI_REG_ADDR_WLED_CTRL(14)
#define WLED_RESISTOR_COMPENSATION_CFG_REG	SSBI_REG_ADDR_WLED_CTRL(15)
#define WLED_HIGH_POLE_CAP_REG		SSBI_REG_ADDR_WLED_CTRL(16)

#define WLED_STRINGS			0x03
#define WLED_OVP_VAL_MASK		0x30
#define WLED_OVP_VAL_BIT_SHFT		0x04
#define WLED_BOOST_LIMIT_MASK		0xE0
#define WLED_BOOST_LIMIT_BIT_SHFT	0x05
#define WLED_BOOST_OFF			0x00
#define WLED_EN_MASK			0x01
#define WLED_CP_SELECT_MAX		0x03
#define WLED_CP_SELECT_MASK		0x03
#define WLED_DIG_MOD_GEN_MASK		0x70
#define WLED_CS_OUT_MASK		0x0E
#define WLED_CTL_DLY_STEP		200
#define WLED_CTL_DLY_MAX		1400
#define WLED_CTL_DLY_MASK		0xE0
#define WLED_CTL_DLY_BIT_SHFT		0x05
#define WLED_MAX_CURR			25
#define WLED_MAX_CURR_MASK		0x1F
#define WLED_OP_FDBCK_MASK		0x1C
#define WLED_OP_FDBCK_BIT_SHFT		0x02

#define WLED_MAX_LEVEL			255
#define WLED_8_BIT_MASK			0xFF
#define WLED_4_BIT_MASK			0x0F
#define WLED_8_BIT_SHFT			0x08
//#define WLED_MAX_DUTY_CYCLE		0xFFF
#define WLED_MAX_DUTY_CYCLE		0xC00 //Taylor--20120821
#define WLED_4_BIT_SHFT			0x04

#define WLED_RESISTOR_COMPENSATION_DEFAULT	20
#define WLED_RESISTOR_COMPENSATION_MAX	320

#define WLED_SYNC_VAL			0x07
#define WLED_SYNC_RESET_VAL		0x00
#define WLED_SYNC_MASK			0xF8

#define ONE_WLED_STRING			1
#define TWO_WLED_STRINGS		2
#define THREE_WLED_STRINGS		3

#define WLED_CABC_ONE_STRING		0x01
#define WLED_CABC_TWO_STRING		0x03
#define WLED_CABC_THREE_STRING		0x07

#define WLED_CABC_SHIFT			3

#define SSBI_REG_ADDR_RGB_CNTL1		0x12D
#define SSBI_REG_ADDR_RGB_CNTL2		0x12E

#define PM8XXX_DRV_RGB_RED_LED		BIT(2)
#define PM8XXX_DRV_RGB_GREEN_LED	BIT(1)
#define PM8XXX_DRV_RGB_BLUE_LED		BIT(0)

#define MAX_FLASH_LED_CURRENT		300
#define MAX_LC_LED_CURRENT		40
#define MAX_KP_BL_LED_CURRENT		300

#define PM8XXX_ID_LED_CURRENT_FACTOR	2  /* Iout = x * 2mA */
#define PM8XXX_ID_FLASH_CURRENT_FACTOR	20 /* Iout = x * 20mA */

#define PM8XXX_FLASH_MODE_DBUS1		1
#define PM8XXX_FLASH_MODE_DBUS2		2
#define PM8XXX_FLASH_MODE_PWM		3

#define MAX_LC_LED_BRIGHTNESS		20
#define MAX_FLASH_BRIGHTNESS		15
#define MAX_KB_LED_BRIGHTNESS		15

#define PM8XXX_LED_OFFSET(id) ((id) - PM8XXX_ID_LED_0)

#define PM8XXX_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP | \
				PM_PWM_LUT_PAUSE_LO_EN | PM_PWM_LUT_PAUSE_HI_EN)

#define PM8XXX_LED_PWM_GRPFREQ_MAX	255
#define PM8XXX_LED_PWM_GRPPWM_MAX	255

#define LED_MAP(_version, _kb, _led0, _led1, _led2, _flash_led0, _flash_led1, \
	_wled, _rgb_led_red, _rgb_led_green, _rgb_led_blue)\
	{\
		.version = _version,\
		.supported = _kb << PM8XXX_ID_LED_KB_LIGHT | \
			_led0 << PM8XXX_ID_LED_0 | _led1 << PM8XXX_ID_LED_1 | \
			_led2 << PM8XXX_ID_LED_2  | \
			_flash_led0 << PM8XXX_ID_FLASH_LED_0 | \
			_flash_led1 << PM8XXX_ID_FLASH_LED_1 | \
			_wled << PM8XXX_ID_WLED | \
			_rgb_led_red << PM8XXX_ID_RGB_LED_RED | \
			_rgb_led_green << PM8XXX_ID_RGB_LED_GREEN | \
			_rgb_led_blue << PM8XXX_ID_RGB_LED_BLUE, \
	}

static int blinktuning_r[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int blinktuning_g[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int blinktuning_b[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int in500out1000tunebase[20] = {
		6, 11, 29, 42, 56, 70, 89, 100, 92, 84,
	   76, 65, 57, 49, 40, 32, 23,  15,  7, 0
};

static int in500out1000tune_red[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int in500out1000tune_green[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int in500out1000tune_blue[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int in1000out1500[50] = {
		5, 10, 15, 20, 25, 30, 
	   35, 40, 45, 50, 55, 60, 65, 
	   70, 75, 80, 85, 90, 95, 100, 
	   95, 93, 90, 88, 85, 80, 77, 
	   74, 70, 67, 64, 60, 57, 54, 50, 
	   47, 44, 40, 37, 34, 30, 27, 24, 
	   20, 17, 14, 10,  7,  3,  0
};
/*
static int in500out1000[50] = {
		6, 13, 19, 25, 31, 38, 44, 
	   50, 56, 63, 69, 75, 81, 88, 94, 100, 
	   97, 94, 91, 88, 85, 82, 79, 76, 
	   73, 70, 67, 64, 61, 58, 55, 52,
	   49, 46, 43, 40, 37, 34, 31, 28,
	   25, 22, 19, 16, 14, 12,	9,	6,
	    3,  0
};
*/
static int in800out1300[60] = {
		4,	8, 12, 15, 20, 25, 30, 
	   35, 40, 45, 50, 54, 58, 62, 66, 
	   70, 74, 78, 82, 86, 90, 95, 100, 
	   95, 93, 90, 88, 85, 83, 80, 78, 
	   75, 73, 70, 68, 65, 63, 60, 58, 
	   55, 50, 48, 45, 43, 40, 38, 35, 
	   33, 30, 28, 25, 22, 19, 16, 13, 
	   10,  8,	5,	3,	0
};
static int in1500out3000[60] = {
		5, 10, 15, 20, 25, 30, 35, 40,
	   45, 50, 55, 60, 65, 70, 75, 80,
	   85, 90, 95, 100,
	   98, 95, 93, 90, 88, 85, 83, 80, 
	   78, 75, 73, 70, 68, 65, 63, 60, 
	   58, 55, 53, 50, 48, 45, 43, 40, 
	   38, 35, 33, 30, 28, 25, 23, 20, 
	   18, 15, 13, 10,	8,	5,	3,	0
};
/*
static int in300out700[50] = {
		6, 12, 18, 24, 30, 36, 42,
	   48, 55, 62, 70, 78, 87, 93,
	  100, 97, 94, 92, 90, 87, 85,
	   83, 80, 77, 75, 73, 70, 68,
	   65, 63, 60, 57, 54, 51, 48,
	   45, 42, 39, 36, 33, 30, 27,
	   24, 21, 18, 15, 11,  7,  3,
	    0	   
};
static int in625out625[50] = {
		0,	4,  8, 12, 16, 20, 24,
	   28, 32, 36, 40, 44, 48, 52,
	   56, 60, 64, 68, 72, 76, 80,
	   85, 90, 95, 100, 96, 92, 88,
	   84, 80, 76, 72, 68, 64, 60,
	   56, 52, 48, 44, 40, 36, 32,
	   28, 24, 20, 16, 12,  8,  4,
	    0	   
};
static int in200out1900[60] = {
	   15, 31, 48, 65, 82, 100,
	   98, 96, 94, 92, 90, 88, 86,
	   84, 82, 80, 78, 76, 74, 72,
	   70, 68, 66, 64, 62, 60, 58,
	   56, 54, 52, 50, 48, 46, 44,
	   42, 40, 38, 36, 34, 32, 30,
	   28, 26, 24, 22, 20, 18, 16,
	   14, 12, 10,  9,  8,  7,  6,
	    5,  4,  3,  2,  0
};
*/
//fade-in 500 then continuous light
static int in500[20] = {
		5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
	   55, 60, 65, 70, 75, 80, 85, 90, 95, 100
};
//fade-out 1000
static int out1000[20] = {
	  100, 95, 90, 85, 80, 75, 70, 65, 60, 55,
	   50, 45, 40, 35, 28, 21, 14,  9,  4,  0
};
static int fadetune_red[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int fadetune_green[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int fadetune_blue[20] = {
		0,	0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};
//fade-in 300, fade-out 700
static int in300out700tunebase[20] = {
		16,  32, 48, 64, 80, 100, 93, 86, 79, 72,
	    65,  58, 50, 43, 35,  28, 21, 14,  7,  0
};
//fade-in 625, fade-out 625
static int in625out625tunebase[20] = {
		10,  20, 30, 40, 50, 60, 70, 80, 90, 100,
	    90,  80, 70, 60, 50, 40, 30, 20, 10,   0
};
static int in200out1900tunebase[20] = {
		50, 100, 95, 89, 83, 77, 70, 65, 60, 54,
	    48,  42, 37, 32, 27, 22, 17, 12,  6,  0
};

//fade-in 200, fade-out 5900
static int in200out5900tunebase[20] = {
		50, 100, 95, 89, 83, 77, 70, 65, 60, 54,
	    48,  42, 37, 32, 27, 22, 17, 12,  6,  0
};

#define PM8XXX_PWM_CURRENT_4MA		4
#define PM8XXX_PWM_CURRENT_8MA		8
#define PM8XXX_PWM_CURRENT_12MA		12

/**
 * supported_leds - leds supported for each PMIC version
 * @version - version of PMIC
 * @supported - which leds are supported on version
 */

struct supported_leds {
	enum pm8xxx_version version;
	u32 supported;
};

static const struct supported_leds led_map[] = {
	LED_MAP(PM8XXX_VERSION_8058, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8921, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8018, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8922, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1),
	LED_MAP(PM8XXX_VERSION_8038, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1),
};

/**
 * struct pm8xxx_led_data - internal led data structure
 * @led_classdev - led class device
 * @id - led index
 * @work - workqueue for led
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @pwm_dev - pointer to PWM device if LED is driven using PWM
 * @pwm_channel - PWM channel ID
 * @pwm_period_us - PWM period in micro seconds
 * @pwm_duty_cycles - struct that describes PWM duty cycles info
 * @use_pwm - controlled by userspace
 */
struct pm8xxx_led_data {
	struct led_classdev	cdev;
	int			id;
	u8			reg;
	u8			wled_mod_ctrl_val;
	u8			lock_update;
	u8			blink;
	struct device		*dev;
	struct work_struct	work;
	struct work_struct	modework;
	struct work_struct	testwork;

	struct mutex		lock;
	struct pwm_device	*pwm_dev;
	int			pwm_channel;
	u32			pwm_period_us;
	struct pm8xxx_pwm_duty_cycles *pwm_duty_cycles;
	struct wled_config_data *wled_cfg;
	int			max_current;
	int			use_pwm;
	int			adjust_brightness;
	u16			pwm_grppwm;
	u16			pwm_grpfreq;
	u16			pwm_pause_hi;
	u16			pwm_pause_lo;
};

static void led_kp_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;

	level = (value << PM8XXX_DRV_KEYPAD_BL_SHIFT) &
				 PM8XXX_DRV_KEYPAD_BL_MASK;

	led->reg &= ~PM8XXX_DRV_KEYPAD_BL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_DRV_KEYPAD,
								led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev,
			"can't set keypad backlight level rc=%d\n", rc);
}

static void led_lc_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, offset;
	u8 level;

	level = (value << PM8XXX_DRV_LED_CTRL_SHIFT) &
				PM8XXX_DRV_LED_CTRL_MASK;

	offset = PM8XXX_LED_OFFSET(led->id);

	led->reg &= ~PM8XXX_DRV_LED_CTRL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_LED_CTRL(offset),
								led->reg);
	if (rc)
		dev_err(led->cdev.dev, "can't set (%d) led value rc=%d\n",
				led->id, rc);
}

static void
led_flash_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;
	u16 reg_addr;

	level = (value << PM8XXX_DRV_FLASH_SHIFT) &
				 PM8XXX_DRV_FLASH_MASK;

	led->reg &= ~PM8XXX_DRV_FLASH_MASK;
	led->reg |= level;

	if (led->id == PM8XXX_ID_FLASH_LED_0)
		reg_addr = SSBI_REG_ADDR_FLASH_DRV0;
	else
		reg_addr = SSBI_REG_ADDR_FLASH_DRV1;

	rc = pm8xxx_writeb(led->dev->parent, reg_addr, led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set flash led%d level rc=%d\n",
			 led->id, rc);
}

static int
led_wled_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, duty;
	u8 val, i, num_wled_strings;

	if (value > WLED_MAX_LEVEL)
		value = WLED_MAX_LEVEL;

	if (value == 0) {
		rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG,
				WLED_BOOST_OFF);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled ctrl config"
				" register rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG,
				led->wled_mod_ctrl_val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled ctrl config"
				" register rc=%d\n", rc);
			return rc;
		}
	}

	duty = (WLED_MAX_DUTY_CYCLE * value) / WLED_MAX_LEVEL;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program brightness control registers */
	for (i = 0; i < num_wled_strings; i++) {
		rc = pm8xxx_readb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG1(i), &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled brightnes ctrl"
				" register1 rc=%d\n", rc);
			return rc;
		}

		val = (val & ~WLED_MAX_CURR_MASK) | (duty >> WLED_8_BIT_SHFT);
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG1(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled brightness ctrl"
				" register1 rc=%d\n", rc);
			return rc;
		}

		val = duty & WLED_8_BIT_MASK;
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG2(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled brightness ctrl"
				" register2 rc=%d\n", rc);
			return rc;
		}
	}
	rc = pm8xxx_readb(led->dev->parent, WLED_SYNC_REG, &val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	/* sync */
	val &= WLED_SYNC_MASK;
	val |= WLED_SYNC_VAL;
	rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	val &= WLED_SYNC_MASK;
	val |= WLED_SYNC_RESET_VAL;
	rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	return 0;
}

static void wled_dump_regs(struct pm8xxx_led_data *led)
{
	int i;
	u8 val;

	for (i = 1; i < 17; i++) {
		pm8xxx_readb(led->dev->parent,
				SSBI_REG_ADDR_WLED_CTRL(i), &val);
		pr_debug("WLED_CTRL_%d = 0x%x\n", i, val);
	}
}

static void
led_rgb_write(struct pm8xxx_led_data *led, u16 addr, enum led_brightness value)
{
	int rc;
	u8 val, mask;

	if (led->id != PM8XXX_ID_RGB_LED_BLUE &&
		led->id != PM8XXX_ID_RGB_LED_RED &&
		led->id != PM8XXX_ID_RGB_LED_GREEN)
		return;

	rc = pm8xxx_readb(led->dev->parent, addr, &val);
	if (rc) {
		dev_err(led->cdev.dev, "can't read rgb ctrl register rc=%d\n",
							rc);
		return;
	}

	switch (led->id) {
	case PM8XXX_ID_RGB_LED_RED:
		mask = PM8XXX_DRV_RGB_RED_LED;
		break;
	case PM8XXX_ID_RGB_LED_GREEN:
		mask = PM8XXX_DRV_RGB_GREEN_LED;
		break;
	case PM8XXX_ID_RGB_LED_BLUE:
		mask = PM8XXX_DRV_RGB_BLUE_LED;
		break;
	default:
		return;
	}

	if (value)
		val |= mask;
	else
		val &= ~mask;

	rc = pm8xxx_writeb(led->dev->parent, addr, val);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set rgb led %d level rc=%d\n",
			 led->id, rc);
}

static void pm8xxx_led_brightnesstest(struct led_classdev *led_cdev,
	enum led_testbrightness testvalue)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	//if (value < LED_OFF || value > led->cdev.max_brightness) {
	//	dev_err(led->cdev.dev, "Invalid brightness value exceeds");
	//	return;
	//}
	led->cdev.ccitest_brightness = testvalue;
	schedule_work(&led->testwork);
}

int redledflag = 0;
int greenledflag = 0;
int blueledflag = 0;

static int pm8xxx_led_pwm_test_work(struct pm8xxx_led_data *led)
{
	int duty_us;
	int rc = 0;

	if (led->pwm_duty_cycles == NULL) {
		duty_us = (led->pwm_period_us * led->cdev.ccitest_brightness) /
								LED_FULL;
		switch (led->id) {
		case PM8XXX_ID_RGB_LED_RED:
			if(led->cdev.ccitest_brightness == 0 && redledflag == 0){
				return rc;
			}
			else if(led->cdev.ccitest_brightness == 0 && redledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				redledflag = 0;
			}
			else if(led->cdev.ccitest_brightness != 0 && redledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				rc = pwm_enable(led->pwm_dev);
			}
			else {
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				rc = pwm_enable(led->pwm_dev);
				redledflag = 1;
			}
			break;
		case PM8XXX_ID_RGB_LED_GREEN:
			if(led->cdev.ccitest_brightness == 0 && greenledflag == 0){
				return rc;
			}
			else if(led->cdev.ccitest_brightness == 0 && greenledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				greenledflag = 0;
			}
			else if(led->cdev.ccitest_brightness != 0 && greenledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				rc = pwm_enable(led->pwm_dev);
			}
			else {
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				rc = pwm_enable(led->pwm_dev);
				greenledflag = 1;
			}
			break;
		case PM8XXX_ID_RGB_LED_BLUE:
			if(led->cdev.ccitest_brightness == 0 && blueledflag == 0){
				return rc;
			}
			else if(led->cdev.ccitest_brightness == 0 && blueledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				blueledflag = 0;
				return rc;
			}
			else if(led->cdev.ccitest_brightness != 0 && blueledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				rc = pwm_enable(led->pwm_dev);
			}
			else {
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.ccitest_brightness);
				rc = pwm_enable(led->pwm_dev);
				blueledflag = 1;
				return rc;
			}
			break;
		default:
			pr_err("led id %d is not supported\n", led->id);
			break;
		}
		
	} else {
		rc = pm8xxx_pwm_lut_enable(led->pwm_dev, led->cdev.ccitest_brightness);
	}
	return rc;
}

static void pm8xxx_led_modeset(struct led_classdev *led_cdev,
	enum led_op_mode modevalue)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	//if (value < LED_OFF || value > led->cdev.max_brightness) {
	//	dev_err(led->cdev.dev, "Invalid brightness value exceeds");
	//	return;
	//}
	led->cdev.ccimode = modevalue;
	schedule_work(&led->modework);
}

static void pm8xxx_led_onmsset(struct led_classdev *led_cdev,
	enum led_op_onms blinkonms)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);
	led->cdev.onMS = blinkonms*1000;
}
static void pm8xxx_led_offmsset(struct led_classdev *led_cdev,
	enum led_op_offms blinkoffms)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);
	led->cdev.offMS = blinkoffms*1000;
}
static enum led_op_onms pm8xxx_led_onmsget(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.onMS;
}
static enum led_op_offms pm8xxx_led_offmsget(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.offMS;
}

static void pm8xxx_led_tunebrightset(struct led_classdev *led_cdev,
	enum led_tunebrightness tunevalue)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);
	led->cdev.tunebrightness = tunevalue;
}
static enum led_tunebrightness pm8xxx_led_tunebrightget(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.tunebrightness;
}

static void pm8xxx_led_batpaset(struct led_classdev *led_cdev,
	enum led_batpa batpavalue)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);
	led->cdev.batpa = batpavalue;
}
static enum led_batpa pm8xxx_led_batpaget(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.batpa;
}


int mode = 0;

int old_mode_red = 0 ;
int old_mode_green = 0 ;
int old_mode_blue = 0 ;

static int pm8xxx_led_pwm_mode_work(struct pm8xxx_led_data *led)
{
	int rc = 0;
	int flags = 0;

	int leveltime = 0;
	int onsteps = 0;
	int i = 0;
	int period_time = 0;
	int blinktunvalue = 0;

	mode = led->cdev.ccimode;
	if(led->id == PM8XXX_ID_WLED){
		pr_err("disable wled pwm_mode_work!\n");
		return rc;
	}	
	if(led->id == PM8XXX_ID_RGB_LED_RED && redledflag == 1){
		rc = pwm_config(led->pwm_dev, 0, led->pwm_period_us);
		pwm_disable(led->pwm_dev);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
		redledflag = 0;
	}
	else if(led->id == PM8XXX_ID_RGB_LED_GREEN && greenledflag == 1){
		rc = pwm_config(led->pwm_dev, 0, led->pwm_period_us);
		pwm_disable(led->pwm_dev);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
		greenledflag = 0;
	}
	else if(led->id == PM8XXX_ID_RGB_LED_BLUE && blueledflag == 1){
		rc = pwm_config(led->pwm_dev, 0, led->pwm_period_us);
		pwm_disable(led->pwm_dev);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
		blueledflag = 0;
	}
	
	if(led->id == PM8XXX_ID_RGB_LED_RED && mode !=0){
		pr_err("set red_led old mode :%d\n",mode);
		old_mode_red = mode;
	}
	else if (led->id == PM8XXX_ID_RGB_LED_GREEN && mode !=0){
		pr_err("set green_led old mode :%d\n",mode);
		old_mode_green = mode;
	}
	else if(led->id == PM8XXX_ID_RGB_LED_BLUE && mode !=0){
		pr_err("set blue_led old mode :%d\n",mode);
		old_mode_blue = mode;
	}

	switch(mode){
		case 0://disable LED flashing
			pr_err("LED mode 0:\n");
			if(led->id == PM8XXX_ID_RGB_LED_RED && old_mode_red){
			if(old_mode_red == 2 || old_mode_red == 3 || old_mode_red == 4 || old_mode_red == 5 ||
				old_mode_red == 6 || old_mode_red == 7 || old_mode_red == 8 || old_mode_red == 9 ||
				old_mode_red == 10 || old_mode_red == 11 || old_mode_red == 12 || old_mode_red == 13 ||
				old_mode_red == 14 || old_mode_red == 15 || old_mode_red == 16 || old_mode_red == 17 ||
				old_mode_red == 18 || old_mode_red == 19 || old_mode_red == 20 || old_mode_red == 21)
			{
				pr_err("old_red_mode=%d\n",old_mode_red);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 0);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				old_mode_red = 0;
			}}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN && old_mode_green){
			if(old_mode_green == 2 || old_mode_green == 3 || old_mode_green == 4 || old_mode_green == 5 ||
				old_mode_green == 6 || old_mode_green == 7 || old_mode_green == 8 || old_mode_green == 9 ||
				old_mode_green == 10 || old_mode_green == 11 || old_mode_green == 12 || old_mode_green == 13 ||
				old_mode_green == 14 || old_mode_green == 15 || old_mode_green == 16 || old_mode_green == 17 ||
				old_mode_green == 18 || old_mode_green == 19 || old_mode_green == 20 || old_mode_green == 21)
			{
				pr_err("old_green_mode=%d\n",old_mode_green);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 0);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				old_mode_green = 0;
			}}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE && old_mode_blue){
			if(old_mode_blue == 2 || old_mode_blue == 3 || old_mode_blue == 4 || old_mode_blue == 5 ||
				old_mode_blue == 6 || old_mode_blue == 7 || old_mode_blue == 8 || old_mode_blue == 9 ||
				old_mode_blue == 10 || old_mode_blue == 11 || old_mode_blue == 12 || old_mode_blue == 13 ||
				old_mode_blue == 14 || old_mode_blue == 15 || old_mode_blue == 16 || old_mode_blue == 17 ||
				old_mode_blue == 18 || old_mode_blue == 19 || old_mode_blue == 20 || old_mode_blue == 21)
			{
				pr_err("old_blue_mode=%d\n",old_mode_blue);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 0);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				old_mode_blue = 0;
			}}
		break;
		case 1://[LED blink]
			pr_err("LED mode 1:\n");
			switch (led->id){
			case PM8XXX_ID_RGB_LED_RED:
				if(led->cdev.brightness != 0 && redledflag == 1){
					rc = pwm_config(led->pwm_dev, 0, led->pwm_period_us);
					pwm_disable(led->pwm_dev);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
					rc = pwm_config(led->pwm_dev, led->cdev.onMS, led->cdev.offMS);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
					rc = pwm_enable(led->pwm_dev);
				} else {
					rc = pwm_config(led->pwm_dev, led->cdev.onMS, led->cdev.offMS);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
					rc = pwm_enable(led->pwm_dev);
					redledflag = 1;
				}				
				break;
			case PM8XXX_ID_RGB_LED_GREEN:
				if(led->cdev.brightness != 0 && greenledflag == 1){
					rc = pwm_config(led->pwm_dev, 0, led->pwm_period_us);
					pwm_disable(led->pwm_dev);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
					rc = pwm_config(led->pwm_dev, led->cdev.onMS, led->cdev.offMS);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
					rc = pwm_enable(led->pwm_dev);
				} else {
					rc = pwm_config(led->pwm_dev, led->cdev.onMS, led->cdev.offMS);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
					rc = pwm_enable(led->pwm_dev);
					greenledflag = 1;
				}				
				break;
			case PM8XXX_ID_RGB_LED_BLUE:
				if(led->cdev.brightness != 0 && blueledflag == 1){
					rc = pwm_config(led->pwm_dev, 0, led->pwm_period_us);
					pwm_disable(led->pwm_dev);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
					rc = pwm_config(led->pwm_dev, led->cdev.onMS, led->cdev.offMS);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
					rc = pwm_enable(led->pwm_dev);
				} else {
					rc = pwm_config(led->pwm_dev, led->cdev.onMS, led->cdev.offMS);
					led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
					rc = pwm_enable(led->pwm_dev);
					blueledflag = 1;
				}				
				break;
			default :
				pr_err("the LED ID:%d is not supported!\n",led->id);
				break;
			}
		break;

		case 2://[fade-in 1000ms, fade-out 1500ms]
			pr_err("LED mode 2:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2500, in1000out1500, 50, 1, 50, 0, 0, flags);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_config() error\n", __func__);

			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
			rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_enable() error\n", __func__);
		break;
		case 3://[fade-in 500ms, fade-out 1000ms mix color]
			pr_err("LED mode 3:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					in500out1000tune_red[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_red, 75, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					in500out1000tune_green[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_green, 75, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					in500out1000tune_blue[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_blue, 75, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}			
		break;
		case 4://[fade-in 800ms, fade-out 1300ms]
			pr_err("LED mode 4:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, in800out1300, 35, 1, 60, 0, 0, flags);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_config() error\n", __func__);

			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
            rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_enable() error\n", __func__);

		break;
		case 5://[fade-in 1500ms, fade-out 3000ms]
			pr_err("LED mode 5:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
            rc = pm8xxx_pwm_lut_config(led->pwm_dev, 4500, in1500out3000, 75, 1, 60, 0, 0, flags);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_config() error\n", __func__);

			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
			rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_enable() error\n", __func__);
			
		break;
		case 6://[pulse once,fade-in 1000ms, fade-out 1500ms]
			pr_err("LED mode 6:\n");

			flags = PM_PWM_LUT_RAMP_UP;
			rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2500, in1000out1500, 50, 1, 50, 0, 0, flags);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_config() error\n", __func__);

			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
			rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_enable() error\n", __func__);
		break;
		case 7://[pulse once,fade-in 500ms, fade-out 1000ms mix color]
			pr_err("LED mode 7:\n");

			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					in500out1000tune_red[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_red, 75, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					in500out1000tune_green[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_green, 75, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					in500out1000tune_blue[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_blue, 75, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}	
		break;
		case 8://[pulse once,fade-in 800ms, fade-out 1300ms]
			pr_err("LED mode 8:\n");

			flags = PM_PWM_LUT_RAMP_UP;
			rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, in800out1300, 35, 1, 60, 0, 0, flags);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_config() error\n", __func__);

			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
			rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_enable() error\n", __func__);

		break;
		case 9://[pulse once,fade-in 1500ms, fade-out 3000ms]
			pr_err("LED mode 9:\n");

			flags = PM_PWM_LUT_RAMP_UP;
            rc = pm8xxx_pwm_lut_config(led->pwm_dev, 4500, in1500out3000, 75, 1, 60, 0, 0, flags);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_config() error\n", __func__);

			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
            rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
            if (rc)
                printk("%s: pm8xxx_pwm_lut_enable() error\n", __func__);
			
		break;
		case 10://[LED blink tuning mode]
			pr_err("LED mode 10:\n");
			leveltime = (led->cdev.onMS+led->cdev.offMS) / 20;
			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				onsteps = led->cdev.onMS / leveltime;
				if((onsteps == 0) && led->cdev.onMS != 0){
					onsteps = 1;
				}
				blinktunvalue = (led->cdev.tunebrightness*100)/255;
				for (i = 0; i<onsteps ; i++)
				{
					blinktuning_r[i] = blinktunvalue;
				}
				leveltime = leveltime / 1000;
				period_time = leveltime*13;
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, period_time, blinktuning_r, leveltime, 1, 13, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				onsteps = led->cdev.onMS / leveltime;
				if((onsteps == 0) && led->cdev.onMS != 0){
					onsteps = 1;
				}
				blinktunvalue = (led->cdev.tunebrightness*100)/255;
				for (i = 0; i<onsteps ; i++)
				{
					blinktuning_g[i] = blinktunvalue;
				}
				leveltime = leveltime / 1000;
				period_time = leveltime*13;
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, period_time, blinktuning_g, leveltime, 21, 13, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				onsteps = led->cdev.onMS / leveltime;
				if((onsteps == 0) && led->cdev.onMS != 0){
					onsteps = 1;
				}
				blinktunvalue = (led->cdev.tunebrightness*100)/255;
				for (i = 0; i<onsteps ; i++)
				{
					blinktuning_b[i] = blinktunvalue;
				}
				leveltime = leveltime / 1000;
				period_time = leveltime*13;
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, period_time, blinktuning_b, leveltime, 41, 13, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);

			}
			for (i = 0; i<20 ; i++)
			{
				blinktuning_r[i] = 0;
				blinktuning_g[i] = 0;
				blinktuning_b[i] = 0;
			}
		break;
		case 11://[fade-in 500ms, fade-out 1000ms tuning mode]
			pr_err("LED mode 11:\n");
			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					in500out1000tune_red[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_red, 75, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					in500out1000tune_green[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_green, 75, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					in500out1000tune_blue[i] = (led->cdev.tunebrightness*in500out1000tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1500, in500out1000tune_blue, 75, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
			}			
		break;
		case 12://[fade-in 300ms, fade-out 700ms mix color]
			pr_err("LED mode 12:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in300out700tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_red, 50, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in300out700tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_green, 50, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in300out700tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_blue, 50, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}
		break;
		case 13://[fade-in 625ms, fade-out 625ms mix color]
			pr_err("LED mode 13:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in625out625tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1260, fadetune_red, 63, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in625out625tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1260, fadetune_green, 63, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in625out625tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1260, fadetune_blue, 63, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}
		break;
		case 14://[fade-in 200ms, fade-out 1900ms mix color]
			pr_err("LED mode 14:\n");

			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in200out1900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, fadetune_red, 105, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in200out1900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, fadetune_green, 105, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in200out1900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, fadetune_blue, 105, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}

		break;
		case 15://[pulse once, fade-in 300ms, fade-out 700ms mix color]
			pr_err("LED mode 15:\n");

			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in300out700tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_red, 50, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in300out700tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_green, 50, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in300out700tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_blue, 50, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}
		break;
		case 16://[pulse once, fade-in 625ms, fade-out 625ms mix color]
			pr_err("LED mode 16:\n");

			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in625out625tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1260, fadetune_red, 63, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in625out625tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1260, fadetune_green, 63, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in625out625tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1260, fadetune_blue, 63, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}

		break;
		case 17://[pulse once, fade-in 200ms, fade-out 1900ms mix color]
			pr_err("LED mode 17:\n");

			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in200out1900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, fadetune_red, 105, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in200out1900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, fadetune_green, 105, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in200out1900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 2100, fadetune_blue, 105, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}

		break;
		case 18://[fade-in 500ms then continuous light with mix color]
			pr_err("LED mode 18:\n");
			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in500[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 500, fadetune_red, 25, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in500[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 500, fadetune_green, 25, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in500[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 500, fadetune_blue, 25, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}			
		break;
		case 19://[fade-out 1000ms with mix color]
			pr_err("LED mode 19:\n");
			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*out1000[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_red, 50, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*out1000[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_green, 50, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*out1000[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 1000, fadetune_blue, 50, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}			
		break;
		case 20://[fade-in 200ms, fade-out 5900ms mix color continuously]
			pr_err("LED mode 20:\n");
			flags = PM_PWM_LUT_RAMP_UP | PM_PWM_LUT_LOOP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in200out5900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 6100, fadetune_red, 305, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in200out5900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 6100, fadetune_green, 305, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in200out5900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 6100, fadetune_blue, 305, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}			
		break;
		case 21://[fade-in 200ms, fade-out 5900ms mix color pulse once]
			pr_err("LED mode 21:\n");
			flags = PM_PWM_LUT_RAMP_UP;
			if(led->id == PM8XXX_ID_RGB_LED_RED){
				for (i=0; i<20 ; i++){
					fadetune_red[i] = (led->cdev.tunebrightness*in200out5900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 6100, fadetune_red, 305, 1, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_red[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_GREEN){
				for (i=0; i<20 ; i++){
					fadetune_green[i] = (led->cdev.tunebrightness*in200out5900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 6100, fadetune_green, 305, 21, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_green[i] = 0;
				}
			}
			else if(led->id == PM8XXX_ID_RGB_LED_BLUE){
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = (led->cdev.tunebrightness*in200out5900tunebase[i])/255;
				}
				rc = pm8xxx_pwm_lut_config(led->pwm_dev, 6100, fadetune_blue, 305, 41, 20, 0, 0, flags);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
					255);
				rc = pm8xxx_pwm_lut_enable(led->pwm_dev, 1);
				for (i=0; i<20 ; i++){
					fadetune_blue[i] = 0;
				}
			}			
		break;
		default:
			pr_err("no mode match!\n");
		break;
	}
	return rc;
}

static void
led_rgb_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	if (value) {
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, value);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL2, value);
	} else {
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL2, value);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, value);
	}
}

static int pm8xxx_adjust_brightness(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	int level = 0;
	struct	pm8xxx_led_data *led;
	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	if (!led->adjust_brightness)
		return value;

	if (led->adjust_brightness == led->cdev.max_brightness)
		return value;

	level = (2 * value * led->adjust_brightness
			+ led->cdev.max_brightness)
		/ (2 * led->cdev.max_brightness);

	if (!level && value)
		level = 1;

	return level;
}

static int pm8xxx_led_pwm_pattern_update(struct pm8xxx_led_data * led)
{
	int start_idx, idx_len;
	int *pcts = NULL;
	int i, rc = 0;
	int temp = 0;
	int pwm_max = 0;
	int total_ms, on_ms;
	int flags;

	if (!led->pwm_duty_cycles || !led->pwm_duty_cycles->duty_pcts) {
		dev_err(led->cdev.dev, "duty_cycles and duty_pcts is not exist\n");
		return -EINVAL;
	}

	if (led->pwm_grppwm > 0 && led->pwm_grpfreq > 0) {
		total_ms = led->pwm_grpfreq * 50;
		on_ms = (led->pwm_grppwm * total_ms) >> 8;
		if (PM8XXX_LED_PWM_FLAGS & PM_PWM_LUT_REVERSE) {
			led->pwm_duty_cycles->duty_ms = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts << 1);
			led->pwm_pause_lo = on_ms %
				(led->pwm_duty_cycles->num_duty_pcts << 1);
		} else {
			led->pwm_duty_cycles->duty_ms = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts);
			led->pwm_pause_lo = on_ms %
				(led->pwm_duty_cycles->num_duty_pcts);
		}
		led->pwm_pause_hi = total_ms - on_ms;
		dev_dbg(led->cdev.dev, "duty_ms %d, pause_hi %d, pause_lo %d, total_ms %d, on_ms %d\n",
				led->pwm_duty_cycles->duty_ms, led->pwm_pause_hi, led->pwm_pause_lo,
				total_ms, on_ms);
	}

	pwm_max = pm8xxx_adjust_brightness(&led->cdev, led->cdev.brightness);
	start_idx = led->pwm_duty_cycles->start_idx;
	idx_len = led->pwm_duty_cycles->num_duty_pcts;
	pcts = led->pwm_duty_cycles->duty_pcts;

	if (led->blink) {
		int mid = (idx_len - 1) >> 1;
		for (i = 0; i <= mid; i++) {
			temp = ((pwm_max * i) << 1) / mid + 1;
			pcts[i] = temp >> 1;
			pcts[idx_len - 1 - i] = temp >> 1;
		}
	} else {
		for (i = 0; i < idx_len; i++) {
			pcts[i] = pwm_max;
		}
	}

	if (idx_len >= PM_PWM_LUT_SIZE && start_idx) {
		pr_err("Wrong LUT size or index\n");
		return -EINVAL;
	}
	if ((start_idx + idx_len) > PM_PWM_LUT_SIZE) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}

	flags = PM8XXX_LED_PWM_FLAGS;
	switch (led->max_current) {
	case PM8XXX_PWM_CURRENT_4MA:
		flags |= PM_PWM_BANK_LO;
		break;
	case PM8XXX_PWM_CURRENT_8MA:
		flags |= PM_PWM_BANK_HI;
		break;
	case PM8XXX_PWM_CURRENT_12MA:
		flags |= (PM_PWM_BANK_LO | PM_PWM_BANK_HI);
		break;
	default:
		flags |= (PM_PWM_BANK_LO | PM_PWM_BANK_HI);
		break;
	}

	rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
			led->pwm_duty_cycles->duty_pcts,
			led->pwm_duty_cycles->duty_ms,
			start_idx, idx_len, led->pwm_pause_lo, led->pwm_pause_hi,
			flags);

	return rc;
}

#ifdef ORG_VER
static int pm8xxx_led_pwm_work(struct pm8xxx_led_data *led)
{
	int duty_us;
	int rc = 0;
	int level = 0;

	level = pm8xxx_adjust_brightness(&led->cdev, led->cdev.brightness);

	if (led->pwm_duty_cycles == NULL) {
		duty_us = (led->pwm_period_us * level) / LED_FULL;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
		if (led->cdev.brightness) {
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
			rc = pwm_enable(led->pwm_dev);
		} else {
			pwm_disable(led->pwm_dev);
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
		}
	} else {
		if (level) {
			pm8xxx_led_pwm_pattern_update(led);
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, level);
		}

		rc = pm8xxx_pwm_lut_enable(led->pwm_dev, level);
		if (!level)
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, level);
	}

	return rc;
}
#else
static int pm8xxx_led_pwm_work(struct pm8xxx_led_data *led)
{
	int duty_us;
	int rc = 0;
	int level = 0;
	
	level = pm8xxx_adjust_brightness(&led->cdev, led->cdev.brightness);

	if (led->pwm_duty_cycles == NULL) {
		duty_us = (led->pwm_period_us * led->cdev.brightness) /
								LED_FULL;
		//duty_us = (led->pwm_period_us * level) / LED_FULL;
		switch (led->id) {
		case PM8XXX_ID_RGB_LED_RED:
			if(led->cdev.brightness == 0 && redledflag == 0){
				return rc;
			}
			else if(led->cdev.brightness == 0 && redledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				redledflag = 0;
			}
			else if(led->cdev.brightness != 0 && redledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				rc = pwm_enable(led->pwm_dev);
			}
			else {
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				rc = pwm_enable(led->pwm_dev);
				redledflag = 1;
			}
			break;
		case PM8XXX_ID_RGB_LED_GREEN:
			if(led->cdev.brightness == 0 && greenledflag == 0){
				return rc;
			}
			else if(led->cdev.brightness == 0 && greenledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				greenledflag = 0;
			}
			else if(led->cdev.brightness != 0 && greenledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				rc = pwm_enable(led->pwm_dev);
			}
			else {
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				rc = pwm_enable(led->pwm_dev);
				greenledflag = 1;
			}
			break;
		case PM8XXX_ID_RGB_LED_BLUE:
			if(led->cdev.brightness == 0 && blueledflag == 0){
				return rc;
			}
			else if(led->cdev.brightness == 0 && blueledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				blueledflag = 0;
				return rc;
			}
			else if(led->cdev.brightness != 0 && blueledflag == 1){
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				pwm_disable(led->pwm_dev);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, 0);
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				rc = pwm_enable(led->pwm_dev);
			}
			else {
				rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
				led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
				rc = pwm_enable(led->pwm_dev);
				blueledflag = 1;
				return rc;
			}
			break;
		default:
			pr_err("led id %d is not supported\n", led->id);
			break;
		}
		
	} else {
		if (level) {
			pm8xxx_led_pwm_pattern_update(led);
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, level);
		}

		rc = pm8xxx_pwm_lut_enable(led->pwm_dev, level);
		if (!level)
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, level);
	}
	return rc;
}
#endif

static void __pm8xxx_led_work(struct pm8xxx_led_data *led,
					enum led_brightness value)
{
	int rc;
	int level = 0;

	mutex_lock(&led->lock);

	level = pm8xxx_adjust_brightness(&led->cdev, value);

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		led_kp_set(led, level);
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led_lc_set(led, level);
		break;
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led_flash_set(led, level);
		break;
	case PM8XXX_ID_WLED:
		rc = led_wled_set(led, level);
		if (rc < 0)
			pr_err("wled brightness set failed %d\n", rc);
		break;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		led_rgb_set(led, level);
		break;
	default:
		dev_err(led->cdev.dev, "unknown led id %d", led->id);
		break;
	}

	mutex_unlock(&led->lock);
}

static void pm8xxx_led_work(struct work_struct *work)
{
	int rc;

	struct pm8xxx_led_data *led = container_of(work,
					 struct pm8xxx_led_data, work);

	dev_dbg(led->cdev.dev, "led %s set %d (%s mode)\n",
			led->cdev.name, led->cdev.brightness,
			(led->pwm_dev ? "pwm" : "manual"));

	if (led->pwm_dev == NULL) {
		__pm8xxx_led_work(led, led->cdev.brightness);
	} else {
		rc = pm8xxx_led_pwm_work(led);
		if (rc)
			pr_err("could not configure PWM mode for LED:%d\n",
								led->id);
	}
}

static void pm8xxx_mode_work(struct work_struct *work)
{
	int rc;

	struct pm8xxx_led_data *led = container_of(work,
					 struct pm8xxx_led_data, modework);
	
	if (led->pwm_dev == NULL) {
		__pm8xxx_led_work(led, led->cdev.brightness);
	} else {
		rc = pm8xxx_led_pwm_mode_work(led);
		if (rc)
			pr_err("could not configure PWM mode for LED:%d\n",
								led->id);
	}
}

static enum led_op_mode pm8xxx_ledmode_get(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.ccimode;
}

static void pm8xxx_test_work(struct work_struct *work)
{
	int rc;

	struct pm8xxx_led_data *led = container_of(work,
					 struct pm8xxx_led_data, testwork);
	
	if (led->pwm_dev == NULL) {
		__pm8xxx_led_work(led, led->cdev.ccitest_brightness);
	} else {
		rc = pm8xxx_led_pwm_test_work(led);
		if (rc)
			pr_err("could not configure PWM mode for LED:%d\n",
								led->id);
	}
}

static void pm8xxx_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value exceeds");
		return;
	}

	if (!led->lock_update) {
		schedule_work(&led->work);
	} else {
		dev_dbg(led->cdev.dev, "set %d pending\n",
				value);
	}
}

static int pm8xxx_set_led_mode_and_adjust_brightness(struct pm8xxx_led_data *led,
		enum pm8xxx_led_modes led_mode, int max_current)
{
	switch (led->id) {
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led->adjust_brightness = max_current /
			PM8XXX_ID_LED_CURRENT_FACTOR;
		if (led->adjust_brightness > MAX_LC_LED_BRIGHTNESS)
			led->adjust_brightness = MAX_LC_LED_BRIGHTNESS;
		led->reg = led_mode;
		break;
	case PM8XXX_ID_LED_KB_LIGHT:
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led->adjust_brightness = max_current /
						PM8XXX_ID_FLASH_CURRENT_FACTOR;
		if (led->adjust_brightness > MAX_FLASH_BRIGHTNESS)
			led->adjust_brightness = MAX_FLASH_BRIGHTNESS;

		switch (led_mode) {
		case PM8XXX_LED_MODE_PWM1:
		case PM8XXX_LED_MODE_PWM2:
		case PM8XXX_LED_MODE_PWM3:
			led->reg = PM8XXX_FLASH_MODE_PWM;
			break;
		case PM8XXX_LED_MODE_DTEST1:
			led->reg = PM8XXX_FLASH_MODE_DBUS1;
			break;
		case PM8XXX_LED_MODE_DTEST2:
			led->reg = PM8XXX_FLASH_MODE_DBUS2;
			break;
		default:
			led->reg = PM8XXX_LED_MODE_MANUAL;
			break;
		}
		break;
	case PM8XXX_ID_WLED:
		led->adjust_brightness = WLED_MAX_LEVEL;
		break;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		break;
	default:
		dev_err(led->cdev.dev, "LED Id is invalid");
		return -EINVAL;
	}

	return 0;
}

static enum led_brightness pm8xxx_led_get(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.brightness;
}

static int __devinit init_wled(struct pm8xxx_led_data *led)
{
	int rc, i;
	u8 val, num_wled_strings, comp;
	u16 temp;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program over voltage protection threshold */
	if (led->wled_cfg->ovp_val > WLED_OVP_27V) {
		dev_err(led->dev->parent, "Invalid ovp value");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_OVP_CFG_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_OVP_VAL_MASK) |
		(led->wled_cfg->ovp_val << WLED_OVP_VAL_BIT_SHFT);

	rc = pm8xxx_writeb(led->dev->parent, WLED_OVP_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}

	/* program current boost limit and output feedback*/
	if (led->wled_cfg->boost_curr_lim > WLED_CURR_LIMIT_1680mA) {
		dev_err(led->dev->parent, "Invalid boost current limit");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_BOOST_CFG_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled boost config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_BOOST_LIMIT_MASK) |
		(led->wled_cfg->boost_curr_lim << WLED_BOOST_LIMIT_BIT_SHFT);

	val = (val & ~WLED_OP_FDBCK_MASK) |
		(led->wled_cfg->op_fdbck << WLED_OP_FDBCK_BIT_SHFT);

	rc = pm8xxx_writeb(led->dev->parent, WLED_BOOST_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled boost config"
			" register rc=%d\n", rc);
		return rc;
	}

	if (led->wled_cfg->comp_res_val) {
		/* Add Validation for compensation resistor value */
		if (!(led->wled_cfg->comp_res_val >=
			WLED_RESISTOR_COMPENSATION_DEFAULT &&
			led->wled_cfg->comp_res_val <=
				WLED_RESISTOR_COMPENSATION_MAX &&
			led->wled_cfg->comp_res_val %
				WLED_RESISTOR_COMPENSATION_DEFAULT == 0)) {
			dev_err(led->dev->parent, "Invalid Value " \
			"for compensation register.\n");
			return -EINVAL;
		}

		/* Compute the compensation resistor register value */
		temp = led->wled_cfg->comp_res_val;
		temp = (temp - WLED_RESISTOR_COMPENSATION_DEFAULT) /
					WLED_RESISTOR_COMPENSATION_DEFAULT;
		comp = (temp << WLED_4_BIT_SHFT);

		rc = pm8xxx_readb(led->dev->parent,
				WLED_RESISTOR_COMPENSATION_CFG_REG, &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled " \
			"resistor compensation config register rc=%d\n", rc);
			return rc;
		}

		val = val && WLED_4_BIT_MASK;
		val = val | comp;

		/* program compenstation resistor register */
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_RESISTOR_COMPENSATION_CFG_REG, val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled " \
			"resistor compensation config register rc=%d\n", rc);
			return rc;
		}
	}

	/* program high pole capacitance */
	if (led->wled_cfg->cp_select > WLED_CP_SELECT_MAX) {
		dev_err(led->dev->parent, "Invalid pole capacitance");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_HIGH_POLE_CAP_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled high pole"
			" capacitance register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_CP_SELECT_MASK) | led->wled_cfg->cp_select;

	rc = pm8xxx_writeb(led->dev->parent, WLED_HIGH_POLE_CAP_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled high pole"
			" capacitance register rc=%d\n", rc);
		return rc;
	}

	/* program activation delay and maximum current */
	for (i = 0; i < num_wled_strings; i++) {
		rc = pm8xxx_readb(led->dev->parent,
				WLED_MAX_CURR_CFG_REG(i), &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled max current"
				" config register rc=%d\n", rc);
			return rc;
		}

		if ((led->wled_cfg->ctrl_delay_us % WLED_CTL_DLY_STEP) ||
			(led->wled_cfg->ctrl_delay_us > WLED_CTL_DLY_MAX)) {
			dev_err(led->dev->parent, "Invalid control delay\n");
			return rc;
		}

		val = val / WLED_CTL_DLY_STEP;
		val = (val & ~WLED_CTL_DLY_MASK) |
			(led->wled_cfg->ctrl_delay_us << WLED_CTL_DLY_BIT_SHFT);

		if ((led->max_current > WLED_MAX_CURR)) {
			dev_err(led->dev->parent, "Invalid max current\n");
			return -EINVAL;
		}

		val = (val & ~WLED_MAX_CURR_MASK) | led->max_current;

		rc = pm8xxx_writeb(led->dev->parent,
				WLED_MAX_CURR_CFG_REG(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled max current"
				" config register rc=%d\n", rc);
			return rc;
		}
	}

	if (led->wled_cfg->cabc_en) {
		rc = pm8xxx_readb(led->dev->parent, WLED_SYNC_REG, &val);
		if (rc) {
			dev_err(led->dev->parent,
				"can't read cabc register rc=%d\n", rc);
			return rc;
		}

		switch (num_wled_strings) {
		case ONE_WLED_STRING:
			val |= (WLED_CABC_ONE_STRING << WLED_CABC_SHIFT);
			break;
		case TWO_WLED_STRINGS:
			val |= (WLED_CABC_TWO_STRING << WLED_CABC_SHIFT);
			break;
		case THREE_WLED_STRINGS:
			val |= (WLED_CABC_THREE_STRING << WLED_CABC_SHIFT);
			break;
		default:
			break;
		}

		rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
		if (rc) {
			dev_err(led->dev->parent,
				"can't write to enable cabc rc=%d\n", rc);
			return rc;
		}
	}

	/* program digital module generator, cs out and enable the module */
	rc = pm8xxx_readb(led->dev->parent, WLED_MOD_CTRL_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled module ctrl"
			" register rc=%d\n", rc);
		return rc;
	}

	if (led->wled_cfg->dig_mod_gen_en)
		val |= WLED_DIG_MOD_GEN_MASK;

	if (led->wled_cfg->cs_out_en)
		val |= WLED_CS_OUT_MASK;

	val |= WLED_EN_MASK;

	rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled module ctrl"
			" register rc=%d\n", rc);
		return rc;
	}
	led->wled_mod_ctrl_val = val;

	/* dump wled registers */
	wled_dump_regs(led);

	return 0;
}

static int __devinit get_init_value(struct pm8xxx_led_data *led, u8 *val)
{
	int rc, offset;
	u16 addr;

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		addr = SSBI_REG_ADDR_DRV_KEYPAD;
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		offset = PM8XXX_LED_OFFSET(led->id);
		addr = SSBI_REG_ADDR_LED_CTRL(offset);
		break;
	case PM8XXX_ID_FLASH_LED_0:
		addr = SSBI_REG_ADDR_FLASH_DRV0;
		break;
	case PM8XXX_ID_FLASH_LED_1:
		addr = SSBI_REG_ADDR_FLASH_DRV1;
		break;
	case PM8XXX_ID_WLED:
		rc = init_wled(led);
		if (rc)
			dev_err(led->cdev.dev, "can't initialize wled rc=%d\n",
								rc);
		return rc;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		addr = SSBI_REG_ADDR_RGB_CNTL1;
		break;
	default:
		dev_err(led->cdev.dev, "unknown led id %d", led->id);
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, addr, val);
	if (rc)
		dev_err(led->cdev.dev, "can't get led(%d) level rc=%d\n",
							led->id, rc);

	return rc;
}

static int pm8xxx_led_pwm_configure(struct pm8xxx_led_data *led)
{
	int duty_us, rc;

	led->pwm_dev = pwm_request(led->pwm_channel,
					led->cdev.name);

	if (IS_ERR_OR_NULL(led->pwm_dev)) {
		pr_err("could not acquire PWM Channel %d, "
			"error %ld\n", led->pwm_channel,
			PTR_ERR(led->pwm_dev));
		led->pwm_dev = NULL;
		return -ENODEV;
	}

	if (led->pwm_duty_cycles != NULL) {
		rc = pm8xxx_led_pwm_pattern_update(led);
	} else {
		duty_us = led->pwm_period_us;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
	}

	return rc;
}

static ssize_t pm8xxx_led_lock_update_show(struct device *dev,
		                struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i, n = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		n += sprintf(&buf[n], "%s is %s\n",
					leds[i].cdev.name,
					(leds[i].lock_update ? "non-updatable" : "updatable"));
	}

	return n;
}

static ssize_t pm8xxx_led_lock_update_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;
		for (i = 0; i < pdata->num_configs; i++)
		{
			leds[i].lock_update = state;
			if (!state) {
				dev_info(dev, "resume %s set %d\n",
						leds[i].cdev.name, leds[i].cdev.brightness);
				schedule_work(&leds[i].work);
			}
		}
	}
	return rc;
}

static DEVICE_ATTR(lock, 0644, pm8xxx_led_lock_update_show, pm8xxx_led_lock_update_store);

static ssize_t pm8xxx_led_grppwm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i,  n = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		n += sprintf(&buf[n], "%s period_us is %d\n", leds[i].cdev.name, leds[i].pwm_grppwm);
	}
	return n;
}

static ssize_t pm8xxx_led_grppwm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;
		if (state < 0)
			state = 0;
		if (state > PM8XXX_LED_PWM_GRPPWM_MAX)
			state = PM8XXX_LED_PWM_GRPPWM_MAX;

		for (i = 0; i < pdata->num_configs; i++)
		{
			leds[i].pwm_grppwm = state;
			dev_dbg(leds[i].cdev.dev, "set grppwm %lu\n", state);
		}
	}
	return rc;
}

static DEVICE_ATTR(grppwm, 0644, pm8xxx_led_grppwm_show, pm8xxx_led_grppwm_store);

static ssize_t pm8xxx_led_grpfreq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i,  n = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		n += sprintf(&buf[n], "%s freq %d\n", leds[i].cdev.name, leds[i].pwm_grpfreq);
	}
	return n;
}

static ssize_t pm8xxx_led_grpfreq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;

		if(state < 0)
			state = 0;
		if(state > PM8XXX_LED_PWM_GRPFREQ_MAX)
			state = PM8XXX_LED_PWM_GRPFREQ_MAX;

		for (i = 0; i < pdata->num_configs; i++)
		{
			leds[i].pwm_grpfreq = state;
			dev_dbg(leds[i].cdev.dev, "set grpfreq %lu\n", state);
		}
	}
	return rc;
}

static DEVICE_ATTR(grpfreq, 0644, pm8xxx_led_grpfreq_show, pm8xxx_led_grpfreq_store);

static ssize_t pm8xxx_led_blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i, blink = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		if (leds[i].blink)
			blink |= 1 << i;
	}
	return sprintf(buf, "%d", blink);
}

static ssize_t pm8xxx_led_blink_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;

		for (i = 0; i < pdata->num_configs; i++)
		{
			if (leds[i].blink != state) {
				leds[i].blink = state;
				if(!leds[i].lock_update)
					rc = pm8xxx_led_pwm_pattern_update(&leds[i]);
			}
		}
	}
	return rc;
}

static DEVICE_ATTR(blink, 0644, pm8xxx_led_blink_show, pm8xxx_led_blink_store);


static int __devinit pm8xxx_led_probe(struct platform_device *pdev)
{
	const struct pm8xxx_led_platform_data *pdata = pdev->dev.platform_data;
	const struct led_platform_data *pcore_data;
	struct led_info *curr_led;
	struct pm8xxx_led_data *led, *led_dat;
	struct pm8xxx_led_config *led_cfg;
	enum pm8xxx_version version;
	bool found = false;
	int rc, i, j;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	pcore_data = pdata->led_core;

	if (pcore_data->num_leds != pdata->num_configs) {
		dev_err(&pdev->dev, "#no. of led configs and #no. of led"
				"entries are not equal\n");
		return -EINVAL;
	}

	led = kcalloc(pcore_data->num_leds, sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < pcore_data->num_leds; i++) {
		curr_led	= &pcore_data->leds[i];
		led_dat		= &led[i];
		led_cfg		= &pdata->configs[i];

		led_dat->id     = led_cfg->id;
		led_dat->pwm_channel = led_cfg->pwm_channel;
		led_dat->pwm_period_us = led_cfg->pwm_period_us;
		led_dat->pwm_duty_cycles = led_cfg->pwm_duty_cycles;
		led_dat->wled_cfg = led_cfg->wled_cfg;
		led_dat->max_current = led_cfg->max_current;
		led_dat->lock_update = 0;
		led_dat->blink = 0;

		if (!((led_dat->id >= PM8XXX_ID_LED_KB_LIGHT) &&
				(led_dat->id < PM8XXX_ID_MAX))) {
			dev_err(&pdev->dev, "invalid LED ID(%d) specified\n",
				led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;

		}

		found = false;
		version = pm8xxx_get_version(pdev->dev.parent);
		for (j = 0; j < ARRAY_SIZE(led_map); j++) {
			if (version == led_map[j].version
			&& (led_map[j].supported & (1 << led_dat->id))) {
				found = true;
				break;
			}
		}

		if (!found) {
			dev_err(&pdev->dev, "invalid LED ID(%d) specified\n",
				led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;
		}

		led_dat->cdev.name		= curr_led->name;
		led_dat->cdev.default_trigger   = curr_led->default_trigger;
		led_dat->cdev.brightness_set    = pm8xxx_led_set;
		led_dat->cdev.brightness_get    = pm8xxx_led_get;
		led_dat->cdev.max_brightness    = LED_FULL;
		led_dat->cdev.brightness	= LED_OFF;
		led_dat->cdev.flags		= curr_led->flags;
		led_dat->dev			= &pdev->dev;
		led_dat->cdev.mode_set          = pm8xxx_led_modeset;
		led_dat->cdev.blinkonms_set     = pm8xxx_led_onmsset;
		led_dat->cdev.blinkoffms_set    = pm8xxx_led_offmsset;
		led_dat->cdev.onms_get    = pm8xxx_led_onmsget;
		led_dat->cdev.offms_get   = pm8xxx_led_offmsget;
		led_dat->cdev.mode_get          = pm8xxx_ledmode_get;
		led_dat->cdev.testbrightness_set = pm8xxx_led_brightnesstest;
		led_dat->cdev.tunebrightness_set = pm8xxx_led_tunebrightset;
		led_dat->cdev.tunebrightness_get = pm8xxx_led_tunebrightget;
		led_dat->cdev.batpa_set = pm8xxx_led_batpaset;
		led_dat->cdev.batpa_get = pm8xxx_led_batpaget;

		rc =  get_init_value(led_dat, &led_dat->reg);
		if (rc < 0)
			goto fail_id_check;

		rc = pm8xxx_set_led_mode_and_adjust_brightness(led_dat,
					led_cfg->mode, led_cfg->max_current);
		if (rc < 0)
			goto fail_id_check;

		mutex_init(&led_dat->lock);
		INIT_WORK(&led_dat->work, pm8xxx_led_work);
		INIT_WORK(&led_dat->modework, pm8xxx_mode_work);
		INIT_WORK(&led_dat->testwork, pm8xxx_test_work);

		rc = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led %d,rc=%d\n",
						 led_dat->id, rc);
			goto fail_id_check;
		}

		/* configure default state */
		if (led_cfg->default_state) {//Taylor--20120907-->B
			if (led_cfg->mode == PM8XXX_LED_MODE_MANUAL) {
				led_dat->cdev.brightness = led_dat->cdev.max_brightness/3;
			}
			else
				led_dat->cdev.brightness = led_dat->cdev.max_brightness;

		}//<--E
		else
			led_dat->cdev.brightness = LED_OFF;

		if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL) {
			if (led_dat->id == PM8XXX_ID_RGB_LED_RED ||
				led_dat->id == PM8XXX_ID_RGB_LED_GREEN ||
				led_dat->id == PM8XXX_ID_RGB_LED_BLUE)
				__pm8xxx_led_work(led_dat, 0);
			else
				__pm8xxx_led_work(led_dat,
					led_dat->cdev.max_brightness);

			if (led_dat->pwm_channel != -1) {
				if (led_cfg->pwm_adjust_brightness) {
					led_dat->adjust_brightness = led_cfg->pwm_adjust_brightness;
				} else {
					led_dat->adjust_brightness = 100;
				}

				rc = pm8xxx_led_pwm_configure(led_dat);
				if (rc) {
					dev_err(&pdev->dev, "failed to "
					"configure LED, error: %d\n", rc);
					goto fail_id_check;
				}
				schedule_work(&led->work);
			}
		} else {
			__pm8xxx_led_work(led_dat, led_dat->cdev.brightness);
		}
	}

	led->use_pwm = pdata->use_pwm;

	platform_set_drvdata(pdev, led);

	rc = device_create_file(&pdev->dev, &dev_attr_lock);
	if (rc) {
		device_remove_file(&pdev->dev, &dev_attr_lock);
		dev_err(&pdev->dev, "failed device_create_file(lock)\n");
	}

	if (led->use_pwm) {
		rc = device_create_file(&pdev->dev, &dev_attr_blink);
		if (rc) {
			device_remove_file(&pdev->dev, &dev_attr_blink);
			dev_err(&pdev->dev, "failed device_create_file(blink)\n");
		}

		rc = device_create_file(&pdev->dev, &dev_attr_grppwm);
		if (rc) {
			device_remove_file(&pdev->dev, &dev_attr_grppwm);
			dev_err(&pdev->dev, "failed device_create_fiaild(grppwm)\n");
		}

		rc = device_create_file(&pdev->dev, &dev_attr_grpfreq);
		if (rc) {
			device_remove_file(&pdev->dev, &dev_attr_grpfreq);
			dev_err(&pdev->dev, "failed device_create_file(grpfreq)\n");
		}
	}

	return 0;

fail_id_check:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			mutex_destroy(&led[i].lock);
			led_classdev_unregister(&led[i].cdev);
			if (led[i].pwm_dev != NULL)
				pwm_free(led[i].pwm_dev);
		}
	}
	kfree(led);
	return rc;
}

static int __devexit pm8xxx_led_remove(struct platform_device *pdev)
{
	int i;
	const struct led_platform_data *pdata =
				pdev->dev.platform_data;
	struct pm8xxx_led_data *led = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		cancel_work_sync(&led[i].testwork);
		cancel_work_sync(&led[i].modework);
		cancel_work_sync(&led[i].work);
		mutex_destroy(&led[i].lock);
		led_classdev_unregister(&led[i].cdev);
		if (led[i].pwm_dev != NULL) {
			pwm_free(led[i].pwm_dev);
		}
	}

	device_remove_file(&pdev->dev, &dev_attr_lock);

	if (led->use_pwm) {
		device_remove_file(&pdev->dev, &dev_attr_blink);
		device_remove_file(&pdev->dev, &dev_attr_grppwm);
		device_remove_file(&pdev->dev, &dev_attr_grpfreq);
	}

	kfree(led);

	return 0;
}

static struct platform_driver pm8xxx_led_driver = {
	.probe		= pm8xxx_led_probe,
	.remove		= __devexit_p(pm8xxx_led_remove),
	.driver		= {
		.name	= PM8XXX_LEDS_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_led_init(void)
{
	return platform_driver_register(&pm8xxx_led_driver);
}
subsys_initcall(pm8xxx_led_init);

static void __exit pm8xxx_led_exit(void)
{
	platform_driver_unregister(&pm8xxx_led_driver);
}
module_exit(pm8xxx_led_exit);

MODULE_DESCRIPTION("PM8XXX LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8xxx-led");
