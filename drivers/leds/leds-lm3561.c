/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/ctype.h>
#include <mach/mpp.h>
#include <linux/timer.h>   
#include <linux/hrtimer.h> 
#include <asm/gpio.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-lm3561.h>
#include <asm/io.h> //2012/12/12
#include <mach/msm_iomap.h> //2012/12/12

//LM3561 register macros
#define SALVE_ID_W              	0xa6
#define SALVE_ID_R              	0xa7
#define REG_ENABLE                  0x10
#define REG_INDICATOR_BRIGHTNESS    0x12
#define REG_GPIO                    0x20
#define REG_VIN_MONITOR             0x80
#define REG_TORCH_BRIGHTNESS        0xa0
#define REG_FLASH_BRIGHTNESS        0xb0
#define REG_FLASH_DURATION          0xc0
#define REG_FLAGS                   0xd0
#define REG_CONFIGURATION_1         0xe0
#define REG_CONFIGURATION_2         0xf0
//B 2012/08/06
#define MASK_REG_ENABLE                 0x07
#define MASK_REG_INDICATOR_BRIGHTNESS   0x07
#define MASK_REG_GPIO                   0x7f
#define MASK_REG_VIN_MONITOR            0x07
#define MASK_REG_TORCH_BRIGHTNESS       0x07
#define MASK_REG_FLASH_BRIGHTNESS       0x0f
#define MASK_REG_FLASH_DURATION         0x3f
#define MASK_REG_FLAGS                  0xbf
#define MASK_REG_CONFIGURATION_1        0xfc
#define MASK_REG_CONFIGURATION_2        0x0f
//E 2012/08/06

#define DEFAULT_VOLTAGE_TORCH 130800
#define DEFAULT_VOLTAGE_FLASH 500000
#define DEFAULT_FLASH_DURATION 0x0f

#define GPIO_CAM_FLASH_CNTL_EN    2
#define GPIO_MSM_FLASH_STROBE     6
#define GPIO_CONFIG(gpio)         (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))
#define GPIO_IN_OUT(gpio)         (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))

struct i2c_client *lm3561_i2c_client = NULL;

struct lm3561_led_data {
	struct led_classdev led;
	struct i2c_client *client;
}*private_data;

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend lm3561_suspend;

extern struct workqueue_struct *suspend_work_queue;

struct deferred_led_change {
	struct work_struct led_change_work;
	struct led_classdev *led_cdev;
    unsigned int strobe;
    unsigned int flash;
    unsigned int reset;
};
#endif

unsigned int strobe_state = 0;
unsigned int flash_state = 0;
unsigned int reset_state = 0;
//B 2012/08/06
unsigned int suspend_state = 0;
unsigned int duration = 0;
unsigned int timeout[32] = 
{
    32,  64,  96, 128, 160, 192, 224, 256, 
   288, 320, 352, 384, 416, 448, 480, 512,
   544, 576, 608, 640, 672, 704, 736, 768,
   800, 832, 864, 896, 928, 960, 992, 1024
};
//E 2012/08/06

int led_i2c_write(struct i2c_client *client, u8 cReg, u8 data) //2012/08/06
{
	u8 buf[2];

	memset(buf, 0X0, sizeof(buf));

	buf[0] = cReg;
	buf[1] = data;

	return i2c_master_send(client, buf, 2);
}

u8 led_i2c_read(struct i2c_client *client, u8 cReg) //2012/08/06
{
	int retry;
    u8 data;

	struct i2c_msg msg[] = {
	    {
	        .addr = client->addr,
	        .flags = 0,
	        .len = 1,
	        .buf = &cReg,
	    },
	    {
	        .addr = client->addr,
	        .flags = I2C_M_RD,
	        .len = 1,
	        .buf = &data,
	    }
	};

	for (retry = 0; retry < 10; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		mdelay(10);
	}
	if (retry == 10) {
		printk(KERN_ERR "i2c_read_block retry over 10\n");
	}
    
    return data;
}

struct led_classdev* lm3561_init(void) {
    enable_lm3561();//2012/12/12
    return &private_data->led;
}
EXPORT_SYMBOL(lm3561_init);

