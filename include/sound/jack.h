#ifndef __SOUND_JACK_H
#define __SOUND_JACK_H

/*
 *  Jack abstraction layer
 *
 *  Copyright 2008 Wolfson Microelectronics plc
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <sound/core.h>
#include <linux/switch.h>  // BAM_S C 130530 [Mig:]

struct input_dev;

/**
 * Jack types which can be reported.  These values are used as a
 * bitmask.
 *
 * Note that this must be kept in sync with the lookup table in
 * sound/core/jack.c.
 */
enum snd_jack_types {
	SND_JACK_HEADPHONE	= 0x0000001,
	SND_JACK_MICROPHONE	= 0x0000002,
	SND_JACK_HEADSET	= SND_JACK_HEADPHONE | SND_JACK_MICROPHONE,
	SND_JACK_LINEOUT	= 0x0000004,
	SND_JACK_MECHANICAL	= 0x0000008, /* If detected separately */
	SND_JACK_VIDEOOUT	= 0x0000010,
	SND_JACK_AVOUT		= SND_JACK_LINEOUT | SND_JACK_VIDEOOUT,
	/* */
	SND_JACK_LINEIN		= 0x0000020,
	SND_JACK_OC_HPHL	= 0x0000040,
	SND_JACK_OC_HPHR	= 0x0000080,
	SND_JACK_UNSUPPORTED	= 0x0000100,
	/* Kept separate from switches to facilitate implementation */
	SND_JACK_BTN_0		= 0x4000000,
	SND_JACK_BTN_1		= 0x2000000,
	SND_JACK_BTN_2		= 0x1000000,
	SND_JACK_BTN_3		= 0x0800000,
	SND_JACK_BTN_4		= 0x0400000,
	SND_JACK_BTN_5		= 0x0200000,
	SND_JACK_BTN_6		= 0x0100000,
	SND_JACK_BTN_7		= 0x0080000,
};

// BAM_S C 130530 [Mig:]
enum hs_sw_dev_state {
	NO_DEVICE = 0,
	DEVICE_HEADSET,
	DEVICE_HEADPHONE,
	DEVICE_UNSUPPORTED = 0xFE00,
	DEVICE_UNKNOWN = 0xFF00,
};
// BAM_E C 130530

struct snd_jack {
	struct input_dev *input_dev;
// BAM_S C 130530 [Mig:]
	struct input_dev *indev_appkey;
	struct switch_dev swdev;
// BAM_E C 130530
	int registered;
	int type;
	const char *id;
	char name[100];
	unsigned int key[8];   /* Keep in sync with definitions above */
	void *private_data;
	void (*private_free)(struct snd_jack *);
};

#ifdef CONFIG_SND_JACK

int snd_jack_new(struct snd_card *card, const char *id, int type,
		 struct snd_jack **jack);
void snd_jack_set_parent(struct snd_jack *jack, struct device *parent);
int snd_jack_set_key(struct snd_jack *jack, enum snd_jack_types type,
		     int keytype);

void snd_jack_report(struct snd_jack *jack, int status);

#else

static inline int snd_jack_new(struct snd_card *card, const char *id, int type,
			       struct snd_jack **jack)
{
	return 0;
}

static inline void snd_jack_set_parent(struct snd_jack *jack,
				       struct device *parent)
{
}

static inline void snd_jack_report(struct snd_jack *jack, int status)
{
}

#endif

#endif
