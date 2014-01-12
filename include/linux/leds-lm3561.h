/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 */
#ifndef __LEDS_LM3561_H__
#define __LEDS_LM3561_H__
#include <linux/leds.h>
#include <linux/i2c.h>
//B 2012/08/06
int led_i2c_write(struct i2c_client *client, u8 cReg, u8 data);
u8 led_i2c_read(struct i2c_client *client, u8 cReg);
int lm3561_set_power(bool on);
void lm3561_reset_set(struct led_classdev *led_cdev, int reset);
void enable_lm3561(void);
void disable_lm3561(void);
//E 2012/08/06
struct led_classdev* lm3561_init(void);
void lm3561_release(struct led_classdev *led_cdev);//2012/08/20
void lm3561_torch_set(struct led_classdev *led_cdev, enum led_brightness brightness);
void lm3561_flash_set(struct led_classdev *led_cdev, enum led_brightness brightness);
#endif
