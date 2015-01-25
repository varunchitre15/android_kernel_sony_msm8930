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
#include <linux/leds.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_samsung.h"
#include "mdp4.h"


static struct mipi_dsi_panel_platform_data *mipi_samsung_pdata;

static struct dsi_buf samsung_tx_buf;
static struct dsi_buf samsung_rx_buf;
static int mipi_samsung_lcd_init(void);

static int wled_trigger_initialized;



/* Samsung FWVGA Init Command */
static char set_address_mode[] = {0x36, 0x00};

static char set_pixel_format[] = {0x3a, 0x77};

static char set_column_address[] = {
	0x2a, 0x00, 0x00, 0x01, 0xdf
};

static char set_page_add[] = {
	0x2b, 0x00, 0x00, 0x03, 0x55
};

static char com_access_protect[] = {0xb0, 0x04};

static char frame_mem_acc[] = {0xb3, 0x02, 0x00};

static char dsi_control[] = {0xb6, 0x52, 0x83};

static char bl_control_1[] = {
	0xB8, 0x00, 0x0F, 0x0F,
	0xFF, 0xFF, 0xC8, 0xC8,
	0x02, 0x18, 0x10, 0x10,
	0x37, 0x5A, 0x87, 0xBE,
	0xFF, 0x00, 0x00, 0x00,
	0x00
};

static char bl_control_2[] = {
	0xB9, 0x00, 0x00,
	0x00, 0x00
};

static char resize_contl[] = {0xBD, 0x00};

static char dri_set_1[] = {0xC0, 0x02, 0x76};

static char dri_set_2[] = {
	0xC1, 0x23, 0x3F, 0x02,
	0x3F, 0x18, 0x00, 0x10,
	0x21, 0x09, 0x09, 0xA5,
	0x0F, 0x58, 0x21, 0x01
};

static char dis_v_timing_set[] = {
	0xC2, 0x08, 0x06, 0x06,
	0x03, 0x03, 0x81
};

static char outl_sharp_contl[] = {0xC6, 0x00, 0x00};
static char panel_dri_set_4[] = {
	0xC7, 0x11, 0x8D, 0xB1,
	0xB1, 0x27
};

static char gamma_set_a_set[] = {
	0xC8, 0x00, 0x07, 0x0E,
	0x18, 0x29, 0x47, 0x37,
	0x23, 0x16, 0x0E, 0x08,
	0x03, 0x00, 0x07, 0x0E,
	0x18, 0x29, 0x47, 0x37,
	0x23, 0x16, 0x0E, 0x08,
	0x03
};

static char gamma_set_b_set[] = {
	0xC9, 0x00, 0x07, 0x0E,
	0x18, 0x29, 0x47, 0x37,
	0x23, 0x16, 0x0E, 0x08,
	0x03, 0x00, 0x07, 0x0E,
	0x18, 0x29, 0x47, 0x37,
	0x23, 0x16, 0x0E, 0x08,
	0x03
};

static char gamma_set_c_set[] = {
	0xCA, 0x00, 0x07, 0x0E,
	0x18, 0x29, 0x47, 0x37,
	0x23, 0x16, 0x0E, 0x08,
	0x03, 0x00, 0x07, 0x0E,
	0x18, 0x29, 0x47, 0x37,
	0x23, 0x16, 0x0E, 0x08,
	0x03
};

static char power_set_1[] = {
	0xD0, 0x91, 0x03, 0xDD,
	0x85, 0x0C, 0x71, 0x20,
	0x10, 0x01, 0x00, 0x01,
	0x01, 0x00, 0x03, 0x01,
	0x00
};

static char power_set_2[] = {
	0xD1, 0x18, 0x0C, 0x23,
	0x05, 0x97, 0x02, 0x50
};
static char power_set_3[] = {0xD3, 0x33};
static char vplvl_vnlvl_set[] = {0xD5, 0x32, 0x32};

static char enter_sleep[] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */
static char exit_sleep[] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
static char display_on[] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */
static char write_memory_start[] = {0x2C, 0x00}; /* DTYPE_DCS_WRITE1 */

