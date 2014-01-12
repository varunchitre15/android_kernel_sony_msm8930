/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#endif
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <linux/leds.h>
#include <mach/gpiomux.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_chimei.h"
#include "mdp4.h"
#define ESD 0


static struct mipi_dsi_panel_platform_data *mipi_chimei_pdata;

static struct dsi_buf chimei_tx_buf;
static struct dsi_buf chimei_rx_buf;
static int mipi_chimei_lcd_init(void);

static int wled_trigger_initialized;



/* chimei FWVGA Init Command */
static char Enable_EXTC_1[] = {0x00, 0x00};
static char Enable_EXTC_2[] = {0xFF, 0x80, 0x09, 0x01};

static char Enable_Orise_mode_1[]= {0x00, 0x80};
static char Enable_Orise_mode_2[]= {0xFF, 0x80, 0x09};

static char Sleep_out[] = {0x11, 0x00};

static char Set_GVDD_NGVDD_1[] = {0x00, 0x00};
static char Set_GVDD_NGVDD_2[] = {0xD8, 0xAF, 0xAF};

static char Gammm_Correction_Setting1_1[] = {0x00, 0x00};
static char Gammm_Correction_Setting1_2[] = {
	0xE1, 0x09, 0x11, 0x17,
	0x0D, 0x06, 0x0E, 0x0A,
	0x08, 0x05, 0x09, 0x0D,
	0x07, 0x0E, 0x0E, 0x0A,
	0x05
};

static char Gammm_Correction_Setting2_1[] = {0x00, 0x00};
static char Gammm_Correction_Setting2_2[] = {
	0xE2, 0x09, 0x11, 0x17,
	0x0D, 0x06, 0x0E, 0x0A,
	0x08, 0x05, 0x09, 0x0D,
	0x07, 0x0E, 0x0E, 0x0A,
	0x05
};

static char Gammm_Correction_Setting3_1[] = {0x00, 0x00};
static char Gammm_Correction_Setting3_2[] = {
	0xE5, 0x09, 0x11, 0x17,
	0x0D, 0x06, 0x0E, 0x0A,
	0x08, 0x05, 0x09, 0x0D,
	0x07, 0x0E, 0x0E, 0x0A,
	0x05
};

static char Gammm_Correction_Setting4_1[] = {0x00, 0x00};
static char Gammm_Correction_Setting4_2[] = {
	0xE6, 0x09, 0x11, 0x17,
	0x0D, 0x06, 0x0E, 0x0A,
	0x08, 0x05, 0x09, 0x0D,
	0x07, 0x0E, 0x0E, 0x0A,
	0x05
};

static char Gamma_Set_1[] = {0x00, 0x00};
static char Gamma_Set_2[] = {0x26, 0x04};

static char Memory_Data_Access_Control_1[] = {0x00, 0x00};
static char Memory_Data_Access_Control_2[] = {0x36, 0xD0};

static char Scan_order_1[] = {0x91, 0x00};
static char Scan_order_2[] = {0xB3, 0xC0};

static char display_on[] = {0x29, 0x00};

static char display_off[] = {0x28, 0x00};

static char sleep_in[] = {0x10, 0x00};

static char disable_cm2_1[] = {0x00, 0x00};
static char disable_cm2_2[] = {0xff, 0xff, 0xff, 0xff};
static char inversion_off_1[] = {0x00, 0x83};
static char inversion_off_2[] = {0xB3, 0xC0};