//B 2012/08/20
void lm3561_release(struct led_classdev *led_cdev) {
    if (led_cdev != NULL) {
        //B 2012/12/12
        if (!gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN))        
            gpio_free(GPIO_MSM_FLASH_STROBE);
        if (!gpio_is_valid(GPIO_MSM_FLASH_STROBE))
            gpio_free(GPIO_CAM_FLASH_CNTL_EN);
        //E 2012/12/12
    }
}
//E 2012/08/20
EXPORT_SYMBOL(lm3561_release);

int lm3561_set_power(bool on)
{
    static struct regulator *reg_lvs1 = NULL;
    int rc = 0;

    //B 2012/08/07
    if (on) {
        if (reg_lvs1 == NULL) {
            reg_lvs1 = regulator_get(NULL,"8038_lvs1");
            if (IS_ERR(reg_lvs1)) {
                pr_err("could not get 8038_lvs1, rc = %ld\n", PTR_ERR(reg_lvs1));
                return -ENODEV;
            }
        }
        rc = regulator_enable(reg_lvs1);
        if (rc) {
            pr_err("enable lvs1 failed, rc=%d\n", rc);
            regulator_put(reg_lvs1);
            reg_lvs1 = NULL;//2012/08/09
            return -ENODEV;
        }
    } else {
        if (reg_lvs1) {
            rc = regulator_disable(reg_lvs1);
            if (rc) {
                pr_err("disable lvs1 failed, rc=%d\n", rc);
                regulator_put(reg_lvs1);
                reg_lvs1 = NULL;//2012/08/09
                return -ENODEV;
            }
            regulator_put(reg_lvs1);
            reg_lvs1 = NULL;//2012/08/09
        }
    }
    //E 2012/08/07
    return rc;
}

void lm3561_torch_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
    int ret = 0;
    struct lm3561_led_data *led_data;
    
    //B 2012/08/20
    if (led_cdev == NULL) {
        printk("%s: NULL pointer\n", __func__);
        return;
    }
    //E 2012/08/20    

    led_data = container_of(led_cdev, struct lm3561_led_data, led);
    
    if (brightness == 0) {
        ret = led_i2c_write(led_data->client, REG_ENABLE, 0x00);
        if (ret < 0) {
            printk("%s, write REG_ENABLE failed", __func__);
            lm3561_reset_set(led_cdev, 1);
            ret = led_i2c_write(led_data->client, REG_ENABLE, 0x00);
            if (ret < 0)
                printk("%s, write REG_ENABLE failed", __func__);
   	    }
        //B 2012/08/06
        if (suspend_state == 1)
            disable_lm3561();
        //E 2012/08/06
    } else {
        //B 2012/08/06
        if (suspend_state == 1)
            enable_lm3561();
        //E 2012/08/06
        ret = led_i2c_write(led_data->client, REG_ENABLE, 0x02);
    	if (ret < 0) {
            printk("%s, write REG_ENABLE failed", __func__);
            lm3561_reset_set(led_cdev, 1);
            ret = led_i2c_write(led_data->client, REG_ENABLE, 0x02);
            if (ret < 0)
                printk("%s, write REG_ENABLE failed", __func__);
   	    }
    }
}
EXPORT_SYMBOL(lm3561_torch_set);

