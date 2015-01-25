/*
* Copyright (C) 2012 Sony Mobile Communications AB.
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/io.h>

static ssize_t somc_crash_test_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
    return 0;
}

static ssize_t somc_crash_test_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t count)
{
    int *p = NULL;
    int val = 100;
    int zero = 0;

    if(buf!=NULL)
    {
        printk(KERN_ERR "somc_crash_test_store %s %d %d\n", buf, strlen(buf), count);

        if (strncmp(buf, "panic", 5) == 0)
            panic("simulate panic");
        else if (strncmp(buf, "null", 4) == 0)
            *p = 651105; //We write any number to the location 0 of memory to trigger the ramdump
        else if (strncmp(buf, "divide", 6) == 0)
            val = val / zero;
        else if (strncmp(buf, "anr_ramdump", 11) == 0)
        {
            *p = 19761105; //We write any number to the location 0 of memory to trigger the ramdump
        }
    }
    else
    {
        printk(KERN_ERR "%s: buf is null pointer\n",__FUNCTION__);
    }

    return count;
}

static struct kobj_attribute somc_crash_test_attr = {
    .attr = {
        .name = "somc_crash_test",
        .mode = 0660,
    },
    .show = &somc_crash_test_show,
    .store = &somc_crash_test_store,
};

static struct kobject *somc_sw_kobj;

int cci_somc_sw_init(void)
{
    int ret = 0;

    somc_sw_kobj = kobject_create_and_add("somc_sw_info", NULL);
    if (somc_sw_kobj)
    {
        ret = sysfs_create_file(somc_sw_kobj, &somc_crash_test_attr.attr);
    }
    
    if (!somc_sw_kobj || ret)
    {
        pr_err("%s: failed to create somc_sw_info in sysfs\n", __func__);
    }

    return 0;
}

static void cci_somc_sw_exit(void) {
    if (somc_sw_kobj)
    {
        sysfs_remove_file(somc_sw_kobj, &somc_crash_test_attr.attr);
    }
}

module_init(cci_somc_sw_init);
module_exit(cci_somc_sw_exit);