static struct dsi_cmd_desc chimei_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Enable_EXTC_1), Enable_EXTC_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Enable_EXTC_2), Enable_EXTC_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Enable_Orise_mode_1), Enable_Orise_mode_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Enable_Orise_mode_2), Enable_Orise_mode_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 200,
		sizeof(Sleep_out), Sleep_out},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Set_GVDD_NGVDD_1), Set_GVDD_NGVDD_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Set_GVDD_NGVDD_2), Set_GVDD_NGVDD_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting1_1), Gammm_Correction_Setting1_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting1_2), Gammm_Correction_Setting1_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting2_1), Gammm_Correction_Setting2_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting2_2), Gammm_Correction_Setting2_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting3_1), Gammm_Correction_Setting3_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting3_2), Gammm_Correction_Setting3_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting4_1), Gammm_Correction_Setting4_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Gammm_Correction_Setting4_2), Gammm_Correction_Setting4_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Gamma_Set_1), Gamma_Set_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Gamma_Set_2), Gamma_Set_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(inversion_off_1), inversion_off_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(inversion_off_2), inversion_off_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Memory_Data_Access_Control_1), Memory_Data_Access_Control_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Memory_Data_Access_Control_2), Memory_Data_Access_Control_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(Scan_order_1), Scan_order_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Scan_order_2), Scan_order_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(disable_cm2_1), disable_cm2_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(disable_cm2_2), disable_cm2_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 100,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc chimei_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 150,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150,
		sizeof(sleep_in), sleep_in},
};
struct dcs_cmd_req cmdreq_chimei;
//extern int cci_fb_UpdateDone;//Taylor--20121105
//Taylor-B
#if ESD
static u32 power_state;
static int lcd_on = 0;
static int read_val = 0;
static struct delayed_work power_state_detect;
struct platform_device *pdev_backup;
struct platform_device *mine_pdev;
struct msm_fb_data_type *mine_mfd;
static int chimei_recover = 0;
#define DISP_RST_GPIO 58
//extern struct platform_device *mdp_dev_backup;
//extern int mipi_timeout;
static struct wake_lock esd_wakelock;
static int esd_lock;
static int mipi_chimei_lcd_on(struct platform_device *pdev);
static int bl_backup;
extern struct fb_var_screeninfo *var_backup;
#endif
//Taylor-E

//static char manufacture_id[2] = {0x04, 0x00}; /* DTYPE_DCS_READ */

//Taylor--B
//static struct work_struct esd_work; 
DEFINE_LED_TRIGGER(bkl_led_trigger);

#if 0
static void check_state_work(struct work_struct *work)
{
        char buf[64];
        char *envp[2];

	snprintf(buf, sizeof(buf), "NAME=ESD");
	envp[0] = buf;
        envp[1] = NULL;
        kobject_uevent_env(&pdev_backup->dev.kobj, KOBJ_CHANGE, envp);
	printk("Taylor: kobject name=%s\n",(&pdev_backup->dev.kobj)->name);
}
#endif

