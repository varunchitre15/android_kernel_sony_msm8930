/*
 * Source for:
 * Cypress TrueTouch(TM) Standard Product (TTSP) I2C touchscreen driver.
 * For use with Cypress Txx2xx and Txx3xx parts.
 * Supported parts include:
 * CY8CTST242
 * CY8CTST341
 * CY8CTMA340
 *
 * Copyright (C) 2009-2012 Cypress Semiconductor, Inc.
 * Copyright (C) 2010-2011 Motorola Mobility, Inc.
 * Copyright (C) 2012 Sony Mobile Communications AB.
 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <kev@cypress.com>
 *
 */

//#include "cyttsp3_core.h"
#include <linux/cyttsp3_core.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#if 1 
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#endif
#if 1 
#include <linux/export.h>
#include <linux/module.h>
#endif
#define TOUCH_GPIO_IRQ_CYTTSP		11
#define TOUCH_GPIO_RST_CYTTSP		52 
/*---------------------  Static Definitions -------------------------*/
#define BOOT_DEBUG 1   //0:disable, 1:enable
#if(BOOT_DEBUG)
    #define Printhh(string, args...)    printk("CYTTSP_BOOT(K)=> "string, ##args);
#else
    #define Printhh(string, args...)
#endif

#define ENG_TIP  0    //give RD information. Set 1 if develop,and set 0 when release.
#if(ENG_TIP)
    #define PrintTip(string, args...)    printk("CYTTSP_ENG(K)=> "string, ##args);
#else
    #define PrintTip(string, args...)
#endif
/*---------------------  Static Classes  ----------------------------*/


#define CY_I2C_DATA_SIZE  250

struct cyttsp_i2c {
	struct cyttsp_bus_ops ops;
	struct i2c_client *client;
	void *ttsp_client;
	u8 wr_buf[CY_I2C_DATA_SIZE];
};

static int ttsp_i2c_read_block_data(void *handle, u8 addr,
	u8 length, void *data)
{
	struct cyttsp_i2c *ts = container_of(handle, struct cyttsp_i2c, ops);
	int retval = 0;


	//Printhh("[%s] enter...\n", __FUNCTION__);

	if (data == NULL) {
		pr_err("%s: packet data missing error\n", __func__);
		retval = -EINVAL;
		goto ttsp_i2c_read_block_data_exit;
	}

	if ((length == 0) || (length > CY_I2C_DATA_SIZE)) {
		pr_err("%s: packet length=%d error (max=%d)\n",
			__func__, length, CY_I2C_DATA_SIZE);
		retval = -EINVAL;
		goto ttsp_i2c_read_block_data_exit;
	}

	retval = i2c_master_send(ts->client, &addr, 1);
	if (retval < 0)
		goto ttsp_i2c_read_block_data_exit;
	else if (retval != 1) {
		retval = -EIO;
		goto ttsp_i2c_read_block_data_exit;
	}

	retval = i2c_master_recv(ts->client, data, length);

ttsp_i2c_read_block_data_exit:
	return (retval < 0) ? retval : retval != length ? -EIO : 0;
}

static int ttsp_i2c_write_block_data(void *handle, u8 addr,
	u8 length, const void *data)
{
	struct cyttsp_i2c *ts = container_of(handle, struct cyttsp_i2c, ops);
	int retval = 0;

	//Printhh("[%s] enter...\n", __FUNCTION__);

	if (data == NULL) {
		pr_err("%s: packet data missing error\n", __func__);
		retval = -EINVAL;
		goto ttsp_i2c_write_block_data_exit;
	}

	if ((length == 0) || (length > CY_I2C_DATA_SIZE)) {
		pr_err("%s: packet length=%d error (max=%d)\n",
			__func__, length, CY_I2C_DATA_SIZE);
		retval = -EINVAL;
		goto ttsp_i2c_write_block_data_exit;
	}

	ts->wr_buf[0] = addr;
	memcpy(&ts->wr_buf[1], data, length);

	retval = i2c_master_send(ts->client, ts->wr_buf, length+1);

ttsp_i2c_write_block_data_exit:
	return (retval < 0) ? retval : retval != length+1 ? -EIO : 0;
}


