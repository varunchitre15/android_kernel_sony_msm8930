/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/delay.h>

#include <linux/module.h>
#include "msm_camera_eeprom.h"
#include "msm_camera_i2c.h"

DEFINE_MUTEX(imx134_eeprom_mutex);
static struct msm_eeprom_ctrl_t imx134_eeprom_t;

static const struct i2c_device_id imx134_eeprom_i2c_id[] = {
	{"imx134_eeprom", (kernel_ulong_t)&imx134_eeprom_t},
	{ }
};

static struct i2c_driver imx134_eeprom_i2c_driver = {
	.id_table = imx134_eeprom_i2c_id,
	.probe  = msm_eeprom_i2c_probe,
	.remove = __exit_p(imx134_eeprom_i2c_remove),
	.driver = {
		.name = "imx134_eeprom",
	},
};

static int __init imx134_eeprom_i2c_add_driver(void)
{
	int rc = 0;
	rc = i2c_add_driver(imx134_eeprom_t.i2c_driver);
	return rc;
}

static struct v4l2_subdev_core_ops imx134_eeprom_subdev_core_ops = {
	.ioctl = msm_eeprom_subdev_ioctl,
};

static struct v4l2_subdev_ops imx134_eeprom_subdev_ops = {
	.core = &imx134_eeprom_subdev_core_ops,
};

uint8_t imx134_wbcalib_data[36];
uint8_t imx134_afcalib_data[6];
uint8_t imx134_lsccalib_data[768];

struct msm_calib_wb32 imx134_wb_data[3];
struct msm_calib_af imx134_af_data;
struct msm_calib_lsc imx134_lsc_data;
//struct msm_calib_dpc imx134_dpc_data;

static struct msm_camera_eeprom_info_t imx134_calib_supp_info = {
	{TRUE, 6, 1, 1},
	{TRUE, 36, 0, 16777216},
	{TRUE, 768, 2, 1},
	{FALSE, 0, 0, 1},
	{FALSE, 0, 0, 1},
};

static struct msm_camera_eeprom_read_t imx134_eeprom_read_tbl[] = {
	{0x70, &imx134_wbcalib_data[0], 8, 0},//rg,bg for High Color Temperature
	{0xBC, &imx134_wbcalib_data[8], 4, 0},//WB Gb/Gr for High Color Temperature(4byte)
	{0x78, &imx134_wbcalib_data[12], 8, 0},//rg,bg for Low Color Temperature
	{0xC0, &imx134_wbcalib_data[20], 4, 0},//WB Gb/Gr for Low Color Temperature(4byte)
	{0x98, &imx134_wbcalib_data[24], 8, 0},//rg,bg for Fluorescent1
	{0xC4, &imx134_wbcalib_data[32], 4, 0},//WB Gb/Gr for Fluorescent1(4byte)
	{0xD0, &imx134_afcalib_data[0], 2, 0},//dac value for inf
	{0xD4, &imx134_afcalib_data[2], 2, 0},//dac value for macro
	{0xD8, &imx134_afcalib_data[4], 2, 0},// AF scan range of INF
	{0x100, &imx134_lsccalib_data, 768, 0},// AF scan range of INF
};


static struct msm_camera_eeprom_data_t imx134_eeprom_data_tbl[] = {
	{&imx134_wb_data, sizeof(struct msm_calib_wb32)*3},
	{&imx134_af_data, sizeof(struct msm_calib_af)},
	{&imx134_lsc_data, sizeof(struct msm_calib_lsc)},
};