#if ESD
static void mipi_chimei_reset(void)
{
	int rc;

	chimei_recover = 1;
	led_trigger_event(bkl_led_trigger, 0);
	printk("%s: ESD Recover start\n",__func__);

#if 1
	printk("%s: Wrong driver IC's register , start to recover\n",__func__);
	rc = gpio_tlmm_config(GPIO_CFG(DISP_RST_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (rc) 
	{
		printk("%s: gpio_tlmm_config  DISP_RST_GPIO failed(%d)\n",  __func__, rc);
		return ;
	}
	gpio_set_value(DISP_RST_GPIO, 0);
	mdelay(1);

	gpio_set_value(DISP_RST_GPIO, 1);
	mdelay(1);
	gpio_set_value(DISP_RST_GPIO, 0);
	mdelay(50);
	gpio_set_value(DISP_RST_GPIO, 1);
	mdelay(5);	
	mipi_chimei_lcd_on(mine_pdev);
	printk("%s: Wrong driver IC's register , recover FINISH\n",__func__);	
#else
	//if (mipi_timeout){
	if (read_mipi_state()){
		wake_lock(&esd_wakelock);//////lock until unlock
		esd_lock=1;
		mdelay(30);
		printk("%s: Driver IC doesn't response, start to recover\n",__func__);
		//mdp4_dmap_done_dsi_cmd(0);//make sure finish dsi_cmd
  		schedule_work(&esd_work);
	}
	else{	
		printk("%s: Wrong driver IC's register , start to recover\n",__func__);
		rc = gpio_tlmm_config(GPIO_CFG(DISP_RST_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		if (rc) 
		{
			printk("%s: gpio_tlmm_config  DISP_RST_GPIO failed(%d)\n",  __func__, rc);
			return ;
		}
		gpio_set_value(DISP_RST_GPIO, 0);
		mdelay(1);

		gpio_set_value(DISP_RST_GPIO, 1);
		mdelay(1);
		gpio_set_value(DISP_RST_GPIO, 0);
		mdelay(50);
		gpio_set_value(DISP_RST_GPIO, 1);
		mdelay(5);	
		mipi_chimei_lcd_on(mine_pdev);
		printk("%s: Wrong driver IC's register , recover FINISH\n",__func__);	
	}
#endif
}

static void mipi_chimei_power_mode_cb(u32 data)
{
	if (data < 0)
		pr_err("%s: Power_mode data < 0\n", __func__);
	else{
		power_state = data & 0x000000FF;
		//power_state =  0xFF;
		//mipi_timeout=1;
		pr_debug("%s: Power_mode=0x%x\n", __func__, power_state);
	}
}

static uint32 mipi_chimei_power_mode(struct msm_fb_data_type *mfd, u16 reg)
{
	struct dsi_cmd_desc chimei_power_mode_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 10, sizeof(reg), (char *) &reg};

	cmdreq_chimei.cmds = &chimei_power_mode_cmd;
	cmdreq_chimei.cmds_cnt = 1;
	cmdreq_chimei.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq_chimei.rlen = 0;
	cmdreq_chimei.cb = mipi_chimei_power_mode_cb;
	mipi_dsi_cmdlist_put(&cmdreq_chimei);

	return power_state;
}

int read_back_information(void)
{
	struct msm_fb_data_type *mfd = mine_mfd;
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	if(!mine_pdev)
		return -EINVAL;

	if (lcd_on==0)
		return 0;

	power_state = mipi_chimei_power_mode(mfd, 0x0A);
	if (!(power_state == 0x9c))
	{
		printk(KERN_ERR "reg 0x0A error\n");
		return 1;
	}

	power_state = mipi_chimei_power_mode(mfd, 0x0B);
	if (!(power_state == 0xd0))
	{
		printk(KERN_ERR "reg 0x0B error\n");
		return 1;
	}

	power_state = mipi_chimei_power_mode(mfd, 0x0D);
	power_state &= 0x18;
	if(power_state)
	{
		printk(KERN_ERR "reg 0x0D error\n");
		return 1;
	}

	return 0;
}

int power_state_count = 1;

static void power_state_detect_work(struct work_struct *work)
{
	struct msm_fb_data_type *mfd;

	if (!(power_state_count%10)){
		power_state_count = 1;
		printk(" %s\n", __func__);
	}
	else
		power_state_count++;

	mfd = platform_get_drvdata(pdev_backup);

	pr_debug("%s: read_val =%d\n ",__func__,read_val );
	//read_val = read_back_information();
	//mipi_timeout=1;
	//mipi_chimei_reset();
#if 1
	if (read_val == 0 && lcd_on == 1){
		mutex_lock(&mfd->dma->ov_mutex); 
		read_val = read_back_information();
		mutex_unlock(&mfd->dma->ov_mutex); 

		if (read_val){
			pr_err("%s: read_val =%d\n ",__func__,read_val );
			mipi_chimei_reset();
		}
		else{
			schedule_delayed_work(&power_state_detect, msecs_to_jiffies(1500)); //schedule the next poll operation
		}
	}
	else if(lcd_on == 0)
		printk("%s : LCD suspend no detect work\n",__func__);
	else{
		mipi_chimei_reset();
	}
#endif
}
#endif
//Taylor--E

//static struct dsi_cmd_desc chimei_manufacture_id_cmd = {
//	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};
/*
static uint32 mipi_chimei_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
	uint32 *lp;

	tp = &chimei_tx_buf;
	rp = &chimei_rx_buf;
	cmd = &chimei_manufacture_id_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 3);
	lp = (uint32 *)rp->data;
	pr_info("%s: manufacture_id=%x", __func__, *lp);
	return *lp;
} */



static int mipi_chimei_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct msm_panel_info *pinfo;
#if ESD
	struct fb_info *fbi;
#endif

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pinfo = &mfd->panel_info;

	mipi  = &mfd->panel_info.mipi;
#if ESD
	mine_pdev = pdev;//Taylor
	mine_mfd = mfd;//Taylor
	fbi = mfd->fbi;//Taylor
#endif
	pr_err("%s: MIPI init cmd start\n",__func__);

#if 1
	cmdreq_chimei.cmds = chimei_cmd_on_cmds;
	cmdreq_chimei.cmds_cnt = ARRAY_SIZE(chimei_cmd_on_cmds);
	cmdreq_chimei.flags = CMD_REQ_COMMIT;
	cmdreq_chimei.rlen = 0;
	cmdreq_chimei.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq_chimei);
#else
	mipi_dsi_cmds_tx(&chimei_tx_buf, chimei_cmd_on_cmds,
		ARRAY_SIZE(chimei_cmd_on_cmds));
#endif

//		mipi_dsi_cmd_bta_sw_trigger(); /* clean up ack_err_status */
//
//		mipi_chimei_manufacture_id(mfd);

	//Taylor--B
#if ESD
	lcd_on = 1;
	read_val = read_back_information();
	if (!read_val){
		//mipi_timeout=0;
		//wrtie_mipi_state(0);
		if (chimei_recover){
			if (esd_lock == 1){
				wake_unlock(&esd_wakelock);
				esd_lock = 0 ;
			}
			fbi->fbops->fb_pan_display(var_backup,fbi);
			mdelay(30);
			led_trigger_event(bkl_led_trigger, bl_backup);
			chimei_recover = 0;//Taylor--recover done
		}
	}

	schedule_delayed_work(&power_state_detect, msecs_to_jiffies(100)); //schedule the next poll operation
#endif
	//Taylor--E
	pr_err("%s: MIPI init cmd end\n",__func__);

	return 0;
}

static int mipi_chimei_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
#if 1
	//if (mipi_timeout){
	/*	
	if (read_mipi_state()){
		lcd_on = 0;
		return 0;
	}
	*/

	cmdreq_chimei.cmds = chimei_display_off_cmds;
	cmdreq_chimei.cmds_cnt = ARRAY_SIZE(chimei_display_off_cmds);
	cmdreq_chimei.flags = CMD_REQ_COMMIT;
	cmdreq_chimei.rlen = 0;
	cmdreq_chimei.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq_chimei);
