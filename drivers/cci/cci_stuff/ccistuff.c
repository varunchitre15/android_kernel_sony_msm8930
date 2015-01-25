/*
* Copyright (C) 2012 Sony Mobile Communications AB.
*/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ptrace.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <asm/cputime.h>
#include <asm/io.h>
#include <mach/msm_iomap.h>
#include <linux/slab.h>
#include <linux/ccistuff.h>
#include <linux/delay.h>
#define CCISTUFF_DEV_ID "ccistuff"

/*******************************************************************************
* Local Variable/Structure Declaration
*******************************************************************************/
#define WDT0_EN        (MSM_TMR_BASE + 0x40)
#define CCISTUFF_VERSION		"1.0.0"
static unsigned int rebootflag = 0;
unsigned int crashflag = STUFF_CRASH_DEFAULT;
unsigned int unknownrebootflag = STUFF_CRASH_DEFAULT;
static unsigned int warmboot = 0;
static unsigned int startup = 0;
unsigned int stopwatchdog = 1;
#ifdef CONFIG_CCI_KLOG
static unsigned int *reboottype= (void*)MSM_KLOG_BASE + 0x3FFFF8;
static unsigned int *unknownreboot= (void*)MSM_KLOG_BASE + 0x3FFFF0;
#endif

static int __init get_warmboot(char* cmdline)
{
	if(cmdline == NULL || strlen(cmdline) == 0)
	{
	
	}
	else
	{
		warmboot = simple_strtoul(cmdline, NULL, 16);
	}

	return 0;
}
__setup("warmboot=", get_warmboot);

static int __init get_startup(char* cmdline)
{
	if(cmdline == NULL || strlen(cmdline) == 0)
	{

	}
	else
	{
		startup = simple_strtoul(cmdline, NULL, 16);
	}

	return 0;
}
__setup("startup=", get_startup);


static ssize_t ccistuff_show_rebootflag(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch(warmboot)
	{
		case CONFIG_WARMBOOT_CLEAR:
		case CONFIG_WARMBOOT_CRASH:
			rebootflag = 0xF;
			break;
		default:
			if((crashflag == CONFIG_WARMBOOT_CRASH) || (unknownrebootflag == CONFIG_WARMBOOT_CRASH))
				rebootflag = 0xF;
			else
				rebootflag = 0xA;
			break;
	}

	printk("%s():crashflag=0x%08x, warmboot=0x%08x, unknownflag=0x%08x\n", __func__, crashflag, warmboot, unknownrebootflag);
	
	return sprintf(buf, "0x%08x\n", rebootflag);
}

static long ccistuff_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}
#if 0
static ssize_t ccistuff_store_startup(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    char *endptr;	/* local pointer to end of parsed string */
    unsigned int ret = simple_strtoul(buf, &endptr, 0);
    startup = ret;
	printk("%s():val=0x%08X, startup=0x%08X,warmboot=0x%08X\n", __func__, ret, startup,warmboot);

	return count;
}
#endif
static ssize_t ccistuff_show_startup(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("%s, startup=0x%08x,warmboot=0x%08x\n", __func__, startup,warmboot);
	
	return sprintf(buf, "startup=0x%08x\n", startup);
}

