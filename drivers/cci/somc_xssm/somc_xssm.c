/*
* Copyright (C) 2012 Sony Mobile Communications AB.
*/
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/slab.h>
#include <linux/suspend.h>

struct somc_xssm_info {
	int master_s;
	int suspend_s;
	int resume_s;
	//struct mutex lock;
	struct notifier_block pm_notifier;
	struct early_suspend early_suspend;
};

static struct somc_xssm_info sony_xssm;

static ssize_t master_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", sony_xssm.master_s);
}

static ssize_t master_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	sscanf(buf, "%du", &sony_xssm.master_s);
	return count;
}

static struct kobj_attribute master_attribute =
	__ATTR(enable, 0664, master_show, master_store);

static ssize_t secondary_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "set_request_next_suspend_prepare_notification") == 0)
		var = sony_xssm.suspend_s;
	else
		var = sony_xssm.resume_s;
	return sprintf(buf, "%d\n", var);
}

static ssize_t secondary_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int var;

	sscanf(buf, "%du", &var);
	if (strcmp(attr->attr.name, "set_request_next_suspend_prepare_notification") == 0)
		sony_xssm.suspend_s = var;
	else
		sony_xssm.resume_s = var;
	return count;
}

static struct kobj_attribute suspend_attribute =
	__ATTR(set_request_next_suspend_prepare_notification, 0664, secondary_show, secondary_store);
static struct kobj_attribute resume_attribute =
	__ATTR(set_late_resume_notifications, 0664, secondary_show, secondary_store);

static struct attribute *attrs[] = {
	&master_attribute.attr,
	&suspend_attribute.attr,
	&resume_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct platform_device *somc_xssm_dev;

#if 0
#ifdef CONFIG_PM
static int somc_xssm_suspend(struct platform_device *dev, pm_message_t state)
{
	//put some suspend stuff here.
	return 0;
}
static int somc_xssm_resume(struct platform_device *dev)
{
	//put some resume stuff here.
	return 0;
}
#endif
#endif
static int ssm_pm_notifier(struct notifier_block *nb, unsigned long event,
                void *ignored)
{
	char *buf;
	char *envp[3];
        pr_debug("%s: event=%lu\n", __func__, event);

        if (event == PM_SUSPEND_PREPARE) {
		if (sony_xssm.master_s){
                	if (sony_xssm.suspend_s) {
				buf = kmalloc (32, GFP_ATOMIC);
				if (!buf){
                                	printk(KERN_ERR "%s kmalloc failed\n", __func__);
                        	}
                        	envp[0] = "NAME=sony_ssm";
                        	snprintf(buf, 32, "EVENT=0");
                        	envp[1] = buf;
                        	envp[2] = NULL;
                        	kobject_uevent_env(&somc_xssm_dev->dev.kobj, KOBJ_CHANGE, envp);
                       		 kfree(buf);
                        	printk("[CCI/SOMC]-send request next suspend prepare_notification\n");
                        	sony_xssm.suspend_s = 0;
                        	return NOTIFY_BAD;
			}
                }
        }
        return NOTIFY_DONE;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
#if 0
static void somc_xssm_early_suspend(struct early_suspend *h)
{
	char *buf;
	char *envp[3];
	if (sony_xssm.master_s){
		if (sony_xssm.suspend_s){
			buf = kmalloc(32, GFP_ATOMIC);
			if (!buf){
				printk(KERN_ERR "%s kmalloc failed\n", __func__);
			}
			envp[0] = "NAME=sony_ssm";
			snprintf(buf, 32, "EVENT=0");
			envp[1] = buf;
			envp[2] = NULL;
			kobject_uevent_env(&somc_xssm_dev->dev.kobj, KOBJ_CHANGE, envp);
			kfree(buf);
			printk("[CCI/SOMC]-send request next suspend prepare_notification\n");
		}
	}
}
#endif
static void somc_xssm_late_resume(struct early_suspend *h)
{
	char *buf;
	char *envp[3];
	if (sony_xssm.master_s){
		if (sony_xssm.resume_s){
			buf = kmalloc(32, GFP_ATOMIC);
			if (!buf){
				printk(KERN_ERR "%s kmalloc failed\n", __func__);
			}
			envp[0] = "NAME=sony_ssm";
			snprintf(buf, 32, "EVENT=1");
			envp[1] = buf;
			envp[2] = NULL;
			kobject_uevent_env(&somc_xssm_dev->dev.kobj, KOBJ_CHANGE, envp);
			kfree(buf);
			printk("[CCI/SOMC]-send late resume notifications\n");
		}
	}
}
#endif

#if 0
// Here we want to register a platform driver
static struct platform_driver somc_xssm_drv = {
#if 0
#ifdef CONFIG_PM
	.suspend = somc_xssm_suspend,
	.resume = somc_xssm_resume,
#endif
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "sony_ssm",
	},
};
#endif

static int __init somc_xssm_init(void)
{
	int retval = 0;
	somc_xssm_dev = platform_device_register_simple("sony_ssm", -1, NULL, 0);
	/* Create the files associated with this kobject */
	retval = sysfs_create_group(&somc_xssm_dev->dev.kobj, &attr_group);
//	retval = platform_driver_register(&somc_xssm_drv);
	sony_xssm.pm_notifier.notifier_call = ssm_pm_notifier;
	sony_xssm.pm_notifier.priority = 0;
	register_pm_notifier(&sony_xssm.pm_notifier);	
#ifdef CONFIG_HAS_EARLYSUSPEND
	sony_xssm.early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
//	sony_xssm.early_suspend.suspend = somc_xssm_early_suspend;
	sony_xssm.early_suspend.resume = somc_xssm_late_resume;
	register_early_suspend(&sony_xssm.early_suspend);
#endif
	return retval;
}

static void __exit somc_xssm_exit(void)
{
//	platform_driver_unregister(&somc_xssm_drv);
	sysfs_remove_group(&somc_xssm_dev->dev.kobj, &attr_group);
	platform_device_unregister(somc_xssm_dev);
}

module_init(somc_xssm_init);
module_exit(somc_xssm_exit);
MODULE_AUTHOR("CCI");
