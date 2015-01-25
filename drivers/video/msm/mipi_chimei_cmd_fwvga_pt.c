/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_chimei.h"

static struct msm_panel_info pinfo;

static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
/* DSI_BIT_CLK at 500MHz, 2 lane, RGB888 */
	{0x09, 0x08, 0x05, 0x00, 0x20},	/* regulator */
		/* timing   */
	{0x73, 0x2e, 0x11, 0x00, 0x3c, 0x48, 0x14,
	0x31, 0x1c, 0x03, 0x04},
	{0x5f, 0x00, 0x00, 0x10},	/* phy ctrl */
 	{0xff, 0x00, 0x06, 0x00},	/* strength */
		/* pll control */
	{0x0, 0xe, 0x30, 0xda, 0x00, 0x10, 0x0f, 0x61,
	0x40, 0x07, 0x03,
	0x00, 0x1a, 0x00, 0x00, 0x02, 0x00, 0x20, 0x00, 0x02},

};

static int __init mipi_cmd_chimei_fwvga_pt_init(void)
{
	int ret;

	if (msm_fb_detect_client("mipi_cmd_chimei_fwvga"))
		return 0;

	pinfo.xres = 480;
	pinfo.yres = 854;
	pinfo.type = MIPI_CMD_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 50;
	pinfo.lcdc.h_front_porch = 50;
	pinfo.lcdc.h_pulse_width = 20;
	pinfo.lcdc.v_back_porch = 11;
	pinfo.lcdc.v_front_porch = 10;
	pinfo.lcdc.v_pulse_width = 5;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 400000000;
	//pinfo.is_3d_panel = FB_TYPE_3D_PANEL;
	pinfo.lcd.vsync_enable = TRUE;
	pinfo.lcd.hw_vsync_mode = TRUE;
	pinfo.lcd.refx100 = 6000; /* adjust refx100 to prevent tearing */
	pinfo.lcd.v_back_porch = 11;
	pinfo.lcd.v_front_porch = 10;
	pinfo.lcd.v_pulse_width = 5;

	pinfo.mipi.mode = DSI_CMD_MODE;
	pinfo.mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.esc_byte_ratio = 4;
	pinfo.mipi.t_clk_post = 0x4;
	pinfo.mipi.t_clk_pre = 0x1a;
	pinfo.mipi.stream = 0;	/* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo.mipi.interleave_max = 1;
	pinfo.mipi.insert_dcs_cmd = TRUE;
	pinfo.mipi.wr_mem_continue = 0x3c;
	pinfo.mipi.wr_mem_start = 0x2c;
	pinfo.mipi.dlane_swap = 0x1;
	pinfo.mipi.frame_rate = 57;
	pinfo.mipi.dsi_phy_db = &dsi_cmd_mode_phy_db;

	ret = mipi_chimei_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_cmd_chimei_fwvga_pt_init);