static ssize_t ccistuff_show_warmboot(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("%s, startup=0x%08x,warmboot=0x%08x\n", __func__, startup,warmboot);

	return sprintf(buf, "warmboot=0x%08x\n", warmboot);
}
#if 0
static ssize_t ccistuff_store_warmboot(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    char *endptr;	/* local pointer to end of parsed string */
    unsigned int ret = simple_strtoul(buf, &endptr, 0);
    warmboot = ret;
	printk("%s():val=0x%08X, startup=0x%08X,warmboot=0x%08X\n", __func__, ret, startup,warmboot);

	return count;
}
#endif
static ssize_t ccistuff_show_crashflag(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int rc = 0;
	if(STUFF_CRASH_DEFAULT == crashflag)
	{
#ifdef CONFIG_CCI_KLOG	
		crashflag = *reboottype;
		*reboottype = 0;
#endif
	}

	if(STUFF_CRASH_DEFAULT == unknownrebootflag)
	{
#ifdef CONFIG_CCI_KLOG	
		unknownrebootflag = *unknownreboot;	
		*unknownreboot = CONFIG_WARMBOOT_CRASH;
#endif
	}
	rc = __raw_readl(WDT0_EN);
	#ifdef CONFIG_CCI_KLOG
	printk("%s():crashflag=0x%08x, unknownrebootflag=0x%08x, *unknownreboot = 0x%08x, rc = 0x%08x\n", __func__, crashflag,unknownrebootflag,*unknownreboot,rc);
	#endif
	return sprintf(buf, "0x%08x\n", crashflag);
}
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC	
static void hang_work(struct work_struct *work)  
{  
	msleep(20000); 
}  
static DECLARE_WORK(hangwork_struct, hang_work);  
#endif
static ssize_t ccistuff_show_test(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
	int print = 1;
	stopwatchdog = 0;	
	schedule_work(&hangwork_struct); 
	while(1)
	{
		if(print == 1)
		{
			printk("%s():sorry, this filenode is create for special purpose\n", __func__);
			printk("%s():trying to made FIQ case\n", __func__);
			printk("%s():crashflag=0x%08x, unknownrebootflag=0x%08x *unknownreboot=0x%08x\n", __func__, crashflag,unknownrebootflag,*unknownreboot);
			printk("%s():startup=0x%08X,warmboot=0x%08X\n", __func__, startup,warmboot);			
			print = 0;
		}
		msleep(20000); 
	}
#endif
	return sprintf(buf, "0x%08x\n", stopwatchdog);
}

static ssize_t ccistuff_show_unknownrebootflag(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%08x\n", unknownrebootflag);
}

DEVICE_ATTR(rebootflag, S_IRUSR | S_IRGRP | S_IROTH, ccistuff_show_rebootflag, NULL);
DEVICE_ATTR(crashflag, S_IRUSR | S_IRGRP | S_IROTH, ccistuff_show_crashflag, NULL);
DEVICE_ATTR(warmboot, S_IRUSR | S_IRGRP | S_IROTH , ccistuff_show_warmboot, NULL);
DEVICE_ATTR(startup, S_IRUSR | S_IRGRP | S_IROTH, ccistuff_show_startup, NULL);
DEVICE_ATTR(unknownrebootflag, S_IRUSR | S_IRGRP | S_IROTH, ccistuff_show_unknownrebootflag, NULL);
DEVICE_ATTR(test, S_IRUSR | S_IRGRP | S_IROTH, ccistuff_show_test, NULL);
static struct attribute *ccistuff_attrs[] =
{
	&dev_attr_rebootflag.attr,
	&dev_attr_warmboot.attr,	
	&dev_attr_startup.attr,	
	&dev_attr_crashflag.attr,	
	&dev_attr_unknownrebootflag.attr,	
	&dev_attr_test.attr,		
	NULL
};

static const struct attribute_group ccistuff_attr_group =
{
	.attrs	= ccistuff_attrs,
};

static const struct file_operations ccistuff_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= ccistuff_ioctl,
};

static struct miscdevice ccistuff_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= CCISTUFF_DEV_ID,
	.fops	= &ccistuff_fops,
};

int ccistuffmisc_init(void)
{
	int retval = 0;

//init miscdev
	retval = misc_register(&ccistuff_miscdev);
	if(retval)
	{
		printk("cannot register miscdev on minor=%d, err=0x%X\n", MISC_DYNAMIC_MINOR, retval);
		goto error_init_miscdev;
	}
	
//init sysfs
	retval = sysfs_create_group(&ccistuff_miscdev.this_device->kobj, &ccistuff_attr_group);
	if(retval < 0)
	{
		printk("init sysfs failed, err=0x%X\n", retval);
		goto error_init_sysfs;
	}

	return 0;

error_init_sysfs:
	sysfs_remove_group(&ccistuff_miscdev.this_device->kobj, &ccistuff_attr_group);

error_init_miscdev:
	misc_deregister(&ccistuff_miscdev);

	return retval;
}

void ccistuffmisc_exit(void)
{
	sysfs_remove_group(&ccistuff_miscdev.this_device->kobj, &ccistuff_attr_group);

	misc_deregister(&ccistuff_miscdev);
}

static int __init ccistuff_init(void)
{
	int retval = 0;

	retval = ccistuffmisc_init();

	return retval;
}

static void __exit ccistuff_exit(void)
{

	ccistuffmisc_exit();

	return;
}

module_init(ccistuff_init);
module_exit(ccistuff_exit);

MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION("stuff");
MODULE_VERSION(CCISTUFF_VERSION);

