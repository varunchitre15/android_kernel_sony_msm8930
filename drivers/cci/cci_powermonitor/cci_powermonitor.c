/*
*  CCI home made driver.
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

extern char export_wakeup_wake_lock;

struct cci_pm_info {
	struct notifier_block pm_notifier;
	struct early_suspend early_suspend;
};

static struct cci_pm_info cci_pm;

static struct platform_device *cci_pm_dev;

#if 0
static int cci_pm_notifier(struct notifier_block *nb, unsigned long event,
                void *ignored)
{
	char *buf;
	char *envp[3];
        pr_debug("%s: event=%lu\n", __func__, event);

        if (event == PM_SUSPEND_PREPARE) {
		buf = kmalloc (32, GFP_ATOMIC);
		if (!buf){
                       	printk(KERN_ERR "%s kmalloc failed\n", __func__);
               	}
               envp[0] = "NAME=sony_ssm";
               snprintf(buf, 32, "EVENT=0");
               envp[1] = buf;
               envp[2] = NULL;
               kobject_uevent_env(&cci_pm_dev->dev.kobj, KOBJ_CHANGE, envp);
               kfree(buf);
               printk("[CCI/SOMC]-send request next suspend prepare_notification\n");
	}
        return NOTIFY_DONE;
}
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
static void cci_pm_early_suspend(struct early_suspend *h)
{
	char *buf;
	char *envp[3];
	buf = kmalloc(32, GFP_ATOMIC);
	if (!buf){
		printk(KERN_ERR "%s kmalloc failed\n", __func__);
	}
	envp[0] = "NAME=cci_pm";
	snprintf(buf, 32, "EVENT=0");
	envp[1] = buf;
	envp[2] = NULL;
	kobject_uevent_env(&cci_pm_dev->dev.kobj, KOBJ_CHANGE, envp);
	kfree(buf);
	printk("[CCI/SOMC]-send request next suspend prepare_notification\n");
}
#endif
static void cci_pm_late_resume(struct early_suspend *h)
{
	char *buf, *buf2;
	char *envp[3];
	buf = kmalloc(32, GFP_ATOMIC);
	if (!buf){
		printk(KERN_ERR "%s kmalloc failed\n", __func__);
	}
	buf2 = kmalloc(32, GFP_ATOMIC);
	if (!buf2){
		printk(KERN_ERR "%s kmalloc failed\n", __func__);
	}
	envp[0] = "NAME=cci_pm";
	snprintf(buf, 32, "EVENT=1");
	snprintf(buf2, 32, "WAKELOCK=%s", &export_wakeup_wake_lock);
	envp[1] = buf;
	envp[2] = NULL;
	envp[2] = buf2;
	kobject_uevent_env(&cci_pm_dev->dev.kobj, KOBJ_CHANGE, envp);
	kfree(buf);
	kfree(buf2);
	printk("[CCI/SOMC]-send late resume notifications\n");
}

static int __init cci_pm_init(void)
{
	int retval = 0;
	cci_pm_dev = platform_device_register_simple("cci_pm", -1, NULL, 0);
	/* Create the files associated with this kobject */
#if 0
	cci_pm.pm_notifier.notifier_call = cci_pm_notifier;
	cci_pm.pm_notifier.priority = 0;
	register_pm_notifier(&cci_pm.pm_notifier);	
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	cci_pm.early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	cci_pm.early_suspend.suspend =cci_pm_early_suspend;
	cci_pm.early_suspend.resume = cci_pm_late_resume;
	register_early_suspend(&cci_pm.early_suspend);
#endif
	return retval;
}

static void __exit cci_pm_exit(void)
{
	platform_device_unregister(cci_pm_dev);
}

module_init(cci_pm_init);
module_exit(cci_pm_exit);
MODULE_AUTHOR("CCI");
MODULE_LICENSE("Dual BSD/GPL");