void lm3561_flash_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
    int ret = 0;
    struct lm3561_led_data *led_data;
    
    //B 2012/08/20
    if (led_cdev == NULL) {
        printk("%s: NULL pointer\n", __func__);
        return;
    }
    //E 2012/08/20
    
    led_data = container_of(led_cdev, struct lm3561_led_data, led);
    
    if (brightness == 0) {
        ret = led_i2c_write(led_data->client, REG_ENABLE, 0x00);
    	if (ret < 0) {
            printk("%s, write REG_ENABLE failed", __func__);
            lm3561_reset_set(led_cdev, 1);
            ret = led_i2c_write(led_data->client, REG_ENABLE, 0x00);
            if (ret < 0)
                printk("%s, write REG_ENABLE failed", __func__);
   	    }
        //B 2012/08/09
        if (suspend_state == 1)
            disable_lm3561();
        //E 2012/08/09
    } else {
        //B 2012/08/06
        if (suspend_state == 1)
            enable_lm3561();
        //E 2012/08/06
        ret = led_i2c_write(led_data->client, REG_ENABLE, 0x03);
    	if (ret < 0) {
            printk("%s, write REG_ENABLE failed", __func__);
            lm3561_reset_set(led_cdev, 1);
            ret = led_i2c_write(led_data->client, REG_ENABLE, 0x03);
            if (ret < 0)
                printk("%s, write REG_ENABLE failed", __func__);
   	    }
        //B 2012/08/06
        mdelay(timeout[duration]);
        flash_state = 0;
        if (suspend_state == 1)
            disable_lm3561();
        //E 2012/08/06
    }
}
EXPORT_SYMBOL(lm3561_flash_set);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void change_flash(struct work_struct *flash_change_data)
{
	struct deferred_led_change *flash_change = container_of(flash_change_data, struct deferred_led_change, led_change_work);
	struct led_classdev *led_cdev = flash_change->led_cdev;
	int flash = flash_change->flash;

	lm3561_flash_set(led_cdev, flash);

	/* Free up memory for the freq_change structure. */
	kfree(flash_change);
}

int queue_flash_change(struct led_classdev *led_cdev, int flash)
{
	/* Initialize the led_change_work and its super-struct. */
	struct deferred_led_change *flash_change = kzalloc(sizeof(struct deferred_led_change), GFP_KERNEL);

	if (!flash_change)
		return -ENOMEM;

	flash_change->led_cdev = led_cdev;
	flash_change->flash = flash;

	INIT_WORK(&(flash_change->led_change_work), change_flash);
	queue_work(suspend_work_queue, &(flash_change->led_change_work));

	return 0;

}
#endif

static ssize_t lm3561_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", flash_state);
}

static ssize_t lm3561_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	u32 flash = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
    flash_state = flash;
    
	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		if (!(led_cdev->flags & LED_SUSPENDED)) {
#ifdef CONFIG_HAS_EARLYSUSPEND
			if (queue_flash_change(led_cdev, flash) != 0)
#endif
				lm3561_flash_set(led_cdev, flash);			
		}
	}

	return ret;
}
static DEVICE_ATTR(flash, 0644, lm3561_flash_show, lm3561_flash_store);


void lm3561_strobe_set(struct led_classdev *led_cdev, int strobe)
{
    //B 2012/08/06
    if (suspend_state == 1)
        enable_lm3561();

    if (gpio_is_valid(GPIO_MSM_FLASH_STROBE))//2012/12/12
        gpio_set_value_cansleep(GPIO_MSM_FLASH_STROBE, strobe);          //GPIO output=0

    strobe_state = 1;
    mdelay(timeout[duration]);
    if (gpio_is_valid(GPIO_MSM_FLASH_STROBE))//2012/12/12
        gpio_set_value_cansleep(GPIO_MSM_FLASH_STROBE, 0);
    strobe_state = 0;
    if (suspend_state == 1)
        disable_lm3561();
    //E 2012/08/06
}
EXPORT_SYMBOL(lm3561_strobe_set);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void change_strobe(struct work_struct *strobe_change_data)
{
	struct deferred_led_change *strobe_change = container_of(strobe_change_data, struct deferred_led_change, led_change_work);
	struct led_classdev *led_cdev = strobe_change->led_cdev;
	int strobe = strobe_change->strobe;

	lm3561_strobe_set(led_cdev, strobe);

	/* Free up memory for the freq_change structure. */
	kfree(strobe_change);
}

int queue_strobe_change(struct led_classdev *led_cdev, int strobe)
{
	/* Initialize the led_change_work and its super-struct. */
	struct deferred_led_change *strobe_change = kzalloc(sizeof(struct deferred_led_change), GFP_KERNEL);

	if (!strobe_change)
		return -ENOMEM;

	strobe_change->led_cdev = led_cdev;
	strobe_change->strobe = strobe;

	INIT_WORK(&(strobe_change->led_change_work), change_strobe);
	queue_work(suspend_work_queue, &(strobe_change->led_change_work));

	return 0;

}
#endif

static ssize_t lm3561_strobe_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", strobe_state);
}