static struct dsi_cmd_desc samsung_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 200,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(set_address_mode), set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(set_pixel_format), set_pixel_format},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_column_address), set_column_address},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_page_add), set_page_add},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(com_access_protect), com_access_protect},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(frame_mem_acc), frame_mem_acc},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(dsi_control), dsi_control},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(bl_control_1), bl_control_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(bl_control_2), bl_control_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(resize_contl), resize_contl},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(dri_set_1), dri_set_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(dri_set_2), dri_set_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(dis_v_timing_set), dis_v_timing_set},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(outl_sharp_contl), outl_sharp_contl},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(panel_dri_set_4), panel_dri_set_4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(gamma_set_a_set), gamma_set_a_set},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(gamma_set_b_set), gamma_set_b_set},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(gamma_set_c_set), gamma_set_c_set},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(power_set_1), power_set_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(power_set_2), power_set_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(power_set_3), power_set_3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(vplvl_vnlvl_set), vplvl_vnlvl_set},
	{DTYPE_DCS_WRITE, 1, 0, 0, 15,
		sizeof(write_memory_start), write_memory_start},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc samsung_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(enter_sleep), enter_sleep},
};

struct dcs_cmd_req cmdreq;
DEFINE_LED_TRIGGER(bkl_led_trigger);

//static char manufacture_id[2] = {0x04, 0x00}; /* DTYPE_DCS_READ */

//static struct dsi_cmd_desc samsung_manufacture_id_cmd = {
//	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};
/*
static uint32 mipi_samsung_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
	uint32 *lp;

	tp = &samsung_tx_buf;
	rp = &samsung_rx_buf;
	cmd = &samsung_manufacture_id_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 3);
	lp = (uint32 *)rp->data;
	pr_info("%s: manufacture_id=%x", __func__, *lp);
	return *lp;
} */



static int mipi_samsung_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct msm_panel_info *pinfo;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pinfo = &mfd->panel_info;

	mipi  = &mfd->panel_info.mipi;

	pr_err("%s: MIPI init cmd start\n",__func__);
#if 1
	cmdreq.cmds = samsung_cmd_on_cmds;
	cmdreq.cmds_cnt = ARRAY_SIZE(samsung_cmd_on_cmds);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq);
#else
	mipi_dsi_cmds_tx(&samsung_tx_buf, samsung_cmd_on_cmds,
		ARRAY_SIZE(samsung_cmd_on_cmds));
#endif

//		mipi_dsi_cmd_bta_sw_trigger(); /* clean up ack_err_status */
//
//		mipi_samsung_manufacture_id(mfd);

	pr_err("%s: MIPI init cmd end\n",__func__);

	return 0;
}

static int mipi_samsung_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
#if 1
	cmdreq.cmds = samsung_display_off_cmds;
	cmdreq.cmds_cnt = ARRAY_SIZE(samsung_display_off_cmds);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);
#else
	mipi_dsi_cmds_tx(&samsung_tx_buf, samsung_display_off_cmds,
			ARRAY_SIZE(samsung_display_off_cmds));
#endif

	if ((mipi_samsung_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		led_trigger_event(bkl_led_trigger, 0);
	}

	return 0;
}

static void mipi_samsung_set_backlight(struct msm_fb_data_type *mfd)
{
	if ((mipi_samsung_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		led_trigger_event(bkl_led_trigger, mfd->bl_level);
		return;
	}
}

static int barrier_mode;

static int __devinit mipi_samsung_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct platform_device *current_pdev;
	static struct mipi_dsi_phy_ctrl *phy_settings;
	static char dlane_swap;

	if (pdev->id == 0) {
		mipi_samsung_pdata = pdev->dev.platform_data;

		if (mipi_samsung_pdata
			&& mipi_samsung_pdata->phy_ctrl_settings) {
			phy_settings = (mipi_samsung_pdata->phy_ctrl_settings);
		}

		if (mipi_samsung_pdata
			&& mipi_samsung_pdata->dlane_swap) {
			dlane_swap = (mipi_samsung_pdata->dlane_swap);
		}

		
		barrier_mode = 0;

		return 0;
	}

	current_pdev = msm_fb_add_device(pdev);

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
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_samsung_lcd_probe,
	.driver = {
		.name   = "dsi_cmd_samsung_fwvga",
	},
};

static struct msm_fb_panel_data samsung_panel_data = {
	.on		= mipi_samsung_lcd_on,
	.off		= mipi_samsung_lcd_off,
	.set_backlight = mipi_samsung_set_backlight,
};

static int ch_used[3];

int mipi_samsung_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_samsung_lcd_init();
	if (ret) {
		pr_err("mipi_samsung_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("dsi_cmd_samsung_fwvga", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	samsung_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &samsung_panel_data,
		sizeof(samsung_panel_data));
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

static int mipi_samsung_lcd_init(void)
{

	led_trigger_register_simple("bkl_trigger", &bkl_led_trigger);
	pr_info("%s: SUCCESS (WLED TRIGGER)\n", __func__);
	wled_trigger_initialized = 1;

	mipi_dsi_buf_alloc(&samsung_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&samsung_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