#else
	mipi_dsi_cmds_tx(&chimei_tx_buf, chimei_display_off_cmds,
			ARRAY_SIZE(chimei_display_off_cmds));
#endif

	if ((mipi_chimei_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		led_trigger_event(bkl_led_trigger, 0);
	}

	//cci_fb_UpdateDone=0; //Taylor--20121105
#if ESD
	lcd_on = 0;//Taylor
#endif
	return 0;
}

static void mipi_chimei_set_backlight(struct msm_fb_data_type *mfd)
{
	/*
	if (!cci_fb_UpdateDone){
		printk("Taylor: No BL before LCM on\n");
		return;
	}
	*/

	if ((mipi_chimei_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		pr_debug("Taylor: chimei BL=%d\n",mfd->bl_level);
		#if ESD
		bl_backup = mfd->bl_level;
		#endif
		led_trigger_event(bkl_led_trigger, mfd->bl_level);
		return;
	}
}

static int barrier_mode;

static int __devinit mipi_chimei_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct platform_device *current_pdev;
	static struct mipi_dsi_phy_ctrl *phy_settings;
	static char dlane_swap;

	if (pdev->id == 0) {
		mipi_chimei_pdata = pdev->dev.platform_data;

		if (mipi_chimei_pdata
			&& mipi_chimei_pdata->phy_ctrl_settings) {
			phy_settings = (mipi_chimei_pdata->phy_ctrl_settings);
		}

		if (mipi_chimei_pdata
			&& mipi_chimei_pdata->dlane_swap) {
			dlane_swap = (mipi_chimei_pdata->dlane_swap);
		}

		
		barrier_mode = 0;

		return 0;
	}

	current_pdev = msm_fb_add_device(pdev);
	#if ESD
	pdev_backup = current_pdev;//Taylor
	#endif

	if (current_pdev) {
		mfd = platform_get_drvdata(current_pdev);
		if (!mfd)
			return -ENODEV;
		if (mfd->key != MFD_KEY)
			return -EINVAL;

		mipi  = &mfd->panel_info.mipi;

		if (phy_settings != NULL)
			mipi->dsi_phy_db = phy_settings;

		if (dlane_swap)
			mipi->dlane_swap = dlane_swap;
	}

	//wake_lock_init(&esd_wakelock, WAKE_LOCK_SUSPEND, "CHIMEI_ESD_WAKE_LOCK");
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_chimei_lcd_probe,
	.driver = {
		.name   = "dsi_cmd_chimei_fwvga",
	},
};

static struct msm_fb_panel_data chimei_panel_data = {
	.on		= mipi_chimei_lcd_on,
	.off		= mipi_chimei_lcd_off,
	.set_backlight = mipi_chimei_set_backlight,
};

static int ch_used[3];

int mipi_chimei_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_chimei_lcd_init();
	if (ret) {
		pr_err("mipi_chimei_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("dsi_cmd_chimei_fwvga", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	chimei_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &chimei_panel_data,
		sizeof(chimei_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_chimei_lcd_init(void)
{

	led_trigger_register_simple("bkl_trigger", &bkl_led_trigger);
	pr_info("%s: SUCCESS (WLED TRIGGER)\n", __func__);
	wled_trigger_initialized = 1;

	mipi_dsi_buf_alloc(&chimei_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&chimei_rx_buf, DSI_BUF_SIZE);
	
	#if ESD
	INIT_DELAYED_WORK(&power_state_detect, power_state_detect_work);
	//INIT_WORK(&esd_work, check_state_work);
	#endif


	return platform_driver_register(&this_driver);
}
