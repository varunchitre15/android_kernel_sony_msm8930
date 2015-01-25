/*
* Copyright (C) 2012 Sony Mobile Communications AB.
*/

#ifndef __driver_stuff__
#define __driver_stuff__

#define SIGNATURE					0x22334455
#define CONFIG_WARMBOOT_NONE        0x00000000
#define CONFIG_WARMBOOT_FOTA        0x6f656d46
#define CONFIG_WARMBOOT_S1          0x6f656d53
#define CONFIG_WARMBOOT_CLEAR       0xabadbabe
#define CONFIG_WARMBOOT_FB          0x77665500
#define CONFIG_WARMBOOT_NORMAL     	0x77665501
#define CONFIG_WARMBOOT_CRASH       0xc0dedead

#define STARTUP_MASK_ONKEY    		0x00000001 
#define STARTUP_MASK_WATCHDOG 		0x00000002 
#define STARTUP_MASK_VBUS     		0x00000004 
#define STARTUP_MASK_SMPL        	0x00001000 
#define STARTUP_MASK_RTC         	0x00002000 
#define STARTUP_MASK_WALLCHARGER 	0x00004000 
#define STARTUP_MASK_HARD_RESET  	0x00008000 
#define STARTUP_MASK_POWERLOCK   	0x00010000 

#define STUFF_CRASH_DEFAULT			0xFBBFFBBF 

enum
{
	none = 1,
	fota,
	s1boot,
	clear,
	fastboot,
	normal,
	crash,
	kpwroffvaluemax,
};

enum
{
	onkey = 1,
	watchdoga,
	vbus,
	smpl,
	rtc,
	wallcharger,
	hard_reset,
	power_lock,
	kpmic_max,
};

typedef struct
{
	unsigned int	pwronindex;
	unsigned int	pwronrecord[0xA];
	unsigned int	pwronrecordcount[kpmic_max];
	unsigned int	pwroffindex;
	unsigned int	pwroffrecord[0xA];
	unsigned int	pwroffrecordcount[kpwroffvaluemax];
	unsigned int	PM_WDT;
	unsigned int	RPM_WDT;
	unsigned int	MSMRST_STAT;
	unsigned int	ACPU0_WDT;
	unsigned int	ACPU1_WDT;
	unsigned int	PM_PWRON;
	unsigned int	FIQ_ADDR;
	unsigned int	REST_REASON;
	unsigned int	CUR_DDRVALUE;
	unsigned int	CUR_IMEMVALUE;
}ccistuff_pwrrec;

typedef struct
{
	ccistuff_pwrrec szpwrd;
	unsigned char	reserved[0x200-sizeof(ccistuff_pwrrec)-sizeof(unsigned int)];
	unsigned int	SIGN;
}ccistuff_rsvrec;

extern unsigned int crashflag;
extern unsigned int unknownrebootflag;
extern unsigned int stopwatchdog;
#endif