#if 0
static int s_iEnDisLvs2(bool bEn)
{
    struct regulator *reg_lvs2;
    int rc = 0;


    Printhh("[%s] enter...\n", __FUNCTION__);

    reg_lvs2 = regulator_get(&client->dev,"vcc_i2c");
    if (IS_ERR(reg_lvs2)) {
        //PrintTip("[%s] could not get 8038_lvs2, rc = %ld\n", __FUNCTION__, PTR_ERR(reg_lvs2));
        pr_err("could not get 8038_lvs2, rc = %ld\n", PTR_ERR(reg_lvs2));
        return -ENODEV;
    }

    if(bEn == true)
    {
        rc = regulator_enable(reg_lvs2);
        if (rc) {
            //PrintTip("[%s] enable lvs2 failed, rc=%d\n", __FUNCTION__, rc);
            pr_err("enable lvs2 failed, rc=%d\n", rc);
            regulator_put(reg_lvs2);
            return -ENODEV;
        }
    }
    else
    {
        rc = regulator_disable(reg_lvs2);
        if (rc) {
            //PrintTip("[%s] disable lvs2 failed, rc=%d\n", __FUNCTION__, rc);
            pr_err("disable lvs2 failed, rc=%d\n", rc);
            regulator_put(reg_lvs2);
            return -ENODEV;
        }
    }
    
    return rc;

}
static int s_iEnDisL9(bool bEn)
{
    struct regulator *reg_l9;
    int rc = 0;


    Printhh("[%s] enter...\n", __FUNCTION__);

    reg_l9 = regulator_get(&client->dev,"vdd");
    if (IS_ERR(reg_l9)) {
        //PrintTip("[%s] could not get reg_l9, rc = %ld\n", __FUNCTION__, PTR_ERR(reg_l9));
        pr_err("could not get 8038_reg_l9, rc = %ld\n", PTR_ERR(reg_l9));
        return -ENODEV;
    }

    if(bEn == true)
    {
        rc = regulator_enable(reg_l9);
        if (rc) {
            //PrintTip("[%s] enable reg_l9 failed, rc=%d\n", __FUNCTION__, rc);
            pr_err("enable reg_l9 failed, rc=%d\n", rc);
            regulator_put(reg_l9);
            return -ENODEV;
        }
    }
    else
    {
        rc = regulator_disable(reg_l9);
        if (rc) {
            //PrintTip("[%s] disable reg_l9 failed, rc=%d\n", __FUNCTION__, rc);
            pr_err("disable reg_l9 failed, rc=%d\n", rc);
            regulator_put(reg_l9);
            return -ENODEV;
        }
    }
    
    return rc;

}

