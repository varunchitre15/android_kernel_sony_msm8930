/* 
 * Copyright (C) 2012 Sony Mobile Communications AB.
 */

#ifndef __RW_INFO_H__
#define __RW_INFO_H__

#include <linux/types.h>
#include <linux/ioctl.h>

#define DEBUG_INFO_IOCTL_ID 0xDA
#define DEBUG_INFO_IOCTL_NORMAL _IO(DEBUG_INFO_IOCTL_ID, 0)
#define DEBUG_INFO_IOCTL_ORIG(cmd) _IO(DEBUG_INFO_IOCTL_ID, cmd)

#define MAX_DEBUG_STR_LENGTH 500
#define MAX_DEBUG_INFO_TITLE_LENGTH 30

struct cci_debug_struct
{
	unsigned long debug_idx;
	unsigned long str_len;
	unsigned long row;
	char debug_str[MAX_DEBUG_STR_LENGTH];
};

#define DBGINFO_NO_SUCH_CMD 1
#define DBGINFO_CMD_OUTOFRANGE 2



#endif