static ssize_t lm3561_strobe_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	u32 strobe = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
    strobe_state = strobe;
	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		if (!(led_cdev->flags & LED_SUSPENDED)) {
#ifdef CONFIG_HAS_EARLYSUSPEND
			if (queue_strobe_change(led_cdev, strobe) != 0)
#endif
				lm3561_strobe_set(led_cdev, strobe);
		}
	}

	return ret;
}
static DEVICE_ATTR(strobe, 0644, lm3561_strobe_show, lm3561_strobe_store);

void lm3561_reset_set(struct led_classdev *led_cdev, int reset)
{
    int ret = 0;
    struct lm3561_led_data *led_data;
    
    led_data = container_of(led_cdev, struct lm3561_led_data, led);
    //B 2012/12/12
    if (!gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN)) {
        if (gpio_request(GPIO_CAM_FLASH_CNTL_EN, "lm3561_en"))
		    printk(KERN_ERR "failed to request gpio lm3561_en\n");
    }
    if (!gpio_is_valid(GPIO_MSM_FLASH_STROBE)) {
        if (gpio_request(GPIO_MSM_FLASH_STROBE, "lm3561_strobe"))
		    printk(KERN_ERR "failed to request gpio lm3561_strobe\n");    
    }
    //E 2012/12/12
    if (reset) {
        //B 2012/12/12
        if (gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN)) {
            //B 2012/08/06
            gpio_set_value_cansleep(GPIO_CAM_FLASH_CNTL_EN, 0);          //GPIO output=0
            mdelay(10);
            gpio_set_value_cansleep(GPIO_CAM_FLASH_CNTL_EN, 1);          //GPIO output=1
            mdelay(50);
            //E 2012/08/06
        }
        //E 2012/12/12
        ret = led_i2c_write(led_data->client, REG_VIN_MONITOR, 0x01);
        if (ret < 0) {
            printk("%s: write REG_VIN_MONITOR failed\n", __func__);
        }    
        
	    ret = led_i2c_write(led_data->client, REG_TORCH_BRIGHTNESS, 0x06);
	    if (ret < 0) {
            printk("%s: write REG_TORCH_BRIGHTNESS failed\n", __func__);
        }
        
	    ret = led_i2c_write(led_data->client, REG_FLASH_BRIGHTNESS, 0x0d);
	    if (ret < 0) {
            printk("%s: write REG_FLASH_BRIGHTNESS failed\n", __func__);
        }
        
        ret = led_i2c_write(led_data->client, REG_FLASH_DURATION, 0x20 | duration);//2012/08/06
	    if (ret < 0) {
            printk("%s: write REG_FLASH_DURATION failed\n", __func__);
        }
        //B 2012/12/19
        if (board_type_with_hw_id() <= DVT2_BOARD_HW_ID) {
            ret = led_i2c_write(led_data->client, REG_CONFIGURATION_1, 0x7c);
        } else {
            ret = led_i2c_write(led_data->client, REG_CONFIGURATION_1, 0x6c);
        }
	    if (ret < 0) {
            printk("%s: write REG_CONFIGURATION_1 failed\n", __func__);
        }
        
        if (board_type_with_hw_id() <= DVT2_BOARD_HW_ID) {
            ret = led_i2c_write(led_data->client, REG_CONFIGURATION_2, 0x0a);
        } else {
            ret = led_i2c_write(led_data->client, REG_CONFIGURATION_2, 0x08);
        }
        //E 2012/12/19
	    if (ret < 0) {
            printk("%s: write REG_CONFIGURATION_2 failed\n", __func__);
        }
        //B 2012/08/06
        printk("%s: REG_ENABLE = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_ENABLE) & MASK_REG_ENABLE);
        printk("%s: REG_INDICATOR_BRIGHTNESS = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_INDICATOR_BRIGHTNESS) & MASK_REG_INDICATOR_BRIGHTNESS);
        printk("%s: REG_GPIO = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_GPIO) & MASK_REG_GPIO);
        printk("%s: REG_VIN_MONITOR = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_VIN_MONITOR) & MASK_REG_VIN_MONITOR);
        printk("%s: REG_TORCH_BRIGHTNESS = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_TORCH_BRIGHTNESS) & MASK_REG_TORCH_BRIGHTNESS);
        printk("%s: REG_FLASH_BRIGHTNESS = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_FLASH_BRIGHTNESS) & MASK_REG_FLASH_BRIGHTNESS);
        printk("%s: REG_FLASH_DURATION = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_FLASH_DURATION) & MASK_REG_FLASH_DURATION);
        printk("%s: REG_FLAGS = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_FLAGS) & MASK_REG_FLAGS);
        printk("%s: REG_CONFIGURATION_1 = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_CONFIGURATION_1) & MASK_REG_CONFIGURATION_1);
        printk("%s: REG_CONFIGURATION_2 = 0x%x\n", __func__, led_i2c_read(led_data->client, REG_CONFIGURATION_2) & MASK_REG_CONFIGURATION_2);
        //E 2012/08/06
    }
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void change_reset(struct work_struct *reset_change_data)
{
	struct deferred_led_change *reset_change = container_of(reset_change_data, struct deferred_led_change, led_change_work);
	struct led_classdev *led_cdev = reset_change->led_cdev;
	int reset = reset_change->reset;

	lm3561_reset_set(led_cdev, reset);

	/* Free up memory for the freq_change structure. */
	kfree(reset_change);
}

int queue_reset_change(struct led_classdev *led_cdev, int reset)
{
	/* Initialize the led_change_work and its super-struct. */
	struct deferred_led_change *reset_change = kzalloc(sizeof(struct deferred_led_change), GFP_KERNEL);

	if (!reset_change)
		return -ENOMEM;

	reset_change->led_cdev = led_cdev;
	reset_change->reset = reset;

	INIT_WORK(&(reset_change->led_change_work), change_reset);
	queue_work(suspend_work_queue, &(reset_change->led_change_work));

	return 0;

}
#endif

static ssize_t lm3561_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", reset_state);
}

static ssize_t lm3561_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	u32 reset = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
    reset_state = reset;
	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		if (!(led_cdev->flags & LED_SUSPENDED)) {
#ifdef CONFIG_HAS_EARLYSUSPEND
			if (queue_reset_change(led_cdev, reset) != 0)
#endif
				lm3561_reset_set(led_cdev, reset);
		}
	}

	return ret;
}
static DEVICE_ATTR(reset, 0644, lm3561_reset_show, lm3561_reset_store);

void enable_lm3561(void)//2012/08/06
{
    //B 2012/08/09
    //lm3561_reset_set(&private_data->led, 1);
    lm3561_set_power(true);
    //B 2012/12/12
    if (!gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN)) {
        if (gpio_request(GPIO_CAM_FLASH_CNTL_EN, "lm3561_en"))
		    printk(KERN_ERR "failed to request gpio lm3561_en\n");
    }
    if (!gpio_is_valid(GPIO_MSM_FLASH_STROBE)) {
        if (gpio_request(GPIO_MSM_FLASH_STROBE, "lm3561_strobe"))
		    printk(KERN_ERR "failed to request gpio lm3561_strobe\n");
    }    
    //E 2012/12/12
    //E 2012/08/09
	return;
}

void disable_lm3561(void)//2012/08/06
{
    //B 2012/08/09
    if (!gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN))//2012/12/12
        //B 2012/08/06
        gpio_free(GPIO_MSM_FLASH_STROBE);
    if (!gpio_is_valid(GPIO_MSM_FLASH_STROBE))//2012/12/12
        gpio_free(GPIO_CAM_FLASH_CNTL_EN);
        //E 2012/08/06
    
    //E 2012/08/09
	lm3561_set_power(false);
	return;
}


static int lm3561_remove(struct i2c_client *client)
{
	struct lm3561_led_data *data = i2c_get_clientdata(client);
	printk("%s\n", __func__);
	
	led_classdev_unregister(&data->led);
	kfree(data);
	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void lm3561_early_suspend(struct early_suspend *h)
{
	disable_lm3561();    //cut lm3561 power
	suspend_state = 1;//2012/08/06
}

void lm3561_later_resume(struct early_suspend *h)
{
	enable_lm3561();
    suspend_state = 0;//2012/08/06
}
#endif


static int __devinit leds_lm3561_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lm3561_led_data *data;
	int rc = 0;
	int ret = 0;
    
	printk("%s, gpio 2 is %d\n", __func__, readl(GPIO_CONFIG(GPIO_CAM_FLASH_CNTL_EN)));
	if (!(data = kzalloc(sizeof(struct lm3561_led_data), GFP_KERNEL))) {
		printk("%s: error to malloc client memory!\n", __func__);
		rc = -ENOMEM;
		goto err_alloc_failed;
	}
    
	ret = led_i2c_write(client, REG_VIN_MONITOR, 0x01);
	if (ret < 0) {
        printk("%s: write REG_VIN_MONITOR failed\n", __func__);
        goto err_probe_failed;
    }
    
	ret = led_i2c_write(client, REG_TORCH_BRIGHTNESS, 0x06);
	if (ret < 0) {
        printk("%s: write REG_TORCH_BRIGHTNESS failed\n", __func__);
        goto err_probe_failed;
    }
    
	ret = led_i2c_write(client, REG_FLASH_BRIGHTNESS, 0x0d);
	if (ret < 0) {
        printk("%s: write REG_FLASH_BRIGHTNESS failed\n", __func__);
        goto err_probe_failed;
    }
    
    ret = led_i2c_write(client, REG_FLASH_DURATION, 0x20 | duration);//2012/08/06
	if (ret < 0) {
        printk("%s: write REG_FLASH_DURATION failed\n", __func__);
        goto err_probe_failed;
    }
    //B 2012/12/19
    if (board_type_with_hw_id() <= DVT2_BOARD_HW_ID) {
        ret = led_i2c_write(client, REG_CONFIGURATION_1, 0x7c);
    } else {
        ret = led_i2c_write(client, REG_CONFIGURATION_1, 0x6c);
    }
	if (ret < 0) {
        printk("%s: write REG_CONFIGURATION_1 failed\n", __func__);
        goto err_probe_failed;
    }
    if (board_type_with_hw_id() <= DVT2_BOARD_HW_ID) {
        ret = led_i2c_write(client, REG_CONFIGURATION_2, 0x0a);
    } else {
        ret = led_i2c_write(client, REG_CONFIGURATION_2, 0x08);
    }
    //E 2012/12/19
	if (ret < 0) {
        printk("%s: write REG_CONFIGURATION_2 failed\n", __func__);
        goto err_probe_failed;
    }
    //B 2012/08/06
    printk("%s: REG_ENABLE = 0x%x\n", __func__, led_i2c_read(client, REG_ENABLE) & MASK_REG_ENABLE);
    printk("%s: REG_INDICATOR_BRIGHTNESS = 0x%x\n", __func__, led_i2c_read(client, REG_INDICATOR_BRIGHTNESS) & MASK_REG_INDICATOR_BRIGHTNESS);
    printk("%s: REG_GPIO = 0x%x\n", __func__, led_i2c_read(client, REG_GPIO) & MASK_REG_GPIO);
    printk("%s: REG_VIN_MONITOR = 0x%x\n", __func__, led_i2c_read(client, REG_VIN_MONITOR) & MASK_REG_VIN_MONITOR);
    printk("%s: REG_TORCH_BRIGHTNESS = 0x%x\n", __func__, led_i2c_read(client, REG_TORCH_BRIGHTNESS) & MASK_REG_TORCH_BRIGHTNESS);
    printk("%s: REG_FLASH_BRIGHTNESS = 0x%x\n", __func__, led_i2c_read(client, REG_FLASH_BRIGHTNESS) & MASK_REG_FLASH_BRIGHTNESS);
    printk("%s: REG_FLASH_DURATION = 0x%x\n", __func__, led_i2c_read(client, REG_FLASH_DURATION) & MASK_REG_FLASH_DURATION);
    printk("%s: REG_FLAGS = 0x%x\n", __func__, led_i2c_read(client, REG_FLAGS) & MASK_REG_FLAGS);
    printk("%s: REG_CONFIGURATION_1 = 0x%x\n", __func__, led_i2c_read(client, REG_CONFIGURATION_1) & MASK_REG_CONFIGURATION_1);
    printk("%s: REG_CONFIGURATION_2 = 0x%x\n", __func__, led_i2c_read(client, REG_CONFIGURATION_2) & MASK_REG_CONFIGURATION_2);
    //E 2012/08/06
		
	lm3561_i2c_client = client;
	data->client = client;
	i2c_set_clientdata(client, data);
    
    data->led.name = "flashlight";
    data->led.brightness_set = lm3561_torch_set;    

    ret = led_classdev_register(NULL, &data->led);
    if (ret) {
	    printk(KERN_ERR "lm3561: led_classdev_register failed\n");
	    goto err_led_classdev_register_failed;
	}

    ret = device_create_file(data->led.dev, &dev_attr_flash);
	if (ret) {
		printk(KERN_ERR "lm3561: device_create_file failed\n");
		goto err_led_classdev_register_failed;
	}
    
    ret = device_create_file(data->led.dev, &dev_attr_strobe);
	if (ret) {
		printk(KERN_ERR "lm3561: device_create_file failed\n");
		goto err_led_classdev_register_failed;
	}

    ret = device_create_file(data->led.dev, &dev_attr_reset);
	if (ret) {
		printk(KERN_ERR "lm3561: device_create_file failed\n");
		goto err_led_classdev_register_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	lm3561_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	lm3561_suspend.suspend = lm3561_early_suspend;
	lm3561_suspend.resume = lm3561_later_resume;
	register_early_suspend(&lm3561_suspend);
#endif

	private_data = data;
    return 0;

err_led_classdev_register_failed:
	led_classdev_unregister(&data->led);
    
err_probe_failed:
	kfree(data);
	i2c_set_clientdata(client, NULL);
	
err_alloc_failed:		
	printk("%s: - probe_failed...\n", __func__);
       return rc;

}

static const struct i2c_device_id lm3561_id[] = {
	{ "lm3561", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3561_id);

static struct i2c_driver lm3561_i2c_driver = {
        .driver = {
//		.owner	= THIS_MODULE,
		.name = "lm3561",
	}, 
        .probe  = leds_lm3561_probe,
        .remove = __devexit_p(lm3561_remove),
        .id_table = lm3561_id,
};

static int __init leds_lm3561_init(void)
{
    int ret = 0;
    unsigned config = GPIO_CFG(GPIO_MSM_FLASH_STROBE, 0, GPIO_CFG_OUTPUT, 
                            GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
    
	printk("%s\n", __func__);
    lm3561_set_power(true);   
    //B 2012/12/12
    if (!gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN)) {
        if (gpio_request(GPIO_CAM_FLASH_CNTL_EN, "lm3561_en"))
		    printk(KERN_ERR "failed to request gpio lm3561_en\n");
    }

    if (!gpio_is_valid(GPIO_MSM_FLASH_STROBE)) {
        if (gpio_request(GPIO_MSM_FLASH_STROBE, "lm3561_strobe"))
		    printk(KERN_ERR "failed to request gpio lm3561_strobe\n");
    }
    //E 2012/12/12
    gpio_tlmm_config(config, 0);
    //B 2012/08/06
    //mdelay(100);
    mdelay(50);
    if (gpio_is_valid(GPIO_CAM_FLASH_CNTL_EN))//2012/12/12
   	    gpio_set_value_cansleep(GPIO_CAM_FLASH_CNTL_EN, 1);          //GPIO output=0
   	if (gpio_is_valid(GPIO_MSM_FLASH_STROBE))//2012/12/12
	    gpio_set_value_cansleep(GPIO_MSM_FLASH_STROBE, 0);          //GPIO output=0
    mdelay(50);
    duration = 0xf;
    //E 2012/08/06
    ret = i2c_add_driver(&lm3561_i2c_driver);
	if (ret) {
		printk(KERN_ERR "leds_lm3561_init: i2c_add_driver() failed\n");
	}
	return 0;
}

static void __exit leds_lm3561_exit(void)
{
	printk("%s\n", __func__);
	i2c_del_driver(&lm3561_i2c_driver);
	if (lm3561_i2c_client) {
		kfree(lm3561_i2c_client);
	}
	lm3561_i2c_client = NULL;
}

module_init(leds_lm3561_init);
module_exit(leds_lm3561_exit);

MODULE_DESCRIPTION("LEDs driver for Qualcomm MSM8930");
MODULE_AUTHOR("Jia-Han Li");
MODULE_LICENSE("GPL v2");