#endif
#if 0
static int cyttsp3_hw_reset(void)
{
	int retval = 0;	
	retval = gpio_request(TOUCH_GPIO_RST_CYTTSP, NULL);
	if (retval < 0) {
		pr_err("%s: Fail request RST pin r=%d\n", __func__, retval);
		pr_err("%s: Try free RST gpio=%d\n", __func__,
			TOUCH_GPIO_RST_CYTTSP);
		gpio_free(TOUCH_GPIO_RST_CYTTSP);
		retval = gpio_request(TOUCH_GPIO_RST_CYTTSP, NULL);
		if (retval < 0) {
			pr_err("%s: Fail 2nd request RST pin r=%d\n", __func__,
				retval);
		}
	}

	if (!(retval < 0)) {
		pr_info("%s: strobe RST(%d) pin\n", __func__,
			TOUCH_GPIO_RST_CYTTSP);

		if (gpio_tlmm_config(GPIO_CFG(TOUCH_GPIO_RST_CYTTSP, 0, GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_6MA),GPIO_CFG_ENABLE)) {
				printk(KERN_ERR "%s: Err: Config GPIO-52 \n",	__func__);	}
		msleep(20);
		gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 1);				
		msleep(20);				
		gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 0);			
		msleep(40);	
		gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 1);				
		msleep(20);	
		gpio_free(TOUCH_GPIO_RST_CYTTSP);
	}

	return retval;
}
#endif
static int cyttsp3_set_reset(void)
{
	int retval = 0;
	int rc = 0;
	retval = gpio_request(TOUCH_GPIO_RST_CYTTSP, NULL);
	if (retval < 0) {
		pr_err("%s: Fail request RST pin r=%d\n", __func__, retval);
		pr_err("%s: Try free RST gpio=%d\n", __func__,
			TOUCH_GPIO_RST_CYTTSP);
		gpio_free(TOUCH_GPIO_RST_CYTTSP);
		retval = gpio_request(TOUCH_GPIO_RST_CYTTSP, NULL);
		if (retval < 0) {
			pr_err("%s: Fail 2nd request RST pin r=%d\n", __func__,
				retval);
		}
	}
	if (!(retval < 0)) {		
		if (gpio_tlmm_config(GPIO_CFG(TOUCH_GPIO_RST_CYTTSP, 0, GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_6MA),GPIO_CFG_ENABLE)) {
				printk(KERN_ERR "%s: Err: Config GPIO-52 \n",	__func__);	}	
		gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 0);		
		rc = gpio_get_value(TOUCH_GPIO_RST_CYTTSP);
		Printhh("[%s] gpio_rst:%d\n",__func__,rc);		
		gpio_free(TOUCH_GPIO_RST_CYTTSP);
		}
   return retval;				
}				
static int __devinit cyttsp_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct cyttsp_i2c *ts;
	struct regulator *reg_l9;
	struct regulator *reg_lvs1;
	 struct regulator *reg_lvs2;
	#if 0
	struct i2c_msg msg[2];
	u8 data[16];
	int ret =0;
	#endif
	int rc =0;
	int retval = 0;
#if 0
      bool bEn = true;
#endif
	Printhh("[%s] enter...\n", __FUNCTION__);
	
	pr_info("%s: Starting %s probe...\n", __func__, CY_I2C_NAME);
	cyttsp3_set_reset(); 
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: fail check I2C functionality\n", __func__);
		retval = -EIO;
		goto cyttsp_i2c_probe_exit;
	}
	/* allocate and clear memory */
	ts = kzalloc(sizeof(struct cyttsp_i2c), GFP_KERNEL);
	if (ts == NULL) {
		pr_err("%s: Error, kzalloc.\n", __func__);
		retval = -ENOMEM;
		goto cyttsp_i2c_probe_exit;
	}
	/* register driver_data */
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->ops.write = ttsp_i2c_write_block_data;
	ts->ops.read = ttsp_i2c_read_block_data;
	ts->ops.dev = &client->dev;
	ts->ops.dev->bus = &i2c_bus_type;
	#if 1
