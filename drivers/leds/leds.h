/*
 * LED Core
 *
 * Copyright 2005 Openedhand Ltd.
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LEDS_H_INCLUDED
#define __LEDS_H_INCLUDED

#include <linux/device.h>
#include <linux/rwsem.h>
#include <linux/leds.h>

static inline void led_set_testbrightness(struct led_classdev *led_cdev,
					enum led_testbrightness testvalue)
{
	led_cdev->ccitest_brightness = testvalue;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->testbrightness_set(led_cdev, testvalue);
}

static inline void led_set_ccimode(struct led_classdev *led_cdev,
					enum led_op_mode modevalue)
{
	led_cdev->ccimode = modevalue;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->mode_set(led_cdev, modevalue);
}

static inline void led_set_onms(struct led_classdev *led_cdev,
					enum led_op_onms blinkonms)
{
	led_cdev->onMS= blinkonms;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->blinkonms_set(led_cdev, blinkonms);
}
static inline void led_set_offms(struct led_classdev *led_cdev,
					enum led_op_onms blinkoffms)
{
	led_cdev->offMS= blinkoffms;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->blinkoffms_set(led_cdev, blinkoffms);
}

static inline void led_set_tunebrightness(struct led_classdev *led_cdev,
					enum led_tunebrightness tunevalue)
{
	led_cdev->tunebrightness = tunevalue;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->tunebrightness_set(led_cdev, tunevalue);
}

static inline void led_set_batterypara(struct led_classdev *led_cdev,
					enum led_batpa batteryparameter)
{
	led_cdev->batpa = batteryparameter;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->batpa_set(led_cdev, batteryparameter);
}


static inline void led_set_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	if (value > led_cdev->max_brightness)
		value = led_cdev->max_brightness;
	led_cdev->brightness = value;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->brightness_set(led_cdev, value);
}

static inline int led_get_brightness(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

#ifdef CONFIG_LEDS_TRIGGERS
void led_trigger_set_default(struct led_classdev *led_cdev);
void led_trigger_set(struct led_classdev *led_cdev,
			struct led_trigger *trigger);
void led_trigger_remove(struct led_classdev *led_cdev);

static inline void *led_get_trigger_data(struct led_classdev *led_cdev)
{
	return led_cdev->trigger_data;
}

#else
#define led_trigger_set_default(x) do {} while (0)
#define led_trigger_set(x, y) do {} while (0)
#define led_trigger_remove(x) do {} while (0)
#define led_get_trigger_data(x) (NULL)
#endif

ssize_t led_trigger_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);
ssize_t led_trigger_show(struct device *dev, struct device_attribute *attr,
			char *buf);

#endif	/* __LEDS_H_INCLUDED */