static void imx134_format_wbdata(void)
{
    int i;//,j;
    for(i=0; i<3; i++)
    {
        /*
    	imx134_wb_data[i].r_over_g = (uint32_t)(imx134_wbcalib_data[3+12*i] << 24) | 
            (imx134_wbcalib_data[2+12*i] << 16) |(imx134_wbcalib_data[1+12*i] << 8) |
    		(imx134_wbcalib_data[0+12*i]);
    	imx134_wb_data[i].b_over_g = (uint32_t)(imx134_wbcalib_data[7+12*i] << 24) | 
            (imx134_wbcalib_data[6+12*i] << 16) |(imx134_wbcalib_data[5+12*i] << 8) |
    		(imx134_wbcalib_data[4+12*i]);
    	imx134_wb_data[i].gr_over_gb = (uint32_t)(imx134_wbcalib_data[11+12*i] << 24) | 
            (imx134_wbcalib_data[10+12*i] << 16) |(imx134_wbcalib_data[9+12*i] << 8) |
    		(imx134_wbcalib_data[8+12*i]);
        */
    // Follow IMX134 EEPROM format
    imx134_wb_data[i].r_over_g = (uint32_t)(imx134_wbcalib_data[0+12*i] << 24) | 
            (imx134_wbcalib_data[1+12*i] << 16) |(imx134_wbcalib_data[2+12*i] << 8) |
            (imx134_wbcalib_data[3+12*i]);
    imx134_wb_data[i].b_over_g = (uint32_t)(imx134_wbcalib_data[4+12*i] << 24) | 
            (imx134_wbcalib_data[5+12*i] << 16) |(imx134_wbcalib_data[6+12*i] << 8) |
             (imx134_wbcalib_data[7+12*i]);
     imx134_wb_data[i].gr_over_gb = (uint32_t)(imx134_wbcalib_data[8+12*i] << 24) | 
            (imx134_wbcalib_data[9+12*i] << 16) |(imx134_wbcalib_data[10+12*i] << 8) |
             (imx134_wbcalib_data[11+12*i]);
    }
    /*
    for(i=0; i<3; i++)
    {
        pr_err("%s: array[%d]:\n", __func__, i); 
        for (j=0; j<12; j++)
        {
            pr_err("%d(0x%x),", j, imx134_wbcalib_data[i*12+j]); 
        }
    }
    pr_err("\n"); 
    */
}

static void imx134_format_afdata(void)
{
    //int i;
	imx134_af_data.inf_dac = (uint16_t)(imx134_afcalib_data[0] << 8) |
		imx134_afcalib_data[1];
	imx134_af_data.macro_dac = (uint16_t)(imx134_afcalib_data[2] << 8) |
		imx134_afcalib_data[3];
	imx134_af_data.start_dac = (uint16_t)(imx134_afcalib_data[4] << 8) |
		imx134_afcalib_data[5];

    //for(i=0; i<6; i++)
    //{
        //pr_err("%s: %d(%d)\n", __func__, i, imx134_afcalib_data[i]); 
    //}
}

static void imx134_format_lscdata(void)
{
    int i,j;
    /*
    for(i=0; i<12; i++)
    {
        //pr_err("%s: array[%d]:\n", __func__, i); 
        for (j=0; j<7; j++)
        {
            char strtemp[128];

            strtemp[0] = '\0';

            for (k=0; k<9; k++)
            {
               sprintf(strtemp+strlen(strtemp),"%d(0x%x),", j*9+k, imx134_lsccalib_data[i*64+j*9+k]); 
            }
            //pr_err("%s",strtemp); 
        }
        //pr_err("c=%d(0x%x)", j, imx134_lsccalib_data[63]); 
    }*/

    for(i=0; i<12; i++)
    {
        //pr_err("%s: array[%d]:\n", __func__, i); 
        for (j=0; j<64; j++)
        {
            //pr_err("%d(0x%x),", j, imx134_lsccalib_data[i*64+j]); 
        }
    }

    //r_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.h_r_gain[i] = imx134_lsccalib_data[i];
    }


    //gr_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.h_gr_gain[i] = imx134_lsccalib_data[i+64];
    }

    //gb_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.h_gb_gain[i] = imx134_lsccalib_data[i+64*9];
    }

    //b_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.h_b_gain[i] = imx134_lsccalib_data[i+64*2];
    }

    //r_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.l_r_gain[i] = imx134_lsccalib_data[i+64*3];
    }


    //gr_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.l_gr_gain[i] = imx134_lsccalib_data[i+64*3+64];
    }

    //gb_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.l_gb_gain[i] = imx134_lsccalib_data[i+64*9+64];
    }

    //b_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.l_b_gain[i] = imx134_lsccalib_data[i+64*3+64*2];
    }

    //r_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.f_r_gain[i] = imx134_lsccalib_data[i+64*6];
    }


    //gr_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.f_gr_gain[i] = imx134_lsccalib_data[i+64*6+64];
    }

    //gb_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.f_gb_gain[i] = imx134_lsccalib_data[i+64*9+64*2];
    }

    //b_gain
    for (i=0; i<63; i++)
    {
        imx134_lsc_data.f_b_gain[i] = imx134_lsccalib_data[i+64*6+64*2];
    }
}

void imx134_format_calibrationdata(void)
{
	imx134_format_wbdata();
	imx134_format_afdata();
	imx134_format_lscdata();
}

int imx134_eeprom_init(struct msm_eeprom_ctrl_t *ectrl,
		struct i2c_adapter *adapter)
{
	int32_t /*i = 0,*/rc = 0;
    unsigned char rxdata[64];
    unsigned char memoryaddr[2];