//    	s_iEnDisLvs2(bEn,&client->dev);
//    	s_iEnDisL9(bEn,&client->dev);

	  reg_lvs1 = regulator_get(NULL,"8038_lvs1");
            if (IS_ERR(reg_lvs1)) {
                pr_err("could not get 8038_lvs1, rc = %ld\n", PTR_ERR(reg_lvs1));
                 retval = -ENODEV;
			goto cyttsp_i2c_probe_exit;
            }
	     rc = regulator_enable(reg_lvs1);
        if (rc) {
            pr_err("enable lvs1 failed, rc=%d\n", rc);
            regulator_put(reg_lvs1);
             retval = -ENODEV;
			goto cyttsp_i2c_probe_exit;
        }
	    // regulator_put(reg_lvs1);
	 reg_lvs2 = regulator_get(&client->dev,"vcc_i2c");
	   if (IS_ERR(reg_lvs2)) {
            pr_err("could not get 8038_lvs2, rc=%ld\n",PTR_ERR(reg_lvs2));
            regulator_put(reg_lvs2);
            retval = -ENODEV;
			goto cyttsp_i2c_probe_exit;
        }
        rc = regulator_enable(reg_lvs2);
        if (rc) {         
            pr_err("enable 8038_reg_lvs2 failed, rc=%d\n", rc);
            regulator_put(reg_lvs2);
           retval = -ENODEV;
		   goto cyttsp_i2c_probe_exit;
        }
	 reg_l9 = regulator_get(&client->dev,"vdd");
	  if (IS_ERR(reg_l9)) {
                pr_err("could not get 8038_l9, rc = %ld\n", PTR_ERR(reg_l9));
               retval = -ENODEV;
			goto cyttsp_i2c_probe_exit;
            }	  
	  rc = regulator_set_voltage(reg_l9, 2850000, 2850000);
        if (rc) {
            pr_err("set reg_l9 failed, rc=%d\n", rc);
            regulator_put(reg_l9);
           retval = -ENODEV;
		   goto cyttsp_i2c_probe_exit;
        }	
        rc = regulator_enable(reg_l9);
        if (rc) {     
            pr_err("enable reg_l9 failed, rc=%d\n", rc);
            regulator_put(reg_l9);
            retval = -ENODEV;
			goto cyttsp_i2c_probe_exit;
        }
 	#endif
	 //msleep(5);
	#if 0 //The first start comunication with slave device
	cyttsp3_hw_reset();
	msleep(5);
	/* read message*/
	msg[0].addr = 0x24;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 7;
	msg[0].buf = data;
	ret = i2c_transfer(ts->client->adapter, msg, 1);
	if (ret < 0) {
        	pr_err("[%s]No cyttsp chip inside ret=%d\n",__func__,retval);
    		goto cyttsp_i2c_probe_exit;
	}
	#endif
	ts->ttsp_client = cyttsp_core_init(&ts->ops, &client->dev,
		client->irq, client->name);

	if (ts->ttsp_client == NULL) {
		kfree(ts);
		ts = NULL;
		retval = -ENODATA;
		pr_err("%s: Registration fail ret=%d\n", __func__, retval);
		goto cyttsp_i2c_probe_exit;
	}

	pr_info("%s: Registration complete\n", __func__);

cyttsp_i2c_probe_exit:
	return retval;
}

/* registered in driver struct */
static int __devexit cyttsp_i2c_remove(struct i2c_client *client)
{
	struct cyttsp_i2c *ts;
	int retval = 0;

	ts = i2c_get_clientdata(client);
	if (ts == NULL) {
		pr_err("%s: client pointer error\n", __func__);
		retval = -EINVAL;
	} else {
		cyttsp_core_release(ts->ttsp_client);
		kfree(ts);
	}
	return retval;
}

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int cyttsp_i2c_suspend(struct i2c_client *client, pm_message_t message)
{
	struct cyttsp_i2c *ts = i2c_get_clientdata(client);

	return cyttsp_suspend(ts);
}

static int cyttsp_i2c_resume(struct i2c_client *client)
{
	struct cyttsp_i2c *ts = i2c_get_clientdata(client);

	return cyttsp_resume(ts);
}
#endif

static const struct i2c_device_id cyttsp_i2c_id[] = {
	{ CY_I2C_NAME, 0 },  { }
};

static struct i2c_driver cyttsp_i2c_driver = {
	.driver = {
		.name = CY_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe = cyttsp_i2c_probe,
	.remove = __devexit_p(cyttsp_i2c_remove),
	.id_table = cyttsp_i2c_id,
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = cyttsp_i2c_suspend,
	.resume = cyttsp_i2c_resume,
#endif
};

static int __init cyttsp_i2c_init(void)
{
	return i2c_add_driver(&cyttsp_i2c_driver);
}

static void __exit cyttsp_i2c_exit(void)
{
	return i2c_del_driver(&cyttsp_i2c_driver);
}

module_init(cyttsp_i2c_init);
module_exit(cyttsp_i2c_exit);
MODULE_ALIAS("i2c:cyttsp");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product (TTSP) I2C driver");
MODULE_AUTHOR("Cypress");
MODULE_DEVICE_TABLE(i2c, cyttsp_i2c_id);