    //pr_err("%s enter", __func__);
    memset(imx134_afcalib_data, sizeof(imx134_afcalib_data), 0);
    memset(imx134_wbcalib_data, sizeof(imx134_wbcalib_data), 0);
    memset(imx134_lsccalib_data, sizeof(imx134_lsccalib_data), 0);
 
   memset((void *)memoryaddr,0,sizeof(memoryaddr));
   /*
   {
	    struct i2c_msg msgs[] = {
		    {
			    .addr  = 0xA1 >> 1,
			    .flags = 0,
			    .len   = 1,
			    .buf   = memoryaddr,
		    },
		    {
			    .addr  = 0xA1 >> 1,
			    .flags = I2C_M_RD,
			    .len   = 64,
			    .buf   = rxdata,
		    },
	    };

        memset((void *)rxdata,0,sizeof(rxdata));
	    rc = i2c_transfer(adapter, msgs, 2);
	    if (rc < 0)
        {
            //pr_err("%s i2c error %d", __func__,rc);
        }

        //pr_err("Read A1");

        for (i=0; i<64; i++)
        {
            //pr_err("RX:%d(0x%x) %c", i, rxdata[i], rxdata[i]); 
        }
    }
    */
    {
	    struct i2c_msg msgs[] = {
		    {
			    .addr  = 0xA3 >> 1,
			    .flags = 0,
			    .len   = 1,
			    .buf   = memoryaddr,
		    },
		    {
			    .addr  = 0xA3 >> 1,
			    .flags = I2C_M_RD,
			    .len   = 256,
			    .buf   = imx134_lsccalib_data,
		    },
	    };

        memset((void *)rxdata,0,sizeof(rxdata));
	    rc = i2c_transfer(adapter, msgs, 2);
	    if (rc < 0)
        {
            //pr_err("%s i2c error %d", __func__,rc);
        }
    }
    {
	    struct i2c_msg msgs[] = {
		    {
			    .addr  = 0xA5 >> 1,
			    .flags = 0,
			    .len   = 1,
			    .buf   = memoryaddr,
		    },
		    {
			    .addr  = 0xA5 >> 1,
			    .flags = I2C_M_RD,
			    .len   = 256,
			    .buf   = imx134_lsccalib_data+0x0100,
		    },
	    };

        memset((void *)rxdata,0,sizeof(rxdata));
	    rc = i2c_transfer(adapter, msgs, 2);
	    if (rc < 0)
        {
            //pr_err("%s i2c error %d", __func__,rc);
        }
    }
    {
	    struct i2c_msg msgs[] = {
		    {
			    .addr  = 0xA7 >> 1,
			    .flags = 0,
			    .len   = 1,
			    .buf   = memoryaddr,
		    },
		    {
			    .addr  = 0xA7 >> 1,
			    .flags = I2C_M_RD,
			    .len   = 256,
			    .buf   = imx134_lsccalib_data+0x0200,
		    },
	    };

        memset((void *)rxdata,0,sizeof(rxdata));
	    rc = i2c_transfer(adapter, msgs, 2);
	    if (rc < 0)
        {
            //pr_err("%s i2c error %d", __func__,rc);
        }
    }

    return 0;
}


static struct msm_eeprom_ctrl_t imx134_eeprom_t = {
	.i2c_driver = &imx134_eeprom_i2c_driver,
	.i2c_addr = 0xA1,
	.eeprom_v4l2_subdev_ops = &imx134_eeprom_subdev_ops,

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	},

	.eeprom_mutex = &imx134_eeprom_mutex,

	.func_tbl = {
		.eeprom_init = imx134_eeprom_init,
		.eeprom_release = NULL,
		.eeprom_get_info = msm_camera_eeprom_get_info,
		.eeprom_get_data = msm_camera_eeprom_get_data,
		.eeprom_set_dev_addr =  NULL,
		.eeprom_format_data = imx134_format_calibrationdata,
	},
	.info = &imx134_calib_supp_info,
	.info_size = sizeof(struct msm_camera_eeprom_info_t),
	.read_tbl = imx134_eeprom_read_tbl,
	.read_tbl_size = ARRAY_SIZE(imx134_eeprom_read_tbl),
	.data_tbl = imx134_eeprom_data_tbl,
	.data_tbl_size = ARRAY_SIZE(imx134_eeprom_data_tbl),
};

subsys_initcall(imx134_eeprom_i2c_add_driver);
MODULE_DESCRIPTION("imx134 EEPROM");
MODULE_LICENSE("GPL v2